/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <stddef.h>
#include <stdio.h>

#include "keyhead.h"
#include "index.h"
#include "cpages.h"
#include "utils.h"

void _index_kh_hole(FSingSet *index,FKeyHeadGeneral *kh,element_type khref,unsigned size)
	{
	FSetHead *head = index->head;
	FAILURE_CHECK((FKeyHeadGeneral *)PAGES_POINTER(index,khref) != kh,"khref does not match khlinks");
	FAILURE_CHECK(KH_BLOCK_NUM(khref) + size > KEYHEADS_IN_BLOCK,"size is too big in");

	if (!--size)
		{ kh->space.reserved = 1; return; }
	// prev не выставляем, у первой дырки не проверяется
	FORMATTED_LOG_HEADER("indexing header block hole %u at %u[%u]\n",size,KH_BLOCK_IDX(khref),KH_BLOCK_NUM(khref));
	if ((kh->links.next = head->key_holes[size]) != KH_ZERO_REF)
		{
		FKeyHeadLinks *w_hole = (FKeyHeadLinks *)pagesPointer(index,kh->links.next);
		w_hole->prev = khref;
		cp_mark_hblock_dirty(index->used_cpages,kh->links.next);
		}
	head->key_holes[size] = khref;
	head->kh_mask |= 1 << size;
	cp_mark_head_dirty(index->used_cpages);
	}

void _deindex_kh_hole(FSingSet *index,FKeyHeadGeneral *kh,element_type khref,unsigned size)
	{
	FKeyHeadLinks *w_khlinks;
	
	FAILURE_CHECK ((FKeyHeadGeneral *)PAGES_POINTER(index,khref) != kh,"khref does not match khlinks");
	FAILURE_CHECK (size <= 1,"size is too small");
	FAILURE_CHECK (size > KEYHEADS_IN_BLOCK,"size is too big");
	FAILURE_CHECK (kh->fields.space_used,"trying to deindex used keyhead");
	FORMATTED_LOG_HEADER("deindexing header block hole %u at %u[%u]\n",size,KH_BLOCK_IDX(khref),KH_BLOCK_NUM(khref));
	
	size--;
	if (index->head->key_holes[size] == khref)
		{ // Деиндексируем первую дырку в цепочке, prev может быть невалиден
		if ((index->head->key_holes[size] = kh->links.next) == KH_ZERO_REF)
			index->head->kh_mask &= ~(1 << size);
		cp_mark_head_dirty(index->used_cpages);
		return;
		}
	w_khlinks = (FKeyHeadLinks *)pagesPointer(index,kh->links.prev); 
	FAILURE_CHECK (((FKeyHead *)w_khlinks)->space_used,"used keyhead in chain");
	w_khlinks->next = kh->links.next;
	cp_mark_hblock_dirty(index->used_cpages,kh->links.prev);
	if (kh->links.next != KH_ZERO_REF)
		{ 
		w_khlinks = (FKeyHeadLinks *)pagesPointer(index,kh->links.next); 
		FAILURE_CHECK (((FKeyHead *)w_khlinks)->space_used,"used keyhead in chain");
		w_khlinks->prev = kh->links.prev;
		cp_mark_hblock_dirty(index->used_cpages,kh->links.next);
		}
	}

// Записывает данные в указанный заголовок, сокращая дырку за ним, если нужно
void kh_expand_chain_up(FSingSet *index,FKeyHeadGeneral *hblock,unsigned hb_idx,unsigned header_num,FKeyHead *data)
	{
	FAILURE_CHECK ((FKeyHeadGeneral *)PAGES_POINTER(index,hb_idx) != hblock,"khref does not match hblock");
	FAILURE_CHECK (!header_num,"bad header_num");
	FAILURE_CHECK (hblock[header_num].space.space_used,"used keyhead at hole start");

	if (!hblock[header_num].space.reserved)
		{ // Дырка индексирована, нужно деиндексировать
		FAILURE_CHECK (header_num == KH_BLOCK_LAST,"keyhead hole size 1 is not reserved");
		FAILURE_CHECK (hblock[header_num + 1].fields.space_used,"keyhead hole size 1 is not reserved");
		unsigned hsize = 2;
		switch (header_num)
			{
			case 1: if (hblock[3].fields.space_used) break; hsize++;
			case 2: if (hblock[4].fields.space_used) break; hsize++;
			case 3: if (hblock[5].fields.space_used) break; hsize++;
			case 4: if (hblock[6].fields.space_used) break; hsize++;
			case 5: if (!hblock[7].fields.space_used) hsize++;
			}
		FORMATTED_LOG_HEADER("shrinking hole %u up at %u[%u]\n",hsize,hb_idx,header_num);
		element_type khref = hb_idx + header_num * KEY_HEAD_SIZE;
		_deindex_kh_hole(index,&hblock[header_num],khref,hsize);
		_index_kh_hole(index,&hblock[header_num + 1],khref + KEY_HEAD_SIZE,hsize - 1); 
		}
	hblock[header_num].fields = *data;
	hblock[header_num-1].fields.chain_stop = 0; // Показываем новые данные (цепочка удлиняется внутри блока)
	cp_mark_hblock_dirty(index->used_cpages,hb_idx);
	}

// Записывает данные в указанный заголовок, сокращая дырку перед ним, если нужно
void kh_expand_chain_down(FSingSet *index,FKeyHeadGeneral *hblock,unsigned hb_idx,unsigned header_num,FKeyHead *data)
	{
	FAILURE_CHECK ((FKeyHeadGeneral *)PAGES_POINTER(index,hb_idx) != hblock,"khref does not match hblock");
	FAILURE_CHECK (header_num == KH_BLOCK_LAST,"bad header_num");
	FAILURE_CHECK (hblock[header_num].space.space_used,"used keyhead at hole end");

	if (!hblock[header_num].space.reserved)
		{	// Дырка индексирована, нужно деиндексировать
		FAILURE_CHECK (!header_num,"keyhead hole size 1 is not reserved");
		FAILURE_CHECK (hblock[header_num - 1].fields.space_used,"keyhead hole size 1 is not reserved");
		unsigned hsize = 2;
		switch (header_num)
			{
			case 6: if (hblock[4].fields.space_used) break; hsize++;
			case 5: if (hblock[3].fields.space_used) break; hsize++;
			case 4: if (hblock[2].fields.space_used) break; hsize++;
			case 3: if (hblock[1].fields.space_used) break; hsize++;
			case 2: if (!hblock[0].fields.space_used) hsize++;
			}
		unsigned hole_num = header_num + 1 - hsize;
		FORMATTED_LOG_HEADER("shrinking hole %u down at %u[%u]\n",hsize,hb_idx,hole_num);
		element_type khref = hb_idx + hole_num * KEY_HEAD_SIZE;
		_deindex_kh_hole(index,&hblock[hole_num],khref,hsize);
		_index_kh_hole(index,&hblock[hole_num],khref,hsize - 1); 
		}
	hblock[header_num].fields = *data;
	hblock[header_num].fields.chain_stop = 0; 
	cp_mark_hblock_dirty(index->used_cpages,hb_idx);
	}
	
FKeyHeadGeneral *_kh_shift_hole(FSingSet *index,unsigned size,element_type *kh_idx)
	{
	FKeyHeadGeneral *rv;
	FSetHead *head = index->head;
	
	size--;
	*kh_idx = head->key_holes[size];
	FAILURE_CHECK(!*kh_idx,"empty kh hole chain");
	rv = (FKeyHeadGeneral *)pagesPointer(index,*kh_idx);
	CHECK_PAGE_TYPE(index,*kh_idx,PT_HEADERS);
	FAILURE_CHECK(rv->space.space_used,"trying to deindex used keyhead");
	FORMATTED_LOG_HEADER("deindexing header block hole %u at %u[%u] from chain start\n",size + 1,KH_BLOCK_IDX(*kh_idx),KH_BLOCK_NUM(*kh_idx));
	// У первой дырки в цепочке prev невалиден, оптимизация
	if ((head->key_holes[size] = rv->links.next) == KH_ZERO_REF)
		head->kh_mask &= ~(1 << size);
	cp_mark_hblock_dirty(index->used_cpages,*kh_idx);
	return rv;
	}

element_type _kh_alloc_from_zone(FSingSet *index)
	{
	FSetHead *head = index->head;
	element_type rv;
	if ((rv = head->kh_alloc_zone) == KH_ZERO_REF)
		{
		unsigned kh_alloc_page = idx_alloc_page(index,PT_HEADERS);
		if (kh_alloc_page == NO_PAGE) return KH_ZERO_REF;
		rv = kh_alloc_page * PAGE_SIZE;
		}
	CHECK_PAGE_TYPE(index,rv,PT_HEADERS);
	head->kh_alloc_zone = rv + KH_BLOCK_SIZE;
	if (!(head->kh_alloc_zone % PAGE_SIZE))
		head->kh_alloc_zone = KH_ZERO_REF;
	cp_mark_hblock_dirty(index->used_cpages,rv);
	return rv;
	}

// Выделяем один заголовок
FKeyHeadGeneral *kh_alloc_one(FSingSet *index,element_type *ref)
	{
	unsigned hsize;
	FKeyHeadGeneral *kh_block;

	if (index->head->kh_mask & 4)
		{ hsize = 3; goto kh_alloc_one_hsize_found; }
	hsize = __builtin_ffs(index->head->kh_mask);
	if (hsize)
		{
kh_alloc_one_hsize_found:
		kh_block = _kh_shift_hole(index,hsize,ref);
		unsigned hole_num = KH_BLOCK_NUM(*ref);
		if (hole_num)
			{ // Если дырка не в начале блока, зарезервируем место между предыдущей и этой цепочкой
			kh_block->space.reserved = 1;
			kh_block++;
			*ref += KEY_HEAD_SIZE;
			if (--hsize == 1) return kh_block;
			}
		_index_kh_hole(index,&kh_block[1],*ref + KEY_HEAD_SIZE,hsize - 1);
		return kh_block;
		}
	if ((*ref = _kh_alloc_from_zone(index)) == KH_ZERO_REF) return NULL;
	// Индексируем остаток
	kh_block = (FKeyHeadGeneral *)pagesPointer(index,*ref);
	_index_kh_hole(index,&kh_block[1],*ref + KEY_HEAD_SIZE,KEYHEADS_IN_BLOCK - 1);
	return kh_block;
	}

// Выделяем блок заголовков меньше 8
FKeyHeadGeneral *kh_alloc_block(FSingSet *index,unsigned size,element_type *ref)
	{
	unsigned hsize;
	FKeyHeadGeneral *kh_block;

	FAILURE_CHECK(size <= 1,"size is too small");
	FAILURE_CHECK(size >= KEYHEADS_IN_BLOCK,"size is too big");

	hsize = __builtin_ffs(index->head->kh_mask >> (size - 1));
	if (hsize)
		{
		hsize += size - 1;
		kh_block = _kh_shift_hole(index,hsize,ref);
		if (hsize == size) return kh_block;

		unsigned hole_num = KH_BLOCK_NUM(*ref);
		if (hole_num)
			{ // Если дырка не в начале блока, зарезервируем место между предыдущей и этой цепочкой
			kh_block->space.reserved = 1;
			kh_block++;
			*ref += KEY_HEAD_SIZE;
			if (--hsize == size) return kh_block;
			}
		_index_kh_hole(index,&kh_block[size],*ref + size * KEY_HEAD_SIZE,hsize - size);
		return kh_block;
		}
	if ((*ref = _kh_alloc_from_zone(index)) == KH_ZERO_REF) return NULL;
	kh_block = (FKeyHeadGeneral *)pagesPointer(index,*ref);
	_index_kh_hole(index,&kh_block[size],*ref + size * KEY_HEAD_SIZE,KEYHEADS_IN_BLOCK - size);
	return kh_block;
	}
	
// Выделяем блок заголовков целиком
FKeyHeadGeneral *kh_alloc_full_block(FSingSet *index,element_type *ref)
	{
	if (index->head->kh_mask & (1 << KH_BLOCK_LAST))
		return _kh_shift_hole(index,KEYHEADS_IN_BLOCK,ref);
	if ((*ref = _kh_alloc_from_zone(index)) == KH_ZERO_REF) return NULL;
	return (FKeyHeadGeneral *)pagesPointer(index,*ref);		
	}

element_type _kh_merge_hole_down(FSingSet *index,FKeyHeadGeneral *kh,element_type kh_idx)
	{
	unsigned start = KH_BLOCK_NUM(kh_idx);
	if (!start || (--kh)->space.space_used)
		return 0;
	if (kh->space.reserved)
		return kh->space.reserved = 0,1;
	FAILURE_CHECK (start == 1,"keyhead hole size 1 is not reserved");
	unsigned hsize = 2;
	kh--;
	while (hsize < start && !kh[-1].space.space_used) hsize++, kh--;
	_deindex_kh_hole(index,kh,kh_idx - hsize * KEY_HEAD_SIZE,hsize);
	return hsize;
	}

element_type _kh_merge_hole_up(FSingSet *index,FKeyHeadGeneral *kh,element_type kh_idx)
	{
	unsigned end = KH_BLOCK_NUM(kh_idx);
	if (!end || kh->space.space_used)
		return 0;
	if (kh->space.reserved)
		return kh->space.reserved = 0,1;
	FAILURE_CHECK (end == KH_BLOCK_LAST,"keyhead hole size 1 is not reserved");
	unsigned hsize = 2;
	while (end + hsize < KEYHEADS_IN_BLOCK && !kh[hsize].space.space_used) hsize++;
	_deindex_kh_hole(index,kh,kh_idx,hsize);
	return hsize;
	}

void kh_free_block(FSingSet *index,FKeyHeadGeneral *kh,element_type kh_idx,unsigned size)
	{
	unsigned i;

	cp_mark_hblock_dirty(index->used_cpages,kh_idx);
	for (i = 0; i < size; i++)
		{
		FAILURE_CHECK (!kh[i].space.space_used,"trying free empty header");
		kh[i].space.space_used = 0;
		kh[i].space.reserved = 0;
		}
	unsigned hsize1 = _kh_merge_hole_down(index,kh,kh_idx);
	unsigned hsize2 = _kh_merge_hole_up(index,&kh[size],kh_idx + size * KEY_HEAD_SIZE);
	_index_kh_hole(index,&kh[-(int)hsize1],kh_idx - hsize1 * KEY_HEAD_SIZE,size + hsize1 + hsize2);
	}
	
// Оптимизация - не пытаемся расшириться в сторону начала блока
void kh_free_last_from_chain(FSingSet *index,FKeyHeadGeneral *kh,element_type kh_idx)
	{
	cp_mark_hblock_dirty(index->used_cpages,kh_idx);
	
	FAILURE_CHECK (!kh->space.space_used,"trying free empty header");
	kh->space.space_used = 0;
	kh->space.reserved = 0;
	unsigned hsize = _kh_merge_hole_up(index,&kh[1],kh_idx + KEY_HEAD_SIZE);
	_index_kh_hole(index,kh,kh_idx,hsize + 1);
	}

int check_head_holes_chains(FSingSet *index,FCheckData *check_data)
	{
	FSetHead *head = index->head;
	unsigned i,j;

	if (head->kh_alloc_zone != KH_ZERO_REF)
		{
		if (!(head->kh_alloc_zone % PAGE_SIZE))
			return idx_set_formatted_error(index,"Keyhead alloc zone %u point to page boundary",head->kh_alloc_zone),1;
		if (head->kh_alloc_zone != KH_BLOCK_IDX(head->kh_alloc_zone))
			return idx_set_formatted_error(index,"Keyhead alloc zone %u not point to block boundary",head->kh_alloc_zone),1;
		CHECK_PAGE_TYPE(index,index->head->kh_alloc_zone,PT_HEADERS);
		check_data->free_headers += PAGE_SIZE - index->head->kh_alloc_zone % PAGE_SIZE;
		}
	for (i = 0; i < KEYHEADS_IN_BLOCK; i++)
		{
		element_type total_free = 0;
		unsigned size = i + 1;
		if (!(head->kh_mask & (1 << i)))
			{
			if (head->key_holes[i] != KH_ZERO_REF)
				return idx_set_formatted_error(index,"Keyhead hole chain size %u is not empty but not masked",size),1;
			continue;
			}
		if (head->key_holes[i] == KH_ZERO_REF)
			return idx_set_formatted_error(index,"Keyhead hole chain size %u is empty but masked",size),1;
		element_type kh_ref = head->key_holes[i];
		element_type prev = KH_ZERO_REF;
		do {
			unsigned kh_num = KH_BLOCK_NUM(kh_ref);
			FKeyHeadGeneral *kh_block = (FKeyHeadGeneral *)pagesPointer(index,KH_BLOCK_IDX(kh_ref));
			if (prev != KH_ZERO_REF && prev != kh_block[kh_num].links.prev)
				return idx_set_formatted_error(index,"Hole chain size %u corrupted",size),1;
			if ((total_free += KEY_HEAD_SIZE * size) >= head->pcnt * PAGE_SIZE) 
				return idx_set_formatted_error(index,"Infinite loop in hole chain size %u",size),1;
			CHECK_PAGE_TYPE(index,kh_ref,PT_HEADERS);
			if (kh_ref & (KEY_HEAD_SIZE - 1))
				return idx_set_formatted_error(index,"Hole %u in keyhead hole chain size %u is not at header boundary",kh_ref,size),1;
			if (kh_num + size > KEYHEADS_IN_BLOCK)
				return idx_set_formatted_error(index,"Hole %u in keyhead hole chain size %u cross header block boundary",kh_ref,size),1;
			if (kh_num && !kh_block[kh_num - 1].space.space_used)
				return idx_set_formatted_error(index,"Empty space before hole %u in keyhead hole chain size %u",kh_ref,size),1;
			if (kh_num + size != KEYHEADS_IN_BLOCK && !kh_block[kh_num + size].space.space_used)
				return idx_set_formatted_error(index,"Empty space after hole %u in keyhead hole chain size %u",kh_ref,size),1;
			for (j = kh_num; j < kh_num + size; j++)
				{
				if (kh_block[kh_num].space.space_used)
					return idx_set_formatted_error(index,"Used header in hole %u in keyhead hole chain size %u",kh_ref,size),1;
				if (kh_block[kh_num].space.reserved)
					return idx_set_formatted_error(index,"Reserved header in hole %u in keyhead hole chain size %u",kh_ref,size),1;
				}
			check_data->free_headers += size * KEY_HEAD_SIZE;
			prev = kh_ref;
			kh_ref = kh_block[kh_num].links.next;
			} 
		while (kh_ref != KH_ZERO_REF);
		}
	return 0;
	}