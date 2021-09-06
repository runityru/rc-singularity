/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <stddef.h>
#include <memory.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <stdarg.h>
#include <sched.h>

#include "config.h"
#include "index.h"
#include "cpages.h"
#include "locks.h"
#include "utils.h"
#include "keyhead.h"
#include "allocator.h"

#define O_NEWFILE (O_RDWR | O_TRUNC | O_CREAT)

const char *FILE_SIGNATURE __attribute__ ((aligned (4))) = "TC01"; // Codec name and major version

#define HS_COUNT 25

const unsigned HASH_TABLE_SIZES[HS_COUNT] = {547,1117,2221,4447,8269,16651,33391,65713,131071,524287,1336337,2014997,2494993,4477457,6328548,8503057,29986577,40960001,
				65610001,126247697,193877777,303595777,384160001,406586897,562448657};

// Выделяет и инициализирует структуру
FSingSet *_alloc_index(void)
	{
	int i;
	FSingSet *rv = NULL;

	if (!(rv = (FSingSet *)malloc(sizeof(FSingSet)))) return NULL;

	rv->head = MAP_FAILED;
	rv->index_fd = rv->pages_fd = -1;
	rv->disk_index_fd = rv->disk_pages_fd = -1;
	rv->used_cpages = rv->real_cpages = NULL;
	rv->disk_index_fname = rv->disk_pages_fname = rv->mem_index_fname = rv->mem_pages_fname = NULL;
	rv->locks_count = rv->manual_locked = 0;
	rv->last_error[0] = 0;

	for (i = 0; i < MAX_PAGES; i++)
		{ rv->pages[i] = MAP_FAILED; }
	
	return rv;
	}

int _set_sharenames(FSingSet *index,const char *setname)
	{
	int inamesize = strlen(setname) + 5;

	if (!(index->mem_index_fname = (char *)malloc(inamesize * 2)))
		return 1;
	index->mem_pages_fname = &index->mem_index_fname[inamesize];
		
	sprintf(index->mem_index_fname,"%s.idx",setname);
	sprintf(index->mem_pages_fname,"%s.dat",setname);
	return 0;
	}

int _set_filenames(FSingSet *index,FSingConfig *config,const char *setname)
	{
	int fnbufsize = (config->base_location?strlen(config->base_location) : 0) + strlen(setname) + 7;
	char *bl = config->base_location ? config->base_location : "./";

	if (!(index->disk_index_fname = (char *)malloc(fnbufsize * 2)))
		return 1;
	index->disk_pages_fname = &index->disk_index_fname[fnbufsize];
		
	sprintf(index->disk_index_fname,"%s%s.idx",bl,setname);
	sprintf(index->disk_pages_fname,"%s%s.dat",bl,setname);
	return 0;
	}

void idx_set_error(FSingSet *index,const char *message)
	{
	strncpy(index->last_error,message,511);
	}

void idx_set_formatted_error(FSingSet *index,const char *format,...)
	{
	va_list args;
	va_start(args, format);
	vsnprintf(index->last_error,511,format, args);
	va_end(args);
	}

// Create empty index
// Return created index with index_fd flocked for subsequent filling
FSingSet *idx_create_set(const char *setname,unsigned keys_count,unsigned flags,FSingConfig *config)
	{
	FSingSet *rv = NULL;
	FSetHead *head;
	char *index_share;
	
	FHeadSizes head_sizes;
	element_type base_head_size,hash_table_size,counters_size,page_types_size,extra_mem_size,locks_size;
	int ifd = -1,pfd = -1;
	int map_flags = MAP_ANONYMOUS | MAP_PRIVATE;

	if (!(rv = _alloc_index()))
		{ cnf_set_error(config,"not enougth memory for set initialization"); return NULL; }

	if (!setname)
		{
		flags |= UF_NOT_PERSISTENT;
		rv->is_private = 1;
		}
	else 
		{
		rv->is_private = 0;
		if (_set_sharenames(rv,setname))
			{ cnf_set_error(config,"not enougth memory for set initialization"); goto idx_empty_index_fail; }
		}
	
	if (flags & CF_READER)
		flags |= CF_FULL_LOAD;
	rv->conn_flags = (config->connect_flags | flags) & CF_MASK;

	if (keys_count < 512)
		keys_count = 512;
	base_head_size = INDEX_HEAD_DISK_PAGES * DISK_PAGE_BYTES;
	hash_table_size = align_up((keys_count + 1) / 2 * sizeof(FKeyHeadGeneral) * KEYHEADS_IN_BLOCK,DISK_PAGE_BYTES);
	counters_size =  (flags & UF_COUNTERS) ? align_up(COUNTERS_SIZE(keys_count) * sizeof(unsigned),DISK_PAGE_BYTES) : 0;

#ifdef MEMORY_CHECK
	page_types_size = PAGE_TYPES_DISK_PAGES * DISK_PAGE_BYTES;
#else
	page_types_size = 0;
#endif

	head_sizes.disk_file_size = base_head_size + hash_table_size + page_types_size + counters_size ;
	
	locks_size = sizeof(FLockSet) + sizeof(uint64_t) * (keys_count / 128 + ((keys_count % 128) ? 1 : 0));
	extra_mem_size = locks_size + ((flags & UF_NOT_PERSISTENT) ? 0 : sizeof(FChangedPages));
	extra_mem_size = align_up(extra_mem_size,DISK_PAGE_BYTES);

	head_sizes.mem_file_size = head_sizes.disk_file_size + extra_mem_size;
	
	if (!(flags & UF_NOT_PERSISTENT))
		{
		if (_set_filenames(rv,config,setname))
			{ cnf_set_error(config,"not enougth memory for set initialization"); goto idx_empty_index_fail; }
		if ((rv->disk_index_fd = open(rv->disk_index_fname,O_NEWFILE, FILE_PERMISSIONS)) == -1)
			{ cnf_set_formatted_error(config,"can not create file %s",rv->disk_index_fname); goto idx_empty_index_fail; }
		if ((rv->disk_pages_fd = open(rv->disk_pages_fname,O_NEWFILE, FILE_PERMISSIONS)) == -1)
			{ cnf_set_formatted_error(config,"can not create file %s",rv->disk_pages_fname); goto idx_empty_index_fail; }
		}
	
	if (!rv->is_private)
		{
		if ((ifd = rv->index_fd = shm_open(rv->mem_index_fname,O_NEWFILE, FILE_PERMISSIONS)) == -1)
			{ cnf_set_formatted_error(config,"can not create share %s",rv->mem_index_fname); goto idx_empty_index_fail; }
		
		if (file_lock(ifd,LOCK_EX))
			{ cnf_set_error(config,"flock failed"); goto idx_empty_index_fail; }

		if ((pfd = rv->pages_fd = shm_open(rv->mem_pages_fname,O_NEWFILE, FILE_PERMISSIONS)) == -1)
			{ cnf_set_formatted_error(config,"can not create share %s",rv->mem_pages_fname); goto idx_empty_index_fail; }

		if (ftruncate(ifd,head_sizes.mem_file_size))
			{ cnf_set_error(config,"can not truncate hash table"); goto idx_empty_index_fail; }
		if (ftruncate(pfd,PAGE_SIZE_BYTES))
			{ cnf_set_error(config,"can not truncate collision table"); goto idx_empty_index_fail; }
		map_flags = MAP_SHARED;
		}

	if ((index_share = (char *)mmap(NULL,head_sizes.mem_file_size,PROT_WRITE | PROT_READ, map_flags, ifd, 0)) == MAP_FAILED)
		{ cnf_set_error(config,"can not map hash table"); goto idx_empty_index_fail; }

	rv->head = head = (FSetHead *)index_share;
	
	head->sizes = head_sizes;	
	head->signature = *((unsigned *)FILE_SIGNATURE);
	head->hashtable_size = rv->hashtable_size = keys_count;
	head->count = 0;
	rv->read_only = 0;
	head->delimiter = config->column_delimiter;
	head->use_flags = flags & ~CF_MASK;
	head->state_flags = 0;

	index_share += base_head_size;
	
#ifdef MEMORY_CHECK
	head->use_flags |= UF_PAGE_TYPES;
	rv->page_types = (unsigned char *)index_share;
	index_share += page_types_size;
#else
	rv->page_types = NULL;
#endif

	if (counters_size)
		{
		rv->counters = (unsigned *)index_share;
		index_share += counters_size;
		}
	else
		rv->counters = NULL;

	rv->hash_table = (FKeyHeadGeneral *)index_share;
	index_share += hash_table_size;

	// Инитим ссылки в локальной структуре на разные структуры в шаре
	rv->lock_set = (FLockSet *)index_share;
	lck_init_locks(rv);
	index_share += locks_size;
	rv->used_cpages = NULL;

	if (!(flags & UF_NOT_PERSISTENT))
		{
		rv->real_cpages = (FChangedPages *)index_share;
		rv->real_cpages->pt_offset = base_head_size / ELEMENT_SIZE;
		rv->real_cpages->counters_offset = rv->real_cpages->pt_offset + page_types_size / ELEMENT_SIZE;
		rv->real_cpages->hashtable_offset = rv->real_cpages->counters_offset + counters_size / ELEMENT_SIZE;
		cp_mark_hashtable_loaded(rv);
		cp_mark_page_loaded(rv->real_cpages,0);
		}
	else
		rv->real_cpages = NULL;

	head->first_pf_spec_page = NO_PAGE;
	head->first_empty_page = NO_PAGE;

	if ((rv->pages[0] = (unsigned *)mmap(NULL,PAGE_SIZE_BYTES,PROT_WRITE | PROT_READ, map_flags, pfd, 0)) == MAP_FAILED)
		{ cnf_set_error(config,"can not map collision table"); goto idx_empty_index_fail; }

#ifdef MEMORY_CHECK
	rv->page_types[0] = PT_GENERAL;
#endif
	
	head->unlocated = 1; // Ссылка на 0 невалидна, добавим туда занятый элемент
	rv->pages[0][0] = 0xFFFFFFFF; // Чтобы не открыло ссылку на 0, сделаем там занятый элемент.
	head->pcnt = 1;
	return rv;
	
idx_empty_index_fail:
	if (rv) 
		sing_unlink_set(rv);

	return NULL;
	}

void idx_creation_done(FSingSet *index,unsigned lock_mode)
	{
	switch (lock_mode)
		{
		case LM_SIMPLE:
			index->head->use_mutex = 1;
			break;
		case LM_PROTECTED:
			index->head->check_mutex = 1;
		case LM_FAST:
			index->head->use_spin = 1;
			break;
		case LM_NONE:
			break;
		case LM_READ_ONLY:
			index->read_only = index->head->read_only = 1;
			break;
		}
	if (index->index_fd != -1) 
		{
		cp_full_flush(index);
		file_lock(index->index_fd,LOCK_UN);
		close(index->index_fd);
		index->index_fd = -1;
		if (index->conn_flags & CF_READER)
			{
			if (index->disk_index_fd != -1) 
				close(index->disk_index_fd), index->disk_index_fd = -1;
			if (index->disk_pages_fd != -1) 
				close(index->disk_pages_fd), index->disk_pages_fd = -1;
			}
		}
	if (index->conn_flags & CF_READER)
		index->read_only = 1;
	}
	
FSingSet *idx_link_set(const char *setname,unsigned flags,FSingConfig *config)
	{
	FSingSet *rv = NULL;
	FSetHead *head;
	FHeadSizes head_sizes;
	element_type base_head_size,counters_size,page_types_size,locks_size;
	char *index_share,*extra_struct;
	unsigned i;
	int share_mode = O_RDWR;
	int file_locked = 0;
	int ifd = -1;
	base_head_size = INDEX_HEAD_DISK_PAGES * DISK_PAGE_BYTES;
	
	if (!setname)
		return cnf_set_error(config,"can not link to unnamed set"), NULL;

	if (!(rv = _alloc_index()))
		return cnf_set_error(config,"not enougth memory for index struct"), NULL;

	if (_set_sharenames(rv,setname))
		{ cnf_set_error(config,"not enougth memory for set initialization"); goto sing_link_set_fail; } 

	if (flags & CF_READER)
		flags |= CF_FULL_LOAD;
	rv->conn_flags = (config->connect_flags | flags) & CF_MASK;
	rv->is_private = 0;

	if ((ifd = shm_open(rv->mem_index_fname,O_RDWR | O_CREAT, FILE_PERMISSIONS)) == -1)
		{ cnf_set_formatted_error(config,"can not open set %s",rv->mem_index_fname); goto sing_link_set_fail; }
	
	// Блочим шару (возможно мы ее только что создали, надо проверить)
	if (file_lock(ifd,LOCK_EX))
		{ cnf_set_error(config,"flock failed"); goto sing_link_set_fail; }
	
	if (file_read(ifd,&head_sizes,sizeof(FHeadSizes)) != sizeof(FHeadSizes))
		{ // Шары не было, пересоздадим из дисковой версии, если такая есть
		size_t to_read;
		share_mode = O_NEWFILE;
		file_locked = 1;

		if (_set_filenames(rv,config,setname))
			{ cnf_set_error(config,"not enougth memory for set initialization"); goto sing_link_set_fail; }
		
		if ((rv->disk_index_fd = open(rv->disk_index_fname,O_RDWR)) == -1)
			{ cnf_set_formatted_error(config,"can not open file %s",rv->disk_index_fname); goto sing_link_set_fail; }

		if (file_read(rv->disk_index_fd,&head_sizes,sizeof(FHeadSizes)) != sizeof(FHeadSizes)) // Читаем полный размер из дискового файла
			{ cnf_set_error(config,"can not read full head set size"); goto sing_link_set_fail; }

		if (ftruncate(ifd,head_sizes.mem_file_size)) // Расширяем в памяти
			{ cnf_set_error(config,"can not truncate hash table"); goto sing_link_set_fail; }

		if ((index_share = (char *)mmap(NULL,head_sizes.mem_file_size,PROT_WRITE | PROT_READ, MAP_SHARED, ifd, 0)) == MAP_FAILED)
			{ cnf_set_error(config,"can not map hash table"); goto sing_link_set_fail; }

		rv->head = head = (FSetHead *)index_share;
		head->sizes = head_sizes;
		to_read = base_head_size - sizeof(FHeadSizes);
		// Дочитываем заголовок
		if (file_read(rv->disk_index_fd,&index_share[sizeof(FHeadSizes)],to_read) != to_read)
			{ cnf_set_error(config,"can not read set head from disk"); goto sing_link_set_fail; }
		if (head->signature != *((unsigned *)FILE_SIGNATURE))
			{ cnf_set_error(config,"incompatible set format"); goto sing_link_set_fail; }
		if (head->use_flags & UF_NOT_PERSISTENT)
			{ cnf_set_error(config,"set with disk copy has NOT_PERSISTENT flag"); goto sing_link_set_fail; }
		if (head->wip)
			{ cnf_set_error(config,"disk copy of set is broken"); goto sing_link_set_fail; }
		}
	else
		{
		file_lock(ifd,LOCK_UN);

		if ((index_share = (char *)mmap(NULL,head_sizes.mem_file_size,PROT_WRITE | PROT_READ, MAP_SHARED, ifd, 0)) == MAP_FAILED)
			{ cnf_set_error(config,"can not map hash table"); goto sing_link_set_fail; }
		rv->head = head = (FSetHead *)index_share;
		if (head->signature != *((unsigned *)FILE_SIGNATURE))
			{ cnf_set_error(config,"incompatible share format"); goto sing_link_set_fail; }
		if (!(head->use_flags & UF_NOT_PERSISTENT))
			{
			if (_set_filenames(rv,config,setname))
				{ cnf_set_error(config,"not enougth memory for set initialization"); goto sing_link_set_fail; }
			if ((rv->disk_index_fd = open(rv->disk_index_fname,O_RDWR)) == -1)
				{ cnf_set_formatted_error(config,"can not open file %s",rv->disk_index_fname); goto sing_link_set_fail; }
			}
		}
	rv->read_only = head->read_only;
	rv->hashtable_size = head->hashtable_size;
	extra_struct = index_share + head_sizes.disk_file_size;
	locks_size = sizeof(FLockSet) + sizeof(uint64_t) * (rv->hashtable_size / 128 + ((rv->hashtable_size % 128) ? 1 : 0));
	index_share += base_head_size;

	if (head->use_flags & UF_PAGE_TYPES)
		{
		page_types_size = PAGE_TYPES_DISK_PAGES * DISK_PAGE_BYTES;
		rv->page_types = (unsigned char *)index_share;
		if (file_locked && file_read(rv->disk_index_fd,index_share,page_types_size) != page_types_size)
			{ cnf_set_error(config,"can not read page types from disk"); goto sing_link_set_fail; }
		index_share += page_types_size;
		}
	else
		page_types_size = 0, rv->page_types = NULL;

	if (head->use_flags & UF_COUNTERS)
		{
		counters_size = align_up(COUNTERS_SIZE(head->hashtable_size) * sizeof(unsigned),DISK_PAGE_BYTES);
		rv->counters = (unsigned *)index_share;
		if (file_locked && file_read(rv->disk_index_fd,index_share,counters_size) != counters_size)
			{ cnf_set_error(config,"can not read counters from disk"); goto sing_link_set_fail; }
		index_share += counters_size;
		}
	else
		counters_size = 0, rv->counters = NULL;

	rv->hash_table = (FKeyHeadGeneral *)index_share;
	
	rv->lock_set = (FLockSet *)extra_struct;
	extra_struct += locks_size;

	// Открывем шару и файл страниц
	if ((rv->pages_fd = shm_open(rv->mem_pages_fname,share_mode, FILE_PERMISSIONS)) == -1)
		{ cnf_set_formatted_error(config,"can not open share %s",rv->mem_pages_fname); goto sing_link_set_fail; }

	if (!(head->use_flags & UF_NOT_PERSISTENT))
		{
		rv->real_cpages = rv->used_cpages = (FChangedPages *)extra_struct;
		if ((rv->disk_pages_fd = open(rv->disk_pages_fname,O_RDWR, FILE_PERMISSIONS)) == -1)
			{ cnf_set_formatted_error(config,"can not open file %s",rv->disk_pages_fname); goto sing_link_set_fail; }
		}
	
	if (file_locked)
		{
		if (ftruncate(rv->pages_fd,head->pcnt * PAGE_SIZE_BYTES)) 
			{ cnf_set_error(config,"can not truncate collision table"); goto sing_link_set_fail; }

		cp_init(rv->used_cpages,page_types_size,counters_size);
		if (rv->conn_flags & CF_FULL_LOAD)
			{
			element_type hash_table_size = align_up((head->hashtable_size + 1) / 2 * sizeof(FKeyHeadGeneral) * KEYHEADS_IN_BLOCK,DISK_PAGE_BYTES);
			if (file_read(rv->disk_index_fd,index_share,hash_table_size) != hash_table_size)
				{ cnf_set_error(config,"can not read hash table from disk"); goto sing_link_set_fail; }
			cp_mark_hashtable_loaded(rv);

			if ((rv->pages[0] = (element_type *)mmap(NULL,head_sizes.mem_file_size,PROT_WRITE | PROT_READ, MAP_SHARED, rv->pages_fd, 0)) == MAP_FAILED)
				{ cnf_set_error(config,"can not map collision table"); goto sing_link_set_fail; }
			for (i = 1; i < head->pcnt; i++)
				rv->pages[i] = rv->pages[0] + i * PAGE_SIZE;
			cp_mark_pages_loaded(rv);
			}
		file_lock(ifd,LOCK_UN);
		file_locked = 0;
		}

	close(ifd);
	if (rv->conn_flags & CF_READER)
		{
		if (rv->disk_index_fd != -1) 
			close(rv->disk_index_fd), rv->disk_index_fd = -1;
		if (rv->disk_pages_fd != -1) 
			close(rv->disk_pages_fd), rv->disk_pages_fd = -1;
		rv->read_only = 1;
		}
	return rv;
	
sing_link_set_fail:
	if (file_locked && ifd != -1)
		{
		shm_unlink(rv->mem_index_fname); // Что-то пошло не так, удалим основную шару, т.к. в других файлах может быть мусор
		file_lock(ifd,LOCK_UN);
		}
	if (ifd != -1) close(ifd);
	if (rv) 
		sing_unlink_set(rv);

	return NULL;
	}

int idx_relink_set(FSingSet *index)
	{
	return 1;
	}
	
void sing_unlink_set(FSingSet *index)
	{
	unsigned i,pcnt;
	
	if (index->head != MAP_FAILED)
		{
		pcnt = index->head->pcnt;

		for (i = 0; i < pcnt; i++)
			{
			if (index->pages[i] != MAP_FAILED)
				munmap(index->pages[i],PAGE_SIZE_BYTES);
			}
		munmap(index->head,index->head->sizes.mem_file_size);
		}
		
	if (index->index_fd != -1) 
		{
		file_lock(index->index_fd,LOCK_UN);
		close(index->index_fd);
		}
	if (index->pages_fd != -1) close(index->pages_fd);
	if (index->disk_index_fd != -1) close(index->disk_index_fd); 	// Файл индекса на диске
	if (index->disk_pages_fd != -1) close(index->disk_pages_fd); 	// Файл страниц на диске
	if (index->mem_index_fname) free(index->mem_index_fname);
	if (index->disk_index_fname) free(index->disk_index_fname);
	free(index);
	}
	
void sing_unload_set(FSingSet *index)
	{
	if (!index->is_private)
		{
		shm_unlink(index->mem_index_fname);
		shm_unlink(index->mem_pages_fname);
		}
	lck_deinit_locks(index);
	sing_unlink_set(index);
	}
	
void sing_delete_set(FSingSet *index)
	{
	if (index->disk_index_fname)
		{
		unlink(index->disk_index_fname);
		unlink(index->disk_pages_fname);
		}
	if (!index->is_private)
		{
		shm_unlink(index->mem_index_fname);
		shm_unlink(index->mem_pages_fname);
		}
	lck_deinit_locks(index);
	sing_unlink_set(index);
	}

////////////////////////////////////////////////////////////////////////////////////////////////
///////                    Операции с ключами и данными
////////////////////////////////////////////////////////////////////////////////////////////////

// Возвращает 1 если ключи одинаковы, 0 если различны или вышли за пределы выделенной области. 
static inline int compareKeys(FSingSet *index,FKeyHead *old_key,FTransformData *tdata)
	{
	FKeyHead *new_key = &tdata->head.fields;
//	if (key1->size != key2->size || key1->data0 != key2->data0) return 0;
	// Есть различающиеся биты в данных или размере, максимум несовпадений должны фильтроваться здесь
	if ((*(element_type *)old_key ^ *(element_type *)new_key) & 0xFFFFFC7E) 
		return 0; 

	element_type *old_data;
	unsigned size = old_key->size;
	if (!size) return 1;
	if (size == 1)
		{
		element_type cmp = new_key->has_value ? tdata->key_rest[0] : new_key->extra;
		if (!old_key->has_value)
			{
			if (old_key->extra == cmp) 
				return 1;
			return 0;
			}
		if (!(old_data = pagesPointerNoError(index,old_key->extra))) 
			return 0;
		if (*old_data == cmp) 
			return 1;
		return 0;
		}
	unsigned pos = 0;
	if (!(old_data = regionPointerNoError(index,old_key->extra,size))) 
		return 0;
	while (pos < size && old_data[pos] == tdata->key_rest[pos]) pos++;
	if (pos >= size)
		return 1;
	return 0;
	}
	
static const FHashTableChain HT_CHAINS[2] = {{1,0,KH_BLOCK_LAST,0},{-1,KH_BLOCK_LAST,0,1}};

static FKeyHeadGeneral *_load_hashtable_entry(FSingSet *index,FKeyHeadGeneral *rv,unsigned hash)
	{
	int disk_index_fd;
	if ((disk_index_fd = index->disk_index_fd) == -1) 
		return NULL;

	file_lock(disk_index_fd,LOCK_EX);
	if (cp_is_hash_entry_loaded(index->real_cpages,hash))
		{ file_lock(disk_index_fd,LOCK_UN); return rv; }

	off_t h_off = (off_t)((index->real_cpages->hashtable_offset + hash * KH_BLOCK_SIZE / 2) / DISK_PAGE_SIZE) * DISK_PAGE_BYTES;
	lseek(disk_index_fd, h_off, SEEK_SET);

	if (file_read(disk_index_fd,((char *)index->head) + h_off,DISK_PAGE_BYTES) != DISK_PAGE_BYTES)
		rv = NULL;
	cp_mark_hash_entry_loaded(index->real_cpages,hash);
	file_lock(disk_index_fd,LOCK_UN);
	return rv;
	}

static inline FKeyHeadGeneral *get_hashtable_entry(FSingSet *index,unsigned hash)
	{
	FKeyHeadGeneral *rv = &index->hash_table[(hash >> 1) * KEYHEADS_IN_BLOCK];
	return (rv->whole || cp_is_hash_entry_loaded(index->real_cpages,hash)) ? rv : _load_hashtable_entry(index,rv,hash);
	}

static FKeyHead *_key_search(FSingSet *index,FTransformData *tdata,FReaderLock *rlock)
	{
	FKeyHeadGeneral *hblock;
	FHashTableChain const *ht_chain;
	unsigned inum,hnum;
	
_key_search_begin:
	lck_readerLock(index->lock_set,rlock);

	hblock = get_hashtable_entry(index,tdata->hash);
	ht_chain = &HT_CHAINS[tdata->hash & 1];

	for (hnum = ht_chain->start;hblock[hnum].fields.space_used;hnum += ht_chain->dir)
		if (compareKeys(index,&hblock[hnum].fields,tdata)) 
			return &hblock[hnum].fields;

	if ((inum = hblock[hnum].links_array.links[ht_chain->link_num]) == KH_ZERO_REF)
		return lck_readerUnlock(index->lock_set,rlock),NULL; 

_key_search_next_block:
	if (!lck_readerCheck(index->lock_set,rlock)) 
		goto _key_search_begin; // Проверяем свою блокировку, чтобы не зациклиться
	CHECK_PAGE_TYPE(index,inum,PT_HEADERS);
	if (!(hblock = (FKeyHeadGeneral *)regionPointerNoError(index,KH_BLOCK_IDX(inum),KH_BLOCK_SIZE))) // Мы вышли за пределы исп. памяти, читаем фигню, повторим
		{ 
		lck_readerUnlock(index->lock_set,rlock); 
		goto _key_search_begin; 
		}
	for(hnum = KH_BLOCK_NUM(inum);hnum < KH_BLOCK_LAST;hnum++)
		{
		if (compareKeys(index,&hblock[hnum].fields,tdata)) 
			return &hblock[hnum].fields;
		if (hblock[hnum].fields.chain_stop) // Дошли до конца цепочки, ничего не найдено
			return lck_readerUnlock(index->lock_set,rlock), NULL; 
		}
	if (!hblock[KH_BLOCK_LAST].space.space_used)
		{ 
		inum = hblock[KH_BLOCK_LAST].links.next; 
		goto _key_search_next_block;
		}
	FAILURE_CHECK(!hblock[KH_BLOCK_LAST].fields.chain_stop,"open chain"); // Для валидного состояния данные в последнем блоке должны завершать цепочку
	if (compareKeys(index,&hblock[KH_BLOCK_LAST].fields,tdata)) 
		return &hblock[KH_BLOCK_LAST].fields;
	return lck_readerUnlock(index->lock_set,rlock),NULL; 
	}

// Ищем наличие одиночного ключа в таблице
int idx_key_search(FSingSet *index,FTransformData *tdata,FReaderLock *rlock)
	{
	do
		{
		if (!_key_search(index,tdata,rlock))
			return RESULT_KEY_NOT_FOUND;
		} 
	while (!lck_readerUnlock(index->lock_set,rlock));
	return 0;
	}

void *_get_value(FSingSet *index,FKeyHead *key_head, unsigned *vsize)
	{
	element_type *key_rest,*value;
	
	if (key_head->has_value)
		{
		key_rest = pagesPointer(index,key_head->extra);
		value = &key_rest[key_head->size];
		*vsize = VALUE_SIZE_BYTES((FValueHead *)value);
		return (void *)&value[1];
		}
	*vsize = 0;
	return NULL;
	}

int idx_key_get(FSingSet *index,FTransformData *tdata,FReaderLock *rlock,void *value_dst,unsigned *value_dst_size)
	{
	FKeyHead *found;
	unsigned vsrc_size,tocpy;
	int rv;
	void *value;
	
	do
		{
		if (!(found = _key_search(index,tdata,rlock)))
			return RESULT_KEY_NOT_FOUND;
		value = _get_value(index,found,&vsrc_size);
		if (vsrc_size > *value_dst_size)
			tocpy = *value_dst_size, rv = RESULT_SMALL_BUFFER;
		else
			tocpy = vsrc_size,rv = 0;
		if (tocpy)
			memcpy(value_dst,value,tocpy);
		} 
	while (!lck_readerUnlock(index->lock_set,rlock));
	*value_dst_size = vsrc_size;
	return rv;
	}

int idx_key_get_cb(FSingSet *index,FTransformData *tdata,FReaderLock *rlock,CSingValueAllocator vacb,void **value,unsigned *vsize)
	{
	FKeyHead *found;
	unsigned vsrc_size;
	void *value_src,*value_dst;
	
	do
		{
		if (!(found = _key_search(index,tdata,rlock)))
			return RESULT_KEY_NOT_FOUND;
		value_src = _get_value(index,found,&vsrc_size);
		if (!vsrc_size)
			value_dst = NULL;
		else
			{
			if (!(value_dst = vacb(vsrc_size)))
				return lck_readerUnlock(index->lock_set,rlock),ERROR_NO_MEMORY;
			memcpy(value_dst,value_src,vsrc_size);
			}
		} 
	while (!lck_readerUnlock(index->lock_set,rlock));
	*vsize = vsrc_size;
	*value = value_dst;
	return 0;
	}
	
#ifdef MEMORY_CHECK
// Возвращает длину цепочки для хеша
unsigned idx_chain_len(FSingSet *index,unsigned hash)
	{
	FKeyHeadGeneral *hblock;
	unsigned inum,hnum,rv = 0;
	FHashTableChain const *ht_chain;

	hblock = get_hashtable_entry(index,hash);
	ht_chain = &HT_CHAINS[hash & 1];
	
	for (hnum = ht_chain->start; hblock[hnum].fields.space_used; hnum += ht_chain->dir)
		rv++;

	if ((inum = hblock[hnum].links_array.links[ht_chain->link_num]) == KH_ZERO_REF)
		return rv;
idx_chain_len_next_block:
	hblock = (FKeyHeadGeneral *)regionPointer(index,KH_BLOCK_IDX(inum),KH_BLOCK_SIZE);
	hnum = KH_BLOCK_NUM(inum);
	while (hnum < KH_BLOCK_LAST)
		{
		rv++;
		FAILURE_CHECK(!hblock[hnum].fields.space_used,"unused header in chain");
		if (hblock[hnum].fields.chain_stop) 
			return rv; 
		hnum++;
		}
	if (hblock[KH_BLOCK_LAST].space.space_used)
		return rv + 1;
		
	inum = hblock[KH_BLOCK_LAST].links.next; 
	goto idx_chain_len_next_block;
	}

void idx_print_chain_distrib(FSingSet *index)
	{
	unsigned counts[20] = {0};
	unsigned total = 0,maxlen = 0;
	unsigned page_counts[5] = {0,0,0,0,0};
			
	int i;
	for (i = 0; i < index->head->pcnt; i++)
		{ page_counts[index->page_types[i]]++; }
	printf("    Unknown pages: %d\n",page_counts[0]);
	printf("    General pages: %d\n",page_counts[1]);
	printf("    Headers pages: %d\n",page_counts[2]);
	printf("    Special pages: %d\n",page_counts[3]);
	printf("    Empty   pages: %d\n",page_counts[4]);
	printf("    ---------\n");
			
	for (i = 0; i < index->hashtable_size; i++)
		{
		unsigned len = idx_chain_len(index,i);
		if (len >= 19)
			counts[19]++;
		else
			counts[len]++;
		}
	for (i = 0; i < 20; i++)
		{
		total += counts[i];
		if (counts[i]) maxlen = i;
		}
	if (maxlen == 19) maxlen --;
	for (i = 0; i <= maxlen; i++)
		printf("    %02d: (%.02f) %d\n",i,(double)counts[i]/total * 100,counts[i]);
	if (counts[19])
		printf("  >=19: (%.02f%%) %d\n",(double)counts[19]/total * 100,counts[19]);
	}
#endif

//////////////////////////////

static int _alloc_and_set_rest(FSingSet *index,FTransformData *tdata)
	{
	unsigned sz = tdata->head.data.size_and_value >> 1,vsize = 0;
	if (sz <= 1) return 1;
	FValueHeadGeneral value_head;
	if (sz >= 64) // Если есть значение - убираем его бит и добавляем размер значения
		{
		sz -= 64;
		unsigned size_e = tdata->value_size / ELEMENT_SIZE;
		unsigned rest = tdata->value_size % ELEMENT_SIZE;
		if (rest || !tdata->value_source[tdata->value_size - 1])
			{ // Не делится нацело или значение заканчивается на нулевой байт
			value_head.fields.size_e = ++size_e;
			value_head.fields.extra_bytes = 4 - rest;
			}
		else
			value_head.whole = size_e;
		vsize = size_e + VALUE_HEAD_SIZE;
		}
	element_type *key_rest;
	if (!(key_rest = idx_general_alloc(index,sz + vsize,&tdata->head.fields.extra)))
		return 0;
	memcpy(key_rest,tdata->key_rest,sz * ELEMENT_SIZE);
	if (!vsize) return 1;

	key_rest[sz] = value_head.whole;
	memcpy(&key_rest[sz+1],(element_type *)tdata->value_source,tdata->value_size);
	if (value_head.fields.extra_bytes)
		((char *)(&key_rest[sz+vsize]))[-1] = 0xFF;

	return 1;
	}

// Заменяет остаток ключа и значение если нужно. Тело старого ключа уже заведомо в памяти
// Возвращает комбинацию KS_CHANGED и KS_NEED_FREE
static int _replace_key_value(FSingSet *index,FKeyHeadGeneral *old_key_head,FTransformData *tdata)
	{
	FKeyHead tmp_key_head;
	unsigned sz = tdata->head.fields.size;
	unsigned old_sz = sz;
	unsigned old_vsize = 0;
	element_type *old_value = NULL;
	int rv = old_key_head->fields.diff_mark ^ tdata->head.fields.diff_mark;
	FORMATTED_LOG_OPERATION("key %s already exists\n",tdata->key_source);
	old_key_head->fields.diff_mark ^= rv; // Мы под блокировкой цепочки
	rv *= KS_MARKED;
	if (old_key_head->fields.has_value)
		{
		element_type *old_data = pagesPointer(index,old_key_head->fields.extra);
		FValueHeadGeneral *old_vhead = (FValueHeadGeneral *)&old_data[sz];
		old_vsize = VALUE_SIZE_BYTES(&old_vhead->fields);
		old_value = &old_data[sz + VALUE_HEAD_SIZE];
		if (tdata->head.fields.has_value && tdata->value_size == old_vsize
				&& !memcmp(old_value,tdata->value_source,tdata->value_size))
			return rv | KS_SUCCESS;
		old_sz += old_vhead->fields.size_e + VALUE_HEAD_SIZE;
		}
	else if (!tdata->head.fields.has_value) 
		return rv | KS_SUCCESS;

	element_type old_key_rest = old_key_head->fields.extra;
	lck_memoryLock(index); // Блокируем операции с памятью
	if (!_alloc_and_set_rest(index,tdata))
		return lck_memoryUnlock(index),ERROR_NO_SET_MEMORY;
	tmp_key_head = old_key_head->fields;
	tmp_key_head.has_value = tdata->head.fields.has_value;
	tmp_key_head.extra = tdata->head.fields.extra;
	old_key_head->fields = tmp_key_head;
	if (old_sz > 1)
		{
		tdata->old_key_rest = old_key_rest;
		tdata->old_key_rest_size = old_sz;
		tdata->old_value_size = old_vsize;
		tdata->old_value = old_value;
		}
	return rv | KS_CHANGED;
	}

static inline void _change_counter(FSingSet *index,unsigned hash,int diff)
	{
	if (index->counters)
		{
		unsigned cnum = HASH_TO_COUNTER(hash);
		index->counters[cnum] += diff;
		cp_mark_counter_dirty(index->used_cpages,cnum);
		}
	index->head->count += diff;
	}

// Добавляем ключ, если нет цепочки коллизий. Если есть, сохраняет адрес цепочки для префетча
// Возвращает комбинацию флагов KS_ADDED, KS_DELETED, KS_NEED_FREE, KS_MARKED, KS_ERROR
// Может повесить блокировку памяти и не снять ее. Необходимость снятия определяется по флагам
int idx_key_try_set(FSingSet *index,FTransformData *tdata)
	{
	FKeyHeadGeneral *hblock;
	element_type kh_idx;
	FKeyHead *key_head;
	FHashTableChain const *ht_chain;
	unsigned hnum;

	hblock = get_hashtable_entry(index,tdata->hash);
	ht_chain = &HT_CHAINS[tdata->hash & 1];
	hnum = ht_chain->start;
	int rv = 0;

	while (hblock[hnum].fields.space_used)
		{
		if (compareKeys(index,&hblock[hnum].fields,tdata)) 
			{
			rv = _replace_key_value(index,&hblock[hnum],tdata);
			if (rv & (KS_CHANGED | KS_MARKED))
				cp_mark_hash_entry_dirty(index->used_cpages,tdata->hash);
			return rv;
			}
		hnum += ht_chain->dir;
		}
	if (hblock[hnum].links_array.links[ht_chain->link_num] != KH_ZERO_REF)
		{
		tdata->chain_idx_ref = &hblock[hnum].links_array.links[ht_chain->link_num];
		return 0;
		}
	FORMATTED_LOG_OPERATION("adding key %s\n",tdata->key_source);

	// Нет продолжения цепочки, добавляем ключ
	lck_memoryLock(index); // Блокируем операции с памятью
	if (!_alloc_and_set_rest(index,tdata))
		return lck_memoryUnlock(index),ERROR_NO_SET_MEMORY;

	_change_counter(index,tdata->hash,1);
	cp_mark_hash_entry_dirty(index->used_cpages,tdata->hash);

	if (hnum != ht_chain->end && !hblock[hnum + ht_chain->dir].fields.space_used)
		{ // Можем продвинуться внутри хеш-таблицы
		hblock[hnum + ht_chain->dir].links_array.links[ht_chain->link_num] = KH_ZERO_REF;
		hblock[hnum].fields = tdata->head.fields;
		return KS_ADDED | KS_MARKED;
		}
	if (!(key_head = (FKeyHead *)kh_alloc_one(index,&kh_idx)))
		return lck_memoryUnlock(index),ERROR_NO_SET_MEMORY;
	FORMATTED_LOG_HEADER("first header in chain allocated at %u[%u]\n",KH_BLOCK_IDX(kh_idx),KH_BLOCK_NUM(kh_idx));
	*key_head = tdata->head.fields;
	hblock[hnum].links_array.links[ht_chain->link_num] = kh_idx;
	return KS_ADDED | KS_MARKED;
	}

// Добавляем или изменяем ключ в таблице. Цепочка коллизий по этому ключу заведомо есть (был вызов idx_key_try_set)
// Может повесить блокировку памяти и не снять ее. Необходимость снятия определяется по флагам
// Возвращает комбинацию флагов KS_ADDED, KS_DELETED, KS_NEED_FREE, KS_MARKED, KS_ERROR
int idx_key_set(FSingSet *index,FTransformData *tdata)
	{
	element_type *chain_block_ref = tdata->chain_idx_ref;
	FAILURE_CHECK(!chain_block_ref,"no chain tail");

	FKeyHead *key_head;
	element_type kh_idx,hb_idx;
	FKeyHeadGeneral *hblock;
	unsigned header_num,chain_start_num,header_chain_len = 0;
	unsigned cb_ref_cpages_num = cp_get_hash_entry_num(index->used_cpages,tdata->hash);
	int rv = 0;

	for(;;)
		{
		CHECK_PAGE_TYPE(index,*chain_block_ref,PT_HEADERS);
		hb_idx = KH_BLOCK_IDX(*chain_block_ref);
		hblock = (FKeyHeadGeneral *)regionPointer(index,hb_idx,KH_BLOCK_SIZE);
		chain_start_num = KH_BLOCK_NUM(*chain_block_ref);

#define CHECK_HEADER_IN_BLOCK(HNUM) case (HNUM): do {\
				FAILURE_CHECK(!hblock[(HNUM)].fields.space_used,"empty header in chain"); \
				if (compareKeys(index,&hblock[(HNUM)].fields,tdata)) \
					{ \
					rv = _replace_key_value(index,&hblock[(HNUM)],tdata); \
					if (rv & (KS_CHANGED | KS_MARKED)) \
						cp_mark_hblock_dirty(index->used_cpages,hb_idx); \
					return rv; \
					} \
				if (hblock[(HNUM)].fields.chain_stop) \
					{ header_num = (HNUM)+1; goto idx_key_set_not_found; } \
				} while (0)

		switch (chain_start_num)
			{
			CHECK_HEADER_IN_BLOCK(0);
			CHECK_HEADER_IN_BLOCK(1);
			CHECK_HEADER_IN_BLOCK(2);
			CHECK_HEADER_IN_BLOCK(3);
			CHECK_HEADER_IN_BLOCK(4);
			CHECK_HEADER_IN_BLOCK(5);
			CHECK_HEADER_IN_BLOCK(6);
			}
		if (hblock[KH_BLOCK_LAST].fields.space_used)
			{ // В последнем блоке тоже данные
			FAILURE_CHECK(!hblock[KH_BLOCK_LAST].fields.chain_stop,"open chain");
			if (compareKeys(index,&hblock[KH_BLOCK_LAST].fields,tdata))
				{
				rv = _replace_key_value(index,&hblock[KH_BLOCK_LAST],tdata);
				if (rv & (KS_CHANGED | KS_MARKED))
					cp_mark_hblock_dirty(index->used_cpages,hb_idx);
				return rv;
				}
			if (chain_start_num)
				{ header_num = 8; goto idx_key_set_not_found; }
			// Полная цепочка из 8 блоков
			lck_memoryLock(index); 
			if (!_alloc_and_set_rest(index,tdata))
				return lck_memoryUnlock(index),ERROR_NO_SET_MEMORY;
			FORMATTED_LOG_OPERATION("adding key %s\n",tdata->key_source);
			_change_counter(index,tdata->hash,1);

			key_head = (FKeyHead *)kh_alloc_block(index,2,&kh_idx); // Выделяем новую цепочку на единицу больше
			FORMATTED_LOG_HEADER("appending new space at %u[%u] to full chain %u[0]\n",KH_BLOCK_IDX(kh_idx),KH_BLOCK_NUM(kh_idx),hb_idx);
			key_head[0] = hblock[KH_BLOCK_LAST].fields; // Копируем данные из последнего заголовка
			key_head[0].chain_stop = 0; // Убираем признак окончания цепочки
			key_head[1] = tdata->head.fields; // Копируем новые данные в конец цепочки
	
			hblock[KH_BLOCK_LAST].links.next = kh_idx; // Заменяем восьмой элемент в старой цепочке ссылкой на новую и делаем данные видимыми
			cp_mark_hblock_dirty(index->used_cpages,hb_idx);
			return KS_ADDED | KS_MARKED;
			}
		cb_ref_cpages_num = cp_get_hblock_num(index->used_cpages,hb_idx);
		chain_block_ref = &hblock[KH_BLOCK_LAST].links.next;
		FAILURE_CHECK(*chain_block_ref == KH_ZERO_REF,"open chain");
		}

idx_key_set_not_found: // Ничего не нашли, цепочка короче восьми заголовков, будем добавлять
	lck_memoryLock(index); 
	if (!_alloc_and_set_rest(index,tdata))
		return lck_memoryUnlock(index),ERROR_NO_SET_MEMORY;
	FORMATTED_LOG_OPERATION("adding key %s\n",tdata->key_source);
	_change_counter(index,tdata->hash,1);

	header_chain_len = header_num - chain_start_num;
	FAILURE_CHECK(header_chain_len >= KEYHEADS_IN_BLOCK,"too long block in chain");
 
	if (chain_start_num == 1 && !hblock[0].fields.space_used) // Если пустой нулевой элемент а цепочка начинается с первого, занимаем его, иначе сначала смотрим вверх
		goto idx_key_set_expand_chain_down;
	if (header_num <= KH_BLOCK_LAST && !hblock[header_num].fields.space_used)
		{ // Есть дырка за этой цепочкой 
		FORMATTED_LOG_HEADER("expanding chain up at %u[%u]\n",hb_idx,chain_start_num); 
		kh_expand_chain_up(index,hblock,hb_idx,header_num,&tdata->head.fields);
		return KS_ADDED | KS_MARKED;
		}
	if (chain_start_num && !hblock[chain_start_num - 1].fields.space_used)
		{ // Есть дырка перед этой цепочкой 
idx_key_set_expand_chain_down:
		FORMATTED_LOG_HEADER("expanding chain down at %u[%u]\n",hb_idx,chain_start_num); 
		kh_expand_chain_down(index,hblock,hb_idx,chain_start_num - 1,&tdata->head.fields);
		*chain_block_ref -= KEY_HEAD_SIZE;
		cp_mark_dirty(index->used_cpages,cb_ref_cpages_num);
		return KS_ADDED | KS_MARKED;
		}
	// Выделяем новую цепочку на единицу больше
	if (header_chain_len == KEYHEADS_IN_BLOCK - 1)
		key_head = (FKeyHead *)kh_alloc_full_block(index,&kh_idx); 
	else
		key_head = (FKeyHead *)kh_alloc_block(index,header_chain_len + 1,&kh_idx);
			
	FORMATTED_LOG_HEADER("moving chain %u at %u[%u] to new location %u[%u]\n",header_chain_len,hb_idx,chain_start_num,KH_BLOCK_IDX(kh_idx),KH_BLOCK_NUM(kh_idx));
		
	switch (header_chain_len)
		{
		case 7: key_head[6] = hblock[chain_start_num + 6].fields;
		case 6: key_head[5] = hblock[chain_start_num + 5].fields;
		case 5: key_head[4] = hblock[chain_start_num + 4].fields;
		case 4: key_head[3] = hblock[chain_start_num + 3].fields;
		case 3: key_head[2] = hblock[chain_start_num + 2].fields;
		case 2: key_head[1] = hblock[chain_start_num + 1].fields;
		case 1: key_head[0] = hblock[chain_start_num].fields;
		}

	key_head[header_chain_len - 1].chain_stop = 0; // Убираем признак окончания цепочки
	key_head[header_chain_len] = tdata->head.fields; // Копируем новый заголовок в последний элемент новой цепочки

	*chain_block_ref = kh_idx;
	cp_mark_dirty(index->used_cpages,cb_ref_cpages_num);

	lck_waitForReaders(index->lock_set);
	kh_free_block(index,&hblock[chain_start_num],hb_idx + chain_start_num * KEY_HEAD_SIZE,header_chain_len); // Удаляем старую цепочку
	return KS_ADDED | KS_MARKED;
	}

// Удаляет тело ключа и значение. Тело ключа заведомо в памяти
static void _del_key_rest(FSingSet *index,FKeyHead kh)
	{
	unsigned sz = kh.size;
	if (kh.has_value)
		{ // Удаляем значение и хвост ключа
		FValueHead *vhead = (FValueHead *)pagesPointer(index,kh.extra + sz);
		sz += vhead->size_e + VALUE_HEAD_SIZE;
		}
	if (sz > 1)
		idx_general_free(index,kh.extra,sz);		
	}

// Удаляет ключ из хештаблицы, to_del и last_head находятся в хештаблице, продолжения цепочки нет
static void _del_from_hash_table(FSingSet *index,unsigned hash,FKeyHead *to_del,FKeyHeadGeneral *last_head)
	{
	lck_memoryLock(index); // Блокируем операции с памятью
	FKeyHead okeyhead = *to_del;	
	_change_counter(index,hash,-1);
	cp_mark_hash_entry_dirty(index->used_cpages,hash);

	if (to_del != &last_head->fields)
		{
		*to_del = last_head->fields; // Если удаляем не последний, копируем последний в удаляемый
		lck_waitForReaders(index->lock_set); // Стираемого нет в цепочке, дождемся пока дочитают его хвост
		last_head->whole = ((uint64_t)KH_ZERO_REF << 32) + KH_ZERO_REF; // Сдвигаем ссылку в хеш-таблице на один (надо также сбросить space_used, поэтому сбрасываем обе ссылки)
		_del_key_rest(index,okeyhead);
		return;
		}
	last_head->whole = ((uint64_t)KH_ZERO_REF << 32) + KH_ZERO_REF; // Удаляем стираемый из цепочки
	lck_waitForReaders(index->lock_set); // Ждем ридеров чтобы стереть хвост
	_del_key_rest(index,okeyhead);
	}

// Удаляет элемент из хештаблицы с продолжением цепочки в один элемент
static void _del_one_after_hash_table(FSingSet *index,unsigned hash,FKeyHead *to_del,FKeyHeadGeneral *last_head,element_type last_head_idx,element_type *ht_links_ref)
	{
	lck_memoryLock(index); 
	_change_counter(index,hash,-1);
	cp_mark_hash_entry_dirty(index->used_cpages,hash);
	FKeyHead okeyhead = *to_del;
	*to_del = last_head->fields; 
	lck_waitForReaders(index->lock_set); 
	*ht_links_ref = KH_ZERO_REF; 
	kh_free_block(index,last_head,last_head_idx,1); 
	_del_key_rest(index,okeyhead);
	}

// Удаляет цепочку из одного элемента после хеш-таблицы
void _del_last_after_hash_table(FSingSet *index,unsigned hash,FKeyHeadGeneral *last_head,element_type last_head_idx,element_type *ht_links_ref)
	{
	lck_memoryLock(index); 
	_change_counter(index,hash,-1);
	cp_mark_hash_entry_dirty(index->used_cpages,hash);
	FKeyHead okeyhead = last_head->fields;
	*ht_links_ref = KH_ZERO_REF; 
	lck_waitForReaders(index->lock_set); 
	kh_free_block(index,last_head,last_head_idx,1); 
	_del_key_rest(index,okeyhead);
	}

void _del_last_in_block2(FSingSet *index,unsigned hash,FKeyHeadGeneral *last_head,element_type last_block_idx,FKeyHeadGeneral *prev_links,element_type prev_links_idx)
	{
	lck_memoryLock(index); // Блокируем операции с памятью
	_change_counter(index,hash,-1);
	FKeyHead okeyhead = last_head->fields;
	FKeyHead wkeyhead = (last_head - 1)->fields; // Единственный оставшийся в этом блоке 
	wkeyhead.chain_stop = 1; // Ставим ему chain_stop
	prev_links->fields = wkeyhead; // Копируем на место ссылки на этот блок
	lck_waitForReaders(index->lock_set);
	kh_free_block(index,&last_head[-1],last_block_idx,2); // Удаляем этот блок
	_del_key_rest(index,okeyhead);
	cp_mark_hblock_dirty(index->used_cpages,prev_links_idx);
	}

void _del_last_in_chain(FSingSet *index,unsigned hash,FKeyHeadGeneral *last_head,element_type last_head_idx)
	{
	lck_memoryLock(index); // Блокируем операции с памятью
	_change_counter(index,hash,-1);
	FKeyHead okeyhead = last_head->fields;
	(last_head - 1)->fields.chain_stop = 1; // Ставим chain_stop предпоследнему
	lck_waitForReaders(index->lock_set);
	kh_free_last_from_chain(index,last_head,last_head_idx); // Удаляем последний
	_del_key_rest(index,okeyhead);
	}

void _del_from_chain_block2(FSingSet *index,unsigned hash,FKeyHead *to_del,element_type to_del_idx,
		FKeyHeadGeneral *last_head,element_type last_block_idx,FKeyHeadGeneral *prev_links,element_type prev_links_idx)
	{
	lck_memoryLock(index); // Блокируем операции с памятью
	if (to_del_idx)
		cp_mark_hblock_dirty(index->used_cpages,to_del_idx);
	else
		cp_mark_hash_entry_dirty(index->used_cpages,hash);
	_change_counter(index,hash,-1);
	FKeyHead okeyhead = *to_del;
	FKeyHead wkeyhead = last_head->fields; // Удаляем не последний, копируем последний в удаляемый
	wkeyhead.chain_stop = 0;
	*to_del = wkeyhead;
	lck_waitForReaders(index->lock_set);
	wkeyhead = (last_head - 1)->fields; // Единственный оставшийся в этом блоке в восьмой элемент предыдущей цепочки
	wkeyhead.chain_stop = 1; // Ставим ему chain_stop
	prev_links->fields = wkeyhead; // Копируем на место ссылки на этот блок
	kh_free_block(index,&last_head[-1],last_block_idx,2); // Удаляем этот блок
	_del_key_rest(index,okeyhead);
	cp_mark_hblock_dirty(index->used_cpages,prev_links_idx);
	}

void _del_from_chain(FSingSet *index,unsigned hash,FKeyHead *to_del,element_type to_del_idx,FKeyHeadGeneral *last_head,element_type last_head_idx)
	{
	lck_memoryLock(index); // Блокируем операции с памятью
	if (to_del_idx)
		cp_mark_hblock_dirty(index->used_cpages,to_del_idx);
	else
		cp_mark_hash_entry_dirty(index->used_cpages,hash);
	_change_counter(index,hash,-1);
	FKeyHead okeyhead = *to_del;
	FKeyHead wkeyhead = last_head->fields; // Удаляем не последний, копируем последний в удаляемый
	wkeyhead.chain_stop = 0;
	*to_del = wkeyhead;
	lck_waitForReaders(index->lock_set);
	(last_head - 1)->fields.chain_stop = 1; // Ставим chain_stop предпоследнему
	kh_free_last_from_chain(index,last_head,last_head_idx); // Удаляем последний
	_del_key_rest(index,okeyhead);
	}

int idx_key_del(FSingSet *index,FTransformData *tdata)
	{
	FKeyHead *to_del = NULL; // Заголовок для удаления (найденный)
	FKeyHeadGeneral *last_head; // Последний заголовок в цепочке
	FKeyHeadGeneral *table_block; // Блок в хеш-таблице
	FKeyHeadGeneral *prev_block = NULL; // Предыдущий блок
	unsigned inum,hnum;
	element_type *ht_links_ref; // Номер заголовка со ссылкой из фрагмента в хеш-таблице
	unsigned last_size = 1; // Размер последнего фрагмента цепочки
	element_type prev_block_idx = 0,khb_idx = 0;
	FHashTableChain const *ht_chain;

	table_block = get_hashtable_entry(index,tdata->hash);
	ht_chain = &HT_CHAINS[tdata->hash & 1];
	for (hnum = ht_chain->start; table_block[hnum].fields.space_used; hnum += ht_chain->dir)
		{
		last_head = &table_block[hnum];
		if (compareKeys(index,&last_head->fields,tdata))
			{
			to_del = &last_head->fields;
			hnum += ht_chain->dir;
			while (table_block[hnum].fields.space_used)
				last_head = &table_block[hnum], hnum += ht_chain->dir;
			break;
			}
		}
	ht_links_ref = &table_block[hnum].links_array.links[ht_chain->link_num];
	if ((inum = *ht_links_ref) == KH_ZERO_REF)
		{
		if (!to_del) 
			return KS_SUCCESS;
		return _del_from_hash_table(index,tdata->hash,to_del,last_head), KS_DELETED;
		}
	last_head = (FKeyHeadGeneral *)pagesPointer(index,inum);
	if (last_head->fields.chain_stop)
		{ // Особый случай, продолжение из одного элемента
		if (to_del)
			return _del_one_after_hash_table(index,tdata->hash,to_del,last_head,inum,ht_links_ref),KS_DELETED;
		if (compareKeys(index,&last_head->fields,tdata))
			return _del_last_after_hash_table(index,tdata->hash,last_head,inum,ht_links_ref),KS_DELETED;
		return KS_SUCCESS;
		}
	if (to_del)
		goto idx_key_del_found;
	if (compareKeys(index,&last_head->fields,tdata))
		{ khb_idx = inum; to_del = &last_head->fields; goto idx_key_del_found; }
	do 
		{
		last_head++;
		if (!last_head->fields.space_used)
			{
			FAILURE_CHECK(last_size != KH_BLOCK_LAST,"bad size of chain link");
			prev_block = last_head;
			prev_block_idx = inum;
			inum = last_head->links.next;
			FAILURE_CHECK(inum == KH_ZERO_REF,"open chain");
			last_head = (FKeyHeadGeneral *)pagesPointer(index,inum);
			last_size = 0;
			}
		last_size++;
		FAILURE_CHECK(last_size > KEYHEADS_IN_BLOCK,"too long chain");
		if (compareKeys(index,&last_head->fields,tdata))
			{
			if (last_head->fields.chain_stop)
				{
				if (prev_block && last_size == 2)
					return _del_last_in_block2(index,tdata->hash,last_head,inum,prev_block,prev_block_idx),KS_DELETED;
				return _del_last_in_chain(index,tdata->hash,last_head,inum + (last_size - 1) * KEY_HEAD_SIZE),KS_DELETED;
				}
			khb_idx = inum; 
			to_del = &last_head->fields;
			goto idx_key_del_found;
			}
		}
	while (!last_head->fields.chain_stop);
	return KS_SUCCESS;

idx_key_del_found:
	FORMATTED_LOG_OPERATION("deleting key %s\n",tdata->key_source);
	do 
		{
		last_head++;
		if (!last_head->fields.space_used)
			{
			FAILURE_CHECK(last_size != KH_BLOCK_LAST,"bad size of chain link");
			prev_block = last_head;
			prev_block_idx = inum;
			inum = last_head->links.next;
			FAILURE_CHECK(inum == KH_ZERO_REF,"open chain");
			last_head = (FKeyHeadGeneral *)pagesPointer(index,inum);
			last_size = 0;
			}
		last_size++;
		FAILURE_CHECK(last_size > KEYHEADS_IN_BLOCK,"too long chain");
		}
	while (!last_head->fields.chain_stop);

	if (prev_block && last_size == 2)
		return _del_from_chain_block2(index,tdata->hash,to_del,khb_idx,last_head,inum,prev_block,prev_block_idx),KS_DELETED;
	return _del_from_chain(index,tdata->hash,to_del,khb_idx,last_head,inum + (last_size - 1) * KEY_HEAD_SIZE),KS_DELETED;
	}
	
void del_unmarked_from_hash_chain(FSingSet *index,unsigned hash,CSingIterateCallbackRaw cb,void *param)
	{
	FKeyHeadGeneral *key_head; // Заголовок для удаления (найденный)
	FKeyHeadGeneral *last_head; // Последний заголовок в цепочке
	FKeyHead wkeyhead,okeyhead; // Последний заголовок (перемещаемый), данные старого заголовка (для очистки)
	FKeyHeadGeneral *table_block; // Блок в хеш-таблице
	FKeyHeadGeneral *last_block; // Последний блок в цепочке (не в хеш-таблице)
	FKeyHeadGeneral *prev_block; // Предыдущий блок
	unsigned inum;
	unsigned hnum;
	unsigned ht_links_num; // Номер заголовка со ссылкой из фрагмента в хеш-таблице
	unsigned last_size = 0; // Размер последнего фрагмента цепочки
	unsigned diff_mark = index->head->state_flags & SF_DIFF_MARK;
	element_type last_block_idx = 0,prev_block_idx = 0,khb_idx;
	element_type *key_rest;
	FHashTableChain const *ht_chain;
	
	table_block = get_hashtable_entry(index,hash);
	ht_chain = &HT_CHAINS[hash & 1];
	
del_unmarked_from_hash_chain_begin:
	last_block = NULL;
	prev_block = NULL;
	key_head = NULL;
	last_head = NULL;
	khb_idx = 0;
	hnum = ht_chain->start;

	lck_chainLock(index,hash);

	while (table_block[hnum].fields.space_used)
		{
		last_head = &table_block[hnum];
		if (!key_head && last_head->fields.diff_mark != diff_mark)
			key_head = last_head;
		hnum += ht_chain->dir;
		}
	ht_links_num = hnum;
	inum = table_block[ht_links_num].links_array.links[ht_chain->link_num];
		
	while (inum != KH_ZERO_REF)
		{
		prev_block = last_block;
		prev_block_idx = last_block_idx;
		last_block_idx = KH_BLOCK_IDX(inum);
		last_block = (FKeyHeadGeneral *)regionPointer(index,last_block_idx,KH_BLOCK_SIZE);
		hnum = KH_BLOCK_NUM(inum);
		last_size = 0;
		
		while (hnum < KH_BLOCK_LAST)
			{
			last_head = &last_block[hnum];
			FAILURE_CHECK(!last_head->fields.space_used,"empty head in chain");
			if (!key_head && last_head->fields.diff_mark != diff_mark)
				khb_idx = last_block_idx, key_head = last_head;
			last_size++;
			if (last_head->fields.chain_stop)
				{
				if (key_head) goto del_unmarked_from_hash_chain_found;
				lck_chainUnlock(index,hash);
				return;
				}
			hnum++;
			}
		if (last_block[KH_BLOCK_LAST].fields.space_used)
			{ // В последнем блоке тоже данные
			last_head = &last_block[KH_BLOCK_LAST];
			FAILURE_CHECK(!last_head->fields.chain_stop,"open chain");
			if (!key_head && last_head->fields.diff_mark != diff_mark) 
				khb_idx = last_block_idx, key_head = last_head;
			last_size++;
			if (key_head) goto del_unmarked_from_hash_chain_found;
			lck_chainUnlock(index,hash);
			return;
			}
		inum = last_block[KH_BLOCK_LAST].links.next;
		}

	if (!key_head)
		{
		lck_chainUnlock(index,hash);
		return;
		}

del_unmarked_from_hash_chain_found:

	lck_memoryLock(index); // Блокируем операции с памятью
	_change_counter(index,hash,-1);
	
	okeyhead = key_head->fields;
	key_rest = (key_head->data.size_and_value > 3) ? pagesPointer(index,key_head->fields.extra) : NULL;

#ifdef LOG_OPERATION
	static char key_buf[MAX_KEY_SOURCE + 1];
	unsigned key_size = cd_decode(&key_buf[0],&key_head->fields,key_rest);
	key_buf[key_size] = 0;
	
	FORMATTED_LOG("deleting key %s\n",key_buf);
#endif
	if (key_head->fields.has_value)
		{
		FValueHeadGeneral *old_vhead = (FValueHeadGeneral *)&key_rest[key_head->fields.size];
		element_type *value = &key_rest[key_head->fields.size + VALUE_HEAD_SIZE];
		unsigned vsize = VALUE_SIZE_BYTES(&old_vhead->fields);
		(*cb)(&key_head->fields,key_rest,value,vsize,param);
		}
	else
		(*cb)(&key_head->fields,key_rest,NULL,0,param);

	if (key_head != last_head)
		{ // Если удаляем не последний, копируем последний в удаляемый
		wkeyhead = last_head->fields;
		wkeyhead.chain_stop = 0;
		key_head->fields = wkeyhead;
		if (khb_idx)
			cp_mark_hblock_dirty(index->used_cpages,khb_idx);
		else
			cp_mark_hash_entry_dirty(index->used_cpages,hash);
		}
	// Ждем ридеров (иначе последний перемещенный элемент может разминуться с ридером, идущим по цепочке)
	lck_waitForReaders(index->lock_set);
	// Удаляем последний элемент
	if (!last_block)
		{ // Последний заголовок находится в хеш-таблице, hnum не в начале цепочки
		// Сдвигаем ссылку в хеш-таблице на один (надо также сбросить space_used, поэтому сбрасываем обе ссылки)
		last_head->whole = ((uint64_t)KH_ZERO_REF << 32) + KH_ZERO_REF; 
		if (hnum - ht_chain->dir != ht_chain->start)
			(last_head - ht_chain->dir)->fields.chain_stop = 1;  // Ставим последнему в хеш-таблице chain_stop
		_del_key_rest(index,okeyhead);
		cp_mark_hash_entry_dirty(index->used_cpages,hash);
		lck_memoryUnlock(index);
		goto del_unmarked_from_hash_chain_begin;
		}
	if (last_size == 1)
		{ // Единственный элемент в блоке, ссылка идет из хеш-таблицы
		table_block[ht_links_num].links_array.links[ht_chain->link_num] = KH_ZERO_REF; // Обнуляем ссылку в хеш-таблице.
		if (ht_links_num != ht_chain->start)
			table_block[ht_links_num - ht_chain->dir].fields.chain_stop = 1;  // Ставим последнему в хеш-таблице chain_stop
		kh_free_block(index,last_head,last_block_idx + KEY_HEAD_SIZE * hnum,1); // Удаляем этот блок
		_del_key_rest(index,okeyhead);
		cp_mark_hash_entry_dirty(index->used_cpages,hash);
		lck_memoryUnlock(index);
		goto del_unmarked_from_hash_chain_begin;
		}
	if (prev_block && last_size == 2)
		{ 
		wkeyhead = (last_head - 1)->fields; // Единственный оставшийся в этом блоке в восьмой элемент предыдущей цепочки
		wkeyhead.chain_stop = 1; // Ставим ему chain_stop
		prev_block[KH_BLOCK_LAST].fields = wkeyhead; // Копируем на место ссылки на этот блок
		kh_free_block(index,&last_head[-1],last_block_idx + KEY_HEAD_SIZE * (hnum - 1),2); // Удаляем этот блок
		_del_key_rest(index,okeyhead);
		cp_mark_hblock_dirty(index->used_cpages,prev_block_idx);
		lck_memoryUnlock(index);
		goto del_unmarked_from_hash_chain_begin;
		}
	
	(last_head - 1)->fields.chain_stop = 1; // Ставим chain_stop предпоследнему
	kh_free_last_from_chain(index,last_head,last_block_idx + KEY_HEAD_SIZE * hnum); // Удаляем последний
	_del_key_rest(index,okeyhead);
	lck_memoryUnlock(index);
	goto del_unmarked_from_hash_chain_begin;
	}

// Пробегаем по всем ненулевым счетчикам или элементам хеша если counters == NULL. 
// Для каждой цепочки ищем непомеченные в маске элементы и удаляем их
void idx_del_unmarked(FSingSet *index,unsigned *counters,CSingIterateCallbackRaw cb,void *param)
	{
	unsigned i,j;
	
	if (!index->counters || !counters)
		{
		for (i = 0; i < index->hashtable_size; i++)
			del_unmarked_from_hash_chain(index,i,cb,param);
		return;
		}
	
	for (i = 0; i < COUNTERS_SIZE(index->hashtable_size) - 1; i++)
		{
		if (!counters[i]) continue;
		for (j = COUNTER_TO_HASH(i); j < COUNTER_TO_HASH(i+1); j++)
			del_unmarked_from_hash_chain(index,j,cb,param);
		}
	if (counters[i])
		{
		for (j = COUNTER_TO_HASH(i) ; j < index->hashtable_size; j++)
			del_unmarked_from_hash_chain(index,j,cb,param);
		}
	}
	
static inline void cb_call(FSingSet *index,FKeyHeadGeneral *key_head,void *cb,int raw,void *param)
	{
	unsigned vsize;
	element_type *key_rest,*value;
	char *value_data;
	
	if (key_head->data.size_and_value > 3)
		{
		key_rest = pagesPointer(index,key_head->fields.extra);
		if (key_head->fields.has_value)
			{
			value = &key_rest[key_head->fields.size];
			if (!key_head->fields.size)
				key_rest = NULL;
			value_data = (char *)&value[1];
			vsize = VALUE_SIZE_BYTES((FValueHead *)value);
			}
		else
			value_data = NULL, vsize = 0;
		}
	else
		key_rest = NULL, value_data = NULL, vsize = 0;
	if (raw)
		{ (*(CSingIterateCallbackRaw)cb)(&key_head->fields,key_rest,value_data,vsize,param); return; }
	char keybuf[MAX_KEY_SOURCE + 1];
	unsigned size = cd_decode(keybuf,&key_head->fields,key_rest);
	keybuf[size] = 0;
	(*(CSingIterateCallback)cb)(keybuf,value_data,vsize,NULL,param);
	}

void process_hash_chain(FSingSet *index,unsigned hash,void *cb,int raw,void *param)
	{
	FKeyHeadGeneral *prev_head,*key_head = NULL; // Заголовок для удаления (найденный)
	FKeyHeadGeneral *table_block; // Блок в хеш-таблице
	FKeyHeadGeneral *last_block; // Последний блок в цепочке (не в хеш-таблице)
	unsigned inum,hnum;
	FHashTableChain const *ht_chain;
	
	table_block = get_hashtable_entry(index,hash);
	ht_chain = &HT_CHAINS[hash & 1];
	hnum = ht_chain->start;

	if (table_block[hnum].fields.space_used)
		{
		key_head = &table_block[hnum];
		if (key_head->data.size_and_value > 3) 
			__builtin_prefetch(pagesPointer(index,key_head->fields.extra));
		hnum += ht_chain->dir;
		while (table_block[hnum].fields.space_used)
			{
			prev_head = key_head;
			key_head = &table_block[hnum];
			if (key_head->data.size_and_value > 3) 
				__builtin_prefetch(pagesPointer(index,key_head->fields.extra));
			hnum += ht_chain->dir;
			cb_call(index,prev_head,cb,raw,param);
			}
		}
	inum = table_block[hnum].links_array.links[ht_chain->link_num];
	if (inum == KH_ZERO_REF)
		{
		if (key_head)
			cb_call(index,key_head,cb,raw,param);
		return;
		}
	while (1)
		{
		last_block = (FKeyHeadGeneral *)regionPointer(index,KH_BLOCK_IDX(inum),KH_BLOCK_SIZE);
		hnum = KH_BLOCK_NUM(inum);
		
		while (hnum < KH_BLOCK_LAST)
			{
			prev_head = key_head;
			key_head = &last_block[hnum];
			FAILURE_CHECK(!key_head->fields.space_used,"unused head in chain");
			if (key_head->data.size_and_value > 3)
				__builtin_prefetch(pagesPointer(index,key_head->fields.extra));
			if (prev_head)
				cb_call(index,prev_head,cb,raw,param);
			if (key_head->fields.chain_stop)
				{ cb_call(index,key_head,cb,raw,param); return; }
			hnum++;
			}
		if (last_block[KH_BLOCK_LAST].fields.space_used)
			{ 
			prev_head = key_head;
			key_head = &last_block[KH_BLOCK_LAST];
			FAILURE_CHECK(!key_head->fields.chain_stop,"open chain");
			if (key_head->data.size_and_value > 3) 
				__builtin_prefetch(pagesPointer(index,key_head->fields.extra));
			if (prev_head)
				cb_call(index,prev_head,cb,raw,param);
			cb_call(index,key_head,cb,raw,param);
			return;
			}
		inum = last_block[KH_BLOCK_LAST].links.next;
		}
	}
	
void idx_process_all(FSingSet *index,void *cb,int raw,void *param)
	{
	unsigned i,j;
	
	if (!index->counters)
		{
		for (i = 0; i < index->hashtable_size; i++)
			process_hash_chain(index,i,cb,raw,param);
		return;
		}
	
	for (i = 0; i < COUNTERS_SIZE(index->hashtable_size) - 1; i++)
		{
		if (!index->counters[i]) continue;
		for (j = COUNTER_TO_HASH(i); j < COUNTER_TO_HASH(i+1); j++)
			process_hash_chain(index,j,cb,raw,param);
		}
	if (index->counters[i])
		{
		for (j = COUNTER_TO_HASH(i) ; j < index->hashtable_size; j++)
			process_hash_chain(index,j,cb,raw,param);
		}
	}

int idx_flush(FSingSet *index)
	{
	lck_globalLock(index->lock_set); // Stop diff/intersect marking
	lck_memoryLock(index); // Stop memory modification
	int dumped = cp_flush(index);
	lck_memoryUnlock(index); 
	lck_globalUnlock(index->lock_set); 
	return dumped;
	}

int idx_revert(FSingSet *index)
	{
	int i,rv;
	unsigned old_pcnt;
	
	// We should map all pages before revert
	if (index->pages_fd == -1)
		return ERROR_INTERNAL; // We shoul not be here with UF_NOT_PERSISTENT
	for (i = 0; i < index->head->pcnt; i++)
		if ((index->pages[i] == MAP_FAILED) &&
				(index->pages[i] = (element_type *)mmap(NULL,PAGE_SIZE_BYTES,PROT_WRITE | PROT_READ, MAP_SHARED, index->pages_fd, i * PAGE_SIZE_BYTES)) == MAP_FAILED) 
			return ERROR_INTERNAL;

	lck_globalLock(index->lock_set); // Stop diff/intersect marking
	lck_memoryLock(index); // Stop memory modification
	old_pcnt = index->head->pcnt;
	if (!(rv = cp_revert(index)))
		{
		for (i = (int)index->head->pcnt ; i < old_pcnt ; i++)
			idx_free_page(index,i);
		}
	lck_memoryUnlock(index); 
	lck_globalUnlock(index->lock_set); 

	return rv;
	}

void update_check_data(FSingSet *index,element_type ref,unsigned size,FCheckData *check_data)
	{
	if (size >= MIN_HOLE_SIZE)
		{
		unsigned page_rest = page_rest = PAGE_SIZE - (ref + size) % PAGE_SIZE;
		check_data->busy_general += size;
		if (page_rest < MIN_HOLE_SIZE)
			check_data->busy_general += page_rest;
		return;
		}
	else
		{
		check_data->busy_small += size;
		spec_page_support_count(index,ref >> PAGE_SHIFT,check_data);
		}
	}

int check_element(FSingSet *index,FKeyHead *key_head,FCheckData *check_data)
	{
	unsigned sz = key_head->size, k_size = key_head->size;
	FValueHead *vhead;
	element_type *key_rest = NULL;
	if (key_head->has_value)
		{
		key_rest = regionPointer(index,key_head->extra,sz + VALUE_HEAD_SIZE);
		vhead = (FValueHead *)&key_rest[sz];
		sz += vhead->size_e + VALUE_HEAD_SIZE;
		}
	else if (key_head->size > 1)
		{
		key_rest = regionPointer(index,key_head->extra,sz);
		vhead = NULL;
		}
	if (!key_rest)
		return 0;
	if (k_size)
		{
		if (!key_rest[k_size - 1])
			return idx_set_formatted_error(index,"Key data ended with zero element at %u",key_head->extra),1;
		}
	unsigned page_rest = PAGE_SIZE - key_head->extra % PAGE_SIZE;
	if (page_rest < sz)
		return idx_set_formatted_error(index,"Crosspage data at %u",key_head->extra),1;
	update_check_data(index,key_head->extra,sz,check_data);
	if (vhead && !key_rest[sz - 1])
		return idx_set_formatted_error(index,"Value data ended with zero element at %u",key_head->extra),1;
	return 0;
	}

int check_hash_chain(FSingSet *index,unsigned hash,FCheckData *check_data)
	{
	FKeyHead *key_head = NULL; // Заголовок для удаления (найденный)
	FKeyHeadGeneral *table_block; // Блок в хеш-таблице
	FKeyHeadGeneral *last_block; // Последний блок в цепочке (не в хеш-таблице)
	unsigned inum,hnum;
	FHashTableChain const *ht_chain;
	
	table_block = get_hashtable_entry(index,hash);
	ht_chain = &HT_CHAINS[hash & 1];
	hnum = ht_chain->start;

	while (table_block[hnum].fields.space_used)
		{
		if (hnum == ht_chain->end)
			return idx_set_formatted_error(index,"Unterminated chain in hash table at %u",hash),1;
		key_head = &table_block[hnum].fields;
		if (check_element(index,key_head,check_data))
			return 1;
		hnum += ht_chain->dir;
		}
	inum = table_block[hnum].links_array.links[ht_chain->link_num];
	if (inum == KH_ZERO_REF)
		return 0;
		
	for (;;)
		{
		last_block = (FKeyHeadGeneral *)regionPointer(index,KH_BLOCK_IDX(inum),KH_BLOCK_SIZE);
		hnum = KH_BLOCK_NUM(inum);
		if (hnum == 1 && !last_block[0].fields.space_used && last_block[0].space.reserved)
			check_data->free_headers += KEY_HEAD_SIZE;
		while (hnum < KH_BLOCK_LAST)
			{
			key_head = &last_block[hnum].fields;
			if (!key_head->space_used)
				return idx_set_formatted_error(index,"Unused head in chain at %u",inum),1;
			if (check_element(index,key_head,check_data))
				return 1;
			check_data->busy_headers += KEY_HEAD_SIZE;
			if (key_head->chain_stop)
				{
				if (!last_block[hnum + 1].fields.space_used && last_block[hnum + 1].space.reserved)
					check_data->free_headers += KEY_HEAD_SIZE;
				return 0;
				}
			hnum++;
			}
		if (last_block[KH_BLOCK_LAST].fields.space_used)
			{ // В последнем блоке тоже данные
			key_head = &last_block[KH_BLOCK_LAST].fields;
			if (!key_head->chain_stop)
				return idx_set_formatted_error(index,"Open chain at %u",inum),1;
			if (check_element(index,key_head,check_data))
				return 1;
			check_data->busy_headers += KEY_HEAD_SIZE;
			return 0;
			}
		if (last_block[KH_BLOCK_LAST].links.next == KH_ZERO_REF)
			return idx_set_formatted_error(index,"Open chain at %u",inum),1;
		inum = last_block[KH_BLOCK_LAST].links.next;
		check_data->busy_headers += KEY_HEAD_SIZE;
		}
	return 0;
	}
	
int idx_check_all(FSingSet *index,int reserved)
	{
	unsigned i,j;
	FCheckData check_data = {0};
	
	if (!index->counters)
		{
		for (i = 0; i < index->hashtable_size; i++)
			if (check_hash_chain(index,i,&check_data))
				return 1;
		}
	else
		{
		for (i = 0; i < COUNTERS_SIZE(index->hashtable_size) - 1; i++)
			{
			if (!index->counters[i]) continue;
			for (j = COUNTER_TO_HASH(i); j < COUNTER_TO_HASH(i+1); j++)
				if(check_hash_chain(index,j,&check_data))
					return 1;
			}
		if (index->counters[i])
			{
			for (j = COUNTER_TO_HASH(i) ; j < index->hashtable_size; j++)
				if (check_hash_chain(index,j,&check_data))
					return 1;
			}
		}
	if (	check_free_pages(index,&check_data) ||
			check_small_holes_chains(index,&check_data) || 
			check_general_holes_chains(index,&check_data) || 
			check_head_holes_chains(index,&check_data)	)
		return 1;
	int64_t sum =	(int64_t)check_data.busy_general + (int64_t)check_data.busy_headers + (int64_t)check_data.busy_small +
						(int64_t)check_data.free_general + (int64_t)check_data.free_headers + (int64_t)check_data.free_small +
						(int64_t)check_data.empty + reserved;
	if (sum != (int64_t)index->head->pcnt * PAGE_SIZE)
		return idx_set_formatted_error(index,"Sizes does not match (free/busy), headers  %u / %u, general %u / %u, small %u / %u, empty %d, reserved %d, sum %u (should be %u)",
								check_data.free_headers,check_data.busy_headers,check_data.free_general,check_data.busy_general,check_data.free_small,check_data.busy_small,
								check_data.empty,reserved,(unsigned)sum,index->head->pcnt * PAGE_SIZE),1;
	return 0;	
	}
