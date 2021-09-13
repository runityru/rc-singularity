/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#ifndef _INDEX_H
#define _INDEX_H

#include <stdint.h>
#include <sys/types.h>

#include "defines.h"
#include "config.h"
#include "allocator.h"
#include "locks.h"

// MAX_KEY_SOURCE - макс. размер исходного ключа
// MAX_KEY_SIZE - макс. размер хвоста ключа
// MAX_VALUE_SIZE - макс. размер данных

// Страница должна быть больше или равна макс. размеру данных
#if (PAGE_SIZE <= MAX_KEY_SIZE) || (PAGE_SIZE <= MAX_VALUE_SIZE)
	#error Page size is too small
#endif

#include "codec.h"

// Служебная структура для движения по цепочке в хештаблице
typedef struct
	{
	int dir; 			// Направление движения
	unsigned start;		// Начало движения
	unsigned end;		// Конец движения
	unsigned link_num; 	// Номер ссылки на продолжение (0 или 1)
	} FHashTableChain;

typedef struct
	{
	element_type mem_file_size;		// Размер шары в памяти - константное значение, не меняется для созданного индекса. Выровнено по дисковой странице
	element_type disk_file_size;	// Размер файла на диске - константное значение, не меняется для созданного индекса. Выровнено по дисковой странице
	} FHeadSizes;

#define SF_DIFF_MARK 0x01

// Флаги экспортируются
#define UF_NOT_PERSISTENT 0x01
#define UF_COUNTERS 0x02
#define UF_PHANTOM_KEYS 0x04

#define UF_PAGE_TYPES 0x80

typedef union FKeyHeadGeneralTg FKeyHeadGeneral;
	
typedef enum ELockModeTg
	{
	LM_DEFAULT,
	LM_SIMPLE,
	LM_PROTECTED,
	LM_FAST,
	LM_NONE,
	LM_READ_ONLY
	} ELockMode;

typedef struct FBadStatesListTg
	{
	uint32_t deleted;
	uint32_t corrupted;
	} FBadStatesList;

typedef union FBadStatesTg
	{
	FBadStatesList states;
	uint64_t some_present;
	} FBadStates;

typedef struct FSetHeadTg
	{ 
	// Первая кешлиния содержит константные данные
	FHeadSizes sizes;						// Размеры данных на диске и в памяти 
	unsigned signature; 					// Подпись формата файла
	unsigned hashtable_size;    		// Размер хештаблицы
	unsigned use_flags;					// Различные флаги, определяемые при создании (SF_...)
	unsigned use_mutex;					// Использовать мутекс при записи
	unsigned use_spin;					// Использовать спинлоки при записи
	unsigned check_mutex;				// Проверять мутекс при записи
	FBadStates bad_states;				// Состояния, препятствующие операции записи
	unsigned read_only;					// Только для чтения (копируется в FSingSet при подключении)
	unsigned delimiter;					// Разделитель столбцов в значениях (char)
	CACHE_LINE_PADDING(padding1,sizeof(FHeadSizes) + sizeof(FBadStates) + sizeof(unsigned) * 7);

	// Вторая кешлиния содержит редко меняющиеся данные
	unsigned state_flags;				// Флаги состояния (HF_...)
	unsigned pcnt;							// Всего выделено страниц
	unsigned first_empty_page;			// Первая целиком пустая выделенная страница
	unsigned first_pf_spec_page; 		// Первая частично пустая страница, на которой выделяются подстраницы
	element_type kh_alloc_zone;		// Частично пустая страница, на которой выделяются блоки заголовков ключей
	element_type alloc_zones[SMALL_SIZES_CNT]; // Подстраницы нужных размеров со свободными частями
	unsigned wip;							// Набор синхронизируется с диском
	CACHE_LINE_PADDING(padding2,sizeof(unsigned) * 5 + sizeof(element_type) * (SMALL_SIZES_CNT + 1));
	
	// Дальше данные, которые меняются каждую операцию
	unsigned count;						// Всего ключей (только для статистики)
	element_type unlocated;				// Первый свободный элемент на странице общего назначения
	element_type holes[HOLESIZE_CNT]; // Ссылки на начальные дырки в цепочках. Для размеров 1 .. (MIN_HOLE_SIZE - 1) - ведут на первые спецстраницы со своб. участками
	uint64_t holemask_low;				// Маска дырок, нижняя часть
	uint64_t holemask_high;				// Маска дырок, верхняя часть
	
	unsigned kh_mask; 			// Маска дырок под ключи
	element_type key_holes[KEYHEADS_IN_BLOCK]; // Начало цепочек блоков с пустыми местами
	} FSetHead;
	
#define INDEX_HEAD_DISK_PAGES (sizeof(FSetHead) / DISK_PAGE_BYTES + ((sizeof(FSetHead) % DISK_PAGE_BYTES) ? 1 : 0))
#define PAGE_TYPES_DISK_PAGES (MAX_PAGES / DISK_PAGE_BYTES + ((MAX_PAGES % DISK_PAGE_BYTES) ? 1 : 0))

typedef struct FChangedPagesTg FChangedPages;
typedef struct FLockSetTg FLockSet;

typedef struct FSingSetTg
	{
	int index_fd; 		// Шара индекса
	int pages_fd; 		// Шара страниц
	int disk_index_fd; 	// Файл индекса на диске
	int disk_pages_fd; 	// Файл страниц на диске

	char *disk_index_fname;
	char *disk_pages_fname;
	char *mem_index_fname;
	char *mem_pages_fname;

	char last_error[512];

	unsigned hashtable_size; 
	unsigned conn_flags;
	unsigned locks_count __attribute__ ((aligned (8))); // Число повешенных блокировок на цепочки
	unsigned manual_locked; // Флаг ручной блокировки
	unsigned read_only; // Набор доступен только для чтения
	unsigned is_private; // Набор доступен только для чтения

	FSetHead *head;
	FChangedPages *real_cpages;
	FChangedPages *used_cpages;
	FLockSet *lock_set;
	FKeyHeadGeneral *hash_table;
	unsigned *counters;
	unsigned char *page_types;
	element_type *pages[MAX_PAGES];
	} FSingSet;

#define FILE_PERMISSIONS (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)

void idx_set_error(FSingSet *index,const char *message);
void idx_set_formatted_error(FSingSet *index,const char *format,...);

FSingSet *idx_create_set(const char *setname,unsigned keys_count,unsigned flags,FSingConfig *config);
void idx_creation_done(FSingSet *index,unsigned lock_mode);
int idx_relink_set(FSingSet *index);

FSingSet *idx_link_set(const char *setname,unsigned flags,FSingConfig *config);
void sing_unlink_set(FSingSet *index);
void sing_unload_set(FSingSet *index);
void sing_delete_set(FSingSet *index);

typedef struct FReaderLockTg FReaderLock;
	
int idx_key_search(FSingSet *index,FTransformData *tdata,FReaderLock *rlock);
int idx_key_get(FSingSet *index,FTransformData *tdata,FReaderLock *rlock,void *value_dst,unsigned *value_dst_size);
int idx_key_compare(FSingSet *index,FTransformData *tdata,FReaderLock *rlock,void *value_cmp,unsigned value_cmp_size);

typedef void *(CSingValueAllocator)(unsigned size);
int idx_key_get_cb(FSingSet *index,FTransformData *tdata,FReaderLock *rlock,CSingValueAllocator vacb,void **value,unsigned *value_size);

#ifdef MEMORY_CHECK
void idx_print_chain_distrib(FSingSet *index);
#endif

#define KS_ADDED		 0x01
#define KS_DELETED 	 0x02
#define KS_CHANGED 	 0x03
#define KS_SUCCESS    0x04
#define KS_MARKED     0x08
#define KS_PRESENT    0x10
#define KS_ERROR      0x80000000

#define ERROR_CONNECTION_LOST (0x0100 | KS_ERROR)
#define ERROR_DATA_CORRUPTED (0x0200 | KS_ERROR)
#define ERROR_INTERNAL (0x0300 | KS_ERROR)
#define ERROR_NO_MEMORY (0x0400 | KS_ERROR)
#define ERROR_NO_SET_MEMORY (0x0500 | KS_ERROR)
#define ERROR_FILE_NOT_FOUND (0x0600 | KS_ERROR)
#define ERROR_OUTPUT_NOT_FOUND (0x0700 | KS_ERROR)
#define ERROR_IMPOSSIBLE_OPERATION (0x0800 | KS_ERROR)
#define ERROR_SYNC_FAILED (0x0900 | KS_ERROR)

#define RESULT_KEY_NOT_FOUND 0x0A00
#define RESULT_IMPOSSIBLE_KEY 0x0B00
#define RESULT_SMALL_BUFFER 0x0C00
#define RESULT_VALUE_DIFFER 0x0D00
#define RESULT_KEY_PRESENT 0x0E00

int idx_key_try_set(FSingSet *index,FTransformData *tdata) __attribute__((regparm(2)));
int idx_key_set(FSingSet *index,FTransformData *tdata) __attribute__((regparm(2)));

static inline int idx_key_set_switch(FSingSet *index,FTransformData *tdata)
	{ return tdata->chain_idx_ref ? idx_key_set(index,tdata) : idx_key_try_set(index,tdata); }

int idx_key_del(FSingSet *index,FTransformData *tdata) __attribute__((regparm(2)));

static inline void lck_chainLock(FSingSet *index,unsigned hash)
	{ if (index->head->use_spin) _lck_chainLock(index,hash); }

// Попытка блокировки цепочки
static inline int lck_tryChainLock(FSingSet *index,unsigned hash)
	{ return (index->head->use_spin) ? lck_tryChainLock(index,hash) : 1; }

// Разблокировка цепочки
static inline void lck_chainUnlock(FSingSet *index,unsigned hash)
	{ if (index->head->use_spin) _lck_chainUnlock(index,hash); }

// Снятие блокировки работы с памятью (снимает memory_lock)
static inline void lck_memoryLock(FSingSet *index)
	{ if (index->head->use_spin) _lck_memoryLock(index->lock_set); }

// Снятие блокировки работы с памятью (снимает memory_lock)
static inline void lck_memoryUnlock(FSingSet *index)
	{ if (index->head->use_spin) _lck_memoryUnlock(index->lock_set); }

static inline void idx_op_finalize(FSingSet *index,FTransformData *tdata,int res)
	{
	if (res & KS_CHANGED)
		{
		if (tdata->old_key_rest_size)
			{
			lck_waitForReaders(index->lock_set);
			idx_general_free(index,tdata->old_key_rest,tdata->old_key_rest_size);
			}
		lck_memoryUnlock(index);
		}
	}

typedef void (*CSingIterateCallbackRaw)(const FKeyHead *key_head,const element_type *key_rest,const void *value,unsigned vsize,void *param);
typedef int (*CSingIterateCallback)(const char *key,const void *value,unsigned vsize,void *new_value,void *param);

void idx_del_unmarked(FSingSet *index,unsigned *counters,CSingIterateCallbackRaw cb,void *param);
void idx_process_all(FSingSet *index,void *cb,int raw,void *param);

typedef struct FCheckDataTg
	{
	element_type busy_headers;
	element_type busy_general;
	element_type busy_small;
	element_type free_headers;
	element_type free_general;
	element_type free_small;
	element_type empty;
	uint64_t checked_subpages[PAGES_MASK_SIZE];
	} FCheckData;

int idx_flush(FSingSet *index);
int idx_revert(FSingSet *index);
int idx_check_all(FSingSet *index,int reserved);

#endif