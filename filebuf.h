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

	// 1 extra byte on operation '+'/'-' for diff, 1 for extra sym after key
#define MT_SIZE (MAX_KEY_SOURCE + 2)

#define MOVABLE_TAIL ((MT_SIZE / 256 + ((MT_SIZE % 256) ? 1 : 0)) * 256)

#define BUFF_STATE_FILL 0
#define BUFF_STATE_PROCESS 1 
#define BUFF_STATE_STOP 2

typedef struct FIOBufferTg
	{
	pthread_mutex_t buffer_lock;
	pthread_cond_t state_changed;
	unsigned state;
	unsigned size;
	CACHE_LINE_PADDING(padding1,sizeof(pthread_mutex_t) + sizeof(pthread_cond_t) + sizeof(unsigned) * 2);
	char data[MOVABLE_TAIL * 2 + BUFFER_SIZE];
	} FIOBuffer;

typedef struct FBufferSetTg
	{
	FIOBuffer buffers[2];
	pthread_t io_thread;
	int fd;
	unsigned no_close;
	unsigned mbuf_num; // Number of main thread buffer
	FIOBuffer *mbuf;
	} FBufferSet;

int fb_init_r(FBufferSet *set,const char *filename);
int fb_init_w(FBufferSet *set,const char *filename);
char *fb_first_block(FBufferSet *set,int *size);
char *fb_next_block(FBufferSet *set,int *size);
char *fb_next_block_partial(FBufferSet *set,int *size,int *crp);
void fb_added(FBufferSet *set,int size);
void fb_add(FBufferSet *set,const char *data,int size);
void fb_finish(FBufferSet *set);
static inline char *fb_get_pos(FBufferSet *set) { return &set->mbuf->data[MOVABLE_TAIL + set->mbuf->size]; };

#endif