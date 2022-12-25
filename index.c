/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <stddef.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
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
#include "codec.h"
#include "utils.h"
#include "keyhead.h"
#include "allocator.h"
#include "filebuf.h"

#define O_NEWFILE (O_RDWR | O_TRUNC | O_CREAT)

const char *FILE_SIGNATURE __attribute__ ((aligned (4))) = "TC01"; // Codec name and major version

// Выделяет и инициализирует структуру

void _init_index(FSingSet *rv,int keep_filenames)
	{
	int i;
	rv->head = MAP_FAILED;
	rv->index_fd = rv->pages_fd = -1;
	rv->disk_index_fd = rv->disk_pages_fd = -1;
	rv->used_cpages = rv->real_cpages = NULL;
	if (!keep_filenames)
		memset(&rv->filenames,0,sizeof(FFileNames));
	rv->protect_lock.whole = 0LL;
	rv->last_error[0] = 0;

	for (i = 0; i < MAX_PAGES; i++)
		{ rv->pages[i] = MAP_FAILED; }

	rv->old_data = NULL;
	}

FSingSet *_alloc_index(void)
	{
	FSingSet *rv = NULL;
	if (posix_memalign((void **)&rv,CACHE_LINE_SIZE,sizeof(FSingSet)))
		return NULL;
	return rv;
	}

static int _create_names(FFileNames *filenames,FSingConfig *config,const char *setname,const char *suffix,unsigned flags)
	{
	if (!suffix) suffix = "";
	int fsize = strlen(setname) + strlen(suffix) + sizeof(".idx"); // ".idx" and terminating 0
	int size = (sizeof(SYSTEM_SHM_PATH) + fsize);
	char *bl;
	if (!(flags & UF_NOT_PERSISTENT))
		{
		bl = config->base_location ? config->base_location : "./";
		size += (strlen(bl) + fsize);
		}
	
	if (!(filenames->index_shm_file = (char *)malloc(size * 2)))
		return 1;
	filenames->pages_shm_file = filenames->index_shm_file + sprintf(filenames->index_shm_file,"%s%s.idx%s",SYSTEM_SHM_PATH,setname,suffix) + 1;
	int psf_size = sprintf(filenames->pages_shm_file,"%s%s.dat%s",SYSTEM_SHM_PATH,setname,suffix) + 1;

	filenames->index_shm = filenames->index_shm_file + sizeof(SYSTEM_SHM_PATH) - 1;
	filenames->pages_shm = filenames->pages_shm_file + sizeof(SYSTEM_SHM_PATH) - 1;

	if (!(flags & UF_NOT_PERSISTENT))
		{
		filenames->index_file = filenames->pages_shm_file + psf_size;
		filenames->pages_file = filenames->index_file + sprintf(filenames->index_file,"%s%s.idx%s",bl,setname,suffix) + 1;
		sprintf(filenames->pages_file,"%s%s.dat%s",bl,setname,suffix);
		}
	return 0;
	}

static inline void _strcpycat(char *dest,const char *src1,unsigned src1len,const char *src2)
	{
	memcpy(dest,src1,src1len);
	strcpy(dest + src1len,src2);
	}

static int _copy_names(FFileNames *old_names,FFileNames *new_names,const char *old_suffix,const char *new_suffix)
	{
	if (!old_suffix) old_suffix = "";
	if (!new_suffix) new_suffix = "";
	int osuflen = strlen(old_suffix), nsuflen = strlen(new_suffix);
	int sbaselen = strlen(old_names->index_shm_file) - osuflen;
	int fbaselen = 0;

	int size = sbaselen + nsuflen + 1;
	if (old_names->index_file)
		{
		fbaselen = strlen(old_names->index_file) - osuflen;
		size += fbaselen + nsuflen + 1;
		}
	if (!(new_names->index_shm_file = (char *)malloc(size * 2)))
		return 1;
	_strcpycat(new_names->index_shm_file,old_names->index_shm_file,sbaselen,new_suffix);
	new_names->pages_shm_file = new_names->index_shm_file + sbaselen + nsuflen + 1;
	_strcpycat(new_names->pages_shm_file,old_names->pages_shm_file,sbaselen,new_suffix);
	new_names->index_shm = new_names->index_shm_file + (old_names->index_shm - old_names->index_shm_file);
	new_names->pages_shm = new_names->pages_shm_file + (old_names->pages_shm - old_names->pages_shm_file);
	if (!old_names->index_file)
		return new_names->index_file = new_names->pages_file = NULL,0;
	new_names->index_file = new_names->pages_shm_file + sbaselen + nsuflen + 1;
	_strcpycat(new_names->index_file,old_names->index_file,fbaselen,new_suffix);
	new_names->pages_file = new_names->index_file + fbaselen + nsuflen + 1;
	_strcpycat(new_names->pages_file,old_names->pages_file,fbaselen,new_suffix);
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

	if ((flags & CF_READER) && (flags & (CF_UNLOAD_ON_CLOSE | CF_KEEP_LOCK)))
		return cnf_set_error(config,"incompatible flags"), NULL;

	if (!(rv = _alloc_index()))
		{ cnf_set_error(config,"not enougth memory for set initialization"); return NULL; }
	_init_index(rv,0);

	if (!setname)
		{
		flags |= UF_NOT_PERSISTENT;
		rv->is_private = 1;
		}
	else 
		{
		rv->is_private = 0;
		if (_create_names(&rv->filenames,config,setname,".tmp",flags))
			{ cnf_set_error(config,"not enougth memory for set initialization"); goto idx_empty_index_fail; }
		// Try to link existing share
		FSingConfig old_config;
		memcpy(&old_config,config,sizeof(FSingConfig));
		old_config.connect_flags = 0; // Remove all flags, we need initial connection with delete possibility
		rv->old_data = idx_link_set(setname,0,config);
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
	
	if (!rv->is_private)
		{
		if ((ifd = rv->index_fd = shm_open(rv->filenames.index_shm,O_NEWFILE | O_EXCL, FILE_PERMISSIONS)) == -1)
			{ cnf_set_formatted_error(config,"can not create share %s",rv->filenames.index_shm); goto idx_empty_index_fail; }
		
		if (file_lock(ifd,LOCK_EX))
			{ cnf_set_error(config,"flock failed"); goto idx_empty_index_fail; }

		if ((pfd = rv->pages_fd = shm_open(rv->filenames.pages_shm,O_NEWFILE, FILE_PERMISSIONS)) == -1)
			{ cnf_set_formatted_error(config,"can not create share %s",rv->filenames.pages_shm); goto idx_empty_index_fail; }

		if (ftruncate(ifd,head_sizes.mem_file_size))
			{ cnf_set_error(config,"can not truncate hash table"); goto idx_empty_index_fail; }
		if (ftruncate(pfd,PAGE_SIZE_BYTES))
			{ cnf_set_error(config,"can not truncate collision table"); goto idx_empty_index_fail; }
		map_flags = MAP_SHARED;

		if (!(flags & UF_NOT_PERSISTENT))
			{
			if ((rv->disk_index_fd = open(rv->filenames.index_file,O_NEWFILE, FILE_PERMISSIONS)) == -1)
				{ cnf_set_formatted_error(config,"can not create file %s",rv->filenames.index_file); goto idx_empty_index_fail; }
			if ((rv->disk_pages_fd = open(rv->filenames.pages_file,O_NEWFILE, FILE_PERMISSIONS)) == -1)
				{ cnf_set_formatted_error(config,"can not create file %s",rv->filenames.pages_file); goto idx_empty_index_fail; }
			}
		}

	if ((index_share = (char *)mmap(NULL,head_sizes.mem_file_size,PROT_WRITE | PROT_READ, map_flags, ifd, 0)) == MAP_FAILED)
		{ cnf_set_error(config,"can not map hash table"); goto idx_empty_index_fail; }

	rv->head = head = (FSetHead *)index_share;
	
	head->sizes = head_sizes;	
	head->signature = *((unsigned *)FILE_SIGNATURE);
	head->hashtable_size = rv->hashtable_size = keys_count;
	head->count = 0;
	head->lock_mode = LM_NONE; // Until initial fill
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

	head->first_pf_spec_page = head->first_empty_page = NO_PAGE;

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
		{
		if (rv->filenames.index_file)
			unlink(rv->filenames.index_file), unlink(rv->filenames.pages_file);
		if (rv->filenames.index_shm)
			shm_unlink(rv->filenames.index_shm), shm_unlink(rv->filenames.pages_shm);
		idx_unlink_set(rv);
		free(rv);
		}
	return NULL;
	}

#define BACKUP_FILE_LINKED 1
#define BACKUP_FILE_MOVED 2

static int _file_relink(FSingSet *kvset)
	{
	unsigned bkstate[4] = {0,0,0,0};
	FFileNames bk_names = {NULL}; // Names of backup files
	FFileNames nnames; // Names of target files
	int i;

	if (_copy_names(&kvset->filenames,&nnames,".tmp",NULL))
		return 1;
	if (kvset->old_data)
		{
		if (_copy_names(&kvset->filenames,&bk_names,".tmp",".bk") || 
				link(kvset->old_data->filenames.index_shm_file,bk_names.index_shm_file))
			goto _file_relink_fail; 
		bkstate[0] = BACKUP_FILE_LINKED;
		if (link(kvset->old_data->filenames.pages_shm_file,bk_names.pages_shm_file))
			goto _file_relink_fail; 
		bkstate[1] = BACKUP_FILE_LINKED;

		if (kvset->old_data->filenames.index_file)
			{
			if (kvset->filenames.index_file)
				{
				if (link(kvset->old_data->filenames.index_file,bk_names.index_file))
					goto _file_relink_fail; 
				bkstate[2] = BACKUP_FILE_LINKED;
				if (link(kvset->old_data->filenames.pages_file,bk_names.pages_file))
					goto _file_relink_fail;
				bkstate[3] = BACKUP_FILE_LINKED;
				}
			else
				{
				if (rename(kvset->old_data->filenames.index_file,bk_names.index_file))
					goto _file_relink_fail; 
				bkstate[2] = BACKUP_FILE_MOVED;
				if (rename(kvset->old_data->filenames.pages_file,bk_names.pages_file))
					goto _file_relink_fail;
				bkstate[3] = BACKUP_FILE_MOVED;
				}
			}
		}

	if (rename(kvset->filenames.index_shm_file,nnames.index_shm_file))
		goto _file_relink_fail;
	bkstate[0] *= 2; // It can be 0 => 0 or BACKUP_FILE_LINKED => BACKUP_FILE_MOVED, not BACKUP_FILE_MOVED => 4
	if (rename(kvset->filenames.pages_shm_file,nnames.pages_shm_file))
		goto _file_relink_fail;
	bkstate[1] *= 2;
	if (kvset->filenames.index_file)
		{
		if (rename(kvset->filenames.index_file,nnames.index_file))
			goto _file_relink_fail;
		bkstate[2] *= 2;  
		if (rename(kvset->filenames.pages_file,nnames.pages_file))
			goto _file_relink_fail;
		bkstate[3] *= 2;
		}

	char *tofree = kvset->filenames.index_shm_file;
	memcpy(&kvset->filenames,&nnames,sizeof(FFileNames));
	free(tofree);

	if (kvset->old_data)
		{
		char *tofree = kvset->old_data->filenames.index_shm_file;
		memcpy(&kvset->old_data->filenames.index_shm_file,&bk_names,sizeof(FFileNames));
		free(tofree);
		}
	for (i = 0; i <  4; i++)
		if (bkstate[i]) unlink(bk_names.names[i]);
	return 0;

_file_relink_fail:
	for (i = 3; i >= 0; i--)
		switch (bkstate[i])
			{
			case BACKUP_FILE_LINKED: unlink(bk_names.pages_file); break;
			case BACKUP_FILE_MOVED: rename(bk_names.pages_file,nnames.pages_file);
			}

	if (nnames.index_shm_file)
		free(nnames.index_shm_file);
	if (bk_names.index_shm_file)
		free(bk_names.index_shm_file);
	return 1;
	}

#undef BACKUP_FILE_LINKED
#undef BACKUP_FILE_MOVED

int idx_creation_done(FSingSet *kvset,unsigned lock_mode)
	{
	switch (kvset->head->lock_mode = lock_mode)
		{
		case LM_PROTECTED:
		case LM_FAST:
			kvset->head->use_spin = 1;
			break;
		case LM_READ_ONLY:
			kvset->read_only = 1;
			break;
		}
	lck_init_locks(kvset);

	if ((kvset->conn_flags & CF_KEEP_LOCK) && lck_manualLock(kvset))
		return 1;

	if (!kvset->is_private && _file_relink(kvset))
		return 1;

	if (kvset->old_data)
		{
		idx_unload_set(kvset->old_data,1);
		kvset->old_data = NULL;
		}

	if (kvset->index_fd != -1) 
		{
		cp_full_flush(kvset);
		file_lock(kvset->index_fd,LOCK_UN);
		close(kvset->index_fd);
		kvset->index_fd = -1;
		if (kvset->conn_flags & CF_READER)
			{
			if (kvset->disk_index_fd != -1) 
				close(kvset->disk_index_fd), kvset->disk_index_fd = -1;
			if (kvset->disk_pages_fd != -1) 
				close(kvset->disk_pages_fd), kvset->disk_pages_fd = -1;
			}
		}
	if (kvset->conn_flags & CF_READER)
		kvset->read_only = 1;
	return 0;
	}
	
static int _link_set(FSingSet *rv,FSingConfig *config)
	{
	FSetHead *head;
	FHeadSizes head_sizes;
	element_type base_head_size = INDEX_HEAD_DISK_PAGES * DISK_PAGE_BYTES;
	element_type counters_size,page_types_size,locks_size;
	char *index_share,*extra_struct;
	unsigned i;
	int share_mode = O_RDWR;
	int file_locked = 0;
	int ifd = -1;

	rv->is_private = 0;
	if ((ifd = shm_open(rv->filenames.index_shm,O_RDWR | O_CREAT, FILE_PERMISSIONS)) == -1)
		{ cnf_set_formatted_error(config,"can not open set %s",rv->filenames.index_shm); goto _link_set_fail; }
	
	// Блочим шару (возможно мы ее только что создали, надо проверить)
	if (file_lock(ifd,LOCK_EX))
		{ cnf_set_error(config,"flock failed"); goto _link_set_fail; }
	
	if (file_read(ifd,&head_sizes,sizeof(FHeadSizes)) != sizeof(FHeadSizes))
		{ // Шары не было, пересоздадим из дисковой версии, если такая есть
		size_t to_read;
		share_mode = O_NEWFILE;
		file_locked = 1;
		
		if ((rv->disk_index_fd = open(rv->filenames.index_file,O_RDWR)) == -1)
			{ cnf_set_formatted_error(config,"can not open file %s",rv->filenames.index_file); goto _link_set_fail; }

		if (file_read(rv->disk_index_fd,&head_sizes,sizeof(FHeadSizes)) != sizeof(FHeadSizes)) // Читаем полный размер из дискового файла
			{ cnf_set_error(config,"can not read full head set size"); goto _link_set_fail; }

		if (ftruncate(ifd,head_sizes.mem_file_size)) // Расширяем в памяти
			{ cnf_set_error(config,"can not truncate hash table"); goto _link_set_fail; }

		if ((index_share = (char *)mmap(NULL,head_sizes.mem_file_size,PROT_WRITE | PROT_READ, MAP_SHARED, ifd, 0)) == MAP_FAILED)
			{ cnf_set_error(config,"can not map hash table"); goto _link_set_fail; }

		rv->head = head = (FSetHead *)index_share;
		head->sizes = head_sizes;
		to_read = base_head_size - sizeof(FHeadSizes);
		// Дочитываем заголовок
		if (file_read(rv->disk_index_fd,&index_share[sizeof(FHeadSizes)],to_read) != to_read)
			{ cnf_set_error(config,"can not read set head from disk"); goto _link_set_fail; }
		if (head->signature != *((unsigned *)FILE_SIGNATURE))
			{ cnf_set_error(config,"incompatible set format"); goto _link_set_fail; }
		if (head->use_flags & UF_NOT_PERSISTENT)
			{ cnf_set_error(config,"set with disk copy has NOT_PERSISTENT flag"); goto _link_set_fail; }
		if (head->wip)
			{ cnf_set_error(config,"disk copy of set is broken"); goto _link_set_fail; }
		}
	else
		{
		file_lock(ifd,LOCK_UN);

		if ((index_share = (char *)mmap(NULL,head_sizes.mem_file_size,PROT_WRITE | PROT_READ, MAP_SHARED, ifd, 0)) == MAP_FAILED)
			{ cnf_set_error(config,"can not map hash table"); goto _link_set_fail; }
		rv->head = head = (FSetHead *)index_share;
		if (head->signature != *((unsigned *)FILE_SIGNATURE))
			{ cnf_set_error(config,"incompatible share format"); goto _link_set_fail; }
		if (!(head->use_flags & UF_NOT_PERSISTENT))
			{
			if ((rv->disk_index_fd = open(rv->filenames.index_file,O_RDWR)) == -1)
				{ cnf_set_formatted_error(config,"can not open file %s",rv->filenames.index_file); goto _link_set_fail; }
			}
		else
			rv->filenames.index_file = rv->filenames.pages_file = NULL;
		}
	rv->read_only = (head->lock_mode == LM_READ_ONLY) ? 1 : 0;
	switch(head->lock_mode)
		{
		case LM_READ_ONLY:
		case LM_NONE:
			if (rv->conn_flags & CF_KEEP_LOCK)
				{ cnf_set_error(config,"incompatible flags and lock mode"); goto _link_set_fail; }
			break;
		}

	rv->hashtable_size = head->hashtable_size;
	extra_struct = index_share + head_sizes.disk_file_size;
	locks_size = sizeof(FLockSet) + sizeof(uint64_t) * (rv->hashtable_size / 128 + ((rv->hashtable_size % 128) ? 1 : 0));
	index_share += base_head_size;

	if (head->use_flags & UF_PAGE_TYPES)
		{
		page_types_size = PAGE_TYPES_DISK_PAGES * DISK_PAGE_BYTES;
		rv->page_types = (unsigned char *)index_share;
		if (file_locked && file_read(rv->disk_index_fd,index_share,page_types_size) != page_types_size)
			{ cnf_set_error(config,"can not read page types from disk"); goto _link_set_fail; }
		index_share += page_types_size;
		}
	else
		page_types_size = 0, rv->page_types = NULL;

	if (head->use_flags & UF_COUNTERS)
		{
		counters_size = align_up(COUNTERS_SIZE(head->hashtable_size) * sizeof(unsigned),DISK_PAGE_BYTES);
		rv->counters = (unsigned *)index_share;
		if (file_locked && file_read(rv->disk_index_fd,index_share,counters_size) != counters_size)
			{ cnf_set_error(config,"can not read counters from disk"); goto _link_set_fail; }
		index_share += counters_size;
		}
	else
		counters_size = 0, rv->counters = NULL;

	rv->hash_table = (FKeyHeadGeneral *)index_share;
	
	rv->lock_set = (FLockSet *)extra_struct;
	extra_struct += locks_size;

	// Открывем шару и файл страниц
	if ((rv->pages_fd = shm_open(rv->filenames.pages_shm,share_mode, FILE_PERMISSIONS)) == -1)
		{ cnf_set_formatted_error(config,"can not open share %s",rv->filenames.pages_shm); goto _link_set_fail; }

	if (!(head->use_flags & UF_NOT_PERSISTENT))
		{
		rv->real_cpages = rv->used_cpages = (FChangedPages *)extra_struct;
		if ((rv->disk_pages_fd = open(rv->filenames.pages_file,O_RDWR, FILE_PERMISSIONS)) == -1)
			{ cnf_set_formatted_error(config,"can not open file %s",rv->filenames.pages_file); goto _link_set_fail; }
		}
	
	if (file_locked)
		{
		if (ftruncate(rv->pages_fd,head->pcnt * PAGE_SIZE_BYTES)) 
			{ cnf_set_error(config,"can not truncate collision table"); goto _link_set_fail; }

		cp_init(rv->used_cpages,page_types_size,counters_size);
		if (rv->conn_flags & CF_FULL_LOAD)
			{
			element_type hash_table_size = align_up((head->hashtable_size + 1) / 2 * sizeof(FKeyHeadGeneral) * KEYHEADS_IN_BLOCK,DISK_PAGE_BYTES);
			if (file_read(rv->disk_index_fd,index_share,hash_table_size) != hash_table_size)
				{ cnf_set_error(config,"can not read hash table from disk"); goto _link_set_fail; }
			cp_mark_hashtable_loaded(rv);

			if ((rv->pages[0] = (element_type *)mmap(NULL,head_sizes.mem_file_size,PROT_WRITE | PROT_READ, MAP_SHARED, rv->pages_fd, 0)) == MAP_FAILED)
				{ cnf_set_error(config,"can not map collision table"); goto _link_set_fail; }
			for (i = 1; i < head->pcnt; i++)
				rv->pages[i] = rv->pages[0] + i * PAGE_SIZE;
			cp_mark_pages_loaded(rv);
			}
		lck_init_locks(rv);
		}

   int code;
	if ((rv->conn_flags & CF_KEEP_LOCK) && (code = lck_manualLock(rv)))
		{ cnf_set_formatted_error(config,"can not set manual lock, code %d",code); goto _link_set_fail; }
		
	if (file_locked)
		file_lock(ifd,LOCK_UN), file_locked = 0;

	close(ifd);
	if (rv->conn_flags & CF_READER)
		{
		if (rv->disk_index_fd != -1) 
			close(rv->disk_index_fd), rv->disk_index_fd = -1;
		if (rv->disk_pages_fd != -1) 
			close(rv->disk_pages_fd), rv->disk_pages_fd = -1;
		rv->read_only = 1;
		}
	return 0;
	
_link_set_fail:
	if (file_locked && ifd != -1)
		{
		shm_unlink(rv->filenames.index_shm); // Что-то пошло не так, удалим основную шару, т.к. в других файлах может быть мусор
		file_lock(ifd,LOCK_UN);
		}
	if (ifd != -1) close(ifd);
	idx_unlink_set(rv);
	return 1;
	}

FSingSet *idx_link_set(const char *setname,unsigned flags,FSingConfig *config)
	{
	FSingSet *rv = NULL;

	if (!setname)
		return cnf_set_error(config,"can not link to unnamed set"), NULL;

	if ((flags & CF_READER) && (flags & (CF_UNLOAD_ON_CLOSE | CF_KEEP_LOCK)))
		return cnf_set_error(config,"incompatible flags"), NULL;

	if (!(rv = _alloc_index()))
		return cnf_set_error(config,"not enougth memory for index struct"), NULL;
	_init_index(rv,0);

	if (_create_names(&rv->filenames,config,setname,NULL,0))
		{ 
		cnf_set_error(config,"not enougth memory for set initialization"); 
		free(rv);
		return NULL;
		} 

	if (flags & CF_READER)
		flags |= CF_FULL_LOAD;
	rv->conn_flags = (config->connect_flags | flags) & CF_MASK;
	if (_link_set(rv,config))
		return free(rv),NULL;
	return rv;
	}

int idx_relink_set(FSingSet *index)
	{
	FSingSet bk_index __attribute__((aligned(CACHE_LINE_SIZE)));
	memcpy(&bk_index,index,sizeof(FSingSet));
	_copy_names(&index->filenames,&bk_index.filenames,NULL,NULL);
	_init_index(index,1);
   FSingConfig config = {0};

	if (!_link_set(index,&config))
		{
		idx_unlink_set(&bk_index);
		return 0;
		}
	memcpy(index,&bk_index,sizeof(FSingSet));
	return 1;
	}
	
void idx_unlink_set(FSingSet *index)
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
	if (index->filenames.index_shm_file) free(index->filenames.index_shm_file);
	}

int idx_unload_set(FSingSet *kvset,int del_from_disk)
	{
	int res = 0;
	if (lck_manualPresent(kvset))
		{ // if we have manual lock - revert to disk copy if present and keep the lock
		if (kvset->head->lock_mode == LM_PROTECTED)
			lck_protectWait(kvset);
		if (!(kvset->head->use_flags & UF_NOT_PERSISTENT))
			res = idx_revert(kvset);
		}
	else if (kvset->head->lock_mode != LM_NONE && kvset->head->lock_mode != LM_READ_ONLY)
		res = lck_manualLock(kvset); // Otherwise set manual lock
	if (res)
		return res;

	if (del_from_disk && kvset->filenames.index_file)
		{
		unlink(kvset->filenames.index_file);
		unlink(kvset->filenames.pages_file);
		}
	if (kvset->filenames.index_shm)
		{
		shm_unlink(kvset->filenames.index_shm);
		shm_unlink(kvset->filenames.pages_shm);
		}
	lck_deinit_locks(kvset); // Deleting mutex and removing all locks
	idx_unlink_set(kvset);
	free(kvset);
	return 0;
	}

////////////////////////////////////////////////////////////////////////////////////////////////
///////                    Операции с ключами и данными
////////////////////////////////////////////////////////////////////////////////////////////////

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

// return 1 if keys are equal, 0 otherwise or if we have left allocated memory. 
// return phantom (deleted) keys only if tdata->use_phantom is setted
static inline int _compare_keys_R(FSingSet *index,FKeyHeadGeneral old_key,FTransformData *tdata)
	{
	if ((old_key.links.next ^ tdata->head.links.next) & 0xFFFFFC7E) // (key1->size != key2->size || key1->data0 != key2->data0)
		return 0; 
	if ((index->head->use_flags & UF_PHANTOM_KEYS) && old_key.fields.diff_or_phantom_mark && !tdata->use_phantom)
		return 0;
	element_type *old_data;
	unsigned size = old_key.fields.size;
	if (!size) return 1;
	if (size > 1)
		{
		unsigned pos = 0;
		if (!(old_data = regionPointerNoError(index,old_key.fields.extra,size))) 
			return 0;
		while (pos < size && old_data[pos] == tdata->key_rest[pos]) pos++;
		return (pos >= size) ? 1 : 0;
		}
	if (!old_key.fields.has_value)
		return (old_key.fields.extra == tdata->key_rest[0]) ? 1 : 0;
	if (!(old_data = pagesPointerNoError(index,old_key.fields.extra))) 
		return 0;
	return (*old_data == tdata->key_rest[0]) ? 1 : 0;
	}

// returns found keyhead with obtained rlock or 0 with releases lock if nothing found
// if needed phantom value, can return key with only normal value, it should be checked later
static uint64_t _key_search_R(FSingSet *index,FTransformData *tdata,FReaderLock *rlock)
	{
	FKeyHeadGeneral *hblock;
	FKeyHeadGeneral rv;
	FHashTableChain const *ht_chain;
	unsigned inum,hnum;
	
_key_search_R_begin:
	lck_readerLock(index->lock_set,rlock);

	hblock = get_hashtable_entry(index,tdata->hash);
	ht_chain = &HT_CHAINS[tdata->hash & 1];

	for (hnum = ht_chain->start;rv.whole = __atomic_load_n(&hblock[hnum].whole,__ATOMIC_RELAXED),rv.fields.space_used;hnum += ht_chain->dir)
		if (_compare_keys_R(index,rv,tdata)) 
			return rv.whole;

	if ((inum = rv.links_array.links[ht_chain->link_num]) == KH_ZERO_REF)
		return lck_readerUnlock(index->lock_set,rlock),0LL; 

_key_search_R_next_block:
	if (!lck_readerCheck(index->lock_set,rlock)) 
		goto _key_search_R_begin; // Проверяем свою блокировку, чтобы не зациклиться
	CHECK_PAGE_TYPE(index,inum,PT_HEADERS);
	if (!(hblock = (FKeyHeadGeneral *)regionPointerNoError(index,KH_BLOCK_IDX(inum),KH_BLOCK_SIZE))) // Мы вышли за пределы исп. памяти, читаем фигню, повторим
		{ 
		lck_readerUnlock(index->lock_set,rlock); 
		goto _key_search_R_begin; 
		}
	for(hnum = KH_BLOCK_NUM(inum);hnum < KH_BLOCK_LAST;hnum++)
		{
		rv.whole = __atomic_load_n(&hblock[hnum].whole,__ATOMIC_RELAXED);
		if (_compare_keys_R(index,rv,tdata)) 
			return rv.whole;
		if (rv.fields.chain_stop) // Дошли до конца цепочки, ничего не найдено
			return lck_readerUnlock(index->lock_set,rlock),0LL; 
		}
	rv.whole = __atomic_load_n(&hblock[KH_BLOCK_LAST].whole,__ATOMIC_RELAXED);
	if (!rv.space.space_used)
		{ 
		inum = rv.links.next; 
		goto _key_search_R_next_block;
		}
	FAILURE_CHECK(!rv.fields.chain_stop,"open chain"); // Для валидного состояния данные в последнем блоке должны завершать цепочку
	if (_compare_keys_R(index,rv,tdata)) 
		return rv.whole;
	return lck_readerUnlock(index->lock_set,rlock),0LL; 
	}

// Ищем наличие одиночного ключа в таблице
int idx_key_search(FSingSet *index,FTransformData *tdata,FReaderLock *rlock)
	{
	FKeyHeadGeneral found;
	element_type *key_rest;
	int rv;
	do
		{
		if (!(found.whole = _key_search_R(index,tdata,rlock)))
			return RESULT_KEY_NOT_FOUND;
		rv = 0;
		if (tdata->use_phantom && !found.fields.diff_or_phantom_mark)
			{ // If we look for phantom, but there are normal value
			if (!found.fields.has_value)
				{ rv = RESULT_KEY_NOT_FOUND; continue; }
			if (!(key_rest = pagesPointerNoError(index,found.fields.extra)))
				continue;
			FValueHead *value_head = (FValueHead *)&key_rest[found.fields.size];
			if (!value_head->phantom)
				rv = RESULT_KEY_NOT_FOUND;
			}
		} 
	while (!lck_readerUnlock(index->lock_set,rlock));
	return rv;
	}

int idx_key_get(FSingSet *index,FTransformData *tdata,FReaderLock *rlock,void *value_dst,unsigned *value_dst_size)
	{
	FKeyHeadGeneral found;
	unsigned vsrc_size,tocpy;
	int rv;
	element_type *key_rest;
	FValueHead *value_head;
	
	do
		{
		if (!(found.whole = _key_search_R(index,tdata,rlock)))
			return *value_dst_size = 0,RESULT_KEY_NOT_FOUND;
		if (!found.fields.has_value)
			{ vsrc_size = 0; rv = (tdata->use_phantom && !found.fields.diff_or_phantom_mark) ? RESULT_KEY_NOT_FOUND : 0; continue; }
		if (!(key_rest = pagesPointerNoError(index,found.fields.extra)))
			{ vsrc_size = 0; rv = 0; continue; }

		value_head = (FValueHead *)&key_rest[found.fields.size];
		if (tdata->use_phantom && !found.fields.diff_or_phantom_mark)
			{
			if (!value_head->phantom)
				{ vsrc_size = 0; rv = RESULT_KEY_NOT_FOUND; continue; }
			value_head = (FValueHead *)VALUE_PHANTOM_HEAD(value_head);
			}
		vsrc_size = VALUE_SIZE_BYTES(value_head);
		if (vsrc_size > *value_dst_size)
			tocpy = *value_dst_size, rv = RESULT_SMALL_BUFFER;
		else
			tocpy = vsrc_size,rv = 0;
		if (tocpy)
			memcpy(value_dst,(void *)&value_head[1],tocpy);
		} 
	while (!lck_readerUnlock(index->lock_set,rlock));
	*value_dst_size = vsrc_size;
	return rv;
	}

int idx_key_compare(FSingSet *index, FTransformData *tdata, FReaderLock *rlock, const void *value_cmp, unsigned value_cmp_size)
	{
	FKeyHeadGeneral found;
	unsigned vsrc_size;
	element_type *key_rest;
	FValueHead *value_head;
	int rv;

	do
		{
		if (!(found.whole = _key_search_R(index,tdata,rlock)))
			return RESULT_KEY_NOT_FOUND;
		if (!found.fields.has_value)
			{ rv =  (tdata->use_phantom && !found.fields.diff_or_phantom_mark) ? RESULT_KEY_NOT_FOUND : (value_cmp_size ? 0 : RESULT_VALUE_DIFFER); continue; }
	
		key_rest = pagesPointerNoError(index,found.fields.extra);
		if (!key_rest)
			{ rv = 0; continue; }

		value_head = (FValueHead *)&key_rest[found.fields.size];
		if (tdata->use_phantom && !found.fields.diff_or_phantom_mark)
			{
			if (!value_head->phantom)
				{ rv = RESULT_KEY_NOT_FOUND; continue; }
			value_head = (FValueHead *)VALUE_PHANTOM_HEAD(value_head);
			}
		vsrc_size = VALUE_SIZE_BYTES(value_head);

		if (vsrc_size != value_cmp_size || memcmp(value_cmp,(void *)&value_head[1],value_cmp_size))
			rv = RESULT_VALUE_DIFFER;
		else
			rv = 0;
		} 
	while (!lck_readerUnlock(index->lock_set,rlock));
	return rv;
	}

int idx_key_get_cb(FSingSet *index,FTransformData *tdata,FReaderLock *rlock,CSingValueAllocator vacb,void **value,unsigned *vsize)
	{
	FKeyHeadGeneral found;
	unsigned vsrc_size;
	void *value_dst;
	element_type *key_rest;
	FValueHead *value_head;
	int rv;
	
	do
		{
		if (!(found.whole = _key_search_R(index,tdata,rlock)))
			return *value=NULL,*vsize = 0,RESULT_KEY_NOT_FOUND;

		if (!found.fields.has_value || !(key_rest = pagesPointerNoError(index,found.fields.extra)))
			{ vsrc_size = 0; value_dst = NULL; rv =  (tdata->use_phantom && !found.fields.diff_or_phantom_mark) ? RESULT_KEY_NOT_FOUND : 0; continue; }

		value_head = (FValueHead *)&key_rest[found.fields.size];
		if (tdata->use_phantom && !found.fields.diff_or_phantom_mark)
			{
			if (!value_head->phantom)
				{ vsrc_size = 0; value_dst = NULL; rv = RESULT_KEY_NOT_FOUND; continue; }
			value_head = (FValueHead *)VALUE_PHANTOM_HEAD(value_head);
			}
		rv = 0;
		vsrc_size = VALUE_SIZE_BYTES(value_head);
		if (!vsrc_size)
			value_dst = NULL;
		else
			{
			if (!(value_dst = vacb(vsrc_size)))
				return lck_readerUnlock(index->lock_set,rlock),ERROR_NO_MEMORY;
			memcpy(value_dst,(void *)&value_head[1],vsrc_size);
			}
		} 
	while (!lck_readerUnlock(index->lock_set,rlock));
	*vsize = vsrc_size;
	*value = value_dst;
	return rv;
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

static inline void _atomic_copy_extra(FKeyHeadGeneral *key_head,FKeyHead *new_data)
	{
	FKeyHead tmp_key_head = key_head->fields;
	tmp_key_head.has_value = new_data->has_value;
	tmp_key_head.extra = new_data->extra;
	key_head->fields = tmp_key_head;
	}

// returns 1 if keys are equal, 0 otherwise. Return phantom (deleted) keys too
static inline int _compare_keys_W(FSingSet *index,FKeyHeadGeneral old_key,FTransformData *tdata)
	{
	if ((old_key.links.next ^ tdata->head.links.next) & 0xFFFFFC7E) // (key1->size != key2->size || key1->data0 != key2->data0)
		return 0; 
	element_type *old_data;
	unsigned size = old_key.fields.size;
	if (!size) return 1;
	if (size > 1)
		{
		unsigned pos = 0;
		old_data = regionPointer(index,old_key.fields.extra,size);
		while (pos < size && old_data[pos] == tdata->key_rest[pos]) pos++;
		return (pos >= size) ? 1 : 0;
		}
	if (!old_key.fields.has_value)
		return (old_key.fields.extra == tdata->key_rest[0]) ? 1 : 0;
	old_data = pagesPointer(index,old_key.fields.extra); 
	return (*old_data == tdata->key_rest[0]) ? 1 : 0;
	}

// Replacing value with data from tdata
static int _alloc_and_set_rest(FSingSet *index,FTransformData *tdata)
	{
	unsigned sz = tdata->head.data.size_and_value >> 1,vsize = 0;
	if (sz <= 1) return 1;
	FValueHeadGeneral value_head = {0};
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
			value_head.fields.size_e = size_e;
		vsize = size_e + VALUE_HEAD_SIZE;
		}
	element_type *key_rest = idx_general_alloc(index,sz + vsize,&tdata->head.fields.extra);
	if (!key_rest)	return 0;
	memcpy(key_rest,tdata->key_rest,sz * ELEMENT_SIZE);
	if (!vsize) return 1;
	key_rest[sz] = value_head.whole;
	memcpy(&key_rest[sz + VALUE_HEAD_SIZE],(element_type *)tdata->value_source,tdata->value_size);
	if (value_head.fields.extra_bytes)
		((char *)(&key_rest[sz + vsize]))[-1] = 0xFF;
	return 1;
	}

// Replacing value with data from tdata and appending data from phantom_value as a phantom
// phantom_value is a phantom or value without phantom and always has final non-zero byte
static int _alloc_and_set_rest_with_phantom(FSingSet *index,FTransformData *tdata,FValueHeadGeneral *phantom_value)
	{
	unsigned sz = tdata->head.data.size_and_value >> 1,vsize = 0,extra_bytes = 0;
	FValueHeadGeneral value_head = {0},phantom_head = *phantom_value;
	phantom_head.fields.phantom = 1;

	if (sz >= 64) // Если есть значение - убираем его бит и добавляем размер значения
		{
		sz -= 64;
		unsigned size_e = tdata->value_size / ELEMENT_SIZE;
		unsigned rest = tdata->value_size % ELEMENT_SIZE;
		if (rest)
			value_head.fields.size_e = ++size_e, extra_bytes = 4 - rest;
		else
			value_head.fields.size_e = size_e;
		vsize = size_e;
		}
	else
		tdata->head.fields.has_value = 1;  // Если значения нет, добавляем его

	vsize += VALUE_HEAD_SIZE; // There will be two value heads even if first value is NULL
	value_head.fields.phantom = 1;
	value_head.fields.size_e += phantom_head.fields.size_e + VALUE_HEAD_SIZE;
	value_head.fields.extra_bytes = extra_bytes + phantom_head.fields.size_e * ELEMENT_SIZE + sizeof(FValueHead);
	element_type *key_rest;
	if (!(key_rest = idx_general_alloc(index,sz + vsize + phantom_head.fields.size_e + VALUE_HEAD_SIZE,&tdata->head.fields.extra)))
		return 0;
	memcpy(key_rest,tdata->key_rest,sz * ELEMENT_SIZE);
	key_rest[sz] = value_head.whole;
	memcpy(&key_rest[sz + VALUE_HEAD_SIZE],tdata->value_source,tdata->value_size);
	key_rest[sz+vsize] = phantom_head.whole;
	memcpy(&key_rest[sz+vsize+1],&phantom_value[VALUE_HEAD_SIZE],phantom_head.fields.size_e * ELEMENT_SIZE);
	return 1;
	}

// append value from tdata as phantom to existing key
static int _append_phantom(FSingSet *index,FKeyHeadGeneral *existing_key,FTransformData *tdata,uint32_t allowed)
	{
	FKeyHeadGeneral key_head = *existing_key;
	unsigned sz = key_head.data.size_and_value >> 1; // Размер остатка ключа
	FValueHeadGeneral value_head;
	element_type *key_rest = NULL;   // Остаток ключа
	element_type *value_rest = NULL; // Тело значения

	if (sz >= 64)
		{
		sz -= 64;
		key_rest = pagesPointer(index,key_head.fields.extra);
		value_head.whole = key_rest[sz];
		if (value_head.fields.phantom)
			{
			if (!(allowed & KS_DELETED))
				return KS_PRESENT;
			}
		else if (!(allowed & KS_ADDED))
			return KS_SUCCESS;
		value_rest = &key_rest[sz + VALUE_HEAD_SIZE];
		tdata->old_key_rest = key_head.fields.extra;
		tdata->old_key_rest_size = sz + value_head.fields.size_e + VALUE_HEAD_SIZE;
		if (value_head.fields.extra_bytes >= ELEMENT_SIZE)
			{ // There are phantom or extra element at the end
			unsigned toremove = value_head.fields.extra_bytes >> LOG_BIN_MACRO(ELEMENT_SIZE);
			value_head.fields.size_e -= toremove;
			value_head.fields.extra_bytes -= toremove * ELEMENT_SIZE;
			}
		}
	else
		{ // Если значения нет, добавляем его
		if (!(allowed & KS_ADDED))
			return KS_SUCCESS;
		value_head.whole = 0;
		key_head.fields.has_value = 1;
		switch (sz)
			{
			case 0: break;
			case 1:
				key_rest = &existing_key->fields.extra;
				break;
			default:
				key_rest = pagesPointer(index,key_head.fields.extra);
				tdata->old_key_rest = key_head.fields.extra;
				tdata->old_key_rest_size = sz;
			}
		}
	value_head.fields.phantom = 1;

	FValueHeadGeneral phantom_head = {0};
	unsigned ph_sz = tdata->head.data.size_and_value >> 1;
	phantom_head.fields.phantom = 1;
	if (ph_sz >= 64) // Если есть значение - убираем его бит и добавляем размер значения
		{
		ph_sz -= 64;
		unsigned size_e = tdata->value_size / ELEMENT_SIZE;
		unsigned rest = tdata->value_size % ELEMENT_SIZE;
		if (rest || !tdata->value_source[tdata->value_size - 1])
			{ // Не делится нацело или значение заканчивается на нулевой байт
			phantom_head.fields.size_e = ++size_e;
			phantom_head.fields.extra_bytes = 4 - rest;
			}
		else
			phantom_head.fields.size_e = size_e;
		}
	element_type oldvsize = value_head.fields.size_e;
	value_head.fields.size_e += phantom_head.fields.size_e + VALUE_HEAD_SIZE;
	value_head.fields.extra_bytes += (phantom_head.fields.size_e + VALUE_HEAD_SIZE) * ELEMENT_SIZE;

	element_type *rest;
	lck_memoryLock(index); // Блокируем операции с памятью
	if (!(rest = idx_general_alloc(index,sz + VALUE_HEAD_SIZE + value_head.fields.size_e,&key_head.fields.extra)))
		return lck_memoryUnlock(index),ERROR_NO_SET_MEMORY;

	memcpy(rest,key_rest,sz * ELEMENT_SIZE);
	rest[sz] = value_head.whole;
	if (oldvsize)
		memcpy(&rest[sz + VALUE_HEAD_SIZE],value_rest,oldvsize * ELEMENT_SIZE);
	rest[sz + VALUE_HEAD_SIZE + oldvsize] = phantom_head.whole;
	if (tdata->value_size)
		memcpy(&rest[sz + VALUE_HEAD_SIZE * 2 + oldvsize],tdata->value_source,tdata->value_size);

	if (phantom_head.fields.extra_bytes)
		((char *)(&rest[sz + VALUE_HEAD_SIZE + value_head.fields.size_e]))[-1] = 0xFF;
	
	existing_key->whole = key_head.whole;
	return KS_CHANGED;
	}

// Replacing value with data from phantom_value as a phantom key. Returns 1 on success, 0 on failure
static int _alloc_and_set_phantom(FSingSet *index,FTransformData *tdata,FValueHeadGeneral *phantom_value)
	{
	unsigned sz = tdata->head.fields.size;
	unsigned vsize = phantom_value->fields.size_e;
	if (!vsize)
		{
		tdata->head.fields.has_value = 0;
		if (sz <= 1) return 1;
		}
	else
		{
		tdata->head.fields.has_value = 1;
		vsize += VALUE_HEAD_SIZE;
		}
	element_type *key_rest;
	if (!(key_rest = idx_general_alloc(index,sz + vsize,&tdata->head.fields.extra)))
		return 0;
	memcpy(key_rest,tdata->key_rest,sz * ELEMENT_SIZE);
	if (!vsize) 
		return 1;
	FValueHeadGeneral phval = *phantom_value;
	phval.fields.phantom = 0;
	key_rest[sz] = phval.whole;
	memcpy(&key_rest[sz+VALUE_HEAD_SIZE],&phantom_value[VALUE_HEAD_SIZE],phantom_value->fields.size_e * ELEMENT_SIZE);
	return 1;
	}

static inline int _mark_value(FSingSet *index,FKeyHeadGeneral *old_key_head,FTransformData *tdata)
	{
	int rv = old_key_head->fields.diff_or_phantom_mark ^ tdata->head.fields.diff_or_phantom_mark;
	old_key_head->fields.diff_or_phantom_mark ^= rv; // Мы под блокировкой цепочки
	return rv * KS_MARKED;
	}

// Compare existing value with tdata, if differ set old value pointer and size in tdata and returns KS_DIFFER | KS_SUCCESS or returns KS_PRESENT if they are equal
static int _compare_value(FSingSet *index,FKeyHeadGeneral *old_key_head,FTransformData *tdata)
	{
	unsigned old_sz = tdata->head.fields.size;
	if (!old_key_head->fields.has_value)
		{
		if (!tdata->head.fields.has_value) 
			return KS_PRESENT;
		if (old_sz > 1)
			{
			tdata->old_key_rest = old_key_head->fields.extra; // There are no old value, just key body
			tdata->old_key_rest_size = old_sz;
			}
		return KS_DIFFER | KS_SUCCESS;
		}
	element_type *old_data = pagesPointer(index,old_key_head->fields.extra);
	FValueHeadGeneral *old_vhead = (FValueHeadGeneral *)&old_data[old_sz];
	unsigned old_vsize = VALUE_SIZE_BYTES(&old_vhead->fields);
	element_type *old_value = &old_data[old_sz + VALUE_HEAD_SIZE];
	if (tdata->head.fields.has_value && tdata->value_size == old_vsize
			&& !memcmp(old_value,tdata->value_source,tdata->value_size))
		return KS_PRESENT;
	old_sz += old_vhead->fields.size_e + VALUE_HEAD_SIZE;
	tdata->old_key_rest = old_key_head->fields.extra;
	tdata->old_key_rest_size = old_sz;
	tdata->old_value_size = old_vsize;
	tdata->old_value = old_value;
	return KS_DIFFER | KS_SUCCESS;
	}

static int _replace_value(FSingSet *index,FKeyHeadGeneral *old_key_head,FTransformData *tdata)
	{
	FORMATTED_LOG_OPERATION("key %s already exists\n",tdata->key_source);
	int rv = _compare_value(index,old_key_head,tdata);
	if (!(rv & KS_DIFFER))
		return rv;
	lck_memoryLock(index); // Блокируем операции с памятью
	if (!_alloc_and_set_rest(index,tdata))
		return lck_memoryUnlock(index),ERROR_NO_SET_MEMORY;
	_atomic_copy_extra(old_key_head,&tdata->head.fields);
	return rv | KS_CHANGED;
	}

// replace value in set with phantom keys.
static int _replace_phantom_value(FSingSet *index,FKeyHeadGeneral *old_key_head,FTransformData *tdata, int marked)
	{
	int rv = marked;
	FORMATTED_LOG_OPERATION("key %s already exists\n",tdata->key_source);
	FValueHeadGeneral *phantom_value = NULL;
	FValueHeadGeneral empty = {0};
	if (!marked)
		{  
		if (tdata->head.fields.diff_or_phantom_mark)
			return KS_SUCCESS; // Old deleted, new deleted - nothing to do
		// Old normal, new normal
		rv = _compare_value(index,old_key_head,tdata);
		if (!(rv & KS_DIFFER))
			return rv; // Values are equal, nothing to do
		goto _replace_phantom_value_replace; // keep old phantom or normal value as phantom
		}
	else
		{ 
		if (!tdata->head.fields.diff_or_phantom_mark) // old deleted, new normal
			goto _replace_phantom_value_replace;
		else
			{ // old normal, new deleted (deleting value and keeping oldest as phantom)
			if (!old_key_head->fields.has_value)
				return KS_SUCCESS; // Nothnig to do, old NULL normal became pure phantom
			element_type *old_data = pagesPointer(index,old_key_head->fields.extra);
			FValueHeadGeneral *old_vhead = (FValueHeadGeneral *)&old_data[old_key_head->fields.size];
			if (!old_vhead->fields.phantom)
				return KS_SUCCESS; // Nothing to do, old normal became pure phantom
			phantom_value = (FValueHeadGeneral *)VALUE_PHANTOM_HEAD(&old_vhead->fields);
			// Converting old phantom to normal form (it will be phantom because of bit in header)
			lck_memoryLock(index); 
			if (!_alloc_and_set_phantom(index,tdata,phantom_value))
				return lck_memoryUnlock(index),ERROR_NO_SET_MEMORY;

			unsigned old_sz = tdata->head.fields.size + old_vhead->fields.size_e + VALUE_HEAD_SIZE;
			tdata->old_key_rest = old_key_head->fields.extra;
			tdata->old_key_rest_size = old_sz;
			// We don't set old value in tdata, because phantom sets does not allow diff operations

			_atomic_copy_extra(old_key_head,&tdata->head.fields);
			return rv | KS_CHANGED;
			}
		}

_replace_phantom_value_replace:
	if (old_key_head->fields.has_value)
		{
		element_type *old_data = pagesPointer(index,old_key_head->fields.extra);
		FValueHeadGeneral *old_vhead = (FValueHeadGeneral *)&old_data[old_key_head->fields.size];
		if (!old_vhead->fields.phantom)
			phantom_value = old_vhead;
		else
			phantom_value = (FValueHeadGeneral *)VALUE_PHANTOM_HEAD(&old_vhead->fields);

		unsigned old_sz = tdata->head.fields.size + old_vhead->fields.size_e + VALUE_HEAD_SIZE;
		tdata->old_key_rest = old_key_head->fields.extra;
		tdata->old_key_rest_size = old_sz;
		// We don't set old value in tdata, because phantom sets does not allow diff operations
		}
	else
		phantom_value = &empty;

	lck_memoryLock(index); // Блокируем операции с памятью
	if (!_alloc_and_set_rest_with_phantom(index,tdata,phantom_value))
		return lck_memoryUnlock(index),ERROR_NO_SET_MEMORY;
	_atomic_copy_extra(old_key_head,&tdata->head.fields);
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

// This call is for diff only, we can skip phantom related work
int idx_key_try_lookup(FSingSet *index,FTransformData *tdata)
	{
	FKeyHeadGeneral *hblock;
	FHashTableChain const *ht_chain;
	unsigned hnum;

	hblock = get_hashtable_entry(index,tdata->hash);
	ht_chain = &HT_CHAINS[tdata->hash & 1];
	hnum = ht_chain->start;
	int rv = 0;

	while (hblock[hnum].fields.space_used)
		{
		if (_compare_keys_W(index,hblock[hnum],tdata)) 
			{
			rv = _mark_value(index,&hblock[hnum],tdata);
			rv |= _compare_value(index,&hblock[hnum],tdata);
			if (rv & KS_MARKED)
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
	return KS_SUCCESS; // We don't set KS_MARKED since key was not added
	}

static inline int _key_set_found(FSingSet *index,FTransformData *tdata,FKeyHeadGeneral *founded,uint32_t allowed)
	{
	int rv;
	if (tdata->use_phantom && !founded->fields.diff_or_phantom_mark)
		{ // Add phantom to normal or replace phantom in phantom value  
		rv = _append_phantom(index,founded,tdata,allowed);
		if (rv & (KS_CHANGED))
			cp_mark_hash_entry_dirty(index->used_cpages,tdata->hash);
		return rv;
		}
	rv = _mark_value(index,founded,tdata);
	if (index->head->use_flags & UF_PHANTOM_KEYS)
		{
		if ((allowed & KS_DELETED) || (rv & KS_MARKED))
			rv |= _replace_phantom_value(index,founded,tdata,rv);
		else
			rv |= KS_PRESENT;
		}
	else
		rv |= (allowed & KS_DELETED) ? _replace_value(index,founded,tdata) : KS_PRESENT; 
	return rv;
	}

// Добавляем или заменяем ключ , если нет цепочки коллизий. Если есть, сохраняет адрес цепочки для префетча
// Возвращает комбинацию флагов KS_ADDED, KS_DELETED, KS_NEED_FREE, KS_MARKED, KS_ERROR
// Может повесить блокировку памяти и не снять ее. Необходимость снятия определяется по флагам
int idx_key_try_set(FSingSet *index,FTransformData *tdata,uint32_t allowed)
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
		if (_compare_keys_W(index,hblock[hnum],tdata)) 
			{
			rv = _key_set_found(index,tdata,&hblock[hnum],allowed);
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
	if (!(allowed & KS_ADDED))
		return KS_SUCCESS;
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
	return KS_ADDED | KS_MARKED; // We set KS_MARKED since key was added to chain and old_counters was increased too
	}

// This call is for diff only, we can skip phantom related work
int idx_key_lookup(FSingSet *index,FTransformData *tdata)
	{
	element_type *chain_block_ref = tdata->chain_idx_ref;
	FAILURE_CHECK(!chain_block_ref,"no chain tail");

	element_type hb_idx;
	FKeyHeadGeneral *hblock;
	int rv = 0;

idx_key_lookup_next:
	CHECK_PAGE_TYPE(index,*chain_block_ref,PT_HEADERS);
	hb_idx = KH_BLOCK_IDX(*chain_block_ref);
	hblock = (FKeyHeadGeneral *)regionPointer(index,hb_idx,KH_BLOCK_SIZE);

#define CHECK_HEADER_IN_BLOCK_LOOKUP(HNUM) case (HNUM): do {\
				FAILURE_CHECK(!hblock[(HNUM)].fields.space_used,"empty header in chain"); \
				if (_compare_keys_W(index,hblock[(HNUM)],tdata)) \
					{ \
					rv = _mark_value(index,&hblock[(HNUM)],tdata); \
					rv |= _compare_value(index,&hblock[(HNUM)],tdata); \
					if (rv & KS_MARKED) \
						cp_mark_hblock_dirty(index->used_cpages,hb_idx); \
					return rv; \
					} \
				if (hblock[(HNUM)].fields.chain_stop) \
					return KS_SUCCESS; \
				} while (0)

	switch (KH_BLOCK_NUM(*chain_block_ref))
		{
		CHECK_HEADER_IN_BLOCK_LOOKUP(0);
		CHECK_HEADER_IN_BLOCK_LOOKUP(1);
		CHECK_HEADER_IN_BLOCK_LOOKUP(2);
		CHECK_HEADER_IN_BLOCK_LOOKUP(3);
		CHECK_HEADER_IN_BLOCK_LOOKUP(4);
		CHECK_HEADER_IN_BLOCK_LOOKUP(5);
		CHECK_HEADER_IN_BLOCK_LOOKUP(6);
		}
	if (!hblock[KH_BLOCK_LAST].fields.space_used)
		{
		chain_block_ref = &hblock[KH_BLOCK_LAST].links.next;
		FAILURE_CHECK(*chain_block_ref == KH_ZERO_REF,"open chain");
		goto idx_key_lookup_next;
		}
	FAILURE_CHECK(!hblock[KH_BLOCK_LAST].fields.chain_stop,"open chain");
	if (_compare_keys_W(index,hblock[KH_BLOCK_LAST],tdata))
		{
		rv = _mark_value(index,&hblock[KH_BLOCK_LAST],tdata);
		rv |= _compare_value(index,&hblock[KH_BLOCK_LAST],tdata);
		if (rv & KS_MARKED)
			cp_mark_hblock_dirty(index->used_cpages,hb_idx);
		return rv;
		}
	return KS_SUCCESS;
	}

// Добавляем или изменяем ключ в таблице. Цепочка коллизий по этому ключу заведомо есть (был вызов idx_key_try_set)
// Может повесить блокировку памяти и не снять ее. Необходимость снятия определяется по флагам
// Возвращает комбинацию флагов KS_ADDED, KS_DELETED, KS_MARKED, KS_ERROR
int idx_key_set(FSingSet *index,FTransformData *tdata,uint32_t allowed)
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

#define CHECK_HEADER_IN_BLOCK_SET(HNUM) case (HNUM): do {\
				FAILURE_CHECK(!hblock[(HNUM)].fields.space_used,"empty header in chain"); \
				if (_compare_keys_W(index,hblock[(HNUM)],tdata)) \
					{ \
					rv = _key_set_found(index,tdata,&hblock[HNUM],allowed); \
					if (rv & (KS_CHANGED | KS_MARKED)) \
						cp_mark_hblock_dirty(index->used_cpages,hb_idx); \
					return rv; \
					} \
				if (hblock[(HNUM)].fields.chain_stop) \
					{ header_num = (HNUM)+1; goto idx_key_set_not_found; } \
				} while (0)
		switch (chain_start_num)
			{
			CHECK_HEADER_IN_BLOCK_SET(0);
			CHECK_HEADER_IN_BLOCK_SET(1);
			CHECK_HEADER_IN_BLOCK_SET(2);
			CHECK_HEADER_IN_BLOCK_SET(3);
			CHECK_HEADER_IN_BLOCK_SET(4);
			CHECK_HEADER_IN_BLOCK_SET(5);
			CHECK_HEADER_IN_BLOCK_SET(6);
			}
		if (hblock[KH_BLOCK_LAST].fields.space_used)
			{ // В последнем блоке тоже данные
			FAILURE_CHECK(!hblock[KH_BLOCK_LAST].fields.chain_stop,"open chain");
			if (_compare_keys_W(index,hblock[KH_BLOCK_LAST],tdata))
				{
				rv = _key_set_found(index,tdata,&hblock[KH_BLOCK_LAST],allowed);
				if (rv & (KS_CHANGED | KS_MARKED))
					cp_mark_hblock_dirty(index->used_cpages,hb_idx);
				return rv;
				}
			if (!(allowed & KS_ADDED))
				return KS_SUCCESS;
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
	if (!(allowed & KS_ADDED))
		return KS_SUCCESS;
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

	lck_waitForReaders(index->lock_set,LCK_NO_DELETION);
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
		lck_waitForReaders(index->lock_set,hash); // If this chain is dumped, we should wait for full dump
		last_head->whole = ((uint64_t)KH_ZERO_REF << 32) + KH_ZERO_REF; // Сдвигаем ссылку в хеш-таблице на один (надо также сбросить space_used, поэтому сбрасываем обе ссылки)
		_del_key_rest(index,okeyhead); 
		return;
		}
	last_head->whole = ((uint64_t)KH_ZERO_REF << 32) + KH_ZERO_REF; // Удаляем стираемый из цепочки
	lck_waitForReaders(index->lock_set,LCK_NO_DELETION); // Last element can be deleted anyway
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
	lck_waitForReaders(index->lock_set,hash); 
	*ht_links_ref = KH_ZERO_REF; 
	lck_waitForReaders(index->lock_set,LCK_NO_DELETION); // Приходится еще раз ждать ридеров
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
	lck_waitForReaders(index->lock_set,LCK_NO_DELETION); 
	kh_free_block(index,last_head,last_head_idx,1); 
	_del_key_rest(index,okeyhead);
	}

// Удаляем последний в блоке из двух элементов не после хеш-таблицы 
void _del_last_in_block2(FSingSet *index,unsigned hash,FKeyHeadGeneral *last_head,element_type last_block_idx,FKeyHeadGeneral *prev_links,element_type prev_links_idx)
	{
	lck_memoryLock(index); // Блокируем операции с памятью
	_change_counter(index,hash,-1);
	FKeyHead okeyhead = last_head->fields;
	FKeyHead wkeyhead = (last_head - 1)->fields; // Единственный оставшийся в этом блоке 
	wkeyhead.chain_stop = 1; // Ставим ему chain_stop
	prev_links->fields = wkeyhead; // Копируем на место ссылки на этот блок
	lck_waitForReaders(index->lock_set,hash); // Last block deletion is safe for reading per key and unsafe for dump, we should wait for dump finish 
	kh_free_block(index,&last_head[-1],last_block_idx,2); 
	_del_key_rest(index,okeyhead);
	cp_mark_hblock_dirty(index->used_cpages,prev_links_idx);
	}

// Удаляем последний элемент в цепочке
void _del_last_in_chain(FSingSet *index,unsigned hash,FKeyHeadGeneral *last_head,element_type last_head_idx)
	{
	lck_memoryLock(index); // Блокируем операции с памятью
	_change_counter(index,hash,-1);
	FKeyHead okeyhead = last_head->fields;
	(last_head - 1)->fields.chain_stop = 1; // Ставим chain_stop предпоследнему
	lck_waitForReaders(index->lock_set,LCK_NO_DELETION);
	kh_free_last_from_chain(index,last_head,last_head_idx); // Удаляем последний
	_del_key_rest(index,okeyhead);
	}

// Удаляем элемент в цепочке, оканчивающейся на блок из двух элементов
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
	lck_waitForReaders(index->lock_set,hash);
	wkeyhead = (last_head - 1)->fields; // Копируем единственный оставшийся в этом блоке в восьмой элемент предыдущей цепочки
	wkeyhead.chain_stop = 1; // Ставим ему chain_stop
	cp_mark_hblock_dirty(index->used_cpages,prev_links_idx);
	prev_links->fields = wkeyhead; // Копируем на место ссылки на этот блок
	_del_key_rest(index,okeyhead);
	lck_waitForReaders(index->lock_set,LCK_NO_DELETION);
	kh_free_block(index,&last_head[-1],last_block_idx,2); // Удаляем этот блок
	}

// Delete element from the middle of a chain
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
	lck_waitForReaders(index->lock_set,hash);
	(last_head - 1)->fields.chain_stop = 1; // Ставим chain_stop предпоследнему
	_del_key_rest(index,okeyhead);
	lck_waitForReaders(index->lock_set,LCK_NO_DELETION);
	kh_free_last_from_chain(index,last_head,last_head_idx); // Удаляем последний
	}

static int _remove_phantom(FSingSet *index,FKeyHeadGeneral *existing_key,FTransformData *tdata)
	{
	FKeyHeadGeneral key_head = *existing_key;
	unsigned sz = key_head.data.size_and_value; // Размер остатка ключа
	if (sz < 128)
		return KS_SUCCESS; // Key has no value and has no phantom too
	sz = (sz - 128) >> 1; 

	element_type *key_rest = pagesPointer(index,key_head.fields.extra); // Остаток ключа
	FValueHeadGeneral value_head = (FValueHeadGeneral)key_rest[sz];
	if (!value_head.fields.phantom)
		return KS_SUCCESS;

	tdata->old_key_rest = key_head.fields.extra;
	tdata->old_key_rest_size = sz + VALUE_HEAD_SIZE + value_head.fields.size_e;

	element_type *value_rest = &key_rest[sz + VALUE_HEAD_SIZE];
	unsigned vsize = VALUE_SIZE_BYTES(&value_head.fields);
	unsigned size_e = vsize / ELEMENT_SIZE;
	unsigned ebytes = vsize % ELEMENT_SIZE;
	if (ebytes || (size_e && !((char *)&value_rest[size_e])[-1]))
		{ // Не делится нацело или значение заканчивается на нулевой байт
		size_e++;
		ebytes = 4 - ebytes;
		}
		
	unsigned rest_size = sz;
	if (size_e)
		{
		rest_size += VALUE_HEAD_SIZE + size_e;
		value_head.fields.phantom = 0;
		value_head.fields.size_e = size_e;
		value_head.fields.extra_bytes = ebytes;
		}
	else 
		{
		sz = rest_size = 0;
		key_head.fields.has_value = 0;
		}

	lck_memoryLock(index); // Блокируем операции с памятью
	if (rest_size)
		{
		element_type *rest;
		if (!(rest = idx_general_alloc(index,rest_size,&key_head.fields.extra)))
			return lck_memoryUnlock(index),ERROR_NO_SET_MEMORY;

		memcpy(rest,key_rest,sz * ELEMENT_SIZE);
		if (size_e)
			{
			rest[sz] = value_head.whole;
			memcpy(&rest[sz + VALUE_HEAD_SIZE],value_rest,size_e * ELEMENT_SIZE);
			if (ebytes)
				((char *)(&rest[sz + VALUE_HEAD_SIZE + size_e]))[-1] = 0xFF;
			}
		}
	existing_key->whole = key_head.whole;
	return KS_CHANGED;
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
		if (_compare_keys_W(index,*last_head,tdata))
			{
			if (tdata->use_phantom && !last_head->fields.diff_or_phantom_mark)
				{
				int rv = _remove_phantom(index,last_head,tdata);
				if (rv & KS_CHANGED)
					cp_mark_hash_entry_dirty(index->used_cpages,tdata->hash);
				return rv;
				}
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
		if (_compare_keys_W(index,*last_head,tdata))
			{
			if (tdata->use_phantom && !last_head->fields.diff_or_phantom_mark)
				{
				int rv = _remove_phantom(index,last_head,tdata);
				if (rv & KS_CHANGED)
					cp_mark_hblock_dirty(index->used_cpages,inum);
				return rv;
				}
			return _del_last_after_hash_table(index,tdata->hash,last_head,inum,ht_links_ref),KS_DELETED;
			}
		return KS_SUCCESS;
		}
	if (to_del)
		goto idx_key_del_found;
	if (_compare_keys_W(index,*last_head,tdata))
		{
		if (tdata->use_phantom && !last_head->fields.diff_or_phantom_mark)
			{
			int rv = _remove_phantom(index,last_head,tdata);
			if (rv & KS_CHANGED)
				cp_mark_hblock_dirty(index->used_cpages,inum);
			return rv;
			}
		khb_idx = inum; 
		to_del = &last_head->fields; 
		goto idx_key_del_found; 
		}
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
		if (_compare_keys_W(index,*last_head,tdata))
			{
			if (tdata->use_phantom && !last_head->fields.diff_or_phantom_mark)
				{
				int rv = _remove_phantom(index,last_head,tdata);
				if (rv & KS_CHANGED)
					cp_mark_hblock_dirty(index->used_cpages,inum);
				return rv;
				}
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

static int result_output(FSingSet *kvset,const FKeyHeadGeneral head,FWriteBufferSet *wbs)
	{
	element_type *key_rest = (head.data.size_and_value > 3) ? pagesPointer(kvset,head.fields.extra) : NULL;
	char *name = fbw_get_ref(wbs);
	unsigned size = cd_decode(&name[0],&head.fields,key_rest);

	if (head.fields.has_value)
		{
		element_type *value = &key_rest[head.fields.size];
		unsigned vsize = VALUE_SIZE_BYTES((FValueHead *)value);
		name[size++] = kvset->head->delimiter;
		memcpy(&name[size],(const char *)&value[1],vsize);
		size += vsize;
		}
	name[size++] = '\n';
	fbw_shift_pos(wbs,size);
	return size;
	}

static int phantom_output(FSingSet *kvset,const FKeyHeadGeneral head,FWriteBufferSet *wbs)
	{
	element_type *key_rest = (head.data.size_and_value > 3) ? pagesPointer(kvset,head.fields.extra) : NULL;
	char *name = fbw_get_ref(wbs);
	name[0] = '+';
	unsigned keysize = cd_decode(&name[1],&head.fields,key_rest);
	unsigned size = keysize + 1;

	if (head.fields.diff_or_phantom_mark)
		name[0] = '-';
	if (head.fields.has_value)
		{
		element_type *value = &key_rest[head.fields.size];
		if (((FValueHead *)value)->phantom)
			{
			unsigned *phantom_value = VALUE_PHANTOM_HEAD((FValueHead *)value);
			name[0] = '!';
			unsigned ph_vsize = VALUE_SIZE_BYTES((FValueHead *)phantom_value);
			name[size++] = kvset->head->delimiter;
			memcpy(&name[size],(const char *)&phantom_value[1],ph_vsize);
			size += ph_vsize;
			name[size++] = '\n';
			name[size++] = '=';
			memcpy(&name[size],&name[1],keysize);
			size += keysize;
			}
		unsigned vsize = VALUE_SIZE_BYTES((FValueHead *)value);
		name[size++] = kvset->head->delimiter;
		memcpy(&name[size],(const char *)&value[1],vsize);
		size += vsize;
		}
	name[size++] = '\n';
	fbw_shift_pos(wbs,size);
	return size;
	}

static void process_unmarked_from_hash_chain(FSingSet *index,unsigned hash,FWriteBufferSet *wbs)
	{
	FKeyHeadGeneral *next_key,*work_key;
	FKeyHeadGeneral *table_block; // Блок в хеш-таблице
	FKeyHeadGeneral *last_block; // Последний блок в цепочке (не в хеш-таблице)
	unsigned inum,hnum;
	FHashTableChain const *ht_chain;
	unsigned diff_mark = index->head->state_flags & SF_DIFF_MARK;
	
	table_block = get_hashtable_entry(index,hash);
	ht_chain = &HT_CHAINS[hash & 1];
	hnum = ht_chain->start;

	work_key = &table_block[hnum];
	while (work_key->fields.space_used)
		{
		next_key = &table_block[hnum += ht_chain->dir];
		if (next_key->fields.space_used && next_key->data.size_and_value > 3 && next_key->fields.diff_or_phantom_mark != diff_mark)
			__builtin_prefetch(pagesPointer(index,next_key->fields.extra));
		if (work_key->fields.diff_or_phantom_mark != diff_mark)
			{
			cp_mark_hash_entry_dirty(index->used_cpages,hash);
			work_key->fields.diff_or_phantom_mark = diff_mark;
			fbw_add_sym(wbs,'-');
			result_output(index,*work_key,wbs);
			fbw_commit(wbs);
			}
		work_key = next_key;
		}
	inum = work_key->links_array.links[ht_chain->link_num];
	if (inum == KH_ZERO_REF)
		return;

process_unmarked_from_hash_chain_next_cycle:
	last_block = (FKeyHeadGeneral *)regionPointer(index,KH_BLOCK_IDX(inum),KH_BLOCK_SIZE);
	hnum = KH_BLOCK_NUM(inum);
	while (hnum < KH_BLOCK_LAST)
		{
		work_key = &last_block[hnum];
		if (!work_key->fields.chain_stop)
			{ 
			next_key = &last_block[++hnum];
			if (next_key->fields.space_used && next_key->data.size_and_value > 3 && next_key->fields.diff_or_phantom_mark != diff_mark)
				__builtin_prefetch(pagesPointer(index,next_key->fields.extra));
			}
		if (work_key->fields.diff_or_phantom_mark != diff_mark)
			{
			cp_mark_hblock_dirty(index->used_cpages,KH_BLOCK_IDX(inum));
			work_key->fields.diff_or_phantom_mark = diff_mark;
			fbw_add_sym(wbs,'-');
			result_output(index,*work_key,wbs);
			fbw_commit(wbs);
			}
		if (work_key->fields.chain_stop)
			return;
 		}
	work_key = &last_block[KH_BLOCK_LAST];
	if (!work_key->fields.space_used)
		{
		inum = work_key->links.next;
		goto process_unmarked_from_hash_chain_next_cycle;
		}
	if (work_key->fields.diff_or_phantom_mark != diff_mark)
		{
		work_key->fields.diff_or_phantom_mark = diff_mark;
		fbw_add_sym(wbs,'-');
		result_output(index,*work_key,wbs);
		fbw_commit(wbs);
		}
	}

static void del_unmarked_from_hash_chain(FSingSet *index,unsigned hash,FWriteBufferSet *wbs)
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

	while (table_block[hnum].fields.space_used)
		{
		last_head = &table_block[hnum];
		if (!key_head && last_head->fields.diff_or_phantom_mark != diff_mark)
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
			if (!key_head && last_head->fields.diff_or_phantom_mark != diff_mark)
				khb_idx = last_block_idx, key_head = last_head;
			last_size++;
			if (last_head->fields.chain_stop)
				{
				if (key_head) goto del_unmarked_from_hash_chain_found;
				return;
				}
			hnum++;
			}
		if (last_block[KH_BLOCK_LAST].fields.space_used)
			{ // В последнем блоке тоже данные
			last_head = &last_block[KH_BLOCK_LAST];
			FAILURE_CHECK(!last_head->fields.chain_stop,"open chain");
			if (!key_head && last_head->fields.diff_or_phantom_mark != diff_mark) 
				khb_idx = last_block_idx, key_head = last_head;
			last_size++;
			if (key_head) goto del_unmarked_from_hash_chain_found;
			return;
			}
		inum = last_block[KH_BLOCK_LAST].links.next;
		}

	if (!key_head)
		return;

del_unmarked_from_hash_chain_found:
	lck_memoryLock(index); // Блокируем операции с памятью
	_change_counter(index,hash,-1);
	
	okeyhead = key_head->fields;

#ifdef LOG_OPERATION
	static char key_buf[MAX_KEY_SOURCE + 1];
	element_type *key_rest = (key_head->data.size_and_value > 3) ? pagesPointer(index,key_head->fields.extra) : NULL;
	unsigned key_size = cd_decode(&key_buf[0],&key_head->fields,key_rest);
	key_buf[key_size] = 0;
	
	FORMATTED_LOG("deleting key %s\n",key_buf);
#endif
	if (wbs)
		{
		fbw_add_sym(wbs,'-');
		result_output(index,*key_head,wbs);
		fbw_commit(wbs);
		}

	if (key_head != last_head)
		{ // Если удаляем не последний, копируем последний в удаляемый
		if (khb_idx)
			cp_mark_hblock_dirty(index->used_cpages,khb_idx);
		else
			cp_mark_hash_entry_dirty(index->used_cpages,hash);
		wkeyhead = last_head->fields;
		wkeyhead.chain_stop = 0;
		key_head->fields = wkeyhead;
		lck_waitForReaders(index->lock_set,hash);
		}
	else
		lck_waitForReaders(index->lock_set,LCK_NO_DELETION);
	// Удаляем последний элемент
	if (!last_block)
		{ // Последний заголовок находится в хеш-таблице, hnum не в начале цепочки
		// Сдвигаем ссылку в хеш-таблице на один (надо также сбросить space_used, поэтому сбрасываем обе ссылки)
		cp_mark_hash_entry_dirty(index->used_cpages,hash);
		last_head->whole = ((uint64_t)KH_ZERO_REF << 32) + KH_ZERO_REF; 
		if (hnum - ht_chain->dir != ht_chain->start)
			(last_head - ht_chain->dir)->fields.chain_stop = 1;  // Ставим последнему в хеш-таблице chain_stop
		_del_key_rest(index,okeyhead);
		lck_memoryUnlock(index);
		goto del_unmarked_from_hash_chain_begin;
		}
	if (last_size == 1)
		{ // Единственный элемент в блоке, ссылка идет из хеш-таблицы
		cp_mark_hash_entry_dirty(index->used_cpages,hash);
		table_block[ht_links_num].links_array.links[ht_chain->link_num] = KH_ZERO_REF; // Обнуляем ссылку в хеш-таблице.
		_del_key_rest(index,okeyhead);
		lck_waitForReaders(index->lock_set,LCK_NO_DELETION);
		kh_free_block(index,last_head,last_block_idx + KEY_HEAD_SIZE * hnum,1); // Удаляем этот блок
		lck_memoryUnlock(index);
		goto del_unmarked_from_hash_chain_begin;
		}
	if (prev_block && last_size == 2)
		{ 
		cp_mark_hblock_dirty(index->used_cpages,prev_block_idx);
		wkeyhead = (last_head - 1)->fields; // Единственный оставшийся в этом блоке в восьмой элемент предыдущей цепочки
		wkeyhead.chain_stop = 1; // Ставим ему chain_stop
		prev_block[KH_BLOCK_LAST].fields = wkeyhead; // Копируем на место ссылки на этот блок
		_del_key_rest(index,okeyhead);
		lck_waitForReaders(index->lock_set,LCK_NO_DELETION);
		kh_free_block(index,&last_head[-1],last_block_idx + KEY_HEAD_SIZE * (hnum - 1),2); // Удаляем этот блок
		lck_memoryUnlock(index);
		goto del_unmarked_from_hash_chain_begin;
		}
	(last_head - 1)->fields.chain_stop = 1; // Ставим chain_stop предпоследнему
	_del_key_rest(index,okeyhead);
	lck_waitForReaders(index->lock_set,LCK_NO_DELETION);
	kh_free_last_from_chain(index,last_head,last_block_idx + KEY_HEAD_SIZE * hnum); // Удаляем последний
	lck_memoryUnlock(index);
	goto del_unmarked_from_hash_chain_begin;
	}

// Пробегаем по всем ненулевым счетчикам или элементам хеша если counters == NULL. 
// Для каждой цепочки вызываем функцию, удаляющую и/или выводящую непомеченные элементы
void idx_process_unmarked(FSingSet *index,unsigned *counters,FWriteBufferSet *wbs,int del)
	{
	unsigned i,j;
	void (*cb)(FSingSet *index,unsigned hash,FWriteBufferSet *wbs);

	cb = del ? del_unmarked_from_hash_chain : process_unmarked_from_hash_chain;

	if (!index->counters || !counters)
		{
		for (i = 0; i < index->hashtable_size; i++)
			{
			lck_chainLock(index,i);
			(*cb)(index,i,wbs);
			lck_chainUnlock(index,i);
			}
		return;
		}
	
	for (i = 0; i < COUNTERS_SIZE(index->hashtable_size) - 1; i++)
		{
		if (index->counters[i] <= counters[i]) continue;
		for (j = COUNTER_TO_HASH(i); j < COUNTER_TO_HASH(i+1); j++)
			{
			lck_chainLock(index,j);
			(*cb)(index,j,wbs);
			lck_chainUnlock(index,j);
			}
		}
	if (index->counters[i] > counters[i])
		for (j = COUNTER_TO_HASH(i) ; j < index->hashtable_size; j++)
			{
			lck_chainLock(index,j);
			(*cb)(index,j,wbs);
			lck_chainUnlock(index,j);
			}
	}

static inline int key_output(FSingSet *kvset,FKeyHeadGeneral key_data,FWriteBufferSet *wbs)
	{
	fbw_check_space(wbs);
	return (kvset->head->use_flags & UF_PHANTOM_KEYS) ? phantom_output(kvset,key_data,wbs) : result_output(kvset,key_data,wbs);
	}

static int dump_hash_chain(FSingSet *index,unsigned hash,FWriteBufferSet *wbs)
	{
	FKeyHeadGeneral next_data,key_data;
	FKeyHeadGeneral *table_block; // Блок в хеш-таблице
	FKeyHeadGeneral *last_block; // Последний блок в цепочке (не в хеш-таблице)
	unsigned inum,hnum;
	FHashTableChain const *ht_chain;
	FReaderLock rlock = READER_LOCK_INIT;
#define DUMPED_DEF_SIZE 40
	uint64_t dumped_def[DUMPED_DEF_SIZE];
	unsigned dumpedcnt = 0,dumped_size = DUMPED_DEF_SIZE;
	uint64_t *dumped = dumped_def;
	
	rlock.keep = 1;
	lck_readerLock(index->lock_set,&rlock);
	table_block = get_hashtable_entry(index,hash);
	ht_chain = &HT_CHAINS[hash & 1];
	hnum = ht_chain->start;
	int rv = 1;
	unsigned last_size;

	key_data.whole = __atomic_load_n(&table_block[hnum].whole,__ATOMIC_RELAXED);
	while (key_data.fields.space_used)
		{
		next_data.whole = __atomic_load_n(&table_block[hnum += ht_chain->dir].whole,__ATOMIC_RELAXED);
		if (next_data.fields.space_used && next_data.data.size_and_value > 3)
			__builtin_prefetch(pagesPointer(index,next_data.fields.extra));
		last_size = key_output(index,key_data,wbs);
		dumped[dumpedcnt++] = key_data.whole;
		if (!lck_readerUnlockCond(index->lock_set,&rlock,hash))
			return 1;
		key_data.whole = __atomic_load_n(&table_block[hnum].whole,__ATOMIC_RELAXED);
		}
	inum = key_data.links_array.links[ht_chain->link_num];
	if (inum == KH_ZERO_REF)
		goto dump_hash_chain_check_last;

process_hash_chain_next_cycle:
	last_block = (FKeyHeadGeneral *)regionPointer(index,KH_BLOCK_IDX(inum),KH_BLOCK_SIZE);
	hnum = KH_BLOCK_NUM(inum);
	while (hnum < KH_BLOCK_LAST)
		{
		key_data.whole = __atomic_load_n(&last_block[hnum].whole,__ATOMIC_RELAXED);
		if (!key_data.fields.chain_stop)
			{ 
			next_data.whole = __atomic_load_n(&last_block[++hnum].whole,__ATOMIC_RELAXED);
			if (next_data.fields.space_used && next_data.data.size_and_value > 3)
				__builtin_prefetch(pagesPointer(index,next_data.fields.extra));
			}
		last_size = key_output(index,key_data,wbs);
		dumped[dumpedcnt++] = key_data.whole;
		if (key_data.fields.chain_stop)
			goto dump_hash_chain_check_last; // It was the last key in chain
		if (!lck_readerUnlockCond(index->lock_set,&rlock,hash))
			goto dump_hash_chain_exit;
		}
	key_data.whole = __atomic_load_n(&last_block[KH_BLOCK_LAST].whole,__ATOMIC_RELAXED);
	if (!key_data.fields.space_used)
		{
		inum = key_data.links.next;
		if (dumpedcnt > dumped_size - KEYHEADS_IN_BLOCK)
			{
			if (dumped == dumped_def)
				{
				dumped = (uint64_t *)malloc(sizeof(uint64_t) * (dumped_size *= 2));
				memcpy(dumped,dumped_def,sizeof(uint64_t) * DUMPED_DEF_SIZE);
				}
			else
				dumped = (uint64_t *)realloc(dumped,sizeof(uint64_t) * (dumped_size *= 2));
			}
		goto process_hash_chain_next_cycle;
		}
	last_size = key_output(index,key_data,wbs);
	if (!lck_readerUnlock(index->lock_set,&rlock))
		goto dump_hash_chain_exit;

dump_hash_chain_check_last:
   rlock.keep = 0;
	if (!lck_readerUnlock(index->lock_set,&rlock))
		goto dump_hash_chain_exit;

	rv = 0;
	if (dumpedcnt > 2)
		{
		unsigned i;
		for (i = 0; i < dumpedcnt - 1; i++)
			{
			if (key_data.whole == dumped[i])
				fbw_shift_pos(wbs,-last_size);
			}
		}
dump_hash_chain_exit:
	if (dumped != dumped_def)
		free(dumped);
	return rv;
	}
	
void idx_dump_all(FSingSet *index,FWriteBufferSet *wbs)
	{
	unsigned i,j;
	
	if (!index->counters)
		{
		for (i = 0; i < index->hashtable_size; i++)
			{
			while (dump_hash_chain(index,i,wbs))
				fbw_rollback(wbs);
			fbw_commit(wbs);
			}
		return;
		}
	
	for (i = 0; i < COUNTERS_SIZE(index->hashtable_size) - 1; i++)
		{
		if (!index->counters[i]) continue;
		for (j = COUNTER_TO_HASH(i); j < COUNTER_TO_HASH(i+1); j++)
			{
			while (dump_hash_chain(index,j,wbs))
				fbw_rollback(wbs);
			fbw_commit(wbs);
			}
		}
	if (index->counters[i])
		{
		for (j = COUNTER_TO_HASH(i) ; j < index->hashtable_size; j++)
			{
			while (dump_hash_chain(index,j,wbs))
				fbw_rollback(wbs);
			fbw_commit(wbs);
			}
		}
	}

static inline int cb_call(FSingSet *index,FKeyHeadGeneral *key_head,CSingIterateCallback cb,void *param)
	{
	unsigned vsize;
	element_type *key_rest,*value;
	char *value_data;
	char new_value[MAX_VALUE_SOURCE];
	
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
	char keybuf[MAX_KEY_SOURCE + 1];
	unsigned size = cd_decode(keybuf,&key_head->fields,key_rest);
	keybuf[size] = 0;
	int res = (*cb)(keybuf,value_data,&vsize,new_value,param);
	return res >= 0 ? 0 : ERROR_BREAK;
	}

static int iterate_hash_chain(FSingSet *index,unsigned hash,CSingIterateCallback cb,void *param)
	{
	FKeyHeadGeneral *next_key,*work_key;
	FKeyHeadGeneral *table_block; // Блок в хеш-таблице
	FKeyHeadGeneral *last_block; // Последний блок в цепочке (не в хеш-таблице)
	unsigned inum,hnum;
	FHashTableChain const *ht_chain;
	unsigned diff_mark = index->head->state_flags & SF_DIFF_MARK;
	
	table_block = get_hashtable_entry(index,hash);
	ht_chain = &HT_CHAINS[hash & 1];
	hnum = ht_chain->start;

	work_key = &table_block[hnum];
	while (work_key->fields.space_used)
		{
		next_key = &table_block[hnum += ht_chain->dir];
		if (next_key->fields.space_used && next_key->data.size_and_value > 3 && next_key->fields.diff_or_phantom_mark != diff_mark)
			__builtin_prefetch(pagesPointer(index,next_key->fields.extra));
		cb_call(index,work_key,cb,param);
		work_key = next_key;
		}
	inum = work_key->links_array.links[ht_chain->link_num];
	if (inum == KH_ZERO_REF)
		return 0;

iterate_hash_chain_next_cycle:
	last_block = (FKeyHeadGeneral *)regionPointer(index,KH_BLOCK_IDX(inum),KH_BLOCK_SIZE);
	hnum = KH_BLOCK_NUM(inum);
	while (hnum < KH_BLOCK_LAST)
		{
		work_key = &last_block[hnum];
		if (!work_key->fields.chain_stop)
			{ 
			next_key = &last_block[++hnum];
			if (next_key->fields.space_used && next_key->data.size_and_value > 3 && next_key->fields.diff_or_phantom_mark != diff_mark)
				__builtin_prefetch(pagesPointer(index,next_key->fields.extra));
			}
		cb_call(index,work_key,cb,param);
		if (work_key->fields.chain_stop)
			return 0;
 		}
	work_key = &last_block[KH_BLOCK_LAST];
	if (!work_key->fields.space_used)
		{
		inum = work_key->links.next;
		goto iterate_hash_chain_next_cycle;
		}
	cb_call(index,work_key,cb,param);
	return 0;
	}

int idx_iterate_all(FSingSet *index,CSingIterateCallback cb,void *param)
	{
	unsigned i,j;
	
	if (!index->counters)
		{
		for (i = 0; i < index->hashtable_size; i++)
			{
			lck_chainLock(index,i);
			iterate_hash_chain(index,i,cb,param);
			lck_chainUnlock(index,i);
			}
		return 0;
		}
	
	for (i = 0; i < COUNTERS_SIZE(index->hashtable_size) - 1; i++)
		{
		if (!index->counters[i]) continue;
		for (j = COUNTER_TO_HASH(i); j < COUNTER_TO_HASH(i+1); j++)
			{
			lck_chainLock(index,j);
			iterate_hash_chain(index,j,cb,param);
			lck_chainUnlock(index,j);
			}
		}
	if (index->counters[i])
		{
		for (j = COUNTER_TO_HASH(i) ; j < index->hashtable_size; j++)
			{
			lck_chainLock(index,j);
			iterate_hash_chain(index,j,cb,param);
			lck_chainUnlock(index,j);
			}
		}
	return 0;
	}

int idx_revert(FSingSet *index)
	{
	int i,rv;
	unsigned old_pcnt;
	
	// We have to map all pages before revert
	if (index->pages_fd == -1)
		return ERROR_INTERNAL; // We should not be here with UF_NOT_PERSISTENT
	for (i = 0; i < index->head->pcnt; i++)
		if ((index->pages[i] == MAP_FAILED) &&
				(index->pages[i] = (element_type *)mmap(NULL,PAGE_SIZE_BYTES,PROT_WRITE | PROT_READ, MAP_SHARED, index->pages_fd, i * PAGE_SIZE_BYTES)) == MAP_FAILED) 
			return ERROR_INTERNAL;

	old_pcnt = index->head->pcnt;
	lck_fullReaderLock(index->lock_set);
	if (!(rv = cp_revert(index)))
		{
		while (index->head->pcnt < old_pcnt)
			idx_free_page(index,index->head->pcnt++);
		}
	lck_fullReaderUnlock(index->lock_set);

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
	if (!(index->head->use_flags & UF_PHANTOM_KEYS) && key_head->diff_or_phantom_mark != (index->head->state_flags & SF_DIFF_MARK) ? 1 : 0)
		return idx_set_formatted_error(index,"Key diff mark is not equal set diff mark"),1;

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
