/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <immintrin.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "filebuf.h"
#include "codec.h"
#include "config.h"
#include "index.h"
#include "locks.h"
#include "fileparse.h"

typedef struct 
	{
	FReadBufferSet *rbs;
	char *buf;
	int pos;
	int size;
	} FReadBufData;

static inline int inc_buf_pos(FReadBufData *bufdata)
	{
	if (++bufdata->pos == bufdata->size)
		{
		if (!(bufdata->buf = fbr_next_block(bufdata->rbs,&bufdata->size)))
			return 0;
		bufdata->pos = 0;
		}
	return 1;
	}

static inline void grep_eols(FReadBufData *bufdata)
	{
	while ((bufdata->buf[bufdata->pos] == '\r' || bufdata->buf[bufdata->pos] == '\n') && inc_buf_pos(bufdata));
	}
	
static inline void scanto_eol(FReadBufData *bufdata)
	{
	char csym;
	do 
		{
		csym = bufdata->buf[bufdata->pos];
		if (!inc_buf_pos(bufdata))
			return;
		}
	while (csym != '\r' && csym != '\n');
	grep_eols(bufdata);
	}
	
// read to column delimiter or eol chars
static inline void scanto_column(FReadBufData *bufdata,char delimiter,unsigned char *vbuf,unsigned *vsize,int *colNum)
	{
	char csym;
	do
		{
		csym = bufdata->buf[bufdata->pos];
		if (csym == delimiter)
			{
			*colNum = inc_buf_pos(bufdata) ? (*colNum) + 1 : 0;
			return;
			}
		if (csym == '\r' || csym == '\n')
			{
			*colNum = 0;
			if (inc_buf_pos(bufdata))	
				grep_eols(bufdata);
			return;
			}
		if (vbuf)
			{
			if (*vsize == MAX_VALUE_SOURCE)
				vbuf = NULL;
			else
				vbuf[(*vsize)++] = csym;
			}
		}
	while (inc_buf_pos(bufdata));
	*colNum = 0;
	}
	
#define TWO_CORE_CHAIN_SIZE 24
#define TWO_CORE_CHAINS_COUNT 4

#define CACHE_ALIGNED_CHAR(A) (CACHE_LINE_SIZE * ((A)/CACHE_LINE_SIZE + (((A)%CACHE_LINE_SIZE)?1:0)))

typedef struct FProcessChainTg
	{
	unsigned char values[TWO_CORE_CHAIN_SIZE][CACHE_ALIGNED_CHAR(MAX_VALUE_SOURCE)] __attribute__ ((aligned (CACHE_LINE_SIZE)));;
	FTransformData tdata[TWO_CORE_CHAIN_SIZE];
	unsigned ctdata;
	unsigned state; // 0 - filling, 1 - processing, 2 - error
	} __attribute__ ((aligned (CACHE_LINE_SIZE))) FProcessChain;

static inline void pchain_init(FProcessChain *pchain, unsigned diff_mark)
	{
	unsigned i;
	for (i = 0; i < TWO_CORE_CHAIN_SIZE; i++)
		{
		pchain->tdata[i].value_source = pchain->values[i];
		pchain->tdata[i].head.fields.chain_stop = 1;
		pchain->tdata[i].head.fields.diff_mark = diff_mark;
		}
	pchain->ctdata = 0; 
	pchain->state = 0;
	}

static inline void pchain_clear_locks(FSingSet *index,FTransformData **tdatas,int res)
	{
	if (tdatas[1])
		lck_chainUnlock(index,tdatas[1]->hash);
	if (tdatas[2])
		lck_chainUnlock(index,tdatas[2]->hash);
	}

static inline int pchain_step(FSingSet *index,FTransformData **tdatas,processParsedItem pcb,void *cb_param)
	{
	unsigned res;
	int got_lock = 1;
	if (tdatas[0])
		__builtin_prefetch(&index->hash_table[(tdatas[0]->hash >> 1) * KEYHEADS_IN_BLOCK]);
	if (tdatas[1])
		{
		if (tdatas[2])  // Предупреждение дедлока
			got_lock = (tdatas[1]->hash >> 1 == tdatas[2]->hash >> 1) ? 0 : lck_tryChainLock(index,tdatas[1]->hash);
		else
			lck_chainLock(index,tdatas[1]->hash);
		if (got_lock)
			{
			if ((res = (*pcb)(index,tdatas[1],cb_param)))
				{ // Закончили действие на этом шаге
				if (__builtin_expect(res & KS_ERROR,0))
					return pchain_clear_locks(index,tdatas,res),1;
				idx_op_finalize(index,tdatas[1],res);
				lck_chainUnlock(index,tdatas[1]->hash);
				tdatas[1] = NULL;
				}
			else // Элемент не найден в хеш-таблице, идем в цепочку
				__builtin_prefetch(regionPointer(index,*(tdatas[1]->chain_idx_ref),KEY_HEAD_SIZE));
			}
		}
	if (!tdatas[2])
		return 0;
	res = (*pcb)(index,tdatas[2],cb_param);
	if (__builtin_expect(res & KS_ERROR,0))
		return pchain_clear_locks(index,tdatas,res),1;
	idx_op_finalize(index,tdatas[2],res);
	if (got_lock)
		{
		lck_chainUnlock(index,tdatas[2]->hash); // Разблокируем цепочку
		return 0;
		}
	if (tdatas[2]->hash >> 1 != tdatas[1]->hash >> 1)
		{
		lck_chainUnlock(index,tdatas[2]->hash);
		lck_chainLock(index,tdatas[1]->hash);
		}
	if ((res = (*pcb)(index,tdatas[1],cb_param)))
		{ 
		if (__builtin_expect(res & KS_ERROR,0))
			{
			lck_memoryUnlock(index);
			lck_chainUnlock(index,tdatas[1]->hash);
			return 1;
			}
		idx_op_finalize(index,tdatas[1],res);
		lck_chainUnlock(index,tdatas[1]->hash);
		tdatas[1] = NULL;
		return 0;
		}
	__builtin_prefetch(regionPointer(index,*(tdatas[1]->chain_idx_ref),KEY_HEAD_SIZE));
	return 0;
	}

static inline int pchain_tail(FSingSet *index,FTransformData **tdatas,processParsedItem pcb,void *cb_param)
	{
	tdatas[0] = NULL; // Нет новых данных
	if (pchain_step(index,tdatas,pcb,cb_param)) return 1;
	tdatas[2] = tdatas[1];
	tdatas[1] = tdatas[0] = NULL;
	if (pchain_step(index,tdatas,pcb,cb_param)) return 1;
	tdatas[2] = NULL;
	return 0;
	}

typedef struct FProcessThreadParamsTg
	{
	FSingSet *index;
	processParsedItem pcb;
	FProcessChain *pchains;
	void *cb_param;
	} FProcessThreadParams;

void *process_thread(void *arg)
	{
	unsigned i,size,state;
	unsigned pcnum = 0;
	FProcessThreadParams *ptp = (FProcessThreadParams *)arg;
	FSingSet *index = ptp->index;
	processParsedItem pcb = ptp->pcb;
	FProcessChain *pchains = ptp->pchains;
	FProcessChain *wchain;

	FTransformData *tdatas[3] = {NULL,NULL,NULL};
	unsigned *need_release = NULL;

	do 
		{
		wchain = &pchains[pcnum];
		if (!(state = __atomic_load_n(&wchain->state,__ATOMIC_ACQUIRE)))
			{ // Парсер запаздывает, пока разберем хвост
			if (need_release)
				{
				if (pchain_tail(index,tdatas,pcb,ptp->cb_param))
					return __atomic_store_n(need_release,2,__ATOMIC_RELEASE),(void *)1;
				__atomic_store_n(need_release,0,__ATOMIC_RELEASE);
				need_release = NULL;
				}
			while (!(state = __atomic_load_n(&wchain->state,__ATOMIC_ACQUIRE)))
				_mm_pause();
			}
		size = wchain->ctdata;
		i = 0;
		if (need_release)
			{ 
			tdatas[0] = size ? &wchain->tdata[0] : NULL;
			if (pchain_step(index,tdatas,pcb,ptp->cb_param))
				return __atomic_store_n(need_release,2,__ATOMIC_RELEASE),(void *)1;
			tdatas[2] = tdatas[1];
			tdatas[1] = tdatas[0];
			tdatas[0] = (size > 1) ? &wchain->tdata[1] : NULL;
			if (pchain_step(index,tdatas,pcb,ptp->cb_param))
				return __atomic_store_n(need_release,2,__ATOMIC_RELEASE),(void *)1;
			tdatas[2] = tdatas[1];
			tdatas[1] = tdatas[0];			
			__atomic_store_n(need_release,0,__ATOMIC_RELEASE);
			i = 2;
			}
		for (;i < size; i++)
			{
			tdatas[0] = &wchain->tdata[i];
			if (pchain_step(index,tdatas,pcb,ptp->cb_param))
				return __atomic_store_n(&wchain->state,2,__ATOMIC_RELEASE),(void *)1;
			tdatas[2] = tdatas[1];
			tdatas[1] = tdatas[0];
			}
		pcnum = (pcnum + 1) % TWO_CORE_CHAINS_COUNT;
		need_release = &wchain->state;
		}
	while(size);
	if (pchain_tail(index,tdatas,pcb,ptp->cb_param))
		return __atomic_store_n(&wchain->state,2,__ATOMIC_RELEASE),(void *)1;
	return (void *)0;
	}

int _process_string(FSingSet *index,const FSingCSVFile *csv_format,FReadBufData *bufdata,FTransformData *tdata,parsedError ecb,int invert_operation)
	{
	int oldpos,ppos,keyfound = 0,column = 0;
	char share_delimiter = index->head->delimiter;

	tdata->value_size = 0;
	while (bufdata->buf)
		{
		if (column != csv_format->key_col_num)
			{
			if (column < 64 && (1LL << column) & csv_format->val_col_mask)
				{
				if (tdata->value_size)
					tdata->value_source[tdata->value_size++] = share_delimiter;
				scanto_column(bufdata,csv_format->delimiter,tdata->value_source,&tdata->value_size,&column);
				}
			else
				scanto_column(bufdata,csv_format->delimiter,NULL,NULL,&column);
			if (!column)
				return keyfound;
			continue;
			}
		if (bufdata->size - bufdata->pos < KEY_BUFFER_SIZE 
					&& !(bufdata->buf = fbr_next_block_partial(bufdata->rbs,&bufdata->size,&bufdata->pos)))
			{
			if (ecb) (*ecb)(&bufdata->buf[bufdata->pos]);
			return scanto_eol(bufdata),0;
			}

		oldpos = bufdata->pos;
		bufdata->pos += cd_opscan(&bufdata->buf[bufdata->pos],tdata,invert_operation);
		if (tdata->operation == OP_OLD)
			return scanto_eol(bufdata),0;

		if (!(ppos = cd_transform(&bufdata->buf[bufdata->pos],MAX_KEY_SOURCE,tdata)))
			{
			if (ecb) (*ecb)(&bufdata->buf[oldpos]);
			scanto_eol(bufdata);
			return 0;
			}
		if ((bufdata->pos += ppos) == bufdata->size) // Это возможно только если файл закончился
			return bufdata->buf = NULL,1;

		char sym = bufdata->buf[bufdata->pos];
		if (sym == csv_format->delimiter)
			{ 
			keyfound = 1; 
			column++; 
			inc_buf_pos(bufdata);
			continue;
			}
		column = 0;
		if (sym != '\n' && sym != '\r')
			{
			if (ecb) (*ecb)(&bufdata->buf[oldpos]);
			scanto_eol(bufdata);
			return 0;
			}
		if (inc_buf_pos(bufdata))	
			grep_eols(bufdata);
		return 1;
		}
	return keyfound;
	}

int fp_parseFile2(FSingSet *index,const FSingCSVFile *csv_format,FReadBufferSet *sourceRbs,processParsedItem pcb,parsedError ecb,unsigned invert,void *cb_param)
	{
	FReadBufData bufdata;
	int rv;
	
	bufdata.rbs = sourceRbs;
	bufdata.size = bufdata.pos = 0;
	unsigned diff_mark = index->head->state_flags & SF_DIFF_MARK;
	
	FProcessChain *pchains;
	if (posix_memalign((void **)&pchains,CACHE_LINE_SIZE,sizeof(FProcessChain) * 4))
		return 1;

	pchain_init(&pchains[0],diff_mark);
	pchain_init(&pchains[1],diff_mark);
	pchain_init(&pchains[2],diff_mark);
	pchain_init(&pchains[3],diff_mark);
	unsigned pcnum = 1, state;
	FProcessChain *wchain;
	FTransformData *tdata;

	FProcessThreadParams ptp = {index,pcb,pchains,cb_param};
	
	if (!(bufdata.buf = fbr_first_block(sourceRbs,&bufdata.size)))
		return free(pchains),0;

	wchain = &pchains[0];
	tdata = &wchain->tdata[0];
	
	pthread_t proc_thread;

	while (bufdata.buf)
		{
		if (!_process_string(index,csv_format,&bufdata,tdata,ecb,invert))
			continue;
		tdata->hash = index->hashtable_size;
		cd_encode(tdata);

		if (++wchain->ctdata == TWO_CORE_CHAIN_SIZE)
			break;
		tdata++;
		}
	if (!wchain->ctdata)
		return free(pchains),0;
	__atomic_store_n(&wchain->state,1,__ATOMIC_RELEASE);
	wchain = &pchains[1];
	tdata = &wchain->tdata[0];
	pthread_create(&proc_thread, NULL, process_thread, &ptp);
	while (bufdata.buf)
		{
		if (!_process_string(index,csv_format,&bufdata,tdata,ecb,invert))
			continue;
		tdata->hash = index->hashtable_size;
		cd_encode(tdata);

		if (++wchain->ctdata == TWO_CORE_CHAIN_SIZE)
			{
			__atomic_store_n(&wchain->state,1,__ATOMIC_RELEASE);
			pcnum = (pcnum + 1) % TWO_CORE_CHAINS_COUNT;
			wchain = &pchains[pcnum];
			while ((state = __atomic_load_n(&wchain->state,__ATOMIC_ACQUIRE)) == 1)
				_mm_pause();
			if (state == 2)
				goto fp_parseFile2_exit;
			wchain->ctdata = 0;
			tdata = &wchain->tdata[0];
			}
		else
			tdata++;
		}
	__atomic_store_n(&wchain->state,1,__ATOMIC_RELEASE);
	if (wchain->ctdata)
		{
		pcnum = (pcnum + 1) % TWO_CORE_CHAINS_COUNT;
		wchain = &pchains[pcnum];
		while ((state = __atomic_load_n(&wchain->state,__ATOMIC_ACQUIRE)) == 1)
			_mm_pause();
		if (state == 2)
			goto fp_parseFile2_exit;
		wchain->ctdata = 0;
		__atomic_store_n(&wchain->state,1,__ATOMIC_RELEASE);
		}
fp_parseFile2_exit:
	pthread_join(proc_thread,(void**)&rv);
	return free(pchains),rv;
	}

#define SINGLE_CHAIN_SIZE 4

int fp_parseFile(FSingSet *index,const FSingCSVFile *csv_format,FReadBufferSet *sourceRbs,processParsedItem pcb,parsedError ecb,unsigned invert,void *cb_param)
	{
	FReadBufData bufdata;
	bufdata.rbs = sourceRbs;
	bufdata.size = bufdata.pos = 0;
	
	unsigned i,ctdata = 0 ;
	FTransformData tdata[SINGLE_CHAIN_SIZE];
	unsigned char *values;
	if (posix_memalign((void **)&values,CACHE_LINE_SIZE,SINGLE_CHAIN_SIZE * CACHE_ALIGNED_CHAR(MAX_VALUE_SOURCE)))
		return 1;	

	FTransformData *tdatas[3] = {&tdata[0],NULL,NULL};
	unsigned diff_mark = index->head->state_flags & SF_DIFF_MARK;

	for (i = 0; i < SINGLE_CHAIN_SIZE; i++)
		{
		tdata[i].value_source = &values[i * CACHE_ALIGNED_CHAR(MAX_VALUE_SOURCE)];
		tdata[i].head.fields.chain_stop = 1;
		tdata[i].head.fields.diff_mark = diff_mark;
		}
	
	if (!(bufdata.buf = fbr_first_block(sourceRbs,&bufdata.size)))
		return free(values),0;
	
	while (bufdata.buf)
		{
		if (!_process_string(index,csv_format,&bufdata,tdatas[0],ecb,invert))
			continue;
		tdatas[0]->hash = index->hashtable_size;
		cd_encode(tdatas[0]);

		if (pchain_step(index,tdatas,pcb,cb_param))
			return free(values),1;
	
		ctdata = (ctdata + 1) % SINGLE_CHAIN_SIZE;
		tdatas[2] = tdatas[1];
		tdatas[1] = tdatas[0];
		tdatas[0] = &tdata[ctdata];
		}
	int rv = pchain_tail(index,tdatas,pcb,cb_param);
	free(values);
	return rv;
	}
	
void std_parse_error(char *buf)
	{
	int i = 0;
	char errline[MAX_KEY_SOURCE + 1];

	for (i = 0; i < MAX_KEY_SOURCE; i++)
		{
		switch (errline[i] = buf[i])
			{
			case '\r': case '\n': case 0: goto parse_error_fnd;
			}
		}
parse_error_fnd:
	errline[i] = 0;
	fprintf(stderr,"Bad character found in domain name %s\n",errline);
	}
	
int fp_countKeys(FReadBufferSet *sourceRbs,off_t file_size)
	{
	int cnt = 0;
	char *block;
	int size = 0,pos = 0,lastpos = 0;
	block = fbr_first_block(sourceRbs,&size);
	if (!block) return 0;
	
	while (1)
		{
		lastpos = pos;
		while (block[pos] != '\r' && block[pos] != '\n')
			{
			pos++;
			if (pos >= size)
				goto fp_countKeys_exit;
			}
		cnt++;
		pos++;
		while (block[pos] == '\r' || block[pos] == '\n')
			{
			pos++;
			if (pos >= size)
				goto fp_countKeys_exit;
			}
		}
fp_countKeys_exit:		
	if (!lastpos) return 0;
	return (int)((uint64_t)file_size * cnt / lastpos);
	}
