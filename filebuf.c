/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h> 
#include <sys/mman.h>
#include <unistd.h>

#include "filebuf.h"
#include "utils.h"

static void _buffer_release(FIOBufferState *state_data,unsigned to_state)
	{
	pthread_mutex_lock(&state_data->buffer_lock); 
	state_data->state = to_state;
	pthread_cond_signal(&state_data->state_changed);
	pthread_mutex_unlock(&state_data->buffer_lock);
	}

static void _buffer_acquire(FIOBufferState *state_data,unsigned from_state)
	{
	pthread_mutex_lock(&state_data->buffer_lock); 
	while (state_data->state == from_state)
		pthread_cond_wait(&state_data->state_changed,&state_data->buffer_lock);
	pthread_mutex_unlock(&state_data->buffer_lock);
	}

static void *read_thread(void *param)
	{
	FReadBufferSet *set = (FReadBufferSet *)param;
	unsigned cur_buf_num = 0;
	FReadBuffer *workbuf = &set->buffers[0];
	size_t rdd;
	unsigned state;
read_thread_repeat:
	rdd = file_read(set->fd,&workbuf->data[KEY_BUFFER_SIZE],BUFFER_SIZE);
	workbuf->size = rdd;
	workbuf->data[rdd + KEY_BUFFER_SIZE] = 0;
	state = (rdd == BUFFER_SIZE) ? BUFF_STATE_PROCESS : BUFF_STATE_STOP;
	_buffer_release(&workbuf->state_data,state);
	if (state == BUFF_STATE_STOP)
		return (void *)0;
	cur_buf_num = 1 - cur_buf_num;
	workbuf = &set->buffers[cur_buf_num];
	_buffer_acquire(&workbuf->state_data,BUFF_STATE_PROCESS);
	if (workbuf->state_data.state == BUFF_STATE_STOP)
		return (void *)0;
	goto read_thread_repeat;
	}

static int _init_state(FIOBufferState *state_data)
	{
	if (pthread_mutex_init(&state_data->buffer_lock,NULL))
		return 1;
	if (pthread_cond_init(&state_data->state_changed,NULL))
		return pthread_mutex_destroy(&state_data->buffer_lock),1;
	state_data->state = BUFF_STATE_FILL;
	return 0;
	}

FReadBufferSet *fbr_create(const char *filename)
	{
	FReadBufferSet *rv;
	if (posix_memalign((void **)&rv,CACHE_LINE_SIZE,sizeof(FReadBufferSet)))
		return NULL;
	rv->thread_started = 0;
	rv->no_close = 0;
	rv->buffers[0].size = rv->buffers[1].size = 0;
	rv->mbuf_num = 0;
	rv->mbuf_pos = 0;
	rv->eof = 0;
	rv->mbuf = &rv->buffers[0];
	rv->buffers[0].state_data.state = rv->buffers[1].state_data.state = BUFF_STATE_FREE;
	if ((rv->fd = open(filename,O_RDONLY)) == -1 || _init_state(&rv->buffers[0].state_data) || _init_state(&rv->buffers[1].state_data)) 
		return fbr_finish(rv),NULL;
	if (pthread_create(&rv->io_thread,NULL,read_thread,rv))
		return fbr_finish(rv),NULL;
	rv->thread_started = 1;
	return rv;
	}

void fbr_finish(FReadBufferSet *set)
	{
	if (!set)
		return;
	if (set->thread_started)
		{
		_buffer_release(&set->mbuf->state_data,BUFF_STATE_STOP);
		pthread_join(set->io_thread,NULL);
		}
	if (set->fd != -1 && !set->no_close)
		close(set->fd);
	if (set->buffers[0].state_data.state != BUFF_STATE_FREE)
		pthread_cond_destroy(&set->buffers[0].state_data.state_changed),pthread_mutex_destroy(&set->buffers[0].state_data.buffer_lock);
	if (set->buffers[1].state_data.state != BUFF_STATE_FREE)
		pthread_cond_destroy(&set->buffers[1].state_data.state_changed),pthread_mutex_destroy(&set->buffers[1].state_data.buffer_lock);
	free(set);
	}

int fbr_first_block(FReadBufferSet *set)
	{
	FReadBuffer *workbuf = &set->buffers[0];
	_buffer_acquire(&workbuf->state_data,BUFF_STATE_FILL);
	return workbuf->size ? 0 : (set->eof = 1);
	}

int fbr_next_block(FReadBufferSet *set)
	{
	if (set->mbuf->state_data.state == BUFF_STATE_STOP)
		return set->eof = 1;
	_buffer_release(&set->mbuf->state_data,BUFF_STATE_FILL);
	set->mbuf_num = 1 - set->mbuf_num;
	set->mbuf = &set->buffers[set->mbuf_num];
	set->mbuf_pos = 0;
	_buffer_acquire(&set->mbuf->state_data,BUFF_STATE_FILL);
	return set->mbuf->size ? 0 : (set->eof = 1);
	}

char *fbr_get_key_ref(FReadBufferSet *set)
	{
	FReadBuffer *obuf = set->mbuf;
	if (obuf->size - set->mbuf_pos >= KEY_BUFFER_SIZE || obuf->state_data.state == BUFF_STATE_STOP)
		return &obuf->data[KEY_BUFFER_SIZE + set->mbuf_pos];
	int tocpy = set->mbuf->size - set->mbuf_pos;
	set->mbuf_num = 1 - set->mbuf_num;
	set->mbuf = &set->buffers[set->mbuf_num];
	_buffer_acquire(&set->mbuf->state_data,BUFF_STATE_FILL);
	if (tocpy)
		memcpy(&set->mbuf->data[KEY_BUFFER_SIZE - tocpy],&obuf->data[KEY_BUFFER_SIZE + set->mbuf_pos],tocpy);
	_buffer_release(&obuf->state_data,BUFF_STATE_FILL);
	set->mbuf_pos = -tocpy;
	return &set->mbuf->data[KEY_BUFFER_SIZE + set->mbuf_pos];
	}

static void *write_thread(void *param)
	{
	FWriteBufferSet *set = (FWriteBufferSet *)param;
	unsigned cur_buf_num = 0;
	FWriteBuffer *workbuf = &set->buffers[0];
write_thread_repeat:
	_buffer_acquire(&workbuf->state_data,BUFF_STATE_FILL);
	file_write(set->fd,workbuf->data,workbuf->filled_size);
	if (workbuf->state_data.state == BUFF_STATE_STOP)
		return (void *)0;
	workbuf->filled_size = 0;
	_buffer_release(&workbuf->state_data,BUFF_STATE_FILL);
	cur_buf_num = 1 - cur_buf_num;
	workbuf = &set->buffers[cur_buf_num];
	goto write_thread_repeat;
	}

static int _init_wbuffer(FWriteBuffer *buf)
	{
	buf->total_size = BUFFER_SIZE + WRITE_BUFFER_GROW;
	buf->filled_size = 0;
	buf->state_data.state = BUFF_STATE_FREE;
	buf->data = mmap(NULL, buf->total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	return (buf->data == MAP_FAILED || _init_state(&buf->state_data)) ? 1 : 0;
	}

FWriteBufferSet *fbw_create(const char *filename)
	{
	FWriteBufferSet *rv;
	if (posix_memalign((void **)&rv,CACHE_LINE_SIZE,sizeof(FReadBufferSet)))
		return NULL;
	rv->thread_started = 0;
	rv->mbuf_num = 0;
	rv->commited_pos = 0;
	rv->mbuf = &rv->buffers[0];
	if (filename)
		rv->fd = open(filename,O_WRONLY | O_TRUNC | O_CREAT,S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH), rv->no_close = 0;
	else
		rv->fd = STDOUT_FILENO, rv->no_close = 1;
	int init_res = _init_wbuffer(&rv->buffers[0]) + _init_wbuffer(&rv->buffers[1]);
	if (rv->fd == -1 || init_res) 
		return fbw_finish(rv),NULL;
	if (pthread_create(&rv->io_thread,NULL,write_thread,rv))
		return fbw_finish(rv),NULL;
	rv->thread_started = 1;
	return rv;
	}

static void _free_wbuffer(FWriteBuffer *buf)
	{
	if (buf->state_data.state != BUFF_STATE_FREE)
		{
		pthread_cond_destroy(&buf->state_data.state_changed);
		pthread_mutex_destroy(&buf->state_data.buffer_lock);
		}
	if (buf->data != MAP_FAILED)
		munmap(buf->data,buf->total_size);
	}

void fbw_finish(FWriteBufferSet *set)
	{
	if (!set)
		return;
	if (set->thread_started)
		{
		_buffer_release(&set->mbuf->state_data,BUFF_STATE_STOP);
		pthread_join(set->io_thread,NULL);
		}
	if (set->fd != -1 && !set->no_close)
		close(set->fd);
	_free_wbuffer(&set->buffers[0]);
	_free_wbuffer(&set->buffers[1]);
	free(set);
	}

void fbw_check_space(FWriteBufferSet *set)
	{
	if (set->mbuf->filled_size + WRITE_BUFFER_GROW  > set->mbuf->total_size)
		{
		set->mbuf->data = mremap(set->mbuf->data,set->mbuf->total_size,set->mbuf->total_size + WRITE_BUFFER_GROW,MREMAP_MAYMOVE);
		set->mbuf->total_size += WRITE_BUFFER_GROW;
		}
	}

void fbw_commit(FWriteBufferSet *set)
	{
	if (set->mbuf->filled_size < BUFFER_SIZE)
		{
		set->commited_pos = set->mbuf->filled_size;
		return;
		}
	unsigned rest_size = set->mbuf->filled_size & (DISK_PAGE_BYTES - 1);
	set->mbuf->filled_size &= ~(DISK_PAGE_BYTES - 1);
	char *data_rest = &set->mbuf->data[set->mbuf->filled_size];
	_buffer_release(&set->mbuf->state_data,BUFF_STATE_PROCESS);
	set->mbuf_num = 1 - set->mbuf_num;
	set->mbuf = &set->buffers[set->mbuf_num];
	_buffer_acquire(&set->mbuf->state_data,BUFF_STATE_PROCESS);
	if (rest_size)
		memcpy(set->mbuf->data,data_rest,rest_size);
	set->commited_pos = set->mbuf->filled_size = rest_size;
	}
