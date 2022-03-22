/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#ifndef _FILEBUF_H
#define _FILEBUF_H

#include <fcntl.h>
#include <pthread.h>

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 65536
#endif

#include "defines.h"
#include "allocator.h"

	// 1 extra byte on operation '+'/'-' for diff, 1 for extra sym after key
#define KEY_BUFFER_SIZE ALIGN_UP(MAX_KEY_SOURCE + 2,CACHE_LINE_SIZE)

#define BUFF_STATE_FILL 0
#define BUFF_STATE_PROCESS 1 
#define BUFF_STATE_STOP 2
#define BUFF_STATE_FREE 3

typedef struct FIOBufferStateTg
	{
	pthread_mutex_t buffer_lock;
	pthread_cond_t state_changed;
	unsigned state;
	} FIOBufferState;

typedef struct FReadBufferTg
	{
	FIOBufferState state_data;
	int size;
	CACHE_LINE_PADDING(padding1,sizeof(FIOBufferState) + sizeof(unsigned));
	char data[KEY_BUFFER_SIZE + BUFFER_SIZE];
	} FReadBuffer;

typedef struct FReadBufferSetTg
	{
	FReadBuffer buffers[2];
	pthread_t io_thread;
	int fd;
	int thread_started;
	unsigned no_close;
	unsigned mbuf_num; // Number of main thread buffer
	int mbuf_pos; // Read position in main buffer
	int eof;
	FReadBuffer *mbuf;
	} FReadBufferSet;

FReadBufferSet *fbr_create(const char *filename);
void fbr_finish(FReadBufferSet *set);
int fbr_first_block(FReadBufferSet *set);
int fbr_next_block(FReadBufferSet *set);
char *fbr_get_key_ref(FReadBufferSet *set);

static inline char *fbr_get_ref(FReadBufferSet *set)
	{ return &set->mbuf->data[KEY_BUFFER_SIZE + set->mbuf_pos]; }

static inline int fbr_get_size(FReadBufferSet *set)
	{ return set->mbuf->size - set->mbuf_pos; }

static inline int fbr_shift_pos(FReadBufferSet *set,int cnt) // After reading key, we should have something left to read if file is not at the end
	{
	if ((set->mbuf_pos += cnt) >= set->mbuf->size) 
		set->eof = 1;
	return set->eof;
	}

static inline char fbr_get_sym(FReadBufferSet *set)
	{ return set->mbuf->data[KEY_BUFFER_SIZE + set->mbuf_pos]; }

static inline int fbr_inc_pos(FReadBufferSet *set)
	{
	if (++set->mbuf_pos >= set->mbuf->size)
		return fbr_next_block(set);
	return 0;
	}

	// 1 extra byte for op sign, 1 byte for divider between key and value, 1 byte for '\n' in output. Doubled for phantom keys
#define WRITE_BUFFER_GROW ALIGN_UP((MAX_KEY_SOURCE + MAX_VALUE_SOURCE + 3) * 2,DISK_PAGE_BYTES) 

typedef struct FWriteBufferTg
	{
	FIOBufferState state_data;
	unsigned total_size;
	unsigned filled_size;
	char *data;
	CACHE_LINE_PADDING(padding1,sizeof(FIOBufferState) + sizeof(unsigned) * 2 + sizeof(char *));
	} FWriteBuffer;

typedef struct FWriteBufferSetTg
	{
	FWriteBuffer buffers[2];
	pthread_t io_thread;
	int fd;
	int thread_started;
	unsigned no_close;
	unsigned commited_pos;
	unsigned mbuf_num; // Number of main thread buffer
	FWriteBuffer *mbuf;
	} FWriteBufferSet;

FWriteBufferSet *fbw_create(const char *filename);
void fbw_finish(FWriteBufferSet *set);
static inline char *fbw_get_ref(FWriteBufferSet *set) 
	{ return &set->mbuf->data[set->mbuf->filled_size]; }
static inline void fbw_shift_pos(FWriteBufferSet *set,int size) 
	{ set->mbuf->filled_size += size; }
static inline void fbw_add_sym(FWriteBufferSet *set,char sym)
	{ set->mbuf->data[set->mbuf->filled_size++] = sym; }
void fbw_check_space(FWriteBufferSet *set);
static inline void fbw_rollback(FWriteBufferSet *set)
	{ set->mbuf->filled_size = set->commited_pos; }
void fbw_commit(FWriteBufferSet *set);

#endif