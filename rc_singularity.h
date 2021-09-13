/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#ifndef _RC_SINGULARITY_H
#define _RC_SINGULARITY_H

#include <stdint.h>

// KVset use flags
#define SING_UF_NOT_PERSISTENT 0x01
#define SING_UF_COUNTERS 0x02
#define SING_UF_PHANTOM_KEYS 0x04

// KVset connection flags
#define SING_CF_MULTICORE_PARSE 0x100
#define SING_CF_PARSE_ERRORS 0x200
#define SING_CF_FULL_LOAD 0x400
#define SING_CF_READER 0x800
#define SING_CF_STAY_LOCKED 0x1000

#ifndef _SING_CSV_FILE
#define _SING_CSV_FILE
typedef struct FSingCSVFileTg
	{
	uint64_t val_col_mask;		// -v key, value columns mask
	char *filename;				// name of file for operation
	uint32_t key_col_num;		// -k key, key column number
	char delimiter; 				// -t key, csv delimiter
	} FSingCSVFile;
#endif

typedef enum ESingLockModeTg
	{
	SING_LM_DEFAULT,
	SING_LM_SIMPLE,
	SING_LM_PROTECTED,
	SING_LM_FAST,
	SING_LM_NONE,
	SING_LM_READ_ONLY
	} ESingLockMode;

#define SING_ERROR 0x80000000

#define SING_ERROR_CONNECTION_LOST (0x0100 | SING_ERROR)
#define SING_ERROR_DATA_CORRUPTED (0x0200 | SING_ERROR)
#define SING_ERROR_INTERNAL (0x0300 | SING_ERROR)
#define SING_ERROR_NO_MEMORY (0x0400 | SING_ERROR)
#define SING_ERROR_NO_SET_MEMORY (0x0500 | SING_ERROR)
#define SING_ERROR_FILE_NOT_FOUND (0x0600 | SING_ERROR)
#define SING_ERROR_OUTPUT_NOT_FOUND (0x0700 | SING_ERROR)
#define SING_ERROR_IMPOSSIBLE_OPERATION (0x0800 | SING_ERROR)
#define SING_ERROR_SYNC_FAILED (0x0900 | SING_ERROR)

#define SING_RESULT_KEY_NOT_FOUND 0x0A00
#define SING_RESULT_IMPOSSIBLE_KEY 0x0B00
#define SING_RESULT_SMALL_BUFFER 0x0C00
#define SING_RESULT_VALUE_DIFFER 0x0D00
#define SING_RESULT_KEY_PRESENT 0x0E00

typedef struct FSingSetTg FSingSet;
typedef struct FKeyHeadTg FKeyHead;
typedef struct FSingConfigTg FSingConfig;
typedef struct FSingCSVFileTg FSingCSVFile;

// Config manipulation
FSingConfig *sing_config_get_default(void);
FSingConfig *sing_config_get_empty(void);
const char *sing_config_get_error(FSingConfig *config);
void sing_config_set_connection_flags(FSingConfig *config,unsigned flags);
void sing_config_set_value_delimiter(FSingConfig *config,char delimiter);
void sing_config_set_base_path(FSingConfig *config,const char *base_path);
void sing_delete_config(FSingConfig *config);

FSingSet *sing_create_set(const char *setname,const FSingCSVFile *csv_file,unsigned keys_count,unsigned flags,unsigned lock_mode,FSingConfig *config);
FSingSet *sing_link_set(const char *setname,unsigned flags,FSingConfig *config);
void sing_unlink_set(FSingSet *index);
void sing_unload_set(FSingSet *index);
void sing_delete_set(FSingSet *index);

unsigned sing_total_count(FSingSet *index);
const char *sing_get_error(FSingSet *index);
uint32_t sing_get_memsize(FSingSet *index);
int sing_check_set(FSingSet *index);
unsigned sing_get_mode(FSingSet *index);

int sing_lock_W(FSingSet *kvset);
int sing_lock_RW(FSingSet *kvset);
int sing_unlock_commit(FSingSet *kvset,uint32_t *saved);
int sing_unlock_revert(FSingSet *kvset);
int sing_flush(FSingSet *kvset,uint32_t *saved);
int sing_revert(FSingSet *kvset);

int sing_add_file(FSingSet *kvset,const FSingCSVFile *csv_file);
int sing_sub_file(FSingSet *kvset,const FSingCSVFile *csv_file);
int sing_diff_file(FSingSet *kvset,const FSingCSVFile *csv_file,const char *outfile);
int sing_diff_replace_file(FSingSet *kvset,const FSingCSVFile *csv_file,const char *outfile);
int sing_intersect_file(FSingSet *kvset,const FSingCSVFile *csv_file,const char *outfile);
int sing_intersect_replace_file(FSingSet *kvset,const FSingCSVFile *csv_file,const char *outfile);
int sing_dump(FSingSet *kvset,char *outfile,unsigned flags);

typedef void *(CSingValueAllocator)(unsigned size);
int sing_get_value_cb_n(FSingSet *kvset,const char *key,unsigned ksize,CSingValueAllocator vacb,void **value,unsigned *vsize);
static inline int sing_get_value_cb(FSingSet *kvset,const char *key,CSingValueAllocator vacb,void **value,unsigned *vsize)
	{ return sing_get_value_cb_n(kvset,key,0xFFFFFFFF,vacb,value,vsize); }

int sing_get_values_cb_n(FSingSet *kvset,const char *const *keys,const unsigned *ksizes,unsigned count,CSingValueAllocator vacb,void **values,unsigned *vsizes,int *results);
static inline int sing_get_values_cb(FSingSet *kvset,const char *const *keys,unsigned count,CSingValueAllocator vacb,void **values,unsigned *vsizes,int *results)
	{ return sing_get_values_cb_n(kvset,keys,NULL,count,vacb,values,vsizes,results); }

int sing_get_value_n(FSingSet *kvset,const char *key,unsigned ksize,void *value,unsigned *vsize);
static inline int sing_get_value(FSingSet *kvset,const char *key,void *value,unsigned *vsize)
	{ return sing_get_value_n(kvset,key,0xFFFFFFFF,value,vsize); }
int sing_get_values_n(FSingSet *kvset,const char *const *keys,const unsigned *ksizes,unsigned count,void *const *values,unsigned *vsizes,int *results);
static inline int sing_get_values(FSingSet *kvset,const char *const *keys,unsigned count,void *const *values,unsigned *vsizes,int *results)
	{ return sing_get_values_n(kvset,keys,NULL,count,values,vsizes,results); }

static inline int sing_get_value32i_n(FSingSet *kvset,const char *key,unsigned ksize,int32_t *value)
	{ unsigned vsize = 4; int rv = sing_get_value_n(kvset,key,ksize,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }
static inline int sing_get_value32i(FSingSet *kvset,const char *key, int32_t *value)
	{ unsigned vsize = 4; *value = 0; int rv = sing_get_value_n(kvset,key,0xFFFFFFFF,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }

static inline int sing_get_value32u_n(FSingSet *kvset,const char *key,unsigned ksize,uint32_t *value)
	{ unsigned vsize = 4; int rv = sing_get_value_n(kvset,key,ksize,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }
static inline int sing_get_value32u(FSingSet *kvset,const char *key, uint32_t *value)
	{ unsigned vsize = 4; *value = 0; int rv = sing_get_value_n(kvset,key,0xFFFFFFFF,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }

static inline int sing_get_value32f_n(FSingSet *kvset,const char *key,unsigned ksize,float *value)
	{ unsigned vsize = 4; int rv = sing_get_value_n(kvset,key,ksize,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }
static inline int sing_get_value32f(FSingSet *kvset,const char *key, float *value)
	{ unsigned vsize = 4; *value = 0; int rv = sing_get_value_n(kvset,key,0xFFFFFFFF,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }

static inline int sing_get_value64i_n(FSingSet *kvset,const char *key,unsigned ksize,int64_t *value)
	{ unsigned vsize = 8; int rv = sing_get_value_n(kvset,key,ksize,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }
static inline int sing_get_value64i(FSingSet *kvset,const char *key, int64_t *value)
	{ unsigned vsize = 8; *value = 0; int rv = sing_get_value_n(kvset,key,0xFFFFFFFF,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }

static inline int sing_get_value64u_n(FSingSet *kvset,const char *key,unsigned ksize,uint64_t *value)
	{ unsigned vsize = 8; int rv = sing_get_value_n(kvset,key,ksize,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }
static inline int sing_get_value64u(FSingSet *kvset,const char *key, uint64_t *value)
	{ unsigned vsize = 8; *value = 0; int rv = sing_get_value_n(kvset,key,0xFFFFFFFF,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }

static inline int sing_get_value64d_n(FSingSet *kvset,const char *key,unsigned ksize,double *value)
	{ unsigned vsize = 8; int rv = sing_get_value_n(kvset,key,ksize,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }
static inline int sing_get_value64d(FSingSet *kvset,const char *key, double *value)
	{ unsigned vsize = 8; *value = 0; int rv = sing_get_value_n(kvset,key,0xFFFFFFFF,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }

static inline int sing_get_pointer_n(FSingSet *kvset,const char *key,unsigned ksize,void **value)
	{ unsigned vsize = sizeof(size_t); int rv = sing_get_value_n(kvset,key,ksize,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }
static inline int sing_get_pointer(FSingSet *kvset,const char *key, void **value)
	{ unsigned vsize = sizeof(size_t); *value = NULL; int rv = sing_get_value_n(kvset,key,0xFFFFFFFF,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }

int sing_key_present_n(FSingSet *kvset,const char *key,unsigned ksize);
static inline int sing_key_present(FSingSet *kvset,const char *key)
	{ return sing_key_present_n(kvset,key,0xFFFFFFFF); }

int sing_keys_present_n(FSingSet *kvset,const char *const *keys,const unsigned *ksizes,unsigned count,int *results);
static inline int sing_keys_present(FSingSet *kvset,const char *const *keys,unsigned count,int *results)
	{ return sing_keys_present_n(kvset,keys,NULL,count,results); }

int sing_value_equal_n(FSingSet *kvset,const char *key,unsigned ksize,void *value,unsigned vsize);
static inline int sing_value_equal(FSingSet *kvset,const char *key,void *value,unsigned vsize)
	{ return sing_value_equal_n(kvset,key,0xFFFFFFFF,value,vsize); }

int sing_set_key(FSingSet *kvset,const char *key,void *value,unsigned vsize);

static inline int sing_set_key32i(FSingSet *kvset,const char *key,int32_t value)
	{ return sing_set_key(kvset,key,&value,4); }
static inline int sing_set_key32u(FSingSet *kvset,const char *key,uint32_t value)
	{ return sing_set_key(kvset,key,&value,4); }
static inline int sing_set_key32f(FSingSet *kvset,const char *key,float value)
	{ return sing_set_key(kvset,key,&value,4); }
static inline int sing_set_key64i(FSingSet *kvset,const char *key,int64_t value)
	{ return sing_set_key(kvset,key,&value,8); }
static inline int sing_set_key64u(FSingSet *kvset,const char *key,uint64_t value)
	{ return sing_set_key(kvset,key,&value,8); }
static inline int sing_set_key64d(FSingSet *kvset,const char *key,double value)
	{ return sing_set_key(kvset,key,&value,8); }
static inline int sing_set_pointer(FSingSet *kvset,const char *key,void *value)
	{ return sing_set_key(kvset,key,&value,sizeof(size_t)); }

int sing_del_key(FSingSet *kvset,const char *key);

typedef int (*CSingIterateCallback)(const char *key,const void *value,unsigned vsize,void *new_value,void *param);
int sing_iterate(FSingSet *kvset,CSingIterateCallback cb,void *param);


#endif 