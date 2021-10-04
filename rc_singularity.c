/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>

#include "config.h"
#include "index.h"
#include "fileparse.h"
#include "locks.h"
#include "filebuf.h"
#include "utils.h"
#include "rc_singularity.h"

// Коллбеки построчной обработки файлов

int std_process(FSingSet *index,FTransformData *tdata, void *cb_param)
	{
	if (tdata->operation & OP_DEL_MASK)
		return idx_key_del(index,tdata);
	return idx_key_set_switch(index,tdata,KS_ADDED | KS_DELETED);
	}

int phantom_process(FSingSet *index,FTransformData *tdata, void *cb_param)
	{
	if (!(tdata->operation < OP_DEL_MASK))
		return idx_key_set_switch(index,tdata,KS_ADDED | KS_DELETED);

	tdata->head.fields.diff_mark = 1;
	if (tdata->head.fields.has_value)
		{
		tdata->head.fields.has_value = 0;
		tdata->value_size = 0;
		if (tdata->head.fields.size == 1)
			tdata->head.fields.extra = tdata->key_rest[0];
		}
	int rv = idx_key_set_switch(index,tdata,KS_ADDED | KS_DELETED);
	tdata->head.fields.diff_mark = 0;
	return rv;
	}

// Коллбеки вывода результатов

static inline void result_output_wval(const FKeyHead *head,const element_type *key_rest,const void *value, unsigned vsize,FWriteBufferSet *resultWbs)
	{
	char *name = fbw_get_ref(resultWbs);
	unsigned size = cd_decode(&name[0],head,key_rest);
	if (vsize)
		{
		name[size++] = '\t';
		memcpy(&name[size],(const char *)value,vsize);
		size += vsize;
		}
	name[size++] = '\n';
	fbw_shift_pos(resultWbs,size);
	}

// Коллбеки дифа

typedef struct FSMWParamTg
	{
	unsigned *new_counters;
	FWriteBufferSet *resultWbs;
	} FSMWParam;

int parse_process_diff(FSingSet *index,FTransformData *tdata, void *cb_param) 
	{
	if (tdata->operation != OP_ADD) 
		return 0;
	int res = idx_key_lookup_switch(index,tdata);

	FSMWParam *smw_param = (FSMWParam *)cb_param;
	if (res & KS_DIFFER)
		{
		fbw_add_sym(smw_param->resultWbs,'!');
		result_output_wval(&(tdata->head.fields),tdata->key_rest,tdata->old_value,tdata->old_value_size,smw_param->resultWbs);
		fbw_commit(smw_param->resultWbs);
		}
	if (res & KS_SUCCESS)
		{
		fbw_add_sym(smw_param->resultWbs,(res & KS_DIFFER) ? '=':'+');
		result_output_wval(&(tdata->head.fields),tdata->key_rest,tdata->value_source,tdata->value_size,smw_param->resultWbs);
		fbw_commit(smw_param->resultWbs);
		}
	if (smw_param->new_counters && (res & KS_MARKED))
		smw_param->new_counters[HASH_TO_COUNTER(tdata->hash)] ++;
	return res;
	}

int parse_process_diff_replace(FSingSet *index,FTransformData *tdata, void *cb_param) 
	{
	if (tdata->operation != OP_ADD) 
		return 0;
	int res = idx_key_set_switch(index,tdata,KS_ADDED | KS_DELETED);

	FSMWParam *smw_param = (FSMWParam *)cb_param;
	if (res & KS_DELETED)
		{
		fbw_add_sym(smw_param->resultWbs,'!');
		result_output_wval(&(tdata->head.fields),tdata->key_rest,tdata->old_value,tdata->old_value_size,smw_param->resultWbs);
		fbw_commit(smw_param->resultWbs);
		}
	if (res & KS_ADDED)
		{
		fbw_add_sym(smw_param->resultWbs,(res & KS_DELETED) ? '=':'+');
		result_output_wval(&(tdata->head.fields),tdata->key_rest,tdata->value_source,tdata->value_size,smw_param->resultWbs);
		fbw_commit(smw_param->resultWbs);
		}
	if (smw_param->new_counters && (res & KS_MARKED))
		smw_param->new_counters[HASH_TO_COUNTER(tdata->hash)] ++;
	return res;
	}

int parse_process_intersect(FSingSet *index,FTransformData *tdata, void *cb_param) 
	{
	if (tdata->operation != OP_ADD) 
		return 0;
	int res = idx_key_lookup_switch(index,tdata);

	FSMWParam *smw_param = (FSMWParam *)cb_param;
	if (smw_param->new_counters && (res & KS_MARKED))
		smw_param->new_counters[HASH_TO_COUNTER(tdata->hash)] ++;
	return res; 
	}

int parse_process_intersect_replace(FSingSet *index,FTransformData *tdata, void *cb_param) 
	{
	if (tdata->operation != OP_ADD) 
		return 0;
	int res = idx_key_set_switch(index,tdata,KS_DELETED);

	FSMWParam *smw_param = (FSMWParam *)cb_param;
	if (smw_param->new_counters && (res & KS_MARKED))
		smw_param->new_counters[HASH_TO_COUNTER(tdata->hash)] ++;
	return res; 
	}

// Функции работы с файлами

static inline int plain_process_file(FSingSet *index,const FSingCSVFile *csv_file,FReadBufferSet *sourceRbs,unsigned invert,void *cb_param)
	{
	fileParseFunc fpc = (index->conn_flags & CF_MULTICORE_PARSE) ? fp_parseFile2 : fp_parseFile;
	parsedError ecb = (index->conn_flags & CF_PARSE_ERRORS) ? std_parse_error : NULL;
	processParsedItem pcb = (index->head->use_flags & UF_PHANTOM_KEYS) ? phantom_process : std_process;
	return (*fpc)(index,csv_file,sourceRbs,pcb,ecb,invert,cb_param);
	}

FSingSet *sing_create_set(const char *setname,const FSingCSVFile *csv_file,unsigned keys_count,unsigned flags,unsigned lock_mode,FSingConfig *config)
	{
	FReadBufferSet *sourceRbs = NULL;
	FSingSet *index;
	off_t filesize = 0;
	FSingConfig *used_config = config;
	if (!used_config && !(used_config = sing_config_get_default()))
		return NULL;
	if (csv_file && csv_file->filename)
		{
		filesize = file_size(csv_file->filename);
		if (filesize == -1 || !(sourceRbs = fbr_create(csv_file->filename)))
			return cnf_set_formatted_error(config,"Source file %s not found",csv_file->filename),NULL;
		if (!keys_count)
			keys_count = fp_countKeys(sourceRbs,filesize) / 4;
		}
	if (!lock_mode)
		lock_mode = setname ? LM_SIMPLE : LM_NONE;

	if ((index = idx_create_set(setname,keys_count,flags,used_config)))
		{
		if (sourceRbs && plain_process_file(index,csv_file,sourceRbs,0,NULL))
			sing_delete_set(index);
		else
			idx_creation_done(index,lock_mode);
		}
	fbr_finish(sourceRbs);
	if (!config)
		sing_delete_config(used_config);
	return index;
	}

FSingSet *sing_link_set(const char *setname,unsigned flags,FSingConfig *config)
	{
	FSingSet *index;
	FSingConfig *used_config = config;
	if (!used_config && !(used_config = sing_config_get_default()))
		return NULL;
	if (!(index = idx_link_set(setname,flags,used_config)) && !config)
		sing_delete_config(used_config);
	return index;
	}

const char *sing_get_error(FSingSet *index)
	{ return index->last_error; }

uint32_t sing_get_memsize(FSingSet *index)
	{ return (index->hashtable_size * KEYHEADS_IN_BLOCK / 2 * KEY_HEAD_SIZE + index->head->pcnt * PAGE_SIZE) / 1024 * ELEMENT_SIZE; }

unsigned sing_total_count(FSingSet *index)
	{ return index->head->count; }

int sing_check_set(FSingSet *index)
	{ return idx_check_all(index,0); }

unsigned sing_get_mode(FSingSet *index)
	{
	if (index->head->use_mutex) return LM_SIMPLE;
	if (index->head->check_mutex) return LM_PROTECTED;
	if (index->head->use_spin) return LM_FAST;
	if (index->head->read_only) return LM_READ_ONLY;
	return LM_NONE;
	}

#define SIMPLE_CALL_CHECKUP(INDEX) if(__atomic_load_n(&(INDEX)->head->bad_states.states.deleted,__ATOMIC_RELAXED) && idx_relink_set(INDEX)) return ERROR_CONNECTION_LOST

int sing_lock_W(FSingSet *kvset)
	{ return lck_manualLock(kvset); }

int sing_unlock_commit(FSingSet *kvset,uint32_t *saved)
	{ return lck_manualUnlock(kvset,1,saved); }

int sing_unlock_revert(FSingSet *kvset)
	{
	if (kvset->head->use_flags & UF_NOT_PERSISTENT)
		return ERROR_IMPOSSIBLE_OPERATION;
	return lck_manualUnlock(kvset,0,NULL); 
	}

int sing_flush(FSingSet *kvset,uint32_t *saved)
	{
	SIMPLE_CALL_CHECKUP(kvset);
	if (kvset->head->use_mutex || kvset->head->check_mutex || kvset->read_only 
			|| kvset->manual_locked || (kvset->head->use_flags & UF_NOT_PERSISTENT))
		return ERROR_IMPOSSIBLE_OPERATION;
	int res = idx_flush(kvset);
	if (res < 0) return res;
	if (saved) *saved = res;
	return 0;
	}

int sing_revert(FSingSet *kvset)
	{
	SIMPLE_CALL_CHECKUP(kvset);
	if (kvset->head->use_mutex || kvset->head->check_mutex || kvset->read_only 
			|| kvset->manual_locked || (kvset->head->use_flags & UF_NOT_PERSISTENT))
		return ERROR_IMPOSSIBLE_OPERATION;
	int res = idx_revert(kvset);
	if (res < 0) return res;
	return 0;
	}

int sing_add_file(FSingSet *kvset,const FSingCSVFile *csv_file)
	{
	FReadBufferSet *sourceRbs;

	if (file_size(csv_file->filename) == -1 || !(sourceRbs = fbr_create(csv_file->filename)))
		return idx_set_formatted_error(kvset,"Source file %s not found",csv_file->filename),ERROR_FILE_NOT_FOUND; 
	int rv = lck_processLock(kvset);
	if (!rv)
		{
		rv = plain_process_file(kvset,csv_file,sourceRbs,0,NULL);
		rv = lck_processUnlock(kvset,rv,1);
		}
	fbr_finish(sourceRbs);
	return rv;
	}

int sing_sub_file(FSingSet *kvset,const FSingCSVFile *csv_file)
	{
	FReadBufferSet *sourceRbs;

	if (file_size(csv_file->filename) == -1 || !(sourceRbs = fbr_create(csv_file->filename)))
		return idx_set_formatted_error(kvset,"Source file %s not found",csv_file->filename),ERROR_FILE_NOT_FOUND; 
	int rv = lck_processLock(kvset);
	if (!rv)
		{
		rv = plain_process_file(kvset,csv_file,sourceRbs,1,NULL);
		rv = lck_processUnlock(kvset,rv,1);
		}
	fbr_finish(sourceRbs);
	return rv;
	}

#define SMW_DIFF 0
#define SMW_DIFF_REPLACE 1
#define SMW_INTERSECT 2
#define SMW_INTERSECT_REPLACE 3

static const processParsedItem op_cbs[4] = {parse_process_diff,parse_process_diff_replace,parse_process_intersect,parse_process_intersect_replace};

static int _sing_marks_work(FSingSet *kvset,const FSingCSVFile *csv_file,const char *outfile,int op)
	{
	FReadBufferSet *rbs = NULL;
	FWriteBufferSet *wbs = NULL;
	FSMWParam smw_param = {NULL,NULL};
	int rv = 0;

	if (kvset->head->use_flags & UF_PHANTOM_KEYS)
		return idx_set_error(kvset,"Share with phantom keys can not be diffed or intersected"), ERROR_IMPOSSIBLE_OPERATION;
	if (file_size(csv_file->filename) == -1 || !(rbs = fbr_create(csv_file->filename)))
		return idx_set_formatted_error(kvset,"Source file %s not found",csv_file->filename), ERROR_FILE_NOT_FOUND; 

	if ((op <= SMW_DIFF_REPLACE || outfile) && !(wbs = fbw_create(outfile)))
		{ 
		idx_set_formatted_error(kvset,"Failed to open %s for writing",outfile); 
		rv = ERROR_OUTPUT_NOT_FOUND;
		goto diff_exit; 
		}

	smw_param.resultWbs = wbs;
	if (kvset->counters && !(smw_param.new_counters = (unsigned *)calloc(COUNTERS_SIZE(kvset->hashtable_size),sizeof(unsigned))))
		{ 
		idx_set_error(kvset,"Failed to allocate memory for counters");
		rv = ERROR_NO_MEMORY;
		goto diff_exit; 
		}

	fileParseFunc fpc = (kvset->conn_flags & CF_MULTICORE_PARSE) ? fp_parseFile2 : fp_parseFile;
	parsedError ecb = (kvset->conn_flags & CF_PARSE_ERRORS) ? std_parse_error : NULL;

	rv = lck_processLock(kvset);
	if (!rv)
		{
		kvset->head->state_flags ^= SF_DIFF_MARK;
		rv = (*fpc)(kvset,csv_file,rbs,op_cbs[op],ecb,0,&smw_param);
		if (!rv)
			switch(op)
				{
				case SMW_DIFF: idx_process_unmarked(kvset,smw_param.new_counters,wbs,0); break;
				case SMW_DIFF_REPLACE: idx_process_unmarked(kvset,smw_param.new_counters,wbs,1); break;
				case SMW_INTERSECT: case SMW_INTERSECT_REPLACE: idx_process_unmarked(kvset,smw_param.new_counters,NULL,1); break;
				}
		rv = lck_processUnlock(kvset,rv,1);
		}
diff_exit:
	if (smw_param.new_counters)
		free(smw_param.new_counters);
	if (wbs)
		fbw_finish(wbs);
	if (rbs)
		fbr_finish(rbs);
	return rv;
	}

int sing_diff_file(FSingSet *kvset,const FSingCSVFile *csv_file,const char *outfile)
	{
	return _sing_marks_work(kvset,csv_file,outfile,SMW_DIFF);
	}

int sing_diff_replace_file(FSingSet *kvset,const FSingCSVFile *csv_file,const char *outfile)
	{
	return _sing_marks_work(kvset,csv_file,outfile,SMW_DIFF_REPLACE);
	}

int sing_intersect_file(FSingSet *kvset,const FSingCSVFile *csv_file)
	{
	return _sing_marks_work(kvset,csv_file,NULL,SMW_INTERSECT);
	}

int sing_intersect_replace_file(FSingSet *kvset,const FSingCSVFile *csv_file)
	{
	return _sing_marks_work(kvset,csv_file,NULL,SMW_INTERSECT_REPLACE);
	}

int sing_dump(FSingSet *index,char *outfile,unsigned flags)
	{
	FWriteBufferSet *wbs = fbw_create(outfile);
	if (!wbs) 
		return idx_set_formatted_error(index,"Failed to open %s for writing",outfile),ERROR_OUTPUT_NOT_FOUND;
	idx_dump_all(index,wbs);
	fbw_finish(wbs);
	return 0;
	}

static inline int _init_tdata(FSingSet *kvset,FTransformData *tdata,const char *key,unsigned ksize,void *value,unsigned vsize)
	{
	tdata->value_source = value;
	tdata->value_size = vsize;
	tdata->head.fields.chain_stop = 1;
	tdata->head.fields.diff_mark = 0;
	if (!key)
		return 1;
	int size = cd_transform(key,ksize,tdata);
	if (size <= 0 || (size < ksize && key[size]))
		return 1;
	tdata->hash = kvset->hashtable_size;
	cd_encode(tdata);
	return 0;
	}

int sing_get_value_cb_n(FSingSet *kvset,const char *key,unsigned ksize,CSingValueAllocator vacb,void **value,unsigned *vsize)
	{
	SIMPLE_CALL_CHECKUP(kvset);
	FTransformData tdata;
	FReaderLock rlock = {0,0,0};
	if(_init_tdata(kvset,&tdata,key,ksize,NULL,0))
		return *value = NULL, *vsize = 0, RESULT_IMPOSSIBLE_KEY;
	return idx_key_get_cb(kvset,&tdata,&rlock,vacb,value,vsize);
	}

int sing_get_values_cb_n(FSingSet *kvset,const char *const *keys,const unsigned *ksizes,unsigned count, CSingValueAllocator vacb, void **values, unsigned *vsizes, int *results)
	{
	SIMPLE_CALL_CHECKUP(kvset);
	FTransformData tdata[2];
	FTransformData *tdatas[2] = {&tdata[0],&tdata[1]};
	FReaderLock rlock = NORMAL_LOCK_INIT;
	unsigned i = 1,rv = 0;
	if (!count)
		return 0;
	if (count > 1)
		rlock.keep = 1;

	if(!_init_tdata(kvset,tdatas[1],keys[0],ksizes ? ksizes[0] : 0xFFFFFFFF,NULL,0))
		__builtin_prefetch(&kvset->hash_table[(tdatas[1]->hash >> 1) * KEYHEADS_IN_BLOCK]);
	else
		values[0] = NULL, vsizes[0] = 0, results[0] = RESULT_IMPOSSIBLE_KEY, tdatas[1] = NULL;
	for (i = 1; i < count; i++)
		{
		if(!_init_tdata(kvset,tdatas[0],keys[i],ksizes ? ksizes[i] : 0xFFFFFFFF,NULL,0))
			__builtin_prefetch(&kvset->hash_table[(tdatas[0]->hash >> 1) * KEYHEADS_IN_BLOCK]);
		else
			values[i] = NULL, vsizes[i] = 0, results[i] = RESULT_IMPOSSIBLE_KEY, tdatas[0] = NULL;
		if (tdatas[1])
			{
			int res = results[i-1] = idx_key_get_cb(kvset,tdatas[1],&rlock,vacb,&values[i-1],&vsizes[i-1]);
			if (__builtin_expect(res < 0,0))
				{
				if (rlock.keeped)
					rlock.keep = 0,lck_readerUnlock(kvset->lock_set,&rlock);
				values[i-1] = NULL, vsizes[i-1] = 0;
				do
					results[i] = res, values[i] = NULL, vsizes[i] = 0;
				while (++i < count);
				return res;
				}
			if (!res)
				rv++;
			}
		tdatas[1] = tdatas[0];
		tdatas[0] = &tdata[i % 2];
		}
	rlock.keep = 0;
	if (tdatas[1])
		{
		int res = results[i-1] = idx_key_get_cb(kvset,tdatas[1],&rlock,vacb,&values[i-1],&vsizes[i-1]);
		if (__builtin_expect(res < 0,0))
			return values[i-1] = NULL, vsizes[i-1] = 0, res;
		if (!res)
			rv++;
		}
	else if (rlock.keeped)
		lck_readerUnlock(kvset->lock_set,&rlock);
	return rv;
	}

int sing_get_value_n(FSingSet *kvset,const char *key,unsigned ksize,void *value,unsigned *vsize)
	{
	SIMPLE_CALL_CHECKUP(kvset);
	FTransformData tdata;
	FReaderLock rlock = {0,0,0};
	if(_init_tdata(kvset,&tdata,key,ksize,NULL,0))
		return *vsize = 0, RESULT_IMPOSSIBLE_KEY;
	return idx_key_get(kvset,&tdata,&rlock,value,vsize);
	}

int sing_get_values_n(FSingSet *kvset, const char *const *keys,const unsigned *ksizes,unsigned count,void *const *values, unsigned *vsizes, int *results)
	{
	SIMPLE_CALL_CHECKUP(kvset);
	FTransformData tdata[2];
	FTransformData *tdatas[2] = {&tdata[0],&tdata[1]};
	FReaderLock rlock = {0,0,0};
	unsigned i = 1,rv = 0;
	if (!count)
		return 0;
	if (count > 1)
		rlock.keep = 1;

	if(!_init_tdata(kvset,tdatas[1],keys[0],ksizes ? ksizes[0] : 0xFFFFFFFF,NULL,0))
		__builtin_prefetch(&kvset->hash_table[(tdatas[1]->hash >> 1) * KEYHEADS_IN_BLOCK]);
	else
		results[0] = RESULT_IMPOSSIBLE_KEY, vsizes[0] = 0, tdatas[1] = NULL;
	for (i = 1; i < count; i++)
		{
		if(!_init_tdata(kvset,tdatas[0],keys[i],ksizes ? ksizes[i] : 0xFFFFFFFF,NULL,0))
			__builtin_prefetch(&kvset->hash_table[(tdatas[0]->hash >> 1) * KEYHEADS_IN_BLOCK]);
		else
			results[i] = RESULT_IMPOSSIBLE_KEY, vsizes[i] = 0, tdatas[0] = NULL;
		if (tdatas[1])
			{
			int res = results[i-1] = idx_key_get(kvset,tdatas[1],&rlock,values[i-1],&vsizes[i-1]);
			if (__builtin_expect(res < 0,0))
				{
				if (rlock.keeped)
					rlock.keep = 0,lck_readerUnlock(kvset->lock_set,&rlock);
				vsizes[i-1] = 0;
				do
					results[i] = res, vsizes[i] = 0;
				while (++i < count);
				return res;
				}
			if (!res)
				rv++;
			}
		tdatas[1] = tdatas[0];
		tdatas[0] = &tdata[i % 2];
		}
	rlock.keep = 0;
	if (tdatas[1])
		{
		int res = results[i-1] = idx_key_get(kvset,tdatas[1],&rlock,values[i-1],&vsizes[i-1]);
		if (__builtin_expect(res < 0,0))
			return vsizes[i-1] = 0,res;
		if (!res)
			rv++;
		}
	else if (rlock.keeped)
		lck_readerUnlock(kvset->lock_set,&rlock);
	return rv;
	}

int sing_key_present_n(FSingSet *kvset,const char *key,unsigned ksize)
	{
	SIMPLE_CALL_CHECKUP(kvset);
	FTransformData tdata;
	FReaderLock rlock = {0,0,0};
	if(_init_tdata(kvset,&tdata,key,ksize,NULL,0))
		return RESULT_IMPOSSIBLE_KEY;
	return idx_key_search(kvset,&tdata,&rlock);
	}

int sing_keys_present_n(FSingSet *kvset, const char *const *keys, const unsigned *ksizes, unsigned count, int *results)
	{
	SIMPLE_CALL_CHECKUP(kvset);
	FTransformData tdata[2];
	FTransformData *tdatas[2] = {&tdata[0],&tdata[1]};
	FReaderLock rlock = {0,0,0};
	unsigned i = 1,rv = 0;
	if (!count)
		return 0;
	if (count > 1)
		rlock.keep = 1;

	if(!_init_tdata(kvset,tdatas[1],keys[0],ksizes ? ksizes[0] : 0xFFFFFFFF,NULL,0))
		__builtin_prefetch(&kvset->hash_table[(tdatas[1]->hash >> 1) * KEYHEADS_IN_BLOCK]);
	else
		results[0] = RESULT_IMPOSSIBLE_KEY, tdatas[1] = NULL;
	for (i = 1; i < count; i++)
		{
		if(!_init_tdata(kvset,tdatas[0],keys[i],ksizes ? ksizes[i] : 0xFFFFFFFF,NULL,0))
			__builtin_prefetch(&kvset->hash_table[(tdatas[0]->hash >> 1) * KEYHEADS_IN_BLOCK]);
		else
			results[i] = RESULT_IMPOSSIBLE_KEY, tdatas[0] = NULL;
		if (tdatas[1])
			{
			int res = results[i-1] = idx_key_search(kvset,tdatas[1],&rlock);
			if (__builtin_expect(res < 0,0))
				{
				if (rlock.keeped)
					rlock.keep = 0,lck_readerUnlock(kvset->lock_set,&rlock);
				do
					results[i] = res;
				while (++i < count);
				return res;
				}
			if (!res)
				rv++;
			}
		tdatas[1] = tdatas[0];
		tdatas[0] = &tdata[i % 2];
		}
	rlock.keep = 0;
	if (tdatas[1])
		{
		int res = results[i-1] = idx_key_search(kvset,tdatas[1],&rlock);
		if (__builtin_expect(res < 0,0))
			return res;
		if (!res)
			rv++;
		}
	else if (rlock.keeped)
		lck_readerUnlock(kvset->lock_set,&rlock);
	return rv;
	}

int sing_value_equal_n(FSingSet *kvset, const char *key, unsigned ksize, void *value, unsigned vsize)
	{
	SIMPLE_CALL_CHECKUP(kvset);
	FTransformData tdata;
	FReaderLock rlock = {0,0,0};
	if(_init_tdata(kvset,&tdata,key,ksize,NULL,0))
		return RESULT_IMPOSSIBLE_KEY;
	return idx_key_compare(kvset,&tdata,&rlock,value,vsize);
	}

int sing_set_key(FSingSet *kvset,const char *key,void *value,unsigned vsize)
	{
	FTransformData tdata;

	if(_init_tdata(kvset,&tdata,key,MAX_KEY_SOURCE,value,vsize))
		return RESULT_IMPOSSIBLE_KEY;

	int rv = lck_processLock(kvset);
	if (rv)
		return rv;
	lck_chainLock(kvset,tdata.hash);
	rv = idx_key_try_set(kvset,&tdata,KS_ADDED | KS_DELETED);
	if (!rv)
		rv = idx_key_set(kvset,&tdata,KS_ADDED | KS_DELETED);
	idx_op_finalize(kvset,&tdata,rv);
	lck_chainUnlock(kvset,tdata.hash);
	rv = lck_processUnlock(kvset,rv,0);
	if (rv & KS_ERROR)
		return rv;
	return ((rv & KS_CHANGED) == KS_ADDED) ? 0 : RESULT_KEY_PRESENT;
	}

int sing_del_key(FSingSet *kvset,const char *key)
	{
	FTransformData tdata;

	if(_init_tdata(kvset,&tdata,key,MAX_KEY_SOURCE,NULL,0))
		return RESULT_IMPOSSIBLE_KEY;

	int rv = lck_processLock(kvset);
	if (rv)
		return rv;
	lck_chainLock(kvset,tdata.hash);
	rv = idx_key_del(kvset,&tdata);
	idx_op_finalize(kvset,&tdata,rv);
	lck_chainUnlock(kvset,tdata.hash);
	rv = lck_processUnlock(kvset,rv,0);
	if (rv < 0)
		return rv;
	return (rv & KS_CHANGED) ? 0 : RESULT_KEY_NOT_FOUND;
	}

int sing_iterate(FSingSet *kvset,CSingIterateCallback cb,void *param)
	{
	int rv = 0;
	if (!kvset->head->read_only && (rv = lck_processLock(kvset)))
		return rv;
	rv = idx_iterate_all(kvset,cb,param);
	if (!kvset->head->read_only)
		rv = lck_processUnlock(kvset,rv,0);
	return rv;
	}

