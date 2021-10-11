/*
 * Copyright (C) �Hostcomm� LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <stddef.h>
#include <stdio.h>
#include <memory.h>

#include "common.h"
#include "../index.h"
#include "../cpages.h"

// ��������� ������ ������� ����� (������� HOLESIZE_IDX � HOLESIZE_CNT)
int hole_size_idx_test(char *errbuf)
	{
	unsigned needed_hs_idx = 0, hs_idx,i;
	// �� 1 �� MAX_KEY_SIZE ������ ������������� �� 1
	for (i = 1; i <= MAX_KEY_SIZE; i++)
		{
		hs_idx = HOLESIZE_IDX(i);
		if (needed_hs_idx != hs_idx)
			{ 
			sprintf(errbuf,"Bad holesize index, size %d, index %d, should be %d",i,hs_idx,needed_hs_idx);
			return 1; 
			}
		needed_hs_idx++;
		}
	// ���� ������� ��� ��� �� ��� � � MAX_KEY_SIZE - ������ ������������� �� 1
	unsigned size = 1;
	while ((size <<= 1) <= MAX_KEY_SIZE);
	for (; i < size; i++)
		{
		hs_idx = HOLESIZE_IDX(i);
		if (needed_hs_idx != hs_idx)
			{ 
			sprintf(errbuf,"Bad holesize index, size %d, index %d, should be %d",i,hs_idx,needed_hs_idx); 
			return 1; 
			}
		needed_hs_idx++;
		}
	// ������ �� PAGE_SIZE ������ ������������� �� 1 � ������ ����� ������� �����
	while (size < PAGE_SIZE)
		{
		hs_idx = HOLESIZE_IDX(size);
		if (needed_hs_idx != hs_idx)
			{ 
			sprintf(errbuf,"Bad holesize index, size %d, index %d, should be %d",size,hs_idx,needed_hs_idx); 
			return 1; 
			}
		hs_idx = HOLESIZE_IDX(size * 2 - 1);
		if (needed_hs_idx != hs_idx)
			{ 
			sprintf(errbuf,"Bad holesize index, size %d, index %d, should be %d",size,hs_idx,needed_hs_idx);
			return 1; 
			}
		needed_hs_idx++;
		size *= 2;
		}
	hs_idx = HOLESIZE_IDX(size);
	if (needed_hs_idx != hs_idx)
		{ sprintf(errbuf,"Bad holesize index, size %d, index %d, should be %d",size,hs_idx,needed_hs_idx); return 1; }
	if (++hs_idx != HOLESIZE_CNT)
		{ sprintf(errbuf,"Bad holesize count, has %d, should be %d",hs_idx,HOLESIZE_CNT); return 1; }
	return 0;	
	}

// �������� ���������/������������ ������� (idx_alloc_page,idx_free_page)
int page_alloc_test(FSingSet *index,int *reserved_memory,element_type prep_data)
	{
	element_type *page;
	unsigned pnum;
	idx_alloc_page(index,PT_GENERAL);
	idx_alloc_page(index,PT_GENERAL);
	if (index->head->pcnt != 3) // �������� ��� ��� �������� ����������
		{ idx_set_formatted_error(index,"Bad total allocated pages count, has %d, should be 3",index->head->pcnt); return 1; }
	idx_free_page(index,1);
	idx_free_page(index,2);
	if (index->head->first_empty_page != 2) // �������� ��� � ��������� ������ �� ��������� ������������� ��������
		{ idx_set_formatted_error(index,"Bad first empty page, has %d, should be 2",index->head->first_empty_page); return 1; }
	idx_alloc_page(index,PT_GENERAL);
	if (index->head->first_empty_page != 1) // �������� ��� ���������� ������������� ����� ������ �� �������
		{ idx_set_formatted_error(index,"Bad first empty page, has %d, should be 1",index->head->first_empty_page); return 1; }
	idx_alloc_page(index,PT_GENERAL);
	if (index->head->first_empty_page != NO_PAGE) // �������� ��� ������ �� ������ ��������� ����������
		{ idx_set_formatted_error(index,"Bad first empty page, has %d, should be %d",index->head->first_empty_page,NO_PAGE); return 1; }
	pnum = idx_alloc_page(index,PT_GENERAL); 
	// �������� ��� ��� ����� ������������� ������������ � ���������� ����� �����
	if (index->head->pcnt != 4) 
		{ idx_set_formatted_error(index,"Bad total allocated pages count, has %d, should be 3",index->head->pcnt); return 1; }
	if (pnum != 3) 
		{ idx_set_formatted_error(index,"Bad allocated page number, has %d, should be 3",pnum); return 1; }
	page = index->pages[pnum];
	if (page[PAGE_SIZE - 1]) // �������� ��� ���������� ����� �������� � �� ����� ������ ������
		{ idx_set_formatted_error(index,"Data at allocated page 3 is not zeroes"); return 1; }
	*reserved_memory += PAGE_SIZE * 3;
	return 0;	
	}

// ��������� �����

#define SMALL_TEST_SIZE (MIN_HOLE_SIZE - 1)

element_type *test_small_alloc(FSingSet *index,unsigned size,element_type *ref_pointer,int *res_mem)
	{
	*res_mem += size;
	return idx_small_alloc(index,size,ref_pointer);
	}

void test_small_free(FSingSet *index,element_type data_idx,unsigned size,int *res_mem)
	{
	*res_mem -= size;
	idx_small_free(index,data_idx,size);
	}

element_type fill_first_subpage(FSingSet *index,unsigned reserv,int *res_mem)
	{
	unsigned i;
	element_type ref;
	unsigned holes_count = (DISK_PAGE_SIZE - SPEC_PAGE_HEAD_SIZE - SUB_PAGE_HEAD_SIZE) / SMALL_TEST_SIZE;
	for (i = 0; i < holes_count - reserv; i++)
		test_small_alloc(index,SMALL_TEST_SIZE,&ref,res_mem);
	return ref;
	}

element_type fill_subpage(FSingSet *index,unsigned reserv,int *res_mem)
	{
	unsigned i;
	element_type ref;
	unsigned holes_count = (DISK_PAGE_SIZE - SUB_PAGE_HEAD_SIZE) / SMALL_TEST_SIZE;
	for (i = 0; i < holes_count - reserv; i++)
		test_small_alloc(index,SMALL_TEST_SIZE,&ref,res_mem);
	return ref;
	}

element_type free_subpage(FSingSet *index,element_type ref,unsigned reserv,int *res_mem)
	{
	unsigned i;
	unsigned pnum = ref >> PAGE_SHIFT;
	unsigned spnum = (ref & OFFSET_MASK) / DISK_PAGE_SIZE;
	unsigned spstart = pnum * PAGE_SIZE + (spnum ? (spnum * DISK_PAGE_SIZE) : SPEC_PAGE_HEAD_SIZE) + SUB_PAGE_HEAD_SIZE;
	unsigned holes_count = (DISK_PAGE_SIZE - (spnum ? SUB_PAGE_HEAD_SIZE : (SUB_PAGE_HEAD_SIZE + SPEC_PAGE_HEAD_SIZE))) / SMALL_TEST_SIZE;
	for (i = reserv; i < holes_count; i++)
		test_small_free(index,spstart + i * SMALL_TEST_SIZE,SMALL_TEST_SIZE,res_mem);
	return spstart;
	}

//(1) ��������� ���������
//(1.1) ���� ����������� � �������
//	1_1_1 ��� ������������ ����� �� ���
// 1_1_2 ��� �� ������������ ����� �� ���
//(1.2) ��� ����� �� ���� �������� ���������� �����������
//	1_2_1 � ��� �������� ����� ����� ���������
//	1_2_2 � ��� �� �������� ����� ����� ���������
//(1.3) ��� ����� � ��� �������� ���������� �����������
//	1_3_1 ��� ������������ � ������� ������������� (��������� ������)
//(1.3.2) ���� ������������ � ������� �������������
//	1_3_2_1 ��� ��������� ������ ����������� �� ���
// 1_3_2_2 ��� �� ��������� ������ ����������� �� ���

int small_alloc_test(FSingSet *index,int *res_mem,element_type prep_data)
	{
	element_type ref;
	cp_mark_head_dirty(index->used_cpages);
	return test_small_alloc(index,SMALL_TEST_SIZE,&ref,res_mem),0;
	}

int small_alloc_test_no_head(FSingSet *index,int *res_mem,element_type prep_data)
	{
	element_type ref;
	return test_small_alloc(index,SMALL_TEST_SIZE,&ref,res_mem),0;
	}

element_type small_alloc_prep_1_1_1(FSingSet *index,int *res_mem)
	{
	element_type ref = fill_first_subpage(index,0,res_mem) - SMALL_TEST_SIZE;
	test_small_free(index,ref,SMALL_TEST_SIZE,res_mem);
	return 0;
	}

element_type small_alloc_prep_1_1_2(FSingSet *index,int *res_mem)
	{
	element_type ref = fill_first_subpage(index,0,res_mem) - SMALL_TEST_SIZE * 2;
	test_small_free(index,ref,SMALL_TEST_SIZE,res_mem);
	test_small_free(index,ref + SMALL_TEST_SIZE,SMALL_TEST_SIZE,res_mem);
	return 0;
	}

element_type small_alloc_prep_1_2_1(FSingSet *index,int *res_mem)
	{ return fill_first_subpage(index,2,res_mem),0; }

element_type small_alloc_prep_1_2_2(FSingSet *index,int *res_mem)
	{ return fill_first_subpage(index,1,res_mem),0; }

element_type small_alloc_prep_1_3_2_1(FSingSet *index,int *res_mem)
	{ 
	unsigned i;
	fill_first_subpage(index,0,res_mem);
	for (i = 1; i < 63; i++)
		fill_subpage(index,0,res_mem);
	return 0;
	}

element_type small_alloc_prep_1_3_2_2(FSingSet *index,int *res_mem)
	{ return fill_first_subpage(index,0,res_mem),0; }

//(2) ��������� ������������
//(2.1) ��� ��������� ������� ������� �� �����������
//(2.1.1) �� ����������� ���� �����
// 2_1_1_1 ����������� ������ � �������
//(2.1.1.2) ����������� �� ������ � �������
// 2_1_1_2_1 ����������� �� ��������� � �������
// 2_1_1_2_2 ����������� ��������� � �������
// 2_1_2 �� ����������� ��� ����� (������� ������������) 
// 2_1_3 ��� �������� ���������� ����������� (����������� ������ 2_1_2)
// 2_1_4 ��� �� �������� ���������� ����������� (����������� ������ 2_1_1_2_1)
//(2.1.5) ��� �������� ������������ �� �����������
// 2_1_5_1 ���� ������ �������� ���������� ��������
// 2_1_5_2 ��� ������ �������� ���������� �������
//(2.1.6) ��� ������������ ���������� ����������� �� ��������
// 2_1_6_1 ��� ������ � ������� ������������ � ������� ������������� (����������� ������ 2_1_1_1)
//(2.1.6.2) ��� �� ������ � ������� ������������ � ������� �������������
// 2_1_6_2_1 ���� ���������
// 2_1_6_2_2 ��� ���������
// 2_1_7 ��� �� ������������ ���������� ����������� �� ��������, �� �������� ����� ������ ����������� (����������� ������ 2_1_1_2_2)
//(2.2) ��� �� ��������� ������� ������� �� �����������
// 2_2_1 �� ��������� � ���� ��������� ��� �������
//(2.2.2) �� �� ��������� � ���� ��������� ��� �������
//(2.2.2.1) �� ����������� ��� �����
// 2_2_2_1_1 ���� ������ ����������� � �������
// 2_2_2_1_2 ��� ������ ���������� � �������
// 2_2_2_2 �� ����������� ���� �����

int small_free_test(FSingSet *index,int *res_mem,element_type prep_data)
	{
	cp_mark_head_dirty(index->used_cpages);
	return test_small_free(index,prep_data,SMALL_TEST_SIZE,res_mem),0; 
	}

int small_free_test_no_head(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return test_small_free(index,prep_data,SMALL_TEST_SIZE,res_mem),0; 
	}

// ����� ��������� ������ 2_1_6_1
element_type small_free_prep_2_1_1_1(FSingSet *index,int *res_mem)
	{ 
	element_type hole1,hole2;
	test_small_alloc(index,SMALL_TEST_SIZE,&hole1,res_mem); 
	test_small_alloc(index,SMALL_TEST_SIZE,&hole2,res_mem);
	test_small_free(index,hole1,SMALL_TEST_SIZE,res_mem);
	return hole2;
	}

// ����� ��������� ������ 2_1_4
element_type small_free_prep_2_1_1_2_1(FSingSet *index,int *res_mem)
	{ 
	element_type hole1,hole2,hole3;
	hole1 = fill_first_subpage(index,0,res_mem);
	hole2 = fill_subpage(index,0,res_mem);
	hole3 = fill_subpage(index,0,res_mem) - SMALL_TEST_SIZE;
	test_small_free(index,hole1,SMALL_TEST_SIZE,res_mem);
	hole2 = free_subpage(index,hole2,1,res_mem);
	test_small_free(index,hole3,SMALL_TEST_SIZE,res_mem);
	return hole2;
	}

// ����� ��������� ������ 2_1_7
element_type small_free_prep_2_1_1_2_2(FSingSet *index,int *res_mem)
	{ 
	element_type hole1,hole2,hole3;
	hole1 = fill_first_subpage(index,0,res_mem);
	test_small_alloc(index,SMALL_TEST_SIZE,&hole2,res_mem); 
	test_small_alloc(index,SMALL_TEST_SIZE,&hole3,res_mem);
	test_small_free(index,hole2,SMALL_TEST_SIZE,res_mem);
	test_small_free(index,hole1,SMALL_TEST_SIZE,res_mem);
	return hole3;
	}

// ����� ��������� ������ 2_1_3
element_type small_free_prep_2_1_2(FSingSet *index,int *res_mem)
	{ 
	element_type hole1;
	test_small_alloc(index,SMALL_TEST_SIZE,&hole1,res_mem);
	return hole1;
	}

element_type small_free_prep_2_1_5_1(FSingSet *index,int *res_mem)
	{
	unsigned i;
	element_type hole,hole2;
	fill_first_subpage(index,0,res_mem);
	for (i = 1; i < 64; i++)
		hole = fill_subpage(index,0,res_mem);
	test_small_alloc(index,SMALL_TEST_SIZE,&hole2,res_mem);
	return free_subpage(index,hole,1,res_mem);
	}

element_type small_free_prep_2_1_5_2(FSingSet *index,int *res_mem)
	{
	unsigned i;
	element_type hole;
	fill_first_subpage(index,0,res_mem);
	for (i = 1; i < 64; i++)
		hole = fill_subpage(index,0,res_mem);
	return free_subpage(index,hole,1,res_mem);
	}

element_type small_free_prep_2_1_6_2_1(FSingSet *index,int *res_mem)
	{
	unsigned i;
	element_type subpages[64];
	element_type hole,hole2,hole3;
	fill_first_subpage(index,0,res_mem);
	for (i = 1; i < 64; i++)
		hole = fill_subpage(index,0,res_mem);
	subpages[0] = fill_first_subpage(index,0,res_mem);
	for (i = 1; i < 64; i++)
		subpages[i] = fill_subpage(index,0,res_mem);
	test_small_alloc(index,SMALL_TEST_SIZE,&hole3,res_mem);
	for (i = 1; i < 64; i++)
		free_subpage(index,subpages[i],0,res_mem);
	hole2 = free_subpage(index,subpages[0],1,res_mem);
	free_subpage(index,hole,0,res_mem);
	return hole2;
	}

element_type small_free_prep_2_1_6_2_2(FSingSet *index,int *res_mem)
	{
	unsigned i;
	element_type hole,hole2;
	fill_first_subpage(index,0,res_mem);
	for (i = 1; i < 64; i++)
		hole = fill_subpage(index,0,res_mem);
	test_small_alloc(index,SMALL_TEST_SIZE,&hole2,res_mem);
	free_subpage(index,hole,0,res_mem);
	return hole2;
	}

element_type small_free_prep_2_2_1(FSingSet *index,int *res_mem)
	{ 
	element_type hole1,hole2;
	test_small_alloc(index,SMALL_TEST_SIZE,&hole1,res_mem); 
	test_small_alloc(index,SMALL_TEST_SIZE,&hole2,res_mem);
	return hole2;
	}

element_type small_free_prep_2_2_2_1_1(FSingSet *index,int *res_mem)
	{ 
	element_type hole1,hole2,hole3;
	hole1 = fill_first_subpage(index,0,res_mem);
	test_small_alloc(index,SMALL_TEST_SIZE,&hole2,res_mem); 
	test_small_alloc(index,SMALL_TEST_SIZE,&hole3,res_mem);
	test_small_free(index,hole1,SMALL_TEST_SIZE,res_mem);
	return hole2;
	}

element_type small_free_prep_2_2_2_1_2(FSingSet *index,int *res_mem)
	{ 
	element_type hole1,hole2;
	test_small_alloc(index,SMALL_TEST_SIZE,&hole1,res_mem); 
	test_small_alloc(index,SMALL_TEST_SIZE,&hole2,res_mem);
	return hole1;
	}

element_type small_free_prep_2_2_2_2(FSingSet *index,int *res_mem)
	{ 
	element_type hole1,hole2,hole3;
	test_small_alloc(index,SMALL_TEST_SIZE,&hole1,res_mem); 
	test_small_alloc(index,SMALL_TEST_SIZE,&hole2,res_mem);
	test_small_alloc(index,SMALL_TEST_SIZE,&hole3,res_mem);
	test_small_free(index,hole2,SMALL_TEST_SIZE,res_mem);
	return hole1;
	}

// ������� �����, �������

void _index_hole(FSingSet *index,element_type data_idx,unsigned size);
void _deindex_hole(FSingSet *index,element_type hole_idx,unsigned hs_idx,FHoleHeader *hole);

// �.�. ������� ���������� ����� ��� ����������� �� �������� ������������ ���������, ��� ���� ������ �����
// ����� � ������ ����� ��������� �� ������ ������������ ������ ��������, ��� ����� ����� ��������� ��� ����
// � ����� ��������� ����� res_mem
void test_index_hole(FSingSet *index,element_type data_idx,unsigned size,int *res_mem)
	{
	*res_mem -= size;
	_index_hole(index,data_idx,size);
	cp_mark_head_dirty(index->used_cpages);
	}

void test_deindex_hole(FSingSet *index,element_type hole_idx,unsigned size,int *res_mem)
	{
	*res_mem += size;
	unsigned hidx = HOLESIZE_IDX(size);
	if (index->head->holes[hidx] == hole_idx)
		cp_mark_head_dirty(index->used_cpages);
	_deindex_hole(index,hole_idx,hidx,(FHoleHeader *)regionPointer(index,hole_idx,size));
	}

//(3) ���������� �����
//	3_1 �� ������ ������� ��� �������
//(3.2) ������ ������� ��� �������
//	3_2_1 ������ ������� >= 64
//	3_2_2 ������ ������� < 64

element_type hole_index_prep_3_1(FSingSet *index,int *res_mem)
	{ return test_index_hole(index,1,MIN_HOLE_SIZE,res_mem),0; }

int hole_index_test_3_1(FSingSet *index,int *res_mem,element_type prep_data)
	{ return test_index_hole(index,1 + MIN_HOLE_SIZE,MIN_HOLE_SIZE,res_mem),0; }

int hole_index_test_3_2_1(FSingSet *index,int *res_mem,element_type prep_data)
	{ return test_index_hole(index,1,PAGE_SIZE - 2,res_mem),0; }

int hole_index_test_3_2_2(FSingSet *index,int *res_mem,element_type prep_data)
	{ return test_index_hole(index,1,MIN_HOLE_SIZE,res_mem),0; }

//(4) ������������ �����
//(4.1) ����� ������ � �������
//(4.1.1) ����� ��������� � �������
//	4_1_1_1 ������ ������� >= 64 
//	4_1_1_2 ������ ������� < 64
//	4_1_2 ����� �� ��������� � �������
//(4.2) ����� �� ������ � �������
//	4_2_1 ����� ��������� � �������
//	4_2_2 ����� �� ��������� � �������

int hole_deindex_test_small(FSingSet *index,int *res_mem,element_type prep_data)
	{ return test_deindex_hole(index,prep_data,MIN_HOLE_SIZE,res_mem),0; }

int hole_deindex_test_big(FSingSet *index,int *res_mem,element_type prep_data)
	{ return test_deindex_hole(index,prep_data,PAGE_SIZE - 2,res_mem),0; }

element_type hole_index_prep_4_1_1_1(FSingSet *index,int *res_mem)
	{ return test_index_hole(index,1,PAGE_SIZE - 2,res_mem),1; }

element_type hole_index_prep_4_1_1_2(FSingSet *index,int *res_mem)
	{ return test_index_hole(index,1,MIN_HOLE_SIZE,res_mem),1; }

element_type hole_index_prep_4_1_2(FSingSet *index,int *res_mem)
	{ 
	test_index_hole(index,1,MIN_HOLE_SIZE,res_mem); 
	test_index_hole(index,1 + MIN_HOLE_SIZE,MIN_HOLE_SIZE,res_mem);
	return 1; 
	}

element_type hole_index_prep_4_2_1(FSingSet *index,int *res_mem)
	{ 
	test_index_hole(index,1,MIN_HOLE_SIZE,res_mem); 
	test_index_hole(index,1 + MIN_HOLE_SIZE,MIN_HOLE_SIZE,res_mem);
	return 1; 
	}

element_type hole_index_prep_4_2_2(FSingSet *index,int *res_mem)
	{ 
	test_index_hole(index,1,MIN_HOLE_SIZE,res_mem); 
	test_index_hole(index,1 + MIN_HOLE_SIZE,MIN_HOLE_SIZE,res_mem);
	test_index_hole(index,1 + 2 * MIN_HOLE_SIZE,MIN_HOLE_SIZE,res_mem);
	return 1 + MIN_HOLE_SIZE; 
	}

// ������� �����, ���������/������������

// �������� ������ � ��������� ������ ���� �����-�� ������
element_type test_large_alloc(FSingSet *index,unsigned size,int *res_mem)
	{
	element_type rv; 
	element_type *space = idx_large_alloc(index,size,&rv);
	if (rv == ZERO_REF) return rv;
	space = PAGES_POINTER(index,rv);
	space[0] = space[size - 1] = INVALID_REF; // ZERO_REF �� ��������, �.�. ����� ���� ������� � ��� ����� ��� ������������
	unsigned i;
	for (i = 1; i < size - 1; i++)
		space[i]++;
	*res_mem += size;
	return rv;
	}

void test_large_free(FSingSet *index,element_type ref,unsigned size,int *res_mem)
	{
	idx_large_free(index,ref,size);
	element_type *space = PAGES_POINTER(index,ref);
	if (size > HOLE_HEADER_SIZE + HOLE_FOOTER_SIZE) 
		memset(&space[HOLE_HEADER_SIZE],0,(size - HOLE_HEADER_SIZE - HOLE_FOOTER_SIZE) * ELEMENT_SIZE);
	*res_mem -= size;
	}

//(5) ������� ���������
// 5_1 ���� ����� ������� �������
//(5.2) ��� ����� ������� �������, ���� ����� >= +MIN_HOLE_SIZE �������
// 5_2_1 ������ �������� ������� >= 64
//(5.2.2) ������ �������� ������� < 64
//	5_2_2_1 ������ ���������� ������� >= 64
//	5_2_2_2 ������ ���������� ������� < 64
//(5.2.3) 
//	5_2_3_1 ������ �������� ������� <= INDEXED_HOLESIZE_CNT (����������� 5_2_2_2)
//	5_2_3_2 ������ �������� ������� > INDEXED_HOLESIZE_CNT
//(5.3) ��� ���������� �����
//	5_3_1 ������� ����� � ������������� ������� >= ���������� ������� (��������� ������)
//	5_3_2 ������� ����� � ������������� ������� < ����. ������� � >= MIN_HOLE_SIZE
//	5_3_3 ������� ����� � ������������� ������� < MIN_HOLE_SIZE

int gen_alloc_test(FSingSet *index,int *res_mem,element_type prep_data)
	{
	cp_mark_head_dirty(index->used_cpages);
	return test_large_alloc(index,MIN_HOLE_SIZE,res_mem),0; 
	}

element_type gen_alloc_prep_5_1(FSingSet *index,int *res_mem)
	{
	element_type ref = test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	test_large_free(index,ref,MIN_HOLE_SIZE,res_mem);
	return 0;
	}

element_type gen_alloc_prep_large_small(FSingSet *index,int *res_mem)
	{
	element_type ref = test_large_alloc(index,PAGE_SIZE - MIN_HOLE_SIZE * 3 - 1,res_mem);
	test_large_alloc(index,MIN_HOLE_SIZE * 3,res_mem);
	test_large_free(index,ref,PAGE_SIZE - MIN_HOLE_SIZE * 3 - 1,res_mem);
	return 0;
	}

int gen_alloc_test_5_2_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	cp_mark_head_dirty(index->used_cpages);
	return test_large_alloc(index,PAGE_SIZE / 2 - MIN_HOLE_SIZE * 2,res_mem),0;
	}

element_type gen_alloc_prep_5_2_2_2(FSingSet *index,int *res_mem)
	{
	element_type ref = test_large_alloc(index,2 * MIN_HOLE_SIZE,res_mem);
	test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	test_large_free(index,ref,2 * MIN_HOLE_SIZE,res_mem);
	return 0;
	}

int gen_alloc_test_5_2_3_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	cp_mark_head_dirty(index->used_cpages);
	return test_large_alloc(index,PAGE_SIZE - MIN_HOLE_SIZE * 2,res_mem),0;
	}

element_type gen_alloc_prep_5_3_2(FSingSet *index,int *res_mem)
	{ return test_large_alloc(index,PAGE_SIZE - MIN_HOLE_SIZE - 1,res_mem),0; }

int gen_alloc_test_5_3_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	cp_mark_head_dirty(index->used_cpages);
	return test_large_alloc(index,MIN_HOLE_SIZE * 2,res_mem),0; 
	}

element_type gen_alloc_prep_5_3_3(FSingSet *index,int *res_mem)
	{
	*res_mem += 1; // ��������������� ������� ����� �� �������� (�.�. ������ ������� ���, �� �� ��������)
	return test_large_alloc(index,PAGE_SIZE - 2,res_mem),0; 
	}

//(6) ������� ������������
//(6.1) �������� �� �������� >= MIN_HOLE_SIZE 
// 6_1_1 ���� �����
// 6_1_2 ���� ��� �����
// 6_2 �������� �� �������� < MIN_HOLE_SIZE (����������� ������ 6_1_2)
//(6.3) ��������� � ������������� �������
// 6_3_1 ��������� � ������ �������� (����������� ������ 6_1_2)
// 6_3_2 ��������� �� � ������ ��������
//(6.4) �� ��������� � ������. �������
// 6_4_1 ������� �������� < MIN_HOLE_SIZE
//(6.4.2) ������� �������� >= MIN_HOLE_SIZE
// 6_4_2_1 ������ ��� �����
// 6_4_2_2 ������ �����
// 6_4_3 ����� ����� �� ��������� � ������. ������� � ������ ����� �������� 
// 6_4_4 ����� ����� �� ��������� � ������. ������� � ������ ������ �������� (����������� ������ 6_4_1)

int gen_free_test(FSingSet *index,int *res_mem,element_type prep_data)
	{
	cp_mark_head_dirty(index->used_cpages);
	return test_large_free(index,prep_data,MIN_HOLE_SIZE,res_mem),0; 
	}

element_type gen_free_prep_6_1_1(FSingSet *index,int *res_mem)
	{
	element_type ref1,ref2;
	ref1 = test_large_alloc(index,MIN_HOLE_SIZE,res_mem); 
	ref2 = test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	test_large_free(index,ref1,MIN_HOLE_SIZE,res_mem);
	return ref2;
	}

element_type gen_free_prep_6_1_2(FSingSet *index,int *res_mem)
	{
	test_large_alloc(index,PAGE_SIZE - 1,res_mem); 
	return test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	}

element_type gen_free_prep_6_3_2(FSingSet *index,int *res_mem)
	{
	test_large_alloc(index,PAGE_SIZE - 1,res_mem); 
	test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	return test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	}

element_type gen_free_prep_6_4_1(FSingSet *index,int *res_mem)
	{
	test_large_alloc(index,PAGE_SIZE - 1 - MIN_HOLE_SIZE - 1,res_mem); 
	element_type rv = test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	return rv;
	}

element_type gen_free_prep_6_4_2_1(FSingSet *index,int *res_mem)
	{
	test_large_alloc(index,PAGE_SIZE - 1 - MIN_HOLE_SIZE * 2,res_mem); 
	element_type rv = test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	return rv;
	}

element_type gen_free_prep_6_4_2_2(FSingSet *index,int *res_mem)
	{
	element_type rv = test_large_alloc(index,MIN_HOLE_SIZE,res_mem); 
	element_type ref = test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	test_large_free(index,ref,MIN_HOLE_SIZE,res_mem);
	return rv;
	}

element_type gen_free_prep_6_4_3(FSingSet *index,int *res_mem)
	{
	test_large_alloc(index,PAGE_SIZE - 1,res_mem); 
	element_type rv = test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	element_type ref = test_large_alloc(index,PAGE_SIZE - MIN_HOLE_SIZE,res_mem);
	test_large_alloc(index,MIN_HOLE_SIZE,res_mem);
	test_large_free(index,ref,MIN_HOLE_SIZE,res_mem);
	return rv;
	}

// ����� �������

int main(void)
	{
	char errbuf[512] = {0};
	int rv = 0;
	if (hole_size_idx_test(errbuf))
		printf("Test hole_size_idx failed: %s\n",errbuf),rv = 1;

	FTestData tests[] = {
		{"page_alloc",NULL,page_alloc_test,LM_NONE,0},

		{"small_alloc_1_1_1",small_alloc_prep_1_1_1,small_alloc_test,LM_NONE,0},
		{"small_alloc_1_1_2",small_alloc_prep_1_1_2,small_alloc_test_no_head,LM_NONE,0},
		{"small_alloc_1_2_1",small_alloc_prep_1_2_1,small_alloc_test,LM_NONE,0},
		{"small_alloc_1_2_2",small_alloc_prep_1_2_2,small_alloc_test,LM_NONE,0},
		{"small_alloc_1_3_1",NULL,small_alloc_test,LM_NONE,0},
		{"small_alloc_1_3_2_1",small_alloc_prep_1_3_2_1,small_alloc_test,LM_NONE,0},
		{"small_alloc_1_3_2_2",small_alloc_prep_1_3_2_1,small_alloc_test,LM_NONE,0},

		{"small_free_2_1_1_1",small_free_prep_2_1_1_1,small_free_test,LM_NONE,0},
		{"small_free_2_1_1_2_1",small_free_prep_2_1_1_2_1,small_free_test_no_head,LM_NONE,0},
		{"small_free_2_1_1_2_2",small_free_prep_2_1_1_2_2,small_free_test,LM_NONE,0},
		{"small_free_2_1_2",small_free_prep_2_1_2,small_free_test,LM_NONE,0},
		{"small_free_2_1_5_1",small_free_prep_2_1_5_1,small_free_test,LM_NONE,0},
		{"small_free_2_1_5_2",small_free_prep_2_1_5_2,small_free_test,LM_NONE,0},
		{"small_free_2_1_6_2_1",small_free_prep_2_1_6_2_1,small_free_test,LM_NONE,0},
		{"small_free_2_1_6_2_2",small_free_prep_2_1_6_2_2,small_free_test,LM_NONE,0},
		{"small_free_2_2_1",small_free_prep_2_2_1,small_free_test,LM_NONE,0},
		{"small_free_2_2_2_1_1",small_free_prep_2_2_2_1_1,small_free_test,LM_NONE,0},
		{"small_free_2_2_2_1_2",small_free_prep_2_2_2_1_2,small_free_test,LM_NONE,0},
		{"small_free_2_2_2_2",small_free_prep_2_2_2_2,small_free_test_no_head,LM_NONE,0},

		{"hole_index_3_1",hole_index_prep_3_1,hole_index_test_3_1,LM_NONE,0},
		{"hole_index_3_2_1",NULL,hole_index_test_3_2_1,LM_NONE,0},
		{"hole_index_3_2_2",NULL,hole_index_test_3_2_2,LM_NONE,0},

		{"hole_index_4_1_1_1",hole_index_prep_4_1_1_1,hole_deindex_test_big,LM_NONE,0},
		{"hole_index_4_1_1_2",hole_index_prep_4_1_1_2,hole_deindex_test_small,LM_NONE,0},
		{"hole_index_4_1_2",hole_index_prep_4_1_2,hole_deindex_test_small,LM_NONE,0},
		{"hole_index_4_2_1",hole_index_prep_4_2_1,hole_deindex_test_small,LM_NONE,0},
		{"hole_index_4_2_2",hole_index_prep_4_2_2,hole_deindex_test_small,LM_NONE,0},

		{"gen_alloc_5_1",gen_alloc_prep_5_1,gen_alloc_test,LM_NONE,0},
		{"gen_alloc_5_2_1",gen_alloc_prep_large_small,gen_alloc_test_5_2_1,LM_NONE,0},
		{"gen_alloc_5_2_2_1",gen_alloc_prep_large_small,gen_alloc_test,LM_NONE,0},
		{"gen_alloc_5_2_2_2",gen_alloc_prep_5_2_2_2,gen_alloc_test,LM_NONE,0},
		{"gen_alloc_5_2_3_2",gen_alloc_prep_large_small,gen_alloc_test_5_2_3_2,LM_NONE,0},
		{"gen_alloc_5_3_1",NULL,gen_alloc_test,LM_NONE,0},
		{"gen_alloc_5_3_2",gen_alloc_prep_5_3_2,gen_alloc_test_5_3_2,LM_NONE,0},
		{"gen_alloc_5_3_3",gen_alloc_prep_5_3_3,gen_alloc_test,LM_NONE,0},

		{"gen_free_6_1_1",gen_free_prep_6_1_1,gen_free_test,LM_NONE,0},
		{"gen_free_6_1_2",gen_free_prep_6_1_2,gen_free_test,LM_NONE,0},
		{"gen_free_6_3_2",gen_free_prep_6_3_2,gen_free_test,LM_NONE,0},
		{"gen_free_6_4_1",gen_free_prep_6_4_1,gen_free_test,LM_NONE,0},
		{"gen_free_6_4_2_1",gen_free_prep_6_4_2_1,gen_free_test,LM_NONE,0},
		{"gen_free_6_4_2_2",gen_free_prep_6_4_2_2,gen_free_test,LM_NONE,0},
		{"gen_free_6_4_3",gen_free_prep_6_4_3,gen_free_test,LM_NONE,0},
		};
	
	unsigned i,tcnt = sizeof(tests) / sizeof(FTestData);
	for (i = 0; i < tcnt; i++)
		{
		if (run_test(&tests[i]))
			rv = 1;
		}
	return rv;
	}