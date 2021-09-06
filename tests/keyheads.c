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

// ���������� / ������������ �����

void _index_kh_hole(FSingSet *index,FKeyHeadGeneral *kh,element_type khref,unsigned size);
void _deindex_kh_hole(FSingSet *index,FKeyHeadGeneral *kh,element_type khref,unsigned size);

// ����������� ����� � ����� ����������, �������� �� ������ �� ������� ������, �������� ��������� ����� � ����� ��� �������
void test_index_kh_hole(FSingSet *index,element_type khref,unsigned size,int *res_mem)
	{
	FKeyHeadGeneral *kh_block = (FKeyHeadGeneral *)regionPointer(index,KH_BLOCK_IDX(khref),KH_BLOCK_SIZE);
	unsigned kh_num = KH_BLOCK_NUM(khref);
	_index_kh_hole(index,&kh_block[kh_num],khref,size);
	if (kh_num)
		kh_block[kh_num - 1].space.space_used = 1;
	if (kh_num + size <= KH_BLOCK_LAST)
		kh_block[kh_num + size].space.space_used = 1;
	cp_mark_hblock_dirty(index->used_cpages,khref);
	if (size > 1)
		*res_mem -= size * KEY_HEAD_SIZE;
	}

// ������������� ����� � ����� ����������, � ��������� �� ������ � ������� ������
void test_deindex_kh_hole(FSingSet *index,element_type khref,unsigned size,int *res_mem)
	{
	_deindex_kh_hole(index,(FKeyHeadGeneral *)pagesPointer(index,khref),khref,size);
	*res_mem += size * KEY_HEAD_SIZE;
	}

// �������� �������� ��� ������ ����� � ��������� �� ������ � ������� ������
element_type alloc_header_page(FSingSet *index,int *res_mem)
	{
	*res_mem += PAGE_SIZE; 
	return idx_alloc_page(index,PT_HEADERS) * PAGE_SIZE; 
	}

//(1) ���������� �����
// 1_1 ������ ����� 1
//(1.2) ���������� �����
// 1_2_1 ������� ����� ������
// 1_2_2 ������� ����� �� ������

element_type index_block_prep(FSingSet *index,int *res_mem)
	{ return alloc_header_page(index,res_mem); }

int index_block_test_1_1(FSingSet *index,int *res_mem,element_type prep_data)
	{ return test_index_kh_hole(index,prep_data,1,res_mem),0;	}

int index_block_test_1_2_1(FSingSet *index,int *res_mem,element_type prep_data)
	{ return test_index_kh_hole(index,prep_data,2,res_mem),0;	}

element_type index_block_prep_1_2_2(FSingSet *index,int *res_mem)
	{
	element_type pref = alloc_header_page(index,res_mem);
	test_index_kh_hole(index,pref,2,res_mem);
	return pref;
	}

int index_block_test_1_2_2(FSingSet *index,int *res_mem,element_type prep_data)
	{ return test_index_kh_hole(index,prep_data + DISK_PAGE_SIZE,2,res_mem),0; }

//(2) ������������ �����
//(2.1) ����� ������ � �������
// 2_1_1 ����� ��������� � �������
// 2_1_2 ����� �� ��������� � �������
//(2.2) ����� �� ������ � �������
// 2_2_1 ����� ��������� � �������
// 2_2_2 ����� �� ��������� � �������

int deindex_block_test(FSingSet *index,int *res_mem,element_type prep_data)
	{ return test_deindex_kh_hole(index,prep_data,2,res_mem),0; }

element_type deindex_block_prep_2_1_1(FSingSet *index,int *res_mem)
	{ 
	element_type ref = alloc_header_page(index,res_mem);
	test_index_kh_hole(index,ref,2,res_mem);
	return ref;
	}

element_type deindex_block_prep_2_1_2(FSingSet *index,int *res_mem)
	{ 
	element_type ref = alloc_header_page(index,res_mem);
	test_index_kh_hole(index,ref,2,res_mem);
	test_index_kh_hole(index,ref + DISK_PAGE_SIZE,2,res_mem);
	return ref + DISK_PAGE_SIZE;
	}

element_type deindex_block_prep_2_2_1(FSingSet *index,int *res_mem)
	{ 
	element_type ref = alloc_header_page(index,res_mem);
	test_index_kh_hole(index,ref,2,res_mem);
	test_index_kh_hole(index,ref + DISK_PAGE_SIZE,2,res_mem);
	return ref;
	}

element_type deindex_block_prep_2_2_2(FSingSet *index,int *res_mem)
	{ 
	element_type ref = alloc_header_page(index,res_mem);
	test_index_kh_hole(index,ref,2,res_mem);
	test_index_kh_hole(index,ref + DISK_PAGE_SIZE,2,res_mem);
	test_index_kh_hole(index,ref + DISK_PAGE_SIZE * 2,2,res_mem);
	return ref + DISK_PAGE_SIZE;
	}

// ���������� �������

//(3) ���������� ������� �����
// 3_1 ��������� ��������������
//(3_2) ��������� �� ��������������
// 3_2_1 ����� � �������� �����
// 3_2_2 ����� � ����� �����
// 3_2_3 ����� �������� 2 (����������� ������ 3_2_1)
// 3_2_4 ����� �������� 7 (����������� ������ 3_2_2)

int expand_chain_up_test(FSingSet *index,int *res_mem,element_type prep_data)
	{ 
	FKeyHead data = {0};
	data.space_used = 1;
	element_type hblock_idx = KH_BLOCK_IDX(prep_data);
	unsigned hnum = KH_BLOCK_NUM(prep_data);
	kh_expand_chain_up(index,(FKeyHeadGeneral *)pagesPointer(index,hblock_idx),hblock_idx,hnum,&data);
	return 0;
	}

element_type expand_chain_up_prep_3_1(FSingSet *index,int *res_mem)
	{ 
	element_type ref = alloc_header_page(index,res_mem);
	FKeyHeadGeneral *khblock = (FKeyHeadGeneral *)pagesPointer(index,ref);
	khblock[0].space.space_used = 1;
	khblock[1].space.reserved = 1;
	khblock[2].space.space_used = 1;
	return ref + KEY_HEAD_SIZE;
	}

element_type expand_chain_up_prep_3_2_1(FSingSet *index,int *res_mem)
	{ 
	element_type ref = alloc_header_page(index,res_mem) + KEY_HEAD_SIZE;
	test_index_kh_hole(index,ref,2,res_mem);
	*res_mem += KEY_HEAD_SIZE * 2; // �.�. �� ���� ������������� � ���� ������
	return ref;
	}

element_type expand_chain_up_prep_3_2_2(FSingSet *index,int *res_mem)
	{ 
	element_type ref = alloc_header_page(index,res_mem) + KEY_HEAD_SIZE;
	test_index_kh_hole(index,ref,7,res_mem);
	*res_mem += KEY_HEAD_SIZE; // �.�. �� ���� ������
	return ref;
	}

//(4) ���������� ������� ����
// 4_1 ��������� ��������������
//(4_2) ��������� �� ��������������
// 4_2_1 ����� � ������ �����
// 4_2_2 ����� � �������� �����
// 4_2_3 ����� �������� 2 (����������� ������ 4_2_1)
// 4_2_4 ����� �������� 7 (����������� ������ 4_2_2)

int expand_chain_down_test(FSingSet *index,int *res_mem,element_type prep_data)
	{ 
	FKeyHead data = {0};
	data.space_used = 1;
	element_type hblock_idx = KH_BLOCK_IDX(prep_data);
	unsigned hnum = KH_BLOCK_NUM(prep_data);
	kh_expand_chain_down(index,(FKeyHeadGeneral *)pagesPointer(index,hblock_idx),hblock_idx,hnum,&data);
	return 0;
	}

element_type expand_chain_down_prep_4_1(FSingSet *index,int *res_mem)
	{ 
	element_type ref = alloc_header_page(index,res_mem);
	FKeyHeadGeneral *khblock = (FKeyHeadGeneral *)pagesPointer(index,ref);
	khblock[0].space.space_used = 1;
	khblock[1].space.reserved = 1;
	khblock[2].space.space_used = 1;
	return ref + KEY_HEAD_SIZE;
	}

element_type expand_chain_down_prep_4_2_1(FSingSet *index,int *res_mem)
	{ 
	element_type ref = alloc_header_page(index,res_mem);
	test_index_kh_hole(index,ref,2,res_mem);
	*res_mem += KEY_HEAD_SIZE * 2; // �.�. �� ���� ������������� � ���� ������
	return ref + KEY_HEAD_SIZE;
	}

element_type expand_chain_down_prep_4_2_2(FSingSet *index,int *res_mem)
	{ 
	element_type ref = alloc_header_page(index,res_mem);
	test_index_kh_hole(index,ref,7,res_mem);
	*res_mem += KEY_HEAD_SIZE; // �.�. �� ���� ������
	return ref + KEY_HEAD_SIZE * 6;
	}

// ����� ����� ����������� �������

FKeyHeadGeneral *_kh_shift_hole(FSingSet *index,unsigned size,element_type *kh_idx);

//(5) ��������� ����� �� ������� ������� �������
// 5_1 ����� ������������ � �������
// 5_2 ����� �� ������������ � �������

int shift_hole_test(FSingSet *index,int *res_mem,element_type prep_data)
	{ 
	element_type kh_idx;
	FKeyHeadGeneral *res = _kh_shift_hole(index,(unsigned)prep_data,&kh_idx);
	if (!res) 
		return 1;
	res->fields.space_used = 1;
	*res_mem += prep_data * KEY_HEAD_SIZE;
	return 0;
	}

element_type shift_hole_prep_5_1(FSingSet *index,int *res_mem)
	{ 
	element_type ref = alloc_header_page(index,res_mem);
	test_index_kh_hole(index,ref,2,res_mem);
	return 2;
	}

element_type shift_hole_prep_5_2(FSingSet *index,int *res_mem)
	{ 
	element_type ref = alloc_header_page(index,res_mem);
	test_index_kh_hole(index,ref,2,res_mem);
	test_index_kh_hole(index,ref + DISK_PAGE_SIZE,2,res_mem);
	return 2;
	}

//  ��������� ����� ���������� �� ������������� �������

element_type _kh_alloc_from_zone(FSingSet *index);

element_type test_kh_alloc_from_zone(FSingSet *index,int *res_mem)
	{
	*res_mem += KEY_HEAD_SIZE * KEYHEADS_IN_BLOCK;
	return _kh_alloc_from_zone(index);
	}

//(6) ��������� ����� ���������� �� ������������� �������
// 6_1 ��� ������������� �������
// 6_2 ���� ������������� ������� ������ ������ �����
// 6_3 ���� ������������� ������� � ���� ����

int alloc_from_zone_test(FSingSet *index,int *res_mem,element_type prep_data)
	{ 
	element_type kh_idx = test_kh_alloc_from_zone(index,res_mem);
	if (kh_idx == KH_ZERO_REF)
		return 1;
	if (index->head->kh_alloc_zone == KH_ZERO_REF)
		return 1;
	FKeyHeadGeneral *kh = (FKeyHeadGeneral *)pagesPointer(index,kh_idx);
	kh->fields.space_used = 1;
	return 0;
	}

element_type alloc_from_zone_prep_6_2(FSingSet *index,int *res_mem)
	{ 
	test_kh_alloc_from_zone(index,res_mem);
	return 0;
	}

element_type alloc_from_zone_prep_6_3(FSingSet *index,int *res_mem)
	{ 
	unsigned i = PAGE_SIZE / KEY_HEAD_SIZE;
	for (i = 0; i < PAGE_SIZE / KEY_HEAD_SIZE - 1; i++)
		test_kh_alloc_from_zone(index,res_mem);
	return 0;
	}

int alloc_from_zone_test_6_3(FSingSet *index,int *res_mem,element_type prep_data)
	{ 
	element_type kh_idx = test_kh_alloc_from_zone(index,res_mem);
	if (kh_idx == KH_ZERO_REF)
		return 1;
	if (index->head->kh_alloc_zone != KH_ZERO_REF)
		return 1;
	FKeyHeadGeneral *kh = (FKeyHeadGeneral *)pagesPointer(index,kh_idx);
	kh->fields.space_used = 1;
	return 0;
	}

// ��������� ����������

//(7) ��������� ������ ���������
// 7_1 ���� ����� ������� 3 � ������ �����
// 7_2 ���� ����� ������� 3 �� � ������ �����
// 7_3 ���� ����� ������� 2 �� � ������ �����
// 7_4 ��� �����

int alloc_one_test(FSingSet *index,int *res_mem,element_type prep_data)
	{
	element_type ref;
	FKeyHeadGeneral *kh = kh_alloc_one(index,&ref);
	if (!kh)
		return 1;
	kh->fields.space_used = 1;
	return 0;
	}

element_type alloc_one_prep_7_1(FSingSet *index,int *res_mem)
	{
	element_type ref = alloc_header_page(index,res_mem);
	test_index_kh_hole(index,ref,3,res_mem);
	*res_mem += KEY_HEAD_SIZE; // �.�. �� ���� ������ � ��������� ����� � 2
	return 0;
	}

element_type alloc_one_prep_7_2(FSingSet *index,int *res_mem)
	{
	element_type ref = alloc_header_page(index,res_mem);
	test_index_kh_hole(index,ref + KEY_HEAD_SIZE,3,res_mem);
	*res_mem += KEY_HEAD_SIZE * 3; // �.�. �� ���� ������ � ��� ���������������
	return 0;
	}

element_type alloc_one_prep_7_3(FSingSet *index,int *res_mem)
	{
	element_type ref = alloc_header_page(index,res_mem);
	test_index_kh_hole(index,ref + KEY_HEAD_SIZE,2,res_mem);
	*res_mem += KEY_HEAD_SIZE * 2; // �.�. �� ���� ������ � ���� ���������������
	return 0;
	}

element_type alloc_one_prep_7_4(FSingSet *index,int *res_mem)
	{
	*res_mem += KEY_HEAD_SIZE; // �.�. �� ���� ������, 7 ����� � ������� � ������� �������� ���� ����� �����
	return 0;
	}


//(8) ��������� ������� 1 < n < 8 ����������
// 8_1 ���� ����� ������� �������
// 8_2 ���� ����� �� ���� ������
// 8_3 ���� ����� �� ��� ������ � ������ �����
// 8_4 ���� ����� �� ��� ������ �� � ������ �����
// 8_5 ��� �����

int alloc_block_test(FSingSet *index,int *res_mem,element_type prep_data)
	{
	element_type ref;
	FKeyHeadGeneral *kh = kh_alloc_block(index,2,&ref);
	if (!kh)
		return 1;
	kh[0].fields.space_used = 1;
	kh[1].fields.space_used = 1;
	return 0;
	}

element_type alloc_block_prep_8_1(FSingSet *index,int *res_mem)
	{
	element_type ref = alloc_header_page(index,res_mem);
	test_index_kh_hole(index,ref,2,res_mem);
	*res_mem += KEY_HEAD_SIZE * 2; // �.�. �� ��� ������
	return 0;
	}

element_type alloc_block_prep_8_2(FSingSet *index,int *res_mem)
	{
	element_type ref = alloc_header_page(index,res_mem);
	test_index_kh_hole(index,ref,3,res_mem);
	*res_mem += KEY_HEAD_SIZE * 3; // �.�. �� ��� ������ � ���� ���������������
	return 0;
	}

element_type alloc_block_prep_8_3(FSingSet *index,int *res_mem)
	{
	element_type ref = alloc_header_page(index,res_mem);
	test_index_kh_hole(index,ref,4,res_mem);
	*res_mem += KEY_HEAD_SIZE * 2; // �.�. �� ��� ������ � ��� ����� � �����
	return 0;
	}

element_type alloc_block_prep_8_4(FSingSet *index,int *res_mem)
	{
	element_type ref = alloc_header_page(index,res_mem);
	test_index_kh_hole(index,ref + KEY_HEAD_SIZE,4,res_mem);
	*res_mem += KEY_HEAD_SIZE * 4; // �.�. �� ��� ������ � ��� ���������������
	return 0;
	}

element_type alloc_block_prep_8_5(FSingSet *index,int *res_mem)
	{
	*res_mem += KEY_HEAD_SIZE * 2; // �.�. �� ��� ������, 6 ����� � ������� � ������� �������� ���� ����� �����
	return 0;
	}

//(9) ��������� ������� 8 ����������
// 9_1 ���� �����
// 9_2 ��� �����

int alloc_full_test(FSingSet *index,int *res_mem,element_type prep_data)
	{
	element_type ref;
	unsigned i;
	FKeyHeadGeneral *kh = kh_alloc_full_block(index,&ref);
	if (!kh)
		return 1;
	for (i = 0; i < KEYHEADS_IN_BLOCK; i++)
		kh[i].fields.space_used = 1;
	return 0;
	}

element_type alloc_full_prep_9_1(FSingSet *index,int *res_mem)
	{
	element_type ref = alloc_header_page(index,res_mem);
	test_index_kh_hole(index,ref,8,res_mem);
	*res_mem += KEY_HEAD_SIZE * 8; // �.�. �� ��� ������ � ��� ���������������
	return 0;
	}

element_type alloc_full_prep_9_2(FSingSet *index,int *res_mem)
	{
	*res_mem += KEY_HEAD_SIZE * 8; 
	return 0;
	}

// ������������ �������

element_type _kh_merge_hole_down(FSingSet *index,FKeyHeadGeneral *kh,element_type block_idx);
element_type _kh_merge_hole_up(FSingSet *index,FKeyHeadGeneral *kh,element_type block_idx);

FKeyHeadGeneral *test_kh_alloc_block(FSingSet *index,unsigned size,element_type *ref,int *res_mem)
	{
	unsigned i;
	FKeyHeadGeneral *kh = kh_alloc_block(index,size,ref);
	for (i = 0; i < size; i++) 
		kh[i].fields.space_used = 1;
	*res_mem += KEY_HEAD_SIZE * size;
	return kh;
	}

void test_kh_free_block(FSingSet *index,FKeyHeadGeneral *kh,element_type kh_idx,unsigned size,int *res_mem)
	{
	kh_free_block(index,kh,kh_idx,size);
	*res_mem -= KEY_HEAD_SIZE * size;
	}

//(10) ������ � ������ �����
// 10_1 ����� ������� ���������
// 10_2 ������������� ������� ��������� � ������� ����� ������
// 10_3 ����� ����������������� ���������
// 10_4 ����� �����, �� ��� �������������� �������
// 10_5 ����� �����, �� ��� ������� �����

element_type merge_hole_up_prep_10_1(FSingSet *index,int *res_mem)
	{
	element_type ref,ref2;
	test_kh_alloc_block(index,4,&ref,res_mem);
	test_kh_alloc_block(index,4,&ref2,res_mem);
	return ref;
	}

int merge_hole_up_test_10_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	prep_data += 4 * KEY_HEAD_SIZE;
	unsigned res = _kh_merge_hole_up(index,(FKeyHeadGeneral *)pagesPointer(index,prep_data),prep_data);
	if (res)
		return 1;
	return 0;
	}

element_type merge_hole_up_prep_10_2(FSingSet *index,int *res_mem)
	{
	element_type ref,ref2;
	test_kh_alloc_block(index,4,&ref,res_mem);
	test_kh_alloc_block(index,4,&ref2,res_mem);
	return ref2;
	}

// ���� 10_2 ���������� 10_1

element_type merge_hole_up_prep_10_3(FSingSet *index,int *res_mem)
	{
	element_type ref,ref2;
	test_kh_alloc_block(index,4,&ref,res_mem);
	test_kh_alloc_block(index,3,&ref2,res_mem);
	*res_mem += KEY_HEAD_SIZE; // �.�. ���������� ��������� � ����� ����� ��������������
	return ref;
	}

int merge_hole_up_test_10_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	prep_data += 4 * KEY_HEAD_SIZE;
	unsigned res = _kh_merge_hole_up(index,(FKeyHeadGeneral *)pagesPointer(index,prep_data),prep_data);
	if (res != 1)
		return 1;
	return 0;
	}

element_type merge_hole_up_prep_10_4(FSingSet *index,int *res_mem)
	{
	element_type ref,ref2,ref3;
	test_kh_alloc_block(index,2,&ref,res_mem);
	FKeyHeadGeneral *kh = test_kh_alloc_block(index,2,&ref2,res_mem);
	test_kh_alloc_block(index,2,&ref3,res_mem); // �� ������ ������ ��������� ������
	test_kh_free_block(index,kh,ref2,2,res_mem); 
	return ref;
	}

int merge_hole_up_test_10_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	prep_data += 2 * KEY_HEAD_SIZE;
	unsigned res = _kh_merge_hole_up(index,(FKeyHeadGeneral *)pagesPointer(index,prep_data),prep_data);
	if (res != 4)
		return 1;
	*res_mem += res * KEY_HEAD_SIZE;
	return 0;
	}

element_type merge_hole_up_prep_10_5(FSingSet *index,int *res_mem)
	{
	element_type ref;
	test_kh_alloc_block(index,4,&ref,res_mem);
	return ref;
	}

int merge_hole_up_test_10_5(FSingSet *index,int *res_mem,element_type prep_data)
	{
	prep_data += 4 * KEY_HEAD_SIZE;
	unsigned res = _kh_merge_hole_up(index,(FKeyHeadGeneral *)pagesPointer(index,prep_data),prep_data);
	if (res != 4)
		return 1;
	*res_mem += res * KEY_HEAD_SIZE;
	return 0;
	}

//(11) ������ � ������ ����
// 11_1 ���� ������� ���������
// 11_2 ������������� ������� ��������� � ������� ����� �����
// 11_3 ���� ����������������� ���������
// 11_4 ���� �����, �� ��� ������� �������
// 11_5 ���� �����, �� ��� ������� �����, �� ��� �����

element_type merge_hole_down_prep_11_1(FSingSet *index,int *res_mem)
	{
	element_type ref,ref2;
	test_kh_alloc_block(index,4,&ref,res_mem);
	test_kh_alloc_block(index,4,&ref2,res_mem);
	return ref2;
	}

int merge_hole_down_test_11_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	unsigned res = _kh_merge_hole_down(index,(FKeyHeadGeneral *)pagesPointer(index,prep_data),prep_data);
	if (res)
		return 1;
	return 0;
	}

element_type merge_hole_down_prep_11_2(FSingSet *index,int *res_mem)
	{
	element_type ref,ref2;
	test_kh_alloc_block(index,4,&ref,res_mem);
	test_kh_alloc_block(index,4,&ref2,res_mem);
	return ref;
	}

// ���� 11_2 ���������� 11_1

element_type merge_hole_down_prep_11_3(FSingSet *index,int *res_mem)
	{
	element_type ref,ref2;
	test_kh_alloc_block(index,3,&ref,res_mem);
	test_kh_alloc_block(index,4,&ref2,res_mem);
	*res_mem += KEY_HEAD_SIZE; // �.�. ���������� ��������� � ����� ����� ��������������
	return ref2;
	}

int merge_hole_down_test_11_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	unsigned res = _kh_merge_hole_down(index,(FKeyHeadGeneral *)pagesPointer(index,prep_data),prep_data);
	if (res != 1)
		return 1;
	return 0;
	}

element_type merge_hole_down_prep_11_4(FSingSet *index,int *res_mem)
	{
	element_type ref = alloc_header_page(index,res_mem);
	test_index_kh_hole(index,ref + KEY_HEAD_SIZE,4,res_mem);
	return ref + KEY_HEAD_SIZE * 5;
	}

int merge_hole_down_test_11_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	unsigned res = _kh_merge_hole_down(index,(FKeyHeadGeneral *)pagesPointer(index,prep_data),prep_data);
	if (res != 4)
		return 1;
	*res_mem += res * KEY_HEAD_SIZE;
	return 0;
	}

element_type merge_hole_down_prep_11_5(FSingSet *index,int *res_mem)
	{
	element_type ref = alloc_header_page(index,res_mem);
	test_index_kh_hole(index,ref,8,res_mem);
	test_index_kh_hole(index,ref + 8 * KEY_HEAD_SIZE,4,res_mem);
	return ref + KEY_HEAD_SIZE * 12;
	}

int merge_hole_down_test_11_5(FSingSet *index,int *res_mem,element_type prep_data)
	{
	unsigned res = _kh_merge_hole_down(index,(FKeyHeadGeneral *)pagesPointer(index,prep_data),prep_data);
	if (res != 4)
		return 1;
	*res_mem += res * KEY_HEAD_SIZE;
	return 0;
	}

// 12 ������������ ������� 

element_type free_block_prep_12(FSingSet *index,int *res_mem)
	{
	element_type ref,ref2;
	test_kh_alloc_block(index,3,&ref,res_mem);
	test_kh_alloc_block(index,2,&ref2,res_mem);
	return ref2;
	}

int free_block_test_12(FSingSet *index,int *res_mem,element_type prep_data)
	{
	test_kh_free_block(index,(FKeyHeadGeneral *)pagesPointer(index,prep_data),prep_data,2,res_mem);
	return 0;
	}

// 13 ������������ ���������� �������� �������

element_type free_last_from_chain_prep_13(FSingSet *index,int *res_mem)
	{
	element_type ref,ref2;
	test_kh_alloc_block(index,3,&ref,res_mem);
	test_kh_alloc_block(index,2,&ref2,res_mem);
	return ref;
	}

int free_last_from_chain_test_13(FSingSet *index,int *res_mem,element_type prep_data)
	{
	prep_data += 2 * KEY_HEAD_SIZE;
	kh_free_last_from_chain(index,(FKeyHeadGeneral *)pagesPointer(index,prep_data),prep_data);
	*res_mem -= KEY_HEAD_SIZE;
	return 0;
	}

int main(void)
	{
	int rv = 0;
	FTestData tests[] = {
		{"index_block_1_1",index_block_prep,index_block_test_1_1,LM_NONE,0},
		{"index_block_1_2_1",index_block_prep,index_block_test_1_2_1,LM_NONE,0},
		{"index_block_1_2_2",index_block_prep_1_2_2,index_block_test_1_2_2,LM_NONE,0},
		{"deindex_block_2_1_1",deindex_block_prep_2_1_1,deindex_block_test,LM_NONE,0},
		{"deindex_block_2_1_2",deindex_block_prep_2_1_2,deindex_block_test,LM_NONE,0},
		{"deindex_block_2_2_1",deindex_block_prep_2_2_1,deindex_block_test,LM_NONE,0},
		{"deindex_block_2_2_2",deindex_block_prep_2_2_2,deindex_block_test,LM_NONE,0},
		{"expand_chain_up_3_1",expand_chain_up_prep_3_1,expand_chain_up_test,LM_NONE,0},
		{"expand_chain_up_3_2_1",expand_chain_up_prep_3_2_1,expand_chain_up_test,LM_NONE,0},
		{"expand_chain_up_3_2_2",expand_chain_up_prep_3_2_2,expand_chain_up_test,LM_NONE,0},
		{"expand_chain_down_4_1",expand_chain_down_prep_4_1,expand_chain_down_test,LM_NONE,0},
		{"expand_chain_down_4_2_1",expand_chain_down_prep_4_2_1,expand_chain_down_test,LM_NONE,0},
		{"expand_chain_down_4_2_2",expand_chain_down_prep_4_2_2,expand_chain_down_test,LM_NONE,0},
		{"shift_hole_5_1",shift_hole_prep_5_1,shift_hole_test,LM_NONE,0},
		{"shift_hole_5_2",shift_hole_prep_5_2,shift_hole_test,LM_NONE,0},
		{"alloc_from_zone_6_1",NULL,alloc_from_zone_test,LM_NONE,0},
		{"alloc_from_zone_6_2",alloc_from_zone_prep_6_2,alloc_from_zone_test,LM_NONE,0},
		{"alloc_from_zone_6_3",alloc_from_zone_prep_6_3,alloc_from_zone_test_6_3,LM_NONE,0},
		{"alloc_one_7_1",alloc_one_prep_7_1,alloc_one_test,LM_NONE,0},
		{"alloc_one_7_2",alloc_one_prep_7_2,alloc_one_test,LM_NONE,0},
		{"alloc_one_7_3",alloc_one_prep_7_3,alloc_one_test,LM_NONE,0},
		{"alloc_one_7_4",alloc_one_prep_7_4,alloc_one_test,LM_NONE,0},
		{"alloc_block_8_1",alloc_block_prep_8_1,alloc_block_test,LM_NONE,0},
		{"alloc_block_8_2",alloc_block_prep_8_2,alloc_block_test,LM_NONE,0},
		{"alloc_block_8_3",alloc_block_prep_8_3,alloc_block_test,LM_NONE,0},
		{"alloc_block_8_4",alloc_block_prep_8_4,alloc_block_test,LM_NONE,0},
		{"alloc_block_8_5",alloc_block_prep_8_5,alloc_block_test,LM_NONE,0},
		{"alloc_full_9_1",alloc_full_prep_9_1,alloc_full_test,LM_NONE,0},
		{"alloc_full_9_2",alloc_full_prep_9_2,alloc_full_test,LM_NONE,0},
		{"merge_hole_up_10_1",merge_hole_up_prep_10_1,merge_hole_up_test_10_1,LM_NONE,0},
		{"merge_hole_up_10_2",merge_hole_up_prep_10_2,merge_hole_up_test_10_1,LM_NONE,0},
		{"merge_hole_up_10_3",merge_hole_up_prep_10_3,merge_hole_up_test_10_3,LM_NONE,0},
		{"merge_hole_up_10_4",merge_hole_up_prep_10_4,merge_hole_up_test_10_4,LM_NONE,0},
		{"merge_hole_up_10_5",merge_hole_up_prep_10_5,merge_hole_up_test_10_5,LM_NONE,0},
		{"merge_hole_down_11_1",merge_hole_down_prep_11_1,merge_hole_down_test_11_1,LM_NONE,0},
		{"merge_hole_down_11_2",merge_hole_down_prep_11_2,merge_hole_down_test_11_1,LM_NONE,0},
		{"merge_hole_down_11_3",merge_hole_down_prep_11_3,merge_hole_down_test_11_3,LM_NONE,0},
		{"merge_hole_down_11_4",merge_hole_down_prep_11_4,merge_hole_down_test_11_4,LM_NONE,0},
		{"merge_hole_down_11_5",merge_hole_down_prep_11_5,merge_hole_down_test_11_5,LM_NONE,0},
		{"free_block_12",free_block_prep_12,free_block_test_12,LM_NONE,0},
		{"free_last_from_chain_13",free_last_from_chain_prep_13,free_last_from_chain_test_13,LM_NONE,0},
		};
	
	unsigned i,tcnt = sizeof(tests) / sizeof(FTestData);
	for (i = 0; i < tcnt; i++)
		{
		if (run_test(&tests[i]))
			rv = 1;
		}
	return rv;
	}
