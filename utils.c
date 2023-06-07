/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>

#include "utils.h"

#ifdef FILE_LOG
#include <stdarg.h>
#include <sys/syscall.h>

#define _FNAME(S) #S
#define FNAME(S) _FNAME(S)

FILE *logFile = NULL;

void setupLog(void)
	{
	logFile = fopen(FNAME(FILE_LOG),"w");
	setvbuf(logFile,NULL,_IONBF,0);
	}

pid_t get_thread_id(void)
	{
#ifdef SYS_gettid
	return syscall(SYS_gettid);
#else
	return 0;
#endif
	}

void addLog(char *message)
	{
	if (!logFile) return;

	struct timespec t1;		
	struct tm tmData;
	time_t ctm;
		
	clock_gettime(CLOCK_REALTIME,&t1);
	ctm = t1.tv_sec;
	localtime_r(&ctm, &tmData);
		
	fprintf(logFile,"%.4d-%.2d-%.2d %.2d-%.2d-%.2d.%.6u: [%d %d] %s\n",tmData.tm_year + 1900,tmData.tm_mon,tmData.tm_mday,
					tmData.tm_hour,tmData.tm_min,tmData.tm_sec,(unsigned)(t1.tv_nsec / 1000),getpid(),get_thread_id(),message);
	}
	
void formattedLog(char *format,...)
	{
	if (!logFile) return;

	va_list args;
	struct timespec t1;
	struct tm tmData;
	time_t ctm;

	clock_gettime(CLOCK_REALTIME,&t1);
	ctm = t1.tv_sec;
	localtime_r(&ctm, &tmData);

	fprintf(logFile,"%.4d-%.2d-%.2d %.2d:%.2d:%.2d.%.6u: [%d %d] ",tmData.tm_year + 1900,tmData.tm_mon,tmData.tm_mday,
						tmData.tm_hour,tmData.tm_min,tmData.tm_sec,(unsigned)(t1.tv_nsec / 1000),getpid(),get_thread_id());
	va_start (args, format);
	vfprintf (logFile,format, args);
	va_end (args);
	}
	
void closeLog(void)
	{
	if (logFile) fclose(logFile);
	}

#endif

#ifdef MEMORY_CHECK
void failureCheck(int res,const char *msg,const char *fname,int lnum,const char *func)
	{
	if (res)
		{
		fprintf(stderr,"%s at %s:%d in %s\n",msg,fname,lnum,func);
		__asm__ volatile("int $0x03");
		exit(1);
		}
	}
#endif

size_t file_read(int fd,void *buf, size_t size)
	{
	size_t tcnt = 0;
	ssize_t cnt;
	while (tcnt < size)
		{
		size_t todo = (size - tcnt > SSIZE_MAX) ? SSIZE_MAX : size - tcnt;
		if (!(cnt = read(fd,(char *)buf + tcnt,todo))) 
			return tcnt;
		if (cnt == -1)
			{
			if (errno == EINTR) continue;
			return -1;
			}
		tcnt += cnt;
		}
	return tcnt;
	}

size_t file_write(int fd,void *buf, size_t size)
	{
	size_t tcnt = 0;
	ssize_t cnt;
	while (tcnt < size)
		{
		size_t todo = (size - tcnt > SSIZE_MAX) ? SSIZE_MAX : size - tcnt;
		if (!(cnt = write(fd,(char *)buf + tcnt,todo))) 
			return tcnt;
		if (cnt == -1)
			{
			if (errno == EINTR) continue;
			return -1;
			}
		tcnt += cnt;
		}
	return tcnt;
	}

int file_lock(int fd,int operation)
	{
	int res;
	while (((res = flock(fd,operation))) == EINTR);
	return res;
	}

off_t file_size(char *filename)
	{
	struct stat st;
	if (!filename || !filename[0] || stat(filename, &st) != 0 || !S_ISREG(st.st_mode))
		return -1;
	return st.st_size;
	}
	
int file_link(const char *oldname, const char *newname)
	{
	int res = unlink(newname);
	if (res && errno != ENOENT)
		return res;
	return link(oldname,newname);
	}