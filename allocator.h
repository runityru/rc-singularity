/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#ifndef _ALLOCATOR_H
#define _ALLOCATOR_H

#include <stdint.h>

#include "defines.h"

typedef struct FValueHeadTg
	{
	element_type size_e:(ELEMENT_SIZE * 8 - 3); // Размер в элементах
	element_type extra_bytes:3; 				// Число добавленных байт. (0-4)
	} FValueHead;
	
typedef union FValueHeadGeneralTg
	{
	FValueHead fields;   // Поделенные на поля
	element_type whole;
	} FValueHeadGeneral;
	
#define VALUE_HEAD_SIZE (sizeof(FValueHead) / ELEMENT_SIZE + (sizeof(FValueHead) % ELEMENT_SIZE) ? 1 : 0)

// Максимальный размер хвоста ключа. (4 байта = 0, 5 байт = 1, 11 байт = 2 etc) + 1 (см. process_ext)
#define MAX_KEY_SIZE ((MAX_KEY_SOURCE + 1)/6 + 1)

#define CACHE_ALIGNED_MAX_KEY_SIZE (CACHE_LINE_SIZE * (MAX_KEY_SIZE * ELEMENT_SIZE / CACHE_LINE_SIZE + (((MAX_KEY_SIZE * ELEMENT_SIZE) % CACHE_LINE_SIZE)?1:0)))

#define MAX_VALUE_SOURCE ((MAX_VALUE_SIZE - VALUE_HEAD_SIZE - 1) * ELEMENT_SIZE)

// Реальный размер value в байтах
#define VALUE_SIZE_BYTES(A) ((A)->size_e * ELEMENT_SIZE - (A)->extra_bytes)

typedef struct
	{
	element_type nonempty_signature; // = 0
	element_type prev;
	element_type next;
	element_type size; // Для поиска хидера при просмотре снизу, может совпасть с size FHoleFooter
	} FHoleHeader;
	
typedef struct
	{
	element_type size; // Для поиска хидера при просмотре сверху
	element_type nonempty_signature; // = 0
	} FHoleFooter;
	
// Минимальный размер дырки в страницах общего назначения.
#define HOLE_HEADER_SIZE (sizeof(FHoleHeader) / ELEMENT_SIZE)
#define HOLE_FOOTER_SIZE (sizeof(FHoleFooter) / ELEMENT_SIZE)

// Минимальный размер дырки на страницах общего назначения. Единицу вычитаем из-за возможного совпадения поля size
#define MIN_HOLE_SIZE (HOLE_HEADER_SIZE + HOLE_FOOTER_SIZE - 1)

// Число размеров меньше минимального
#define SMALL_SIZES_CNT (MIN_HOLE_SIZE - 1)

// Все дырки, имеющие старший бит тот-же, что и у MAX_KEY_SIZE + MIN_HOLE_SIZE, индексируются точно по размеру.
// Большие - по двоичному логарифму (берется дырка на размер больше)
// MIN_HOLE_SIZE добавляется для быстрейшего выделения памяти под все размеры ключей, т.к. получение дырки из логарифмической области почти всегда приводит к ее делению
// Размеры меньше минимального тоже имеют ссылку на первую подстраницу, так что их тоже считаем
#define INDEXED_HOLESIZE_CNT ((1 << (LOG_BIN_MACRO(MAX_KEY_SIZE + MIN_HOLE_SIZE) + 1)) - 1)

// Индекс дырки, S - размер
// Единицу вычитаем, т.к. дырок нулевого размера нет
#define HOLESIZE_IDX(S) (((S) <= INDEXED_HOLESIZE_CNT ? (S) : (INDEXED_HOLESIZE_CNT + LOG_BIN(S) - LOG_BIN_MACRO(MAX_KEY_SIZE + MIN_HOLE_SIZE))) - 1)

// Всего цепочек дырок.
#define HOLESIZE_CNT (INDEXED_HOLESIZE_CNT + LOG_BIN_MACRO(PAGE_SIZE) - LOG_BIN_MACRO(MAX_KEY_SIZE + MIN_HOLE_SIZE))

typedef struct
	{
	unsigned total_count;       // Всего элементов на подстранице
	unsigned used_count;        // Использовано элементов. 0 - подстраница удаляется из цепочки и помечается как свободная.
	element_type first_hole;	// Первая дырка
	element_type prev_sub_page; // Ссылка на пред. подстраницу с дырками
	element_type next_sub_page; // Ссылка на след. подстраницу с дырками
	unsigned chunk_size;
	} FSubpageHead;

#define SUB_PAGE_HEAD_SIZE (sizeof(FSubpageHead) / ELEMENT_SIZE)
#define SUB_PAGE_MASK (~(DISK_PAGE_SIZE - 1))
	
typedef struct
	{
	uint64_t use_mask; // Маска занятых подстраниц
	unsigned prev_pf_spec_page; // Предыдущая спецстраница с дырками
	unsigned next_pf_spec_page; // Следующая спецстраница с дырками
	} FSpecPageHead;
	
#define SPEC_PAGE_HEAD_SIZE (sizeof(FSpecPageHead) / ELEMENT_SIZE)

typedef struct FSingSetTg FSingSet;

typedef enum EPageTypesTg { PT_UNKNOWN,PT_GENERAL,PT_HEADERS,PT_SPECIAL,PT_EMPTY } EPageTypes;

unsigned idx_alloc_page(FSingSet *index,EPageTypes page_type);

#ifdef MEMORY_CHECK
void check_page_type(FSingSet *index,element_type idx,EPageTypes page_type);
#define CHECK_PAGE_TYPE(INDEX,IDX,PAGE_TYPE) check_page_type((INDEX),(IDX),(PAGE_TYPE))
element_type *PAGES_POINTER(FSingSet *index,element_type idx);
#else 
#define CHECK_PAGE_TYPE(A,B,C)
#define PAGES_POINTER(index,idx) (&index->pages[(idx) >> PAGE_SHIFT][(idx) & OFFSET_MASK])
#endif
void idx_free_page(FSingSet *index,unsigned pnum);

element_type *regionPointer(FSingSet *index,element_type idx,unsigned size);
element_type *regionPointerNoError(FSingSet *index,element_type idx,unsigned size);
element_type *pagesPointer(FSingSet *index,element_type idx);
element_type *pagesPointerNoError(FSingSet *index,element_type idx);

// Выделяет запрошенное количество памяти, size - количество элементов
element_type *idx_small_alloc(FSingSet *index,unsigned size,element_type *ref_pointer);
element_type *idx_large_alloc(FSingSet *index,unsigned size,element_type *ref_pointer);
static inline element_type *idx_general_alloc(FSingSet *index,unsigned size,element_type *ref_pointer)
	{ return (size < MIN_HOLE_SIZE) ? idx_small_alloc(index,size,ref_pointer) : idx_large_alloc(index,size,ref_pointer); }
// Освобождает указанный участок памяти
void idx_small_free(FSingSet *index,element_type data_idx,unsigned size);
void idx_large_free(FSingSet *index,element_type data_idx,unsigned size);
static inline void idx_general_free(FSingSet *index,element_type data_idx,unsigned size)
	{
	if (size < MIN_HOLE_SIZE) 
		{ idx_small_free(index,data_idx,size); return; }
	idx_large_free(index,data_idx,size);
	}


typedef struct FCheckDataTg FCheckData;
// Добавляет к *free_size размер пустых страниц
int check_free_pages(FSingSet *index,FCheckData *check_data);
// Добавляет к busy_size размер заголовкаов страницы и подстраниц, а к *free_size размер свободных подстраниц
void spec_page_support_count(FSingSet *index,unsigned pnum,FCheckData *check_data);
// Добавляет к *free_size размер неразмеченных участков и суммарный размер маленьких дырок в цепочках
int check_small_holes_chains(FSingSet *index,FCheckData *check_data);
// Добавляет к *free_size размер неразмеченного участка и суммарный размер больших дырок в цепочках
int check_general_holes_chains(FSingSet *index,FCheckData *check_data);

#endif