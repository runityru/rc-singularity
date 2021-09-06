/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h> 
#include <unistd.h>

#include "filebuf.h"
#include "utils.h"

static void _buffer_release(FBufferSet *set,unsigned buf_num, unsigned to_state)
	{
	FIOBuffer *work = &set->buffers[buf_num];
	pthread_mutex_lock(&work->buffer_lock); 
	work->state = to_state;
	pthread_cond_signal(&work->state_changed);
	pthread_mutex_unlock(&work->buffer_lock);
	}

static void _buffer_acquire(FBufferSet *set,unsigned buf_num, unsigned from_state)
	{
	FIOBuffer *work = &set->buffers[buf_num];
	pthread_mutex_lock(&work->buffer_lock); 
	while (work->state == from_state)
		pthread_cond_wait(&work->state_changed,&work->buffer_lock);
	pthread_mutex_unlock(&work->buffer_lock);
	}

static void *read_thread(void *param)
	{
	FBufferSet *set = (FBufferSet *)param;
	unsigned cur_buf_num = 0;
	FIOBuffer *workbuf = &set->buffers[0];
	size_t rdd;
	unsigned state;
read_thread_repeat:
	rdd = file_read(set->fd,&workbuf->data[MOVABLE_TAIL],BUFFER_SIZE);
	workbuf->size = rdd;
	workbuf->data[rdd + MOVABLE_TAIL] = 0;
	state = (rdd == BUFFER_SIZE) ? BUFF_STATE_PROCESS : BUFF_STATE_STOP;
	_buffer_release(set,cur_buf_num,state);
	if (state == BUFF_STATE_STOP)
		return (void *)0;
	cur_buf_num = 1 - cur_buf_num;
	_buffer_acquire(set,cur_buf_num,BUFF_STATE_PROCESS);
	workbuf = &set->buffers[cur_buf_num];
	if (workbuf->state == BUFF_STATE_STOP)
		return (void *)0;
	goto read_thread_repeat;
	}

static void *write_thread(void *param)
	{
	FBufferSet *set = (FBufferSet *)param;
	unsigned cur_buf_num = 0;
	FIOBuffer *workbuf = &set->buffers[0];
write_thread_repeat:
	_buffer_acquire(set,cur_buf_num,BUFF_STATE_FILL);
	file_write(set->fd,&workbuf->data[MOVABLE_TAIL],workbuf->size);
	if (workbuf->state == BUFF_STATE_STOP)
		return (void *)0;
	_buffer_release(set,cur_buf_num,BUFF_STATE_FILL);
	cur_buf_num = 1 - cur_buf_num;
	workbuf = &set->buffers[cur_buf_num];
	goto write_thread_repeat;
	}

static void _init(FBufferSet *set)
	{
	pthread_mutex_init(&set->buffers[0].buffer_lock,NULL);
	pthread_cond_init(&set->buffers[0].state_changed,NULL);
	set->buffers[0].state = BUFF_STATE_FILL;
	set->buffers[0].size = 0;
	pthread_mutex_init(&set->buffers[1].buffer_lock,NULL);
	pthread_cond_init(&set->buffers[1].state_changed,NULL);
	set->buffers[1].state = BUFF_STATE_FILL;
	set->buffers[1].size = 0;
	set->mbuf_num = 0;
	set->mbuf = &set->buffers[0];
	}

int fb_init_r(FBufferSet *set,const char *filename)
	{
	_init(set);
	if ((set->fd = open(filename,O_RDONLY)) == -1) return 0;
	set->no_close = 0;
	pthread_create(&set->io_thread,NULL,read_thread,set);
	return 1;
	}

int fb_init_w(FBufferSet *set,const char *filename)
	{
	_init(set);
	if (filename)
		{
		if ((set->fd = open(filename,O_WRONLY | O_TRUNC | O_CREAT,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) return 0;
		set->no_close = 0;
		}
	else
		set->fd = STDOUT_FILENO,set->no_close = 1;
	pthread_create(&set->io_thread,NULL,write_thread,set);
	return 1;
	}

char *fb_first_block(FBufferSet *set,int *size)
	{
	FIOBuffer *workbuf = &set->buffers[0];
	_buffer_acquire(set,0,BUFF_STATE_FILL);
	*size = workbuf->size;
	return workbuf->size ? &workbuf->data[MOVABLE_TAIL] : NULL;
	}

char *fb_next_block(FBufferSet *set,int *size)
	{
	if (set->mbuf->state == BUFF_STATE_STOP)
		return NULL;
	_buffer_release(set,set->mbuf_num,BUFF_STATE_FILL);
	set->mbuf_num = 1 - set->mbuf_num;
	_buffer_acquire(set,set->mbuf_num,BUFF_STATE_FILL);
	set->mbuf = &set->buffers[set->mbuf_num];
	*size = set->mbuf->size;
	return &set->mbuf->data[MOVABLE_TAIL];
	}

char *fb_next_block_partial(FBufferSet *set,int *size,int *crp)
	{
	int tocpy,onum = set->mbuf_num;
	if (set->mbuf->state == BUFF_STATE_STOP)
		return &set->mbuf->data[MOVABLE_TAIL];
	set->mbuf_num = 1 - set->mbuf_num;
	_buffer_acquire(set,set->mbuf_num,BUFF_STATE_FILL);
	set->mbuf = &set->buffers[set->mbuf_num];
	if ((tocpy = *size - *crp))
		memcpy(&set->mbuf->data[MOVABLE_TAIL - tocpy],&set->buffers[onum].data[MOVABLE_TAIL + *crp],tocpy);
	_buffer_release(set,onum,BUFF_STATE_FILL);
	*size = set->mbuf->size;
	*crp = -tocpy;
	return &set->mbuf->data[MOVABLE_TAIL];
	}

void fb_added(FBufferSet *set,int size)
	{
	int obsize;
	if ((obsize = (set->mbuf->size += size)) >= BUFFER_SIZE)
		{
		char *rest = &set->mbuf->data[MOVABLE_TAIL + BUFFER_SIZE];
		set->mbuf->size = BUFFER_SIZE;
		_buffer_release(set,set->mbuf_num,BUFF_STATE_PROCESS);
		set->mbuf_num = 1 - set->mbuf_num;
		_buffer_acquire(set,set->mbuf_num,BUFF_STATE_PROCESS);
		set->mbuf = &set->buffers[set->mbuf_num];
		if ((set->mbuf->size = obsize - BUFFER_SIZE))
			memcpy(&set->mbuf->data[MOVABLE_TAIL],rest,set->mbuf->size);
		}
	}

void fb_add(FBufferSet *set,const char *data,int size)
	{
	int rest,tocpy;
	rest = BUFFER_SIZE - set->mbuf->size;
	tocpy = size > rest ? rest : size;
	memcpy(&set->mbuf->data[MOVABLE_TAIL + set->mbuf->size],data,tocpy);
	data += tocpy;
	set->mbuf->size += tocpy;
	if (!(size -= tocpy))
		return;
	_buffer_release(set,set->mbuf_num,BUFF_STATE_PROCESS);
	set->mbuf_num = 1 - set->mbuf_num;
	_buffer_acquire(set,set->mbuf_num,BUFF_STATE_PROCESS);
	set->mbuf = &set->buffers[set->mbuf_num];
fb_add_repeat:
	tocpy = size > BUFFER_SIZE ? BUFFER_SIZE : size;
	memcpy(&set->mbuf->data[MOVABLE_TAIL],data,tocpy);
	data += tocpy;
	set->mbuf->size = tocpy;
	if (!(size -= tocpy))
		return;
	_buffer_release(set,set->mbuf_num,BUFF_STATE_PROCESS);
	set->mbuf_num = 1 - set->mbuf_num;
	_buffer_acquire(set,set->mbuf_num,BUFF_STATE_PROCESS);
	set->mbuf = &set->buffers[set->mbuf_num];
	goto fb_add_repeat;
	}

void fb_finish(FBufferSet *set)
	{
	if (set->fd == -1) return;
	_buffer_release(set,set->mbuf_num,BUFF_STATE_STOP);
	pthread_join(set->io_thread,NULL);
	if (!set->no_close)
		close(set->fd);
	set->fd = -1;
	}
