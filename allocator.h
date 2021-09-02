/*
 * Copyright (C) �Hostcomm� LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#ifndef _ALLOCATOR_H
#define _ALLOCATOR_H

#include <stdint.h>

#include "defines.h"

typedef struct FValueHeadTg
	{
	element_type size_e:(ELEMENT_SIZE * 8 - 3); // ������ � ���������
	element_type extra_bytes:3; 				// ����� ����������� ����. (0-4)
	} FValueHead;
	
typedef union FValueHeadGeneralTg
	{
	FValueHead fields;   // ���������� �� ����
	element_type whole;
	} FValueHeadGeneral;
	
#define VALUE_HEAD_SIZE (sizeof(FValueHead) / ELEMENT_SIZE + (sizeof(FValueHead) % ELEMENT_SIZE) ? 1 : 0)

// ������������ ������ ������ �����. (4 ����� = 0, 5 ���� = 1, 11 ���� = 2 etc) + 1 (��. process_ext)
#define MAX_KEY_SIZE ((MAX_KEY_SOURCE + 1)/6 + 1)

#define CACHE_ALIGNED_MAX_KEY_SIZE (CACHE_LINE_SIZE * (MAX_KEY_SIZE * ELEMENT_SIZE / CACHE_LINE_SIZE + (((MAX_KEY_SIZE * ELEMENT_SIZE) % CACHE_LINE_SIZE)?1:0)))

#define MAX_VALUE_SOURCE ((MAX_VALUE_SIZE - VALUE_HEAD_SIZE - 1) * ELEMENT_SIZE)

// �������� ������ value � ������
#define VALUE_SIZE_BYTES(A) ((A)->size_e * ELEMENT_SIZE - (A)->extra_bytes)

typedef struct
	{
	element_type nonempty_signature; // = 0
	element_type prev;
	element_type next;
	element_type size; // ��� ������ ������ ��� ��������� �����, ����� �������� � size FHoleFooter
	} FHoleHeader;
	
typedef struct
	{
	element_type size; // ��� ������ ������ ��� ��������� ������
	element_type nonempty_signature; // = 0
	} FHoleFooter;
	
// ����������� ������ ����� � ��������� ������ ����������.
#define HOLE_HEADER_SIZE (sizeof(FHoleHeader) / ELEMENT_SIZE)
#define HOLE_FOOTER_SIZE (sizeof(FHoleFooter) / ELEMENT_SIZE)

// ����������� ������ ����� �� ��������� ������ ����������. ������� �������� ��-�� ���������� ���������� ���� size
#define MIN_HOLE_SIZE (HOLE_HEADER_SIZE + HOLE_FOOTER_SIZE - 1)

// ����� �������� ������ ������������
#define SMALL_SIZES_CNT (MIN_HOLE_SIZE - 1)

// ��� �����, ������� ������� ��� ���-��, ��� � � MAX_KEY_SIZE + MIN_HOLE_SIZE, ������������� ����� �� �������.
// ������� - �� ��������� ��������� (������� ����� �� ������ ������)
// MIN_HOLE_SIZE ����������� ��� ����������� ��������� ������ ��� ��� ������� ������, �.�. ��������� ����� �� ��������������� ������� ����� ������ �������� � �� �������
// ������� ������ ������������ ���� ����� ������ �� ������ �����������, ��� ��� �� ���� �������
#define INDEXED_HOLESIZE_CNT ((1 << (LOG_BIN_MACRO(MAX_KEY_SIZE + MIN_HOLE_SIZE) + 1)) - 1)

// ������ �����, S - ������
// ������� ��������, �.�. ����� �������� ������� ���
#define HOLESIZE_IDX(S) (((S) <= INDEXED_HOLESIZE_CNT ? (S) : (INDEXED_HOLESIZE_CNT + LOG_BIN(S) - LOG_BIN_MACRO(MAX_KEY_SIZE + MIN_HOLE_SIZE))) - 1)

// ����� ������� �����.
#define HOLESIZE_CNT (INDEXED_HOLESIZE_CNT + LOG_BIN_MACRO(PAGE_SIZE) - LOG_BIN_MACRO(MAX_KEY_SIZE + MIN_HOLE_SIZE))

typedef struct
	{
	unsigned total_count;       // ����� ��������� �� �����������
	unsigned used_count;        // ������������ ���������. 0 - ����������� ��������� �� ������� � ���������� ��� ���������.
	element_type first_hole;	// ������ �����
	element_type prev_sub_page; // ������ �� ����. ����������� � �������
	element_type next_sub_page; // ������ �� ����. ����������� � �������
	unsigned chunk_size;
	} FSubpageHead;

#define SUB_PAGE_HEAD_SIZE (sizeof(FSubpageHead) / ELEMENT_SIZE)
#define SUB_PAGE_MASK (~(DISK_PAGE_SIZE - 1))
	
typedef struct
	{
	uint64_t use_mask; // ����� ������� ����������
	unsigned prev_pf_spec_page; // ���������� ������������ � �������
	unsigned next_pf_spec_page; // ��������� ������������ � �������
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

// �������� ����������� ���������� ������, size - ���������� ���������
element_type *idx_small_alloc(FSingSet *index,unsigned size,element_type *ref_pointer);
element_type *idx_large_alloc(FSingSet *index,unsigned size,element_type *ref_pointer);
static inline element_type *idx_general_alloc(FSingSet *index,unsigned size,element_type *ref_pointer)
	{ return (size < MIN_HOLE_SIZE) ? idx_small_alloc(index,size,ref_pointer) : idx_large_alloc(index,size,ref_pointer); }
// ����������� ��������� ������� ������
void idx_small_free(FSingSet *index,element_type data_idx,unsigned size);
void idx_large_free(FSingSet *index,element_type data_idx,unsigned size);
static inline void idx_general_free(FSingSet *index,element_type data_idx,unsigned size)
	{
	if (size < MIN_HOLE_SIZE) 
		{ idx_small_free(index,data_idx,size); return; }
	idx_large_free(index,data_idx,size);
	}


typedef struct FCheckDataTg FCheckData;
// ��������� � *free_size ������ ������ �������
int check_free_pages(FSingSet *index,FCheckData *check_data);
// ��������� � busy_size ������ ����������� �������� � ����������, � � *free_size ������ ��������� ����������
void spec_page_support_count(FSingSet *index,unsigned pnum,FCheckData *check_data);
// ��������� � *free_size ������ ������������� �������� � ��������� ������ ��������� ����� � ��������
int check_small_holes_chains(FSingSet *index,FCheckData *check_data);
// ��������� � *free_size ������ �������������� ������� � ��������� ������ ������� ����� � ��������
int check_general_holes_chains(FSingSet *index,FCheckData *check_data);

#endif