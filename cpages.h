/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#ifndef _CHANGED_PAGES_H
#define _CHANGED_PAGES_H

#include <stdint.h>

#include "defines.h"
#include "utils.h"
#include "keyhead.h"
#include "index.h"

#define PAGES_CHAIN 0
#define INDEX_CHAIN 1

typedef struct FChangedPagesTg
	{ 
	unsigned counters_offset;
	unsigned pt_offset;
	unsigned hashtable_offset;
	unsigned padding1;

	uint64_t head_loaded[MAX_PAGES];
	uint64_t page_loaded[PAGES_MASK_SIZE];
	unsigned dirty_first[2];
	unsigned dirty_chain[2][MAX_PAGES];
	uint64_t dirty_masks[2][MAX_PAGES];
	} FChangedPages;

typedef struct FSingSetTg FSingSet;
	
void cp_init(FChangedPages *cpages,unsigned pt_size,unsigned counters_size);
void cp_reset(FChangedPages *cpages);

#define INDEX_HEAD_MASK ((1LL << INDEX_HEAD_DISK_PAGES) - 1LL)

static inline int cp_is_hash_entry_loaded(FChangedPages *cpages,unsigned hash)
	{ 
	if (!cpages) return 1;
	unsigned num = cpages->hashtable_offset + hash * KH_BLOCK_SIZE / 2;
	return (cpages->head_loaded[num / PAGE_SIZE] & (1LL << ((num % PAGE_SIZE) / DISK_PAGE_SIZE))) ? 1 : 0;
	}

static inline void cp_mark_hash_entry_loaded(FChangedPages *cpages,unsigned hash)
	{ 
	if (!cpages) return;
	unsigned num = cpages->hashtable_offset + hash * KH_BLOCK_SIZE / 2;
	cpages->head_loaded[num / PAGE_SIZE] |= (1LL << ((num % PAGE_SIZE) / DISK_PAGE_SIZE));
	}

static inline int cp_is_page_loaded(FChangedPages *cpages,unsigned pnum)
	{
	return (cpages->page_loaded[pnum / 64] & (1LL << pnum % 64)) ? 1 : 0;
	}

static inline void cp_mark_page_loaded(FChangedPages *cpages,unsigned pnum)
	{
	FAILURE_CHECK(pnum >= MAX_PAGES,"page number is too big");
	cpages->page_loaded[pnum / 64] |= (1LL << pnum % 64);
	}

static inline void cp_mark_dirty(FChangedPages *cpages,unsigned num)
	{
	if (!cpages) return;
	unsigned pnum = num / 64, bitnum = num % 64;
	unsigned cnum = pnum / MAX_PAGES;
	pnum %= MAX_PAGES;
	if (!cpages->dirty_masks[cnum][pnum])
		cpages->dirty_chain[cnum][pnum] = cpages->dirty_first[cnum], cpages->dirty_first[cnum] = pnum;
	cpages->dirty_masks[cnum][pnum] |= 1LL << bitnum;
	}

static inline void _mark_pages_dirty(FChangedPages *cpages,unsigned pnum,uint64_t mask)
	{
	FAILURE_CHECK(pnum >= MAX_PAGES,"page number is too big");
	if (!cpages->dirty_masks[PAGES_CHAIN][pnum])
		cpages->dirty_chain[PAGES_CHAIN][pnum] = cpages->dirty_first[PAGES_CHAIN], cpages->dirty_first[PAGES_CHAIN] = pnum;
	cpages->dirty_masks[PAGES_CHAIN][pnum] |= mask;
	}

static inline void cp_mark_page_dirty(FChangedPages *cpages,element_type num,unsigned size)
	{
	if (!cpages) return;
	unsigned rest = num % PAGE_SIZE;
	unsigned rest2 = (num + size - 1) % PAGE_SIZE;
	FAILURE_CHECK(rest2 < rest,"crosspage operation");
	rest /= DISK_PAGE_SIZE, rest2 /= DISK_PAGE_SIZE;
	_mark_pages_dirty(cpages,num / PAGE_SIZE,(1LL << rest2) + ((1LL << rest2) - 1) - ((1LL << rest) - 1));
	}
	
static inline void cp_mark_page_element_dirty(FChangedPages *cpages,element_type num)
	{ 
	if (!cpages) return;
	_mark_pages_dirty(cpages,num / PAGE_SIZE,1LL << ((num % PAGE_SIZE) / DISK_PAGE_SIZE));
	}

static inline void cp_mark_hblock_dirty(FChangedPages *cpages,element_type num)
	{ // Блок заголовков всегда попадает в одну страницу
	if (!cpages) return;
	_mark_pages_dirty(cpages,num / PAGE_SIZE,1LL << ((num % PAGE_SIZE) / DISK_PAGE_SIZE));
	}

static inline unsigned cp_get_hblock_num(FChangedPages *cpages,element_type num)
	{ return num / DISK_PAGE_SIZE; }
	
static inline void _mark_index_dirty(FChangedPages *cpages,unsigned pnum,uint64_t mask)
	{
	FAILURE_CHECK(pnum >= MAX_PAGES,"page number is too big");
	if (!cpages->dirty_masks[INDEX_CHAIN][pnum])
		cpages->dirty_chain[INDEX_CHAIN][pnum] = cpages->dirty_first[INDEX_CHAIN], cpages->dirty_first[INDEX_CHAIN] = pnum;
	cpages->dirty_masks[INDEX_CHAIN][pnum] |= mask;
	}

static inline void cp_mark_head_dirty(FChangedPages *cpages)
	{ 
	if (!cpages) return;
	_mark_index_dirty(cpages,0,1LL);
	}

static inline void cp_mark_hash_entry_dirty(FChangedPages *cpages,unsigned hash)
	{ 
	if (!cpages) return;
	unsigned num = cpages->hashtable_offset + hash * KH_BLOCK_SIZE / 2;
	_mark_index_dirty(cpages,num / PAGE_SIZE,1LL << ((num % PAGE_SIZE) / DISK_PAGE_SIZE));
	}

static inline unsigned cp_get_hash_entry_num(FChangedPages *cpages,unsigned hash)
	{
	if (!cpages) return 0;
	return MAX_PAGES * 64 + (cpages->hashtable_offset + hash * KH_BLOCK_SIZE / 2) / DISK_PAGE_SIZE; 
	}

static inline void cp_mark_counter_dirty(FChangedPages *cpages,unsigned num)
	{
	if (!cpages) return;
	num += cpages->counters_offset;
	_mark_index_dirty(cpages,num / PAGE_SIZE,1LL << ((num % PAGE_SIZE) / DISK_PAGE_SIZE));
	}

static inline void cp_mark_pagetype_dirty(FChangedPages *cpages,unsigned num)
	{
	if (!cpages) return;
	num = cpages->pt_offset + num / ELEMENT_SIZE; // Page type has 1 byte size
	_mark_index_dirty(cpages,num / PAGE_SIZE,1LL << ((num % PAGE_SIZE) / DISK_PAGE_SIZE));
	}
	
void cp_mark_hashtable_loaded(FSingSet *index);
void cp_mark_pages_loaded(FSingSet *index);

int cp_full_flush(FSingSet *index);
int cp_flush(FSingSet *index);
int cp_revert(FSingSet *index);

unsigned cp_dirty_mask_check(FSingSet *index);
	
#endif