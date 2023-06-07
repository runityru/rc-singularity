/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#ifndef _UTILS_H
#define _UTILS_H

#ifdef FILE_LOG

#include <stdio.h>

void setupLog(void);
void closeLog(void);
void addLog(char *message);
void formattedLog(char *format,...);

	#define LOG_RECORD(A) addLog(A)
	#define FORMATTED_LOG(...) formattedLog(__VA_ARGS__)
	#ifdef LOG_LOCKS
		#define FORMATTED_LOG_LOCKS(...) formattedLog(__VA_ARGS__)
	#else
		#define FORMATTED_LOG_LOCKS(...)
	#endif
	#ifdef LOG_MEMORY
		#define FORMATTED_LOG_MEMORY(...) formattedLog(__VA_ARGS__)
	#else
		#define FORMATTED_LOG_MEMORY(...)
	#endif
	#ifdef LOG_OPERATION
		#define FORMATTED_LOG_OPERATION(...) formattedLog(__VA_ARGS__)
	#else
		#define FORMATTED_LOG_OPERATION(...)
	#endif
	#ifdef LOG_HEADER
		#define FORMATTED_LOG_HEADER(...) formattedLog(__VA_ARGS__)
	#else
		#define FORMATTED_LOG_HEADER(...)
	#endif
	#define INIT_LOG setupLog()
	#define CLOSE_LOG closeLog()

#else
	#define LOG_RECORD(A)
	#define FORMATTED_LOG(...)
	#define FORMATTED_LOG_MEMORY(...)
	#define FORMATTED_LOG_LOCKS(...)
	#define FORMATTED_LOG_OPERATION(...)
	#define FORMATTED_LOG_HEADER(...)
	#define INIT_LOG
	#define CLOSE_LOG
#endif

#include <stdlib.h>

#ifdef MEMORY_CHECK
void failureCheck(int res,const char *msg,const char *fname,int lnum,const char *funcname);
#define FAILURE_CHECK(A,B) failureCheck(A,B,__FILE__,__LINE__,__func__)
#else
#define FAILURE_CHECK(A,B)
#endif

static inline size_t align_up(size_t size,size_t align)
	{
	return (size / align + ((size % align) ? 1 : 0)) * align; 
	}

size_t file_read(int fd,void *buf, size_t size);
size_t file_write(int fd,void *buf, size_t size);
int file_lock(int fd,int operation);
off_t file_size(char *filename);

int file_link(const char *oldname, const char *newname);

#endif