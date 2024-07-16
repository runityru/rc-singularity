/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <stddef.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#include "allocator.h"
#include "index.h"
#include "utils.h"
#include "cpages.h"
#include "locks.h"

#ifdef MEMORY_CHECK
void check_page_type(FSingSet *index,element_type idx,EPageTypes page_type)
	{
	if (index->page_types)
		FAILURE_CHECK(index->page_types[idx/PAGE_SIZE] != (unsigned char)page_type,"bad page type");
	}
#endif

// При поиске нашли ссылку на страницу, которой нет в памяти
// 0 - ошибка mmap
// 1 - успешно
int _load_page(FSingSet *index,unsigned pnum)
	{
	if (pnum >= index->head->pcnt || index->pages_fd == -1)
		return 0;

	if ((index->pages[pnum] = (element_type *)mmap(NULL,PAGE_SIZE_BYTES,PROT_WRITE | PROT_READ, MAP_SHARED, index->pages_fd, pnum * PAGE_SIZE_BYTES)) == MAP_FAILED) 
		return 0;

	if (!index->real_cpages || cp_is_page_loaded(index->real_cpages,pnum))
		return 1;

	int rv = 0;
	int disk_pages_fd = index->disk_pages_fd;
	
	if (disk_pages_fd == -1 && (disk_pages_fd = open(index->filenames.pages_file,O_RDONLY)) == -1) 
		return 0;

	file_lock(disk_pages_fd,LOCK_EX); // Preventing concurent reading
	if (!cp_is_page_loaded(index->real_cpages,pnum))
		{
		lseek(disk_pages_fd, (off_t)pnum * PAGE_SIZE_BYTES, SEEK_SET);
		if (file_read(disk_pages_fd,index->pages[pnum],PAGE_SIZE_BYTES) == PAGE_SIZE_BYTES)
			cp_mark_page_loaded(index->real_cpages,pnum), rv = 1;
		}
	file_lock(disk_pages_fd,LOCK_UN);
	if (index->disk_pages_fd == -1)
		close(disk_pages_fd);
	return rv;
	}
	
// Быстрое получение ссылки в заведомо правильной и прилинкованной области
#ifdef MEMORY_CHECK
element_type *PAGES_POINTER(FSingSet *index,element_type idx)
	{
	unsigned pnum = idx >> PAGE_SHIFT;
	unsigned rst = idx & OFFSET_MASK;

	FAILURE_CHECK(pnum >= index->head->pcnt,"bad pages pointer");
	FAILURE_CHECK(!idx || idx == ZERO_REF,"zero pages pointer");
	FAILURE_CHECK(index->pages[pnum] == MAP_FAILED,"page was not mapped");
	return &index->pages[pnum][rst];
	}
#endif

// Получение ссылки по индексу с проверкой ее валидности для региона
element_type *regionPointer(FSingSet *index,element_type idx,unsigned size)
	{
	FAILURE_CHECK(!idx || idx == ZERO_REF,"zero pages pointer");
	unsigned pnum = idx >> PAGE_SHIFT;
	unsigned rst = idx & OFFSET_MASK;
	FAILURE_CHECK(pnum >= index->head->pcnt,"bad pages pointer");
	FAILURE_CHECK(size > PAGE_SIZE || rst + size > PAGE_SIZE,"crosspage region");

	if (index->pages[pnum] == MAP_FAILED)
		_load_page(index,pnum);
	FAILURE_CHECK(index->pages[pnum] == MAP_FAILED,"page not found");
	return &index->pages[pnum][rst];
	}

// Получение ссылки по индексу с проверкой ее валидности для региона (возврат NULL в случае ошибки)
element_type *regionPointerNoError(FSingSet *index,element_type idx,unsigned size)
	{
	FAILURE_CHECK(!idx || idx == ZERO_REF,"zero pages pointer");
	unsigned pnum = idx >> PAGE_SHIFT;
	unsigned rst = idx & OFFSET_MASK;
	
	// Это может быть и штатная ситуация
	if (size > PAGE_SIZE || rst + size > PAGE_SIZE)	return NULL;
	return (index->pages[pnum] != MAP_FAILED || _load_page(index,pnum)) ? &index->pages[pnum][rst] : NULL;
	}
	
// Получение ссылки по индексу с подгрузкой страницы
element_type *pagesPointer(FSingSet *index,element_type idx)
	{
	FAILURE_CHECK(!idx || idx == ZERO_REF,"zero pages pointer");
	unsigned pnum = idx >> PAGE_SHIFT;
	FAILURE_CHECK(pnum >= index->head->pcnt,"bad pages pointer");
	if (index->pages[pnum] == MAP_FAILED)
		_load_page(index,pnum);
	FAILURE_CHECK(index->pages[pnum] == MAP_FAILED,"page not found");
	return &index->pages[pnum][idx & OFFSET_MASK];
	}

// Получение ссылки по индексу с подгрузкой страницы и проверкой ее валидности
element_type *pagesPointerNoError(FSingSet *index,element_type idx)
	{
	FAILURE_CHECK(!idx || idx == ZERO_REF,"zero pages pointer");
	unsigned pnum = idx >> PAGE_SHIFT;
	return (index->pages[pnum] != MAP_FAILED || _load_page(index,pnum)) ? &index->pages[pnum][idx & OFFSET_MASK] : NULL;
	}

static inline element_type *pagePointer(FSingSet *index,unsigned pnum)
	{
	if (index->pages[pnum] == MAP_FAILED) 
		_load_page(index,pnum);
	FAILURE_CHECK(index->pages[pnum] == MAP_FAILED,"page not found");
	return index->pages[pnum];
	}

// Выделение страницы. Возвращает номер прогруженной страницы
unsigned idx_alloc_page(FSingSet *index,EPageTypes page_type)
	{
	unsigned pnum;

	if ((pnum = index->head->first_empty_page) != NO_PAGE)
		{
		element_type *np = pagePointer(index,pnum);
		index->head->first_empty_page = *np;
		}
	else
		{
		pnum = index->head->pcnt;
		if (pnum == MAX_PAGES) return NO_PAGE;
		if (index->pages_fd == -1)
			index->pages[pnum] = (unsigned *)mmap(NULL,PAGE_SIZE_BYTES,PROT_WRITE | PROT_READ, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
		else
			{
			element_type csize = pnum * PAGE_SIZE_BYTES;
			if (ftruncate(index->pages_fd,csize + PAGE_SIZE_BYTES))
				return NO_PAGE;
			index->pages[pnum] = (unsigned *)mmap(NULL,PAGE_SIZE_BYTES,PROT_WRITE | PROT_READ, MAP_SHARED, index->pages_fd, csize);
			}

		if (index->pages[pnum] == MAP_FAILED)
			return NO_PAGE;
	
		index->head->pcnt++;
		if (index->real_cpages)
			cp_mark_page_loaded(index->real_cpages,pnum);
		}
	cp_mark_head_dirty(index->used_cpages);
	FORMATTED_LOG_MEMORY("page %u allocated\n",pnum);
	if (index->page_types)
		{
		index->page_types[pnum] = page_type;
		cp_mark_pagetype_dirty(index->used_cpages,pnum);
		}
	return pnum;	
	}

void idx_free_page(FSingSet *index,unsigned pnum)
	{
	cp_mark_page_element_dirty(index->used_cpages,pnum << PAGE_SHIFT);
	index->pages[pnum][0] = index->head->first_empty_page;
	index->head->first_empty_page = pnum;
	FORMATTED_LOG_MEMORY("page %u deallocated\n",pnum);
	if (index->page_types)
		{
		cp_mark_pagetype_dirty(index->used_cpages,pnum);
		index->page_types[pnum] = PT_EMPTY;
		}
	}

// Добавляет к *free_size размер пустых страниц
int check_free_pages(FSingSet *index,FCheckData *check_data)
	{
	unsigned pnum = index->head->first_empty_page;
	unsigned pcnt = 0;

	while (pnum != NO_PAGE)
		{
		if (pnum >= MAX_PAGES)
			return idx_set_formatted_error(index,"Bad page number in free page chain"),1;	
		check_data->empty += PAGE_SIZE;
		element_type *np = pagePointer(index,pnum);
		pnum = *np;
		if (++pcnt > MAX_PAGES)
			return idx_set_formatted_error(index,"Infinite loop in free page chain"),1;			
		}
	return 0;
	}

////////////////////////////////////////////////////////////////////////////////////////////////
///////                    Выделение и освобождение памяти
////////////////////////////////////////////////////////////////////////////////////////////////

// Маленькие дырки

// Цепочки подстраниц с дырками

// Смещение подстраницы внутри страницы
static inline unsigned _sub_page_offset(element_type subpage_idx)
	{
	unsigned spnum = (subpage_idx & OFFSET_MASK) / DISK_PAGE_SIZE;
	return spnum ? (spnum * DISK_PAGE_SIZE) : SPEC_PAGE_HEAD_SIZE;
	}

// Смещение подстраницы внутри страницы
static inline unsigned _sub_page_ref(element_type subpage_idx)
	{
	unsigned spnum = (subpage_idx & OFFSET_MASK) / DISK_PAGE_SIZE;
	return (subpage_idx & ~OFFSET_MASK) + (spnum ? (spnum * DISK_PAGE_SIZE) : SPEC_PAGE_HEAD_SIZE);
	}
	
// Выделение места на специальных подстраницах
element_type *idx_small_alloc(FSingSet *index,unsigned size,element_type *ref_pointer)
	{
	element_type ref,subpage_idx;
	FSetHead *head = index->head;
	unsigned hs_idx = size - 1;
	unsigned pf_spec_page,sp_start;
	
	element_type *page,*small_hole;
	FSubpageHead *subp_head;
	FSpecPageHead *page_head;	
	
	if ((subpage_idx = head->holes[hs_idx]) != ZERO_REF)
		{ // Если есть дырка нужного размера
		page = pagePointer(index,subpage_idx >> PAGE_SHIFT);
		subp_head = (FSubpageHead *)&page[_sub_page_offset(subpage_idx)];
		ref = subp_head->first_hole;
		FORMATTED_LOG_MEMORY("small hole %u allocated at %u from subpage at %u\n",size,ref,subpage_idx);
		small_hole = &page[ref & OFFSET_MASK];
		subp_head->first_hole = *small_hole;
		if (++subp_head->used_count == subp_head->total_count) 
			{ // Подстраница занята целиком, выкидываем из цепочки подстраниц с дырками
			FORMATTED_LOG_MEMORY("subpage at %u has no more holes\n",subpage_idx);
			index->head->holes[hs_idx] = subp_head->next_sub_page;
			}
		cp_mark_page_dirty(index->used_cpages,subpage_idx,SUB_PAGE_HEAD_SIZE);
		cp_mark_page_dirty(index->used_cpages,ref,size);
		CHECK_PAGE_TYPE(index,ref,PT_SPECIAL);
		*ref_pointer = ref;
		return (element_type *)small_hole;
		}
	if ((ref = head->alloc_zones[hs_idx]) != ZERO_REF)
		{ // Если есть частично выделенная подстраница
		pf_spec_page = ref >> PAGE_SHIFT;
		sp_start = _sub_page_offset(ref);
		
		page = pagePointer(index,pf_spec_page);
		subp_head = (FSubpageHead *)&page[sp_start];
		subp_head->used_count++;
		subp_head->total_count++;
		
		FORMATTED_LOG_MEMORY("small hole %u allocated from alloc_zone at %u\n",size,ref);

		if (ref % DISK_PAGE_SIZE + size * 2 > DISK_PAGE_SIZE)
			{
			head->alloc_zones[hs_idx] = ZERO_REF; // На подстранице нет места для двух дырок (этой и еще одной), удаляем ссылку на нее как зону выделения места
			FORMATTED_LOG_MEMORY("alloc_zone for sizes %u is full\n",size);
			}
		else
			head->alloc_zones[hs_idx] += size; // Иначе сдвигаем адрес зоны выделения на размер выделения

		cp_mark_page_dirty(index->used_cpages,pf_spec_page * PAGE_SIZE + sp_start,SUB_PAGE_HEAD_SIZE);
		cp_mark_page_dirty(index->used_cpages,ref,size);
		CHECK_PAGE_TYPE(index,ref,PT_SPECIAL);
		*ref_pointer = ref;
		return pagesPointer(index,ref);
		}

	// Нет дырок и частично выделенной подстраницы
	if ((pf_spec_page = head->first_pf_spec_page) == NO_PAGE)
		{ // Нет спецстраницы с пустыми подстраницами, выделяем и размечаем
		pf_spec_page = idx_alloc_page(index,PT_SPECIAL);
		if (pf_spec_page == NO_PAGE)
			return NULL;
		head->first_pf_spec_page = pf_spec_page;
		page = index->pages[pf_spec_page];
		page_head = (FSpecPageHead *)page;
		page_head->use_mask = 1; // Маска занятых подстраниц
		page_head->next_pf_spec_page = NO_PAGE;
		sp_start = SPEC_PAGE_HEAD_SIZE;
		subp_head = (FSubpageHead *)&page[SPEC_PAGE_HEAD_SIZE];
		subpage_idx = pf_spec_page * PAGE_SIZE + SPEC_PAGE_HEAD_SIZE;
		// Помечаем одновременно модификацию заголовков страницы, подстраницы и выделенного места
		cp_mark_page_dirty(index->used_cpages,pf_spec_page * PAGE_SIZE,SPEC_PAGE_HEAD_SIZE + SUB_PAGE_HEAD_SIZE + size); 
		}
	else
		{
		page = pagePointer(index,pf_spec_page);
		page_head = (FSpecPageHead *)page;
		unsigned spnum = __builtin_ffsll(~page_head->use_mask) - 1;
		sp_start = spnum ? (spnum * DISK_PAGE_SIZE) : SPEC_PAGE_HEAD_SIZE;
		cp_mark_page_dirty(index->used_cpages,pf_spec_page << PAGE_SHIFT,SPEC_PAGE_HEAD_SIZE);
		if ((page_head->use_mask |= (1LL << spnum)) == 0xFFFFFFFFFFFFFFFFLL)
			{ // Страница занята целиком, выкидываем из цепочки спецстраниц с дырками (она там первая)
			index->head->first_pf_spec_page = page_head->next_pf_spec_page;
			FORMATTED_LOG_MEMORY("special page %u has no unallocated subpages\n",pf_spec_page);
			}

		subp_head = (FSubpageHead *)&page[sp_start];
		subpage_idx = pf_spec_page * PAGE_SIZE + sp_start;
		cp_mark_page_dirty(index->used_cpages,subpage_idx,SUB_PAGE_HEAD_SIZE + size); // Помечаем одновременно модификацию заголовка и выделенного места
		}
	FORMATTED_LOG_MEMORY("subpage for sizes %u allocated at %u\n",size,subpage_idx);
	subp_head->total_count = subp_head->used_count = 1;
	subp_head->first_hole = ZERO_REF;
	subp_head->next_sub_page = ZERO_REF;
	subp_head->chunk_size = size;
	ref = subpage_idx + SUB_PAGE_HEAD_SIZE;
	head->alloc_zones[hs_idx] = ref + size; // Последующие элементы того-же размера будут выделяться на той же подстранице
	CHECK_PAGE_TYPE(index,ref,PT_SPECIAL);
	FORMATTED_LOG_MEMORY("small hole %u allocated from begin of alloc_zone at %u\n",size,ref);
	*ref_pointer = ref;
	return &index->pages[pf_spec_page][sp_start + SUB_PAGE_HEAD_SIZE]; // Страница заведомо прогружена
	}

void idx_small_free(FSingSet *index,element_type data_idx,unsigned size)
	{
	FSetHead *head = index->head;
	element_type *page,*small_hole;

	CHECK_PAGE_TYPE(index,data_idx,PT_SPECIAL);
	
	unsigned hs_idx = size - 1;
	unsigned pnum = data_idx >> PAGE_SHIFT;
	page = pagePointer(index,pnum);
	FSpecPageHead *page_head = (FSpecPageHead *)page;
	
	unsigned spnum = (data_idx & OFFSET_MASK) / DISK_PAGE_SIZE;
	FAILURE_CHECK(!(page_head->use_mask & (1LL << spnum)),"free element from empty subpage");
	unsigned spstart = spnum ? (spnum * DISK_PAGE_SIZE) : SPEC_PAGE_HEAD_SIZE;
	FSubpageHead *subp_head = (FSubpageHead *)&page[spstart];
	
	FORMATTED_LOG_MEMORY("small hole %u deallocated at %u\n",size,data_idx);

	if (subp_head->used_count == 1)
		{ // Освобождаем подстраницу
		unsigned sp_idx = pnum * PAGE_SIZE + spstart;
		if (subp_head->total_count != 1) // Убираем из цепочки подстраниц с дырками
			{
			FSubpageHead *w_subp_head;
			if (sp_idx == index->head->holes[hs_idx])
				index->head->holes[hs_idx] = subp_head->next_sub_page;
			else
				{
				w_subp_head = (FSubpageHead *)pagesPointer(index,subp_head->prev_sub_page);
				w_subp_head->next_sub_page = subp_head->next_sub_page;
				cp_mark_page_dirty(index->used_cpages,subp_head->prev_sub_page,SUB_PAGE_HEAD_SIZE);
				if (subp_head->next_sub_page != ZERO_REF)
					{
					w_subp_head = (FSubpageHead *)pagesPointer(index,subp_head->next_sub_page);
					w_subp_head->prev_sub_page = subp_head->prev_sub_page;
					cp_mark_page_dirty(index->used_cpages,subp_head->next_sub_page,SUB_PAGE_HEAD_SIZE);
					}
				}
			}
		if (index->head->alloc_zones[hs_idx] / DISK_PAGE_SIZE == sp_idx / DISK_PAGE_SIZE )
			{ // Убираем ссылку на alloc_zone
			index->head->alloc_zones[hs_idx] = ZERO_REF;
			FORMATTED_LOG_MEMORY("alloc_zone for sizes %u is empty\n",size);
			}
		FORMATTED_LOG_MEMORY("subpage for sizes %u deallocated at at %u\n",size,sp_idx);
		
		if (page_head->use_mask == 0xFFFFFFFFFFFFFFFFLL)
			{
			FORMATTED_LOG_MEMORY("special page %u has free subpages\n",pnum);
			if (head->first_pf_spec_page != NO_PAGE)
				{
				FSpecPageHead *next_page_head = (FSpecPageHead *)pagePointer(index,head->first_pf_spec_page);
				next_page_head->prev_pf_spec_page = pnum;
				cp_mark_page_dirty(index->used_cpages,head->first_pf_spec_page << PAGE_SHIFT,SPEC_PAGE_HEAD_SIZE); 
				}
			page_head->next_pf_spec_page = head->first_pf_spec_page;
			head->first_pf_spec_page = pnum;
			}
		page_head->use_mask &= ~(1LL << spnum);
		if (!page_head->use_mask)
			{
			FSpecPageHead *w_page_head;
	
			FORMATTED_LOG_MEMORY("special page %u has no used subpages\n",pnum);

			if (pnum == index->head->first_pf_spec_page)
				index->head->first_pf_spec_page = page_head->next_pf_spec_page;
			else
				{
				w_page_head = (FSpecPageHead *)pagePointer(index,page_head->prev_pf_spec_page);
				w_page_head->next_pf_spec_page = page_head->next_pf_spec_page;
				cp_mark_page_dirty(index->used_cpages,page_head->prev_pf_spec_page << PAGE_SHIFT,SPEC_PAGE_HEAD_SIZE);

				if (page_head->next_pf_spec_page != NO_PAGE)
					{
					w_page_head = (FSpecPageHead *)pagePointer(index,page_head->next_pf_spec_page);
					w_page_head->prev_pf_spec_page = page_head->prev_pf_spec_page;
					cp_mark_page_dirty(index->used_cpages,page_head->next_pf_spec_page << PAGE_SHIFT,SPEC_PAGE_HEAD_SIZE);
					}	
				}
			idx_free_page(index,pnum);
			return;
			}
		cp_mark_page_dirty(index->used_cpages,pnum << PAGE_SHIFT,SPEC_PAGE_HEAD_SIZE); 
		return;
		}
		
	if (data_idx + size == head->alloc_zones[hs_idx])
		{
		FORMATTED_LOG_MEMORY("small hole %u returned to alloc_zone at %u\n",size,data_idx);
		subp_head->used_count--;
		subp_head->total_count--;
		cp_mark_page_dirty(index->used_cpages,(pnum << PAGE_SHIFT) + spstart,SUB_PAGE_HEAD_SIZE); 
		head->alloc_zones[hs_idx] -= size;
		return;
		}
		
	element_type sub_page = pnum * PAGE_SIZE + spstart;
	if (subp_head->used_count == subp_head->total_count)
		{ // Добавляем в цепочку подстраниц с дырками
		FORMATTED_LOG_MEMORY("subpage at %u has holes\n",pnum * PAGE_SIZE + spstart);
		FSubpageHead *w_subp_head;
		if (head->holes[hs_idx] != ZERO_REF)
			{
			w_subp_head = (FSubpageHead *)pagesPointer(index,head->holes[hs_idx]);
			w_subp_head->prev_sub_page = sub_page;
			cp_mark_page_dirty(index->used_cpages,head->holes[hs_idx],SUB_PAGE_HEAD_SIZE); 
			}
		subp_head->next_sub_page = head->holes[hs_idx];
		head->holes[hs_idx] = sub_page;
		}
		
	subp_head->used_count--;
	small_hole = &page[data_idx & OFFSET_MASK];
	*small_hole = subp_head->first_hole;
	subp_head->first_hole = data_idx;
	
	cp_mark_page_dirty(index->used_cpages,data_idx,1); 
	cp_mark_page_dirty(index->used_cpages,sub_page,SUB_PAGE_HEAD_SIZE); 
	}

// Добавляет к busy_size размер заголовков страницы и подстраниц, а к *free_size размер свободных подстраниц и невыделенное место на занятых
void spec_page_support_count(FSingSet *index,unsigned pnum,FCheckData *check_data)
	{
	if (check_data->checked_subpages[pnum / 64] & (1LL << pnum % 64)) return;
	check_data->checked_subpages[pnum / 64] |= 1LL << pnum % 64;
	check_data->busy_small += SPEC_PAGE_HEAD_SIZE + SUB_PAGE_HEAD_SIZE * 64;
	FSubpageHead *subp_head;
	FSpecPageHead *page_head = (FSpecPageHead *)pagePointer(index,pnum);
	unsigned total_cap = DISK_PAGE_SIZE - SPEC_PAGE_HEAD_SIZE - SUB_PAGE_HEAD_SIZE;
	if (!(page_head->use_mask & 1))
		check_data->free_small += total_cap;
	else
		{
		subp_head = (FSubpageHead *)pagesPointer(index,(pnum << PAGE_SHIFT) + SPEC_PAGE_HEAD_SIZE);
		check_data->free_small += total_cap - subp_head->total_count * subp_head->chunk_size;
		}
	unsigned i;
	total_cap = DISK_PAGE_SIZE - SUB_PAGE_HEAD_SIZE;
	for (i = 1 ;i < 64; i++)
		if (!(page_head->use_mask & (1LL << i)))
			check_data->free_small += total_cap;
		else
			{
			subp_head = (FSubpageHead *)pagesPointer(index,(pnum << PAGE_SHIFT) + i * DISK_PAGE_SIZE);
			check_data->free_small += total_cap - subp_head->total_count * subp_head->chunk_size;
			}
	}

// Добавляет к *free_size размер неразмеченных участков и суммарный размер маленьких дырок в цепочках
int check_small_holes_chains(FSingSet *index,FCheckData *check_data)
	{
	FSetHead *head = index->head;
	unsigned i,pcnt = 0,pnum;
// Проверяем цепочку страниц с пустыми подстраницами
	unsigned pf_spec_page = head->first_pf_spec_page,prev_pf_spec_page = NO_PAGE;
	while (pf_spec_page != NO_PAGE)
		{
		if (index->page_types && index->page_types[pf_spec_page] != PT_SPECIAL)
			return idx_set_formatted_error(index,"Bad page %u type, is %u, must be %u",pf_spec_page,index->page_types[pf_spec_page],PT_SPECIAL),1;
		spec_page_support_count(index,pf_spec_page,check_data);
		FSpecPageHead *page_head = (FSpecPageHead *)pagePointer(index,pf_spec_page);
		if (prev_pf_spec_page != NO_PAGE && page_head->prev_pf_spec_page != prev_pf_spec_page)
			return idx_set_formatted_error(index,"Partially free page chain corrupted at page %u",pf_spec_page),1;
		if (!page_head->use_mask)
			return idx_set_formatted_error(index,"Empty page %u in partially free page chain",pf_spec_page),1;
		if (page_head->use_mask == 0xFFFFFFFFFFFFFFFFLL)
			return idx_set_formatted_error(index,"Full page %u in partially free page chain",pf_spec_page),1;
		if (++pcnt > index->head->pcnt)
			return idx_set_formatted_error(index,"Infinite loop in partyally free page chain"),1;
		prev_pf_spec_page = pf_spec_page;
		pf_spec_page = page_head->next_pf_spec_page;
		}
	for (i = 1; i < MIN_HOLE_SIZE; i++)
		{
		// Проверяем цепочку подстраниц с дырками определенного размера
		element_type subpage_idx = head->holes[i - 1], sp_cnt = 0;
		element_type hole_idx,prev_sp_idx = ZERO_REF;
		unsigned max_count = PAGE_SIZE / i;

		while (subpage_idx != ZERO_REF)
			{
			unsigned hcnt = 0;
			pnum = subpage_idx >> PAGE_SHIFT;
			if (index->page_types && index->page_types[pnum] != PT_SPECIAL)
				return idx_set_formatted_error(index,"Bad page %u type, is %u, must be %u",pnum,index->page_types[pnum],PT_SPECIAL),1;
			spec_page_support_count(index,pnum,check_data);
			FSubpageHead *subp_head = (FSubpageHead *)pagesPointer(index,_sub_page_ref(subpage_idx));
			if (prev_sp_idx != ZERO_REF && prev_sp_idx != subp_head->prev_sub_page)
				return idx_set_formatted_error(index,"Holed subpages chain size %u corrupted at %u",i,subpage_idx),1;
			if (subp_head->used_count == subp_head->total_count)
				return idx_set_formatted_error(index,"Full subpage %u in hole chain size %u",subpage_idx,i),1;
			if ((hole_idx = subp_head->first_hole) == ZERO_REF)
				return idx_set_formatted_error(index,"Zero first hole subpage %u in hole chain size %u",subpage_idx,i),1;
			do
				{
				if ((hole_idx & SUB_PAGE_MASK) != (subpage_idx & SUB_PAGE_MASK))
					return idx_set_formatted_error(index,"Hole %u is not in subpage %u in hole chain size %u",hole_idx,subpage_idx,i),1;
				hole_idx = *pagesPointer(index,hole_idx);
				if (++hcnt >= max_count)
					return idx_set_formatted_error(index,"Infinite loop in hole chain size %u at subpage %u",i,subpage_idx),1;
				check_data->free_small += i;
				}
			while (hole_idx != ZERO_REF);
			if (subp_head->total_count - subp_head->used_count != hcnt)
				return idx_set_formatted_error(index,"Count of holes does not match for subpage %u in hole chain size %u",subpage_idx,i),1;
			prev_sp_idx = subpage_idx;
			subpage_idx = subp_head->next_sub_page;
			if (++sp_cnt > index->head->pcnt * 64)
				return idx_set_formatted_error(index,"Infinite loop in holed subpages chain size %u",i),1;
			}

		// Проверяем неразмеченные участки
		element_type ref = head->alloc_zones[i - 1];
		if (ref == ZERO_REF) continue;
		pnum = ref >> PAGE_SHIFT;
		if (index->page_types && index->page_types[pnum] != PT_SPECIAL)
			return idx_set_formatted_error(index,"Bad page %u type, is %u, must be %u",pnum,index->page_types[pnum],PT_SPECIAL),1;
		spec_page_support_count(index,pnum,check_data);
		FSubpageHead *subp_head = (FSubpageHead *)pagesPointer(index,_sub_page_ref(ref));

		if (!subp_head->total_count)
			return idx_set_formatted_error(index,"Empty subpage used as alloc zone for size %u",i),1;
		if (subp_head->total_count == max_count)
			return idx_set_formatted_error(index,"Full subpage used as alloc zone for size %u",i),1;
		}
	return 0;
	}


// Большие дырки

// Не отмечает грязность заголовка, должна отметиться извне, если нужна
void _deindex_hole_first(FSingSet *index,element_type hole_idx,unsigned hs_idx,FHoleHeader *hole)
	{
	FAILURE_CHECK((FHoleHeader *)PAGES_POINTER(index,hole_idx) != hole,"bad hole_idx in _deindex_hole");

	FORMATTED_LOG_MEMORY("deindexing memory hole %u at %u\n",hole->size,hole_idx);
	if ((index->head->holes[hs_idx] = hole->next) == ZERO_REF)
		{
		if (hs_idx >= 64)
			index->head->holemask_high &= ~(1LL << (hs_idx - 64));
		else
			index->head->holemask_low &= ~(1LL << hs_idx);
		}
	}

// Удаляем дырку из цепочек (размер >= MIN_HOLE_SIZE). Не отмечает грязности заголовка
void _deindex_hole(FSingSet *index,element_type hole_idx,unsigned hs_idx,FHoleHeader *hole)
	{
	FHoleHeader *w_hole;
	FAILURE_CHECK((FHoleHeader *)PAGES_POINTER(index,hole_idx) != hole,"bad hole_idx in _deindex_hole");

	FORMATTED_LOG_MEMORY("deindexing memory hole %u at %u\n",hole->size,hole_idx);
	if (index->head->holes[hs_idx] == hole_idx)
		{ _deindex_hole_first(index,hole_idx,hs_idx,hole); return; }
	w_hole = (FHoleHeader *)pagesPointer(index,hole->prev); 
	w_hole->next = hole->next; 
	cp_mark_page_dirty(index->used_cpages,hole->prev,MIN_HOLE_SIZE); 
	if (hole->next == ZERO_REF) return;
	w_hole = (FHoleHeader *)pagesPointer(index,hole->next); 
	w_hole->prev = hole->prev; 
	cp_mark_page_dirty(index->used_cpages,hole->next,HOLE_HEADER_SIZE); 
	}

// Добавляем дырку в цепочки (размер >= MIN_HOLE_SIZE) без проверок на прилегание к невыделенной области. Не отмечает грязности заголовка
void _index_hole(FSingSet *index,element_type data_idx,unsigned size)
	{
	unsigned pnum = data_idx >> PAGE_SHIFT;
	unsigned rest = data_idx & OFFSET_MASK;
	element_type *page;
	FSetHead *head = index->head;

	FORMATTED_LOG_MEMORY("indexing memory hole %u at %u\n",size,data_idx);
	page = pagePointer(index,pnum);
	FHoleHeader *hole = (FHoleHeader *)&page[rest];
	FHoleFooter *hole_footer = (FHoleFooter *)&page[rest + size - HOLE_FOOTER_SIZE];
	unsigned hs_idx = HOLESIZE_IDX(size);
	if (head->holes[hs_idx] != ZERO_REF)
		{
		FHoleHeader *w_hole = (FHoleHeader *)pagesPointer(index,head->holes[hs_idx]);
		w_hole->prev = data_idx;
		cp_mark_page_dirty(index->used_cpages,head->holes[hs_idx],HOLE_HEADER_SIZE); 
		}
	else if (hs_idx >= 64)
		head->holemask_high |= 1LL << (hs_idx - 64);
	else
		head->holemask_low |= 1LL << hs_idx;
	hole->nonempty_signature = 0;	
	hole->next = head->holes[hs_idx];
	hole->size = hole_footer->size = size;
	hole_footer->nonempty_signature = 0;	
	head->holes[hs_idx] = data_idx;

	cp_mark_page_dirty(index->used_cpages,data_idx,HOLE_HEADER_SIZE); 
	cp_mark_page_dirty(index->used_cpages,data_idx + size - HOLE_FOOTER_SIZE,HOLE_FOOTER_SIZE); 
	}
	
element_type *idx_large_alloc(FSingSet *index,unsigned size,element_type *ref_pointer)
	{
	element_type ref;
	FSetHead *head = index->head;
	
	unsigned hs_idx;
	if (size <= INDEXED_HOLESIZE_CNT && (ref = head->holes[hs_idx = size - 1]) != ZERO_REF)
		{ // Получили дырку точно нужного размера
		FHoleHeader *hole = (FHoleHeader *)pagesPointer(index,ref);
		FAILURE_CHECK(hole->nonempty_signature,"Non empty space in hole chain in idx_general_alloc");
		_deindex_hole_first(index,ref,hs_idx,hole);
		CHECK_PAGE_TYPE(index,ref,PT_GENERAL);
		cp_mark_page_dirty(index->used_cpages,ref,size);
		*ref_pointer = ref;
		return (element_type *)hole;
		}

	unsigned hsize = size + MIN_HOLE_SIZE; // Adding min hole size for splitting hole
	hs_idx = HOLESIZE_IDX(hsize); 
	if (hsize > INDEXED_HOLESIZE_CNT)
		hs_idx++;
	
	unsigned bitnum = 0;
	if (hs_idx >= 64)
		{
		if ((bitnum = __builtin_ffsll(head->holemask_high >> (hs_idx - 64))))
			{
			hs_idx += bitnum - 1;
			goto idx_general_alloc_hole_found;
			}
		}
	else
		{
		if ((bitnum = __builtin_ffsll(head->holemask_low >> hs_idx)))
			{
			hs_idx += bitnum - 1;
			goto idx_general_alloc_hole_found;
			}
		if ((bitnum = __builtin_ffsll(head->holemask_high)))
			{
			hs_idx = bitnum + 63;
idx_general_alloc_hole_found: // Нечитаемо, но минимум переходов
			ref = head->holes[hs_idx];
			FAILURE_CHECK(ref == ZERO_REF,"Holes bitmask corrupted in idx_general_alloc");
			FHoleHeader *hole = (FHoleHeader *)pagesPointer(index,ref);
			FAILURE_CHECK(hole->nonempty_signature,"Non empty space in larger hole chain in idx_general_alloc");
			unsigned hole_size = hole->size;
			_deindex_hole(index,ref,hs_idx,hole);
			_index_hole(index,ref + size,hole_size - size);
			CHECK_PAGE_TYPE(index,ref,PT_GENERAL);
			cp_mark_page_dirty(index->used_cpages,ref,size); 
			*ref_pointer = ref;
			return (element_type *)hole;
			}
		}

	// Дырки нет, берем из неразмеченной области
	hsize = (PAGE_SIZE - (head->unlocated & OFFSET_MASK)) & OFFSET_MASK; // Остаток места на странице, граница страниц - это заполненная страница а не пустая
	if (hsize >= size)
		{ // have an empty space
		ref = head->unlocated;
		FORMATTED_LOG_MEMORY("memory %u allocated at %u\n",size,ref);
		head->unlocated += size;
		CHECK_PAGE_TYPE(index,ref,PT_GENERAL);
		cp_mark_page_dirty(index->used_cpages,ref,size);
		*ref_pointer = ref;
		return pagesPointer(index,ref);
		}

	if (hsize >= MIN_HOLE_SIZE) // Остаток пустого места на странице отметим как дырку, если для нее хватит места
		_index_hole(index,head->unlocated,hsize);

	unsigned pnum = idx_alloc_page(index,PT_GENERAL);
	if (pnum == NO_PAGE)
		return NULL;
	ref = pnum * PAGE_SIZE;
	head->unlocated = ref + size;
	cp_mark_page_dirty(index->used_cpages,ref,size);
	*ref_pointer = ref;
	return index->pages[pnum]; // Страница заведомо в памяти
	}
	
void idx_large_free(FSingSet *index,element_type data_idx,unsigned size)
	{
	FHoleHeader *hole;
	FHoleFooter *hole_footer;
	// Ищем дырки вверх и вниз, деиндексируем и сливаем
	unsigned pnum = data_idx / PAGE_SIZE;
	unsigned rest = data_idx % PAGE_SIZE;
	unsigned after_data = rest + size;
	unsigned page_rest;

	FAILURE_CHECK(after_data > PAGE_SIZE,"region cross page boundary in idx_general_free");
	
	element_type *page = pagePointer(index,pnum);
	
	if (rest >= MIN_HOLE_SIZE && !(hole_footer = (FHoleFooter *)&page[rest - HOLE_FOOTER_SIZE])->nonempty_signature)
		{ // Ниже по странице дырка
		hole = (FHoleHeader *)&page[rest - hole_footer->size];
		data_idx -= hole_footer->size;
		size += hole_footer->size;
		_deindex_hole(index,data_idx,HOLESIZE_IDX(hole_footer->size),hole);
		}

	if (data_idx + size == index->head->unlocated) // Добавляем к неразмеченной области
		{ 
		FSetHead *head = index->head;
		head->unlocated -= size;
		FORMATTED_LOG_MEMORY("memory %u returned to unlocated at %u\n",size,head->unlocated);
		if (!(head->unlocated & OFFSET_MASK))
			{
			head->unlocated = 0;
			idx_free_page(index,pnum);
			}
		return; 
		}

	page_rest = PAGE_SIZE - after_data;
	if (page_rest >= MIN_HOLE_SIZE)
		{ // Дальше по странице может быть дырка, проверим
		FHoleHeader *n_hole = (FHoleHeader *)&page[after_data];
		if (!n_hole->nonempty_signature)
			{
			_deindex_hole(index,data_idx + size,HOLESIZE_IDX(n_hole->size),n_hole);
			size += n_hole->size;
			}
		}
	else // Добавляем остаток страницы к дырке, там ничего быть не может
		size += page_rest;

	if (size == PAGE_SIZE)
		{ idx_free_page(index,pnum); return; }
	// Теперь размечаем и индексируем новую дырку
	_index_hole(index,data_idx,size);
	}

// Добавляет к *free_size размер неразмеченного участка и суммарный размер больших дырок в цепочках
int check_general_holes_chains(FSingSet *index,FCheckData *check_data)
	{
	unsigned i;
	FSetHead *head = index->head;
	FHoleHeader *hheader;
	FHoleFooter *hfooter;
	check_data->busy_general ++; // Занятый элемент по индексу 0
	for (i = MIN_HOLE_SIZE; i <= HOLESIZE_CNT; i++)
		{
		unsigned hs_idx = i - 1;
		element_type total_free = 0;
		element_type hole_ref = head->holes[hs_idx],prev_ref = ZERO_REF;
		if (hole_ref == ZERO_REF)
			{
			if (hs_idx < 64)
				{
				if (index->head->holemask_low & (1LL << hs_idx))
					return idx_set_formatted_error(index,"Hole chain size %u is empty but masked",i),1;
				}
			else if (index->head->holemask_high & (1LL << (hs_idx - 64)))
				return idx_set_formatted_error(index,"Hole chain size %u is empty but masked",i),1;
			continue;
			}
		if (hs_idx < 64)
			{
			if (!(index->head->holemask_low & (1LL << hs_idx)))
				return idx_set_formatted_error(index,"Hole chain size %u is not empty but not masked",i),1;
			}
		else if (!(index->head->holemask_high & (1LL << (hs_idx - 64))))
			return idx_set_formatted_error(index,"Hole chain size %u is not empty but not masked",i),1;
		do
			{
			hheader = (FHoleHeader *)regionPointer(index,hole_ref,HOLE_HEADER_SIZE);
			element_type hfooter_ref = hole_ref + hheader->size - HOLE_FOOTER_SIZE;
			if (hfooter_ref >> PAGE_SHIFT != hole_ref >> PAGE_SHIFT)
				return idx_set_formatted_error(index,"Hole at %u and footer at %u is not on one page in hole chain size %u",hole_ref,hfooter_ref,i),1;
			hfooter = (FHoleFooter *)regionPointer(index,hfooter_ref,HOLE_FOOTER_SIZE);
			if (hheader->size != hfooter->size)
				return idx_set_formatted_error(index,"Hole at %u and footer at %u has not same size in hole chain size %u",hole_ref,hfooter_ref,i),1;
			if (prev_ref != ZERO_REF && prev_ref != hheader->prev)
				return idx_set_formatted_error(index,"Hole chain size %u corrupted",i),1;
			unsigned h_idx = HOLESIZE_IDX(hheader->size);
			if (hs_idx != h_idx)
				return idx_set_formatted_error(index,"Hole with size index %u in chain has size %u",hs_idx,hheader->size),1;
			if ((total_free += hheader->size) >= head->pcnt * PAGE_SIZE)
				return idx_set_formatted_error(index,"Infinite loop in chain size %u",i),1;
			if (hheader->nonempty_signature)
				return idx_set_formatted_error(index,"Nonempty signature present at hole %u in header",hole_ref),1;
			if (hfooter->nonempty_signature)
				return idx_set_formatted_error(index,"Nonempty signature present at hole %u in footer",hole_ref),1;
			prev_ref = hole_ref;
			hole_ref = hheader->next; 
			}
		while (hole_ref != ZERO_REF);
		check_data->free_general += total_free;
		}
	check_data->free_general += (PAGE_SIZE - (head->unlocated & OFFSET_MASK)) & OFFSET_MASK;
	return 0;
	}