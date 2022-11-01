/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#ifndef _RC_SINGULARITY_H
#define _RC_SINGULARITY_H

#ifdef __INTELLISENSE__
#define __null 0
#endif

#include <stddef.h>
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

// config manipulation

FSingConfig *sing_config_get_default(void);
FSingConfig *sing_config_get_empty(void);
const char *sing_config_get_error(FSingConfig *config);
void sing_config_set_connection_flags(FSingConfig *config,unsigned flags);
void sing_config_set_value_delimiter(FSingConfig *config,char delimiter);
void sing_config_set_base_path(FSingConfig *config,const char *base_path);
void sing_delete_config(FSingConfig *config);

// set manipulation

FSingSet *sing_create_set(const char *setname,const FSingCSVFile *csv_file,unsigned keys_count,unsigned flags,unsigned lock_mode,FSingConfig *config);
FSingSet *sing_link_set(const char *setname,unsigned flags,FSingConfig *config);
void sing_unlink_set(FSingSet *index);
void sing_unload_set(FSingSet *index);
void sing_delete_set(FSingSet *index);

// utility calls 

unsigned sing_total_count(FSingSet *index);
const char *sing_get_error(FSingSet *index);
uint32_t sing_get_memsize(FSingSet *index);
int sing_check_set(FSingSet *index);
unsigned sing_get_mode(FSingSet *index);

// locks and disk sinchronisation

int sing_lock_W(FSingSet *kvset);
int sing_unlock_commit(FSingSet *kvset,uint32_t *saved);
int sing_unlock_revert(FSingSet *kvset);
int sing_flush(FSingSet *kvset,uint32_t *saved);
int sing_revert(FSingSet *kvset);

// file calls

int sing_add_file(FSingSet *kvset,const FSingCSVFile *csv_file);
int sing_sub_file(FSingSet *kvset,const FSingCSVFile *csv_file);
int sing_remove_file(FSingSet *kvset,const FSingCSVFile *csv_file);
int sing_diff_file(FSingSet *kvset,const FSingCSVFile *csv_file,const char *outfile);
int sing_diff_replace_file(FSingSet *kvset,const FSingCSVFile *csv_file,const char *outfile);
int sing_intersect_file(FSingSet *kvset,const FSingCSVFile *csv_file);
int sing_intersect_replace_file(FSingSet *kvset,const FSingCSVFile *csv_file);
int sing_dump(FSingSet *kvset,char *outfile,unsigned flags);

// simple reading calls

#define SING_KEY_SIZE_UNKNOWN 0xFFFFFFFF

int sing_key_present_n(FSingSet *kvset,const char *key,unsigned ksize);
int sing_phantom_present_n(FSingSet *kvset,const char *key,unsigned ksize);

static inline int sing_key_present(FSingSet *kvset,const char *key)
	{ return sing_key_present_n(kvset,key,SING_KEY_SIZE_UNKNOWN); }

static inline int sing_phantom_present(FSingSet *kvset,const char *key)
	{ return sing_phantom_present_n(kvset,key,SING_KEY_SIZE_UNKNOWN); }

int sing_keys_present_n(FSingSet *kvset,const char *const *keys,const unsigned *ksizes,unsigned count,int *results);

static inline int sing_keys_present(FSingSet *kvset,const char *const *keys,unsigned count,int *results)
	{ return sing_keys_present_n(kvset,keys,NULL,count,results); }

int sing_value_equal_n(FSingSet *kvset,const char *key,unsigned ksize,const void *value,unsigned vsize);
int sing_phantom_equal_n(FSingSet *kvset,const char *key,unsigned ksize,const void *value,unsigned vsize);

static inline int sing_value_equal(FSingSet *kvset,const char *key,const void *value,unsigned vsize)
	{ return sing_value_equal_n(kvset,key,SING_KEY_SIZE_UNKNOWN,value,vsize); }

static inline int sing_phantom_equal(FSingSet *kvset,const char *key,const void *value,unsigned vsize)
	{ return sing_phantom_equal_n(kvset,key,SING_KEY_SIZE_UNKNOWN,value,vsize); }

int sing_values_equal_n(FSingSet *kvset,const char *const *keys, const unsigned *ksizes, unsigned count,const void **values,const unsigned *vsizes,int *results);

static inline int sing_values_equal(FSingSet *kvset,const char *const *keys, unsigned count,const void **values,const unsigned *vsizes,int *results)
	{ return sing_values_equal_n(kvset,keys,NULL,count,values,vsizes,results); }

// callback getting of one key

typedef void *(CSingValueAllocator)(unsigned size);

int sing_get_value_cb_n(FSingSet *kvset,const char *key,unsigned ksize,CSingValueAllocator vacb,void **value,unsigned *vsize);
int sing_get_phantom_cb_n(FSingSet *kvset,const char *key,unsigned ksize,CSingValueAllocator vacb,void **value,unsigned *vsize);

static inline int sing_get_value_cb(FSingSet *kvset,const char *key,CSingValueAllocator vacb,void **value,unsigned *vsize)
	{ return sing_get_value_cb_n(kvset,key,SING_KEY_SIZE_UNKNOWN,vacb,value,vsize); }

static inline int sing_get_phantom_cb(FSingSet *kvset,const char *key,CSingValueAllocator vacb,void **value,unsigned *vsize)
	{ return sing_get_phantom_cb_n(kvset,key,SING_KEY_SIZE_UNKNOWN,vacb,value,vsize); }

// callback getting of many keys

int sing_get_values_cb_n(FSingSet *kvset,const char *const *keys,const unsigned *ksizes,unsigned count,CSingValueAllocator vacb,void **values,unsigned *vsizes,int *results);

static inline int sing_get_values_cb(FSingSet *kvset,const char *const *keys,unsigned count,CSingValueAllocator vacb,void **values,unsigned *vsizes,int *results)
	{ return sing_get_values_cb_n(kvset,keys,NULL,count,vacb,values,vsizes,results); }

// preallocates getting of one key

int sing_get_value_n(FSingSet *kvset,const char *key,unsigned ksize,void *value,unsigned *vsize);
int sing_get_phantom_n(FSingSet *kvset,const char *key,unsigned ksize,void *value,unsigned *vsize);

static inline int sing_get_value(FSingSet *kvset,const char *key,void *value,unsigned *vsize)
	{ return sing_get_value_n(kvset,key,SING_KEY_SIZE_UNKNOWN,value,vsize); }

static inline int sing_get_phantom(FSingSet *kvset,const char *key,void *value,unsigned *vsize)
	{ return sing_get_phantom_n(kvset,key,SING_KEY_SIZE_UNKNOWN,value,vsize); }

static inline int sing_get_value32i_n(FSingSet *kvset,const char *key,unsigned ksize,int32_t *value)
	{ unsigned vsize = 4; int rv = sing_get_value_n(kvset,key,ksize,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }
static inline int sing_get_value32i(FSingSet *kvset,const char *key, int32_t *value)
	{ unsigned vsize = 4; *value = 0; int rv = sing_get_value_n(kvset,key,SING_KEY_SIZE_UNKNOWN,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }

static inline int sing_get_value32u_n(FSingSet *kvset,const char *key,unsigned ksize,uint32_t *value)
	{ unsigned vsize = 4; int rv = sing_get_value_n(kvset,key,ksize,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }
static inline int sing_get_value32u(FSingSet *kvset,const char *key, uint32_t *value)
	{ unsigned vsize = 4; *value = 0; int rv = sing_get_value_n(kvset,key,SING_KEY_SIZE_UNKNOWN,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }

static inline int sing_get_value32f_n(FSingSet *kvset,const char *key,unsigned ksize,float *value)
	{ unsigned vsize = 4; int rv = sing_get_value_n(kvset,key,ksize,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }
static inline int sing_get_value32f(FSingSet *kvset,const char *key, float *value)
	{ unsigned vsize = 4; *value = 0; int rv = sing_get_value_n(kvset,key,SING_KEY_SIZE_UNKNOWN,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }

static inline int sing_get_value64i_n(FSingSet *kvset,const char *key,unsigned ksize,int64_t *value)
	{ unsigned vsize = 8; int rv = sing_get_value_n(kvset,key,ksize,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }
static inline int sing_get_value64i(FSingSet *kvset,const char *key, int64_t *value)
	{ unsigned vsize = 8; *value = 0; int rv = sing_get_value_n(kvset,key,SING_KEY_SIZE_UNKNOWN,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }

static inline int sing_get_value64u_n(FSingSet *kvset,const char *key,unsigned ksize,uint64_t *value)
	{ unsigned vsize = 8; int rv = sing_get_value_n(kvset,key,ksize,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }
static inline int sing_get_value64u(FSingSet *kvset,const char *key, uint64_t *value)
	{ unsigned vsize = 8; *value = 0; int rv = sing_get_value_n(kvset,key,SING_KEY_SIZE_UNKNOWN,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }

static inline int sing_get_value64d_n(FSingSet *kvset,const char *key,unsigned ksize,double *value)
	{ unsigned vsize = 8; int rv = sing_get_value_n(kvset,key,ksize,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }
static inline int sing_get_value64d(FSingSet *kvset,const char *key, double *value)
	{ unsigned vsize = 8; *value = 0; int rv = sing_get_value_n(kvset,key,SING_KEY_SIZE_UNKNOWN,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }

static inline int sing_get_pointer_n(FSingSet *kvset,const char *key,unsigned ksize,void **value)
	{ unsigned vsize = sizeof(size_t); int rv = sing_get_value_n(kvset,key,ksize,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }
static inline int sing_get_pointer(FSingSet *kvset,const char *key, void **value)
	{ unsigned vsize = sizeof(size_t); *value = NULL; int rv = sing_get_value_n(kvset,key,SING_KEY_SIZE_UNKNOWN,value,&vsize); return (rv == SING_RESULT_SMALL_BUFFER) ? 0 : rv; }

// preallocated getting of many keys

int sing_get_values_n(FSingSet *kvset,const char *const *keys,const unsigned *ksizes,unsigned count,void *const *values,unsigned *vsizes,int *results);

static inline int sing_get_values(FSingSet *kvset,const char *const *keys,unsigned count,void *const *values,unsigned *vsizes,int *results)
	{ return sing_get_values_n(kvset,keys,NULL,count,values,vsizes,results); }

int sing_get_values_simple_n(FSingSet *kvset,const char *const *keys,const unsigned *ksizes,unsigned count,void **values,unsigned *vsizes,int *results);

static inline int sing_get_values_simple(FSingSet *kvset,const char *const *keys,unsigned count,void **values,unsigned *vsizes,int *results)
	{ return sing_get_values_simple_n(kvset,keys,NULL,count,values,vsizes,results); }

int sing_get_values_same_n(FSingSet *kvset,const char *const *keys,const unsigned *ksizes,unsigned count,void *values,unsigned vsize,int *results);

static inline int sing_get_values_same(FSingSet *kvset,const char *const *keys,unsigned count,void *values,unsigned vsize,int *results)
	{ return sing_get_values_same_n(kvset,keys,NULL,count,values,vsize,results); }

static inline int sing_get_values32i_n(FSingSet *kvset,const char *const *keys,const unsigned *ksizes,unsigned count,int32_t *values,int *results)
	{ return sing_get_values_same_n(kvset,keys,ksizes,count,values,sizeof(int32_t),results); }
static inline int sing_get_values32i(FSingSet *kvset,const char *const *keys,unsigned count,int32_t *values,int *results)
	{ return sing_get_values_same_n(kvset,keys,NULL,count,values,sizeof(int32_t),results); }

static inline int sing_get_values32u_n(FSingSet *kvset,const char *const *keys,const unsigned *ksizes,unsigned count,uint32_t *values,int *results)
	{ return sing_get_values_same_n(kvset,keys,ksizes,count,values,sizeof(uint32_t),results); }
static inline int sing_get_values32u(FSingSet *kvset,const char *const *keys,unsigned count,uint32_t *values,int *results)
	{ return sing_get_values_same_n(kvset,keys,NULL,count,values,sizeof(uint32_t),results); }

static inline int sing_get_values32f_n(FSingSet *kvset,const char *const *keys,const unsigned *ksizes,unsigned count,float *values,int *results)
	{ return sing_get_values_same_n(kvset,keys,ksizes,count,values,sizeof(float),results); }
static inline int sing_get_values32f(FSingSet *kvset,const char *const *keys,unsigned count,float *values,int *results)
	{ return sing_get_values_same_n(kvset,keys,NULL,count,values,sizeof(float),results); }

static inline int sing_get_values64i_n(FSingSet *kvset,const char *const *keys,const unsigned *ksizes,unsigned count,int64_t *values,int *results)
	{ return sing_get_values_same_n(kvset,keys,ksizes,count,values,sizeof(int64_t),results); }
static inline int sing_get_values64i(FSingSet *kvset,const char *const *keys,unsigned count,int64_t *values,int *results)
	{ return sing_get_values_same_n(kvset,keys,NULL,count,values,sizeof(int64_t),results); }

static inline int sing_get_values64u_n(FSingSet *kvset,const char *const *keys,const unsigned *ksizes,unsigned count,uint64_t *values,int *results)
	{ return sing_get_values_same_n(kvset,keys,ksizes,count,values,sizeof(uint64_t),results); }
static inline int sing_get_values64u(FSingSet *kvset,const char *const *keys,unsigned count,uint64_t *values,int *results)
	{ return sing_get_values_same_n(kvset,keys,NULL,count,values,sizeof(uint64_t),results); }

static inline int sing_get_values64d_n(FSingSet *kvset,const char *const *keys,const unsigned *ksizes,unsigned count,double *values,int *results)
	{ return sing_get_values_same_n(kvset,keys,ksizes,count,values,sizeof(double),results); }
static inline int sing_get_values64d(FSingSet *kvset,const char *const *keys,unsigned count,double *values,int *results)
	{ return sing_get_values_same_n(kvset,keys,NULL,count,values,sizeof(double),results); }

static inline int sing_get_pointers_n(FSingSet *kvset,const char *const *keys,const unsigned *ksizes,unsigned count,void **values,int *results)
	{ return sing_get_values_same_n(kvset,keys,ksizes,count,values,sizeof(void *),results); }
static inline int sing_get_pointers(FSingSet *kvset,const char *const *keys,unsigned count,void **values,int *results)
	{ return sing_get_values_same_n(kvset,keys,NULL,count,values,sizeof(void *),results); }

// modification calls

int sing_add_key_n(FSingSet *kvset,const char *key,unsigned ksize,void *value,unsigned vsize);
int sing_add_phantom_n(FSingSet *kvset,const char *key,unsigned ksize,void *value,unsigned vsize);

static inline int sing_add_key(FSingSet *kvset,const char *key,void *value,unsigned vsize)
	{ return sing_add_key_n(kvset,key,SING_KEY_SIZE_UNKNOWN,value,vsize); }

static inline int sing_add_phantom(FSingSet *kvset,const char *key,void *value,unsigned vsize)
	{ return sing_add_phantom_n(kvset,key,SING_KEY_SIZE_UNKNOWN,value,vsize); }

int sing_add_keys_n(FSingSet *kvset,const char *const *keys,const unsigned *ksizes,unsigned count,const void *const *values,const unsigned *vsizes,int *results);

static inline int sing_add_keys(FSingSet *kvset,const char *const *keys,unsigned count,const void *const *values,const unsigned *vsizes,int *results)
	{ return sing_add_keys_n(kvset,keys,NULL,count,values,vsizes,results); }

int sing_set_key_n(FSingSet *kvset,const char *key,unsigned ksize,void *value,unsigned vsize);
int sing_set_phantom_n(FSingSet *kvset,const char *key,unsigned ksize,void *value,unsigned vsize);

static inline int sing_set_key(FSingSet *kvset,const char *key,void *value,unsigned vsize)
	{ return sing_set_key_n(kvset,key,SING_KEY_SIZE_UNKNOWN,value,vsize); }

static inline int sing_set_phantom(FSingSet *kvset,const char *key,void *value,unsigned vsize)
	{ return sing_set_phantom_n(kvset,key,SING_KEY_SIZE_UNKNOWN,value,vsize); }

static inline int sing_set_key32i_n(FSingSet *kvset,const char *key,unsigned ksize,int32_t value)
	{ return sing_set_key_n(kvset,key,ksize,&value,sizeof(int32_t)); }
static inline int sing_set_key32i(FSingSet *kvset,const char *key,int32_t value)
	{ return sing_set_key_n(kvset,key,SING_KEY_SIZE_UNKNOWN,&value,sizeof(int32_t)); }

static inline int sing_set_key32u_n(FSingSet *kvset,const char *key,unsigned ksize,uint32_t value)
	{ return sing_set_key_n(kvset,key,ksize,&value,sizeof(uint32_t)); }
static inline int sing_set_key32u(FSingSet *kvset,const char *key,uint32_t value)
	{ return sing_set_key_n(kvset,key,SING_KEY_SIZE_UNKNOWN,&value,sizeof(uint32_t)); }

static inline int sing_set_key32f_n(FSingSet *kvset,const char *key,unsigned ksize,float value)
	{ return sing_set_key_n(kvset,key,ksize,&value,sizeof(float)); }
static inline int sing_set_key32f(FSingSet *kvset,const char *key,float value)
	{ return sing_set_key_n(kvset,key,SING_KEY_SIZE_UNKNOWN,&value,sizeof(float)); }

static inline int sing_set_key64i_n(FSingSet *kvset,const char *key,unsigned ksize,int64_t value)
	{ return sing_set_key_n(kvset,key,ksize,&value,sizeof(int64_t)); }
static inline int sing_set_key64i(FSingSet *kvset,const char *key,int64_t value)
	{ return sing_set_key_n(kvset,key,SING_KEY_SIZE_UNKNOWN,&value,sizeof(int64_t)); }

static inline int sing_set_key64u_n(FSingSet *kvset,const char *key,unsigned ksize,uint64_t value)
	{ return sing_set_key_n(kvset,key,ksize,&value,sizeof(uint64_t)); }
static inline int sing_set_key64u(FSingSet *kvset,const char *key,uint64_t value)
	{ return sing_set_key_n(kvset,key,SING_KEY_SIZE_UNKNOWN,&value,sizeof(uint64_t)); }

static inline int sing_set_key64d_n(FSingSet *kvset,const char *key,unsigned ksize,double value)
	{ return sing_set_key_n(kvset,key,ksize,&value,sizeof(double)); }
static inline int sing_set_key64d(FSingSet *kvset,const char *key,double value)
	{ return sing_set_key_n(kvset,key,SING_KEY_SIZE_UNKNOWN,&value,sizeof(double)); }

static inline int sing_set_pointer_n(FSingSet *kvset,const char *key,unsigned ksize,void *value)
	{ return sing_set_key_n(kvset,key,ksize,&value,sizeof(size_t)); }
static inline int sing_set_pointer(FSingSet *kvset,const char *key,void *value)
	{ return sing_set_key_n(kvset,key,SING_KEY_SIZE_UNKNOWN,&value,sizeof(size_t)); }

int sing_set_keys_n(FSingSet *kvset,const char *const *keys,const unsigned *ksizes,unsigned count,const void *const *values,const unsigned *vsizes,int *results);

static inline int sing_set_keys(FSingSet *kvset,const char *const *keys,unsigned count,const void *const *values,const unsigned *vsizes,int *results)
	{ return sing_set_keys_n(kvset,keys,NULL,count,values,vsizes,results); }

int sing_del_key_n(FSingSet *kvset,const char *key,unsigned ksize);
int sing_del_phantom_n(FSingSet *kvset,const char *key,unsigned ksize);
int sing_del_full_n(FSingSet *kvset,const char *key,unsigned ksize);

static inline int sing_del_phantom(FSingSet *kvset,const char *key)
	{ return sing_del_phantom_n(kvset,key,SING_KEY_SIZE_UNKNOWN); }

static inline int sing_del_full(FSingSet *kvset,const char *key)
	{ return sing_del_full_n(kvset,key,SING_KEY_SIZE_UNKNOWN); }

static inline int sing_del_key(FSingSet *kvset,const char *key)
	{ return sing_del_key_n(kvset,key,SING_KEY_SIZE_UNKNOWN); }

int sing_del_keys_n(FSingSet *kvset,const char *const *keys,const unsigned *ksizes,unsigned count,int *results);

static inline int sing_del_keys(FSingSet *kvset,const char *const *keys,unsigned count,int *results)
	{ return sing_del_keys_n(kvset,keys,NULL,count,results); }

// iterator

typedef int (*CSingIterateCallback)(const char *key,const void *value,unsigned *vsize,void *new_value,void *param);
int sing_iterate(FSingSet *kvset,CSingIterateCallback cb,void *param);


#endif 