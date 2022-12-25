/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <sys/types.h>
#include <unistd.h>
#include <stddef.h>
#include <stdio.h>
#include <memory.h>

#include "config.h"
#include "cpages.h"
#include "index.h"

void cp_init(FChangedPages *cpages,unsigned pt_size,unsigned counters_size)
	{
	cpages->pt_offset = INDEX_HEAD_DISK_PAGES * DISK_PAGE_SIZE;
	cpages->counters_offset = cpages->pt_offset + pt_size / ELEMENT_SIZE;
	cpages->hashtable_offset = cpages->counters_offset + counters_size / ELEMENT_SIZE;
	cp_reset(cpages);
	}

void cp_reset(FChangedPages *cpages)
	{
	cpages->dirty_first[PAGES_CHAIN] = cpages->dirty_first[INDEX_CHAIN] = NO_PAGE;
	}

void cp_mark_hashtable_loaded(FSingSet *index)
	{
	unsigned maxnum = index->real_cpages->hashtable_offset + index->hashtable_size * KH_BLOCK_SIZE / 2;
	unsigned pcnt = maxnum / PAGE_SIZE;
	if (maxnum % PAGE_SIZE)
		pcnt++;
	memset(index->real_cpages->head_loaded,0xFF,sizeof(uint64_t) * pcnt);
	}

void cp_mark_pages_loaded(FSingSet *index)
	{
	unsigned full_pages = index->head->pcnt / 64;
	unsigned rest = index->head->pcnt % 64;
	memset(index->real_cpages->page_loaded,0xFF,sizeof(uint64_t) * full_pages);
	if (rest)
		index->real_cpages->page_loaded[full_pages] = (1LL << rest) - 1;
	}

// Полный сброс данных на диск, при создании новой шары. Подготавливает структуру cpages к использованию
// Не используем блокировку страниц т.к. 1. шара заведомо в памяти - читатели не будут подгружать страницы с диска и 2. Мы имеем блокировку записи 
int cp_full_flush(FSingSet *index)
	{
	unsigned i;
	FChangedPages *cpages = index->real_cpages;

	if (!cpages) return 0;

	index->head->wip = 1;
	lseek(index->disk_index_fd, 0 , SEEK_SET);
	file_write(index->disk_index_fd,index->head,index->head->sizes.disk_file_size);

	lseek(index->disk_pages_fd, 0 , SEEK_SET);
	for (i = 0 ; i < index->head->pcnt; i++)
		file_write(index->disk_pages_fd,index->pages[i],PAGE_SIZE_BYTES);
	
	cp_reset(cpages);
	index->used_cpages = index->real_cpages;
	index->head->wip = 0;
	lseek(index->disk_index_fd, 0 , SEEK_SET);
	file_write(index->disk_index_fd,(char *)index->head, DISK_PAGE_BYTES);
	return index->head->pcnt * 64 + (int)(index->head->sizes.disk_file_size / DISK_PAGE_BYTES) + 1;
	}

int cp_flush(FSingSet *index)
	{
	unsigned i,workpg;
	uint64_t mask;
	off_t file_offset;
	unsigned pcnt;
	FChangedPages *cpages = index->used_cpages;

	if (!cpages) 
		return cp_full_flush(index);
	
	if (cpages->dirty_first[PAGES_CHAIN] == NO_PAGE && cpages->dirty_first[INDEX_CHAIN] == NO_PAGE) return 0;
	// Заголовочную часть первой страницы сохраним в случае любых изменений
	index->head->wip = 1; // Это попадет на диск
	int rv = INDEX_HEAD_DISK_PAGES + 1; // Первая страница будет записана дважды
	lseek(index->disk_index_fd, 0 , SEEK_SET);
	if (file_write(index->disk_index_fd,(char *)index->head,INDEX_HEAD_DISK_PAGES * DISK_PAGE_BYTES) != INDEX_HEAD_DISK_PAGES * DISK_PAGE_BYTES)
		return ERROR_SYNC_FAILED;
	cpages->dirty_masks[INDEX_CHAIN][0] &= ~INDEX_HEAD_MASK; // Чтобы не записывать дважды

	workpg = cpages->dirty_first[INDEX_CHAIN];
	while (workpg != NO_PAGE) 
		{
		for (mask = 1, i = 0; i < 64; mask <<= 1, i++)
			{
			if (cpages->dirty_masks[INDEX_CHAIN][workpg] & mask)
				{
				file_offset = (off_t)workpg * PAGE_SIZE_BYTES + i * DISK_PAGE_BYTES;
				lseek(index->disk_index_fd, file_offset, SEEK_SET);
				pcnt = 1;
				mask <<= 1;	i++;
				while (i < 64 && (cpages->dirty_masks[INDEX_CHAIN][workpg] & mask))
					{
					pcnt++;
					mask <<= 1; i++;
					}
				if (file_write(index->disk_index_fd,((char *)index->head) + file_offset,pcnt * DISK_PAGE_BYTES) != pcnt * DISK_PAGE_BYTES)
					{
					cpages->dirty_first[INDEX_CHAIN] = workpg;
					return ERROR_SYNC_FAILED;
					}
				rv += pcnt;
				}
			}
		cpages->dirty_masks[INDEX_CHAIN][workpg] = 0;
		workpg = cpages->dirty_chain[INDEX_CHAIN][workpg];
		}
	cpages->dirty_first[INDEX_CHAIN] = NO_PAGE;
		
	if ((workpg = cpages->dirty_first[PAGES_CHAIN]) != NO_PAGE)
		{
		if (ftruncate(index->disk_pages_fd,index->head->pcnt * PAGE_SIZE_BYTES))
			return ERROR_SYNC_FAILED;
		do
			{
			for (mask = 1, i = 0; i < 64; mask <<= 1, i++)
				{
				if (cpages->dirty_masks[PAGES_CHAIN][workpg] & mask)
					{
					unsigned stt = i;
					lseek(index->disk_pages_fd, (off_t)workpg * PAGE_SIZE_BYTES + i * DISK_PAGE_BYTES, SEEK_SET);
					pcnt = 1;
					mask <<= 1;	i++;
					while (i < 64 && (cpages->dirty_masks[PAGES_CHAIN][workpg] & mask))
						{
						pcnt++;
						mask <<= 1; i++;
						}
					if (file_write(index->disk_pages_fd,&index->pages[workpg][stt * DISK_PAGE_SIZE],pcnt * DISK_PAGE_BYTES) != pcnt * DISK_PAGE_BYTES)
						{
						cpages->dirty_first[PAGES_CHAIN] = workpg;
						return ERROR_SYNC_FAILED;
						}
					rv += pcnt;
					}
				}
			cpages->dirty_masks[PAGES_CHAIN][workpg] = 0;
			workpg = cpages->dirty_chain[PAGES_CHAIN][workpg];
			}
		while (workpg != NO_PAGE);
		cpages->dirty_first[PAGES_CHAIN] = NO_PAGE;
		}
	index->head->wip = 0;
	lseek(index->disk_index_fd, 0 , SEEK_SET);
	if (file_write(index->disk_index_fd,(char *)index->head, DISK_PAGE_BYTES) != DISK_PAGE_BYTES)
		{
		_mark_index_dirty(cpages,0,1LL);
		return ERROR_SYNC_FAILED;
		}
	return rv * (unsigned)DISK_PAGE_BYTES /1024;
	}

int cp_revert(FSingSet *index)
	{
	FChangedPages *cpages = index->used_cpages;
	unsigned workpg;
	uint64_t mask;
	off_t file_offset;

	if (!cpages) return 0;
	if (cpages->dirty_first[INDEX_CHAIN] == NO_PAGE && cpages->dirty_first[PAGES_CHAIN] == NO_PAGE) return 0;

	lseek(index->disk_index_fd, 0 , SEEK_SET);
	if (file_read(index->disk_index_fd,(char *)index->head,INDEX_HEAD_DISK_PAGES * DISK_PAGE_BYTES) != INDEX_HEAD_DISK_PAGES * DISK_PAGE_BYTES)
		goto cp_revert_error;
	cpages->dirty_masks[INDEX_CHAIN][0] &= ~INDEX_HEAD_MASK; 

	workpg = cpages->dirty_first[INDEX_CHAIN];
	while (workpg != NO_PAGE) 
		{
		mask = cpages->dirty_masks[INDEX_CHAIN][workpg];
		int pos = 0;
		while (mask)
			{
			unsigned stt = __builtin_ffsll(mask) - 1;
			mask >>= stt;
			unsigned len = __builtin_ffsll(~mask);
			if (!len) 
            len = 64 - pos;
         else
            len--;
			mask >>= len;
			stt += pos;
			file_offset = (off_t)workpg * PAGE_SIZE_BYTES + stt * DISK_PAGE_BYTES;
			lseek(index->disk_index_fd, file_offset, SEEK_SET);
			if (file_read(index->disk_index_fd,((char *)index->head) + file_offset,len * DISK_PAGE_BYTES) != len * DISK_PAGE_BYTES)
				goto cp_revert_error;
			pos += stt + len;
			}
		cpages->dirty_masks[INDEX_CHAIN][workpg] = 0;
		workpg = cpages->dirty_chain[INDEX_CHAIN][workpg];
		}
	cpages->dirty_first[INDEX_CHAIN] = NO_PAGE;
	
	if ((workpg = cpages->dirty_first[PAGES_CHAIN]) != NO_PAGE)
		{
		if (ftruncate(index->disk_pages_fd,index->head->pcnt * PAGE_SIZE_BYTES))
			goto cp_revert_error;
		do
			{
			if (workpg < index->head->pcnt)
				{
				mask = cpages->dirty_masks[PAGES_CHAIN][workpg];
				int pos = 0;
				while (mask)
					{
					unsigned stt = __builtin_ffsll(mask) - 1;
					mask >>= stt;
					unsigned len = __builtin_ffsll(~mask);
					if (!len) 
						len = 64 - pos;
					else
						len--;
					mask >>= len;
					stt += pos;
					lseek(index->disk_pages_fd, (off_t)workpg * PAGE_SIZE_BYTES + stt * DISK_PAGE_BYTES, SEEK_SET);
					if (file_read(index->disk_pages_fd,&index->pages[workpg][stt * DISK_PAGE_SIZE],len * DISK_PAGE_BYTES) != len * DISK_PAGE_BYTES)
						goto cp_revert_error;
					pos += stt + len;
					}
				}
			cpages->dirty_masks[PAGES_CHAIN][workpg] = 0;
			workpg = cpages->dirty_chain[PAGES_CHAIN][workpg];
			}
		while (workpg != NO_PAGE);
		cpages->dirty_first[PAGES_CHAIN] = NO_PAGE;
		}
	return 0;
cp_revert_error:
	index->head->bad_states.states.corrupted = 2; // Heavy failure, set is not usable anyway
	return ERROR_INTERNAL;
	}

static unsigned buffer_comparison(unsigned char *buf1,unsigned char *buf2,unsigned length)
	{
	unsigned i;
	for (i = 0 ; i < length; i++)
		if (buf1[i] != buf2[i])
			return i+1;
	return 0;
	}

static unsigned is_buffer_zerofilled(unsigned char *buf1,unsigned length)
	{
	unsigned i;
	for (i = 0 ; i < length; i++)
		if (buf1[i] != 0)
			return i+1;
	return 0;
	}
	
unsigned cp_dirty_mask_check(FSingSet *index)
	{
	unsigned load_space[INDEX_HEAD_DISK_PAGES * DISK_PAGE_SIZE];
	unsigned char *check_buffer = (unsigned char *)load_space;
	unsigned char *check_space = (unsigned char *)index->head;
	FChangedPages *cpages = index->used_cpages;
	unsigned i,pnum,bitnum,diff_byte;
	unsigned old_pcnt;
	unsigned head_dirty = 0;

	if (!cpages) return 0;
	if (cpages->dirty_first[INDEX_CHAIN] == NO_PAGE && cpages->dirty_first[PAGES_CHAIN] == NO_PAGE) return 0;

	lseek(index->disk_index_fd, 0 , SEEK_SET);
	file_read(index->disk_index_fd,load_space,INDEX_HEAD_DISK_PAGES * DISK_PAGE_BYTES);

	old_pcnt = load_space[offsetof(FSetHead,pcnt) / sizeof(unsigned)];

	if (!(memcmp(check_space,check_buffer,INDEX_HEAD_DISK_PAGES * DISK_PAGE_BYTES)))
		{
		if (cpages->dirty_masks[INDEX_CHAIN][0] & 1)
			return idx_set_error(index,"index head marked as changed, but not changed"),1;
		}
	else if (!(cpages->dirty_masks[INDEX_CHAIN][0] & 1))
		head_dirty = 1;

	check_space += INDEX_HEAD_DISK_PAGES * DISK_PAGE_BYTES;
	for (i = INDEX_HEAD_DISK_PAGES; i < index->head->sizes.disk_file_size / DISK_PAGE_BYTES; i++)
		{
		file_read(index->disk_index_fd,load_space,DISK_PAGE_BYTES);
		pnum = i / 64, bitnum = i % 64;
		if (cpages->dirty_masks[INDEX_CHAIN][pnum])
			head_dirty = 0;
		if (!(diff_byte = buffer_comparison(check_space,check_buffer,DISK_PAGE_BYTES)))
			{
			if (cpages->dirty_masks[INDEX_CHAIN][pnum] & (1LL << bitnum))
				return idx_set_formatted_error(index,"index disk page at %lu marked as changed, but not changed",i * DISK_PAGE_BYTES),1;
			}
		else if (!(cpages->dirty_masks[INDEX_CHAIN][pnum] & (1LL << bitnum)))
			return idx_set_formatted_error(index,"index at position %lu changed, but not marked as changed",i * DISK_PAGE_BYTES + diff_byte - 1),1;
		check_space += DISK_PAGE_BYTES;
		}
	
	lseek(index->disk_pages_fd, 0 , SEEK_SET);
	for (pnum = 0; pnum < old_pcnt; pnum++)
		{
		if (cpages->dirty_masks[PAGES_CHAIN][pnum])
			head_dirty = 0;
		for (i = 0; i < 64; i++)
			{
			file_read(index->disk_pages_fd,load_space,DISK_PAGE_BYTES);
			if (!(diff_byte = buffer_comparison((unsigned char *)&index->pages[pnum][i * DISK_PAGE_SIZE],check_buffer,DISK_PAGE_BYTES)))
				{
				if (cpages->dirty_masks[PAGES_CHAIN][pnum] & (1LL << i))
					return idx_set_formatted_error(index,"page %u at %lu marked as changed, but not changed",pnum,i * DISK_PAGE_BYTES),1;
				}
			else if (!(cpages->dirty_masks[PAGES_CHAIN][pnum] & (1LL << i)))
				return idx_set_formatted_error(index,"page %u at position %lu changed, but not marked as changed",pnum, i * DISK_PAGE_BYTES + diff_byte - 1),1;
			}
		}
	for (pnum = old_pcnt; pnum < index->head->pcnt; pnum++)
		{
		if (cpages->dirty_masks[PAGES_CHAIN][pnum])
			head_dirty = 0;
		for (i = 0; i < 64; i++)
			{
			if (!(diff_byte = is_buffer_zerofilled((unsigned char *)&index->pages[pnum][i * DISK_PAGE_SIZE],DISK_PAGE_BYTES)))
				{
				if (cpages->dirty_masks[PAGES_CHAIN][pnum] & (1LL << i))
					return idx_set_formatted_error(index,"added page %u at %lu marked as changed, but not changed",pnum,i * DISK_PAGE_BYTES),1;
				}
			else if (!(cpages->dirty_masks[PAGES_CHAIN][pnum] & (1LL << i)))
				return idx_set_formatted_error(index,"added page %u at position %lu changed, but not marked as changed",pnum, i * DISK_PAGE_BYTES + diff_byte - 1),1;
			}
		}
	if (head_dirty)
		return idx_set_formatted_error(index,"index head changed, but not marked as changed"),1;

	return 0;
	}
