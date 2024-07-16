/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <stddef.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "../defines.h"
#include "../filebuf.h"
#include "../utils.h"

static const char *pattern = "ABCDEFGHIJK";

// FReadBufferSet
// 1 no file
// 2 empty file, fbr_first_block
//(3) BUFFER_SIZE - 10 bytes, 
// 3_1 fbr_first_block + fbr_next_block
// 3_2 fbr_first_block + fbr_get_key_ref
//(4) BUFFER_SIZE
// 4_1 fbr_first_block + fbr_next_block
// 4_2 fbr_first_block + fbr_get_key_ref
//(5) BUFFER_SIZE + 10 bytes
// 5_1 fbr_first_block + fbr_next_block
// 5_2 fbr_first_block + fbr_get_key_ref
// 5_3 fbr_first_block twice + fbr_next_block
// 6 BUFFER_SIZE * 2 fbr_first_block + fbr_next_block
// 7 BUFFER_SIZE * 3 fbr_first_block and stop

void create_test_file(char *filename,int size)
	{
	int pos = 0;
	FILE *f = fopen(filename,"w");
	while (pos + 11 <= size)
		fputs(pattern,f),pos += 11;
	fprintf(f,"%.*s",size - pos,pattern);
	fclose(f);
	}

int test_pattern(char *buf,int size)
	{
	int i;
	for (i = 0; i <= size - 11; i += 11)
		if (strncmp(&buf[i],pattern,11))
			return 1;
	if (strncmp(&buf[i],pattern,size - i))
		return 1;
	return 0;
	}

int read_buffer_test_1(FReadBufferSet *rbs,char *res_buffer)
	{
	return rbs ? 1 : 0;
	}

int read_buffer_test_2(FReadBufferSet *rbs,char *res_buffer)
	{
	if (!rbs)
		return 1;
	if (!fbr_first_block(rbs))
		return 1;
	if (!fbr_next_block(rbs))
		return 1;
	return 0;
	}

int read_buffer_test_3_1(FReadBufferSet *rbs,char *res_buffer)
	{
	if (fbr_first_block(rbs))
		return 1;
	char *block = fbr_get_ref(rbs);
	int block_size = fbr_get_size(rbs);
	if (block_size != BUFFER_SIZE - 10) 
		return 1;
	memcpy(res_buffer,block,block_size);
	return fbr_next_block(rbs) ? 0 : 1;
	}

int read_buffer_test_3_2(FReadBufferSet *rbs,char *res_buffer)
	{
	if (fbr_first_block(rbs))
		return 1;
	char *block = fbr_get_ref(rbs);
	int block_size = fbr_get_size(rbs);
	if (block_size != BUFFER_SIZE - 10) 
		return 1;
	memcpy(res_buffer,block,BUFFER_SIZE - 20);
	fbr_shift_pos(rbs,BUFFER_SIZE - 20);
	char *block2 = fbr_get_key_ref(rbs);
	block_size = fbr_get_size(rbs);
	if (block2 != block + BUFFER_SIZE - 20 || block_size != 10)
		return 1;
	memcpy(res_buffer + BUFFER_SIZE - 20,block2,block_size);
	return 0;
	}

int read_buffer_test_4_1(FReadBufferSet *rbs,char *res_buffer)
	{
	if (fbr_first_block(rbs))
		return 1;
	char *block = fbr_get_ref(rbs);
	int block_size = fbr_get_size(rbs);
	if (block_size != BUFFER_SIZE) 
		return 1;
	memcpy(res_buffer,block,block_size);
	return fbr_next_block(rbs) ? 0 : 1;
	}

int read_buffer_test_4_2(FReadBufferSet *rbs,char *res_buffer)
	{
	if (fbr_first_block(rbs))
		return 1;
	char *block = fbr_get_ref(rbs);
	int block_size = fbr_get_size(rbs);
	if (block_size != BUFFER_SIZE) 
		return 1;
	memcpy(res_buffer,block,BUFFER_SIZE - 20);
	fbr_shift_pos(rbs,BUFFER_SIZE - 20);
	char *block2 = fbr_get_key_ref(rbs);
	block_size = fbr_get_size(rbs);
	if (block2 == block + BUFFER_SIZE - 20 || block_size != 20)
		return 1;
	memcpy(res_buffer + BUFFER_SIZE - 20,block2,block_size);
	return 0;
	}

int read_buffer_test_5_1(FReadBufferSet *rbs,char *res_buffer)
	{
	if (fbr_first_block(rbs))
		return 1;
	char *block = fbr_get_ref(rbs);
	int block_size = fbr_get_size(rbs);
	if (block_size != BUFFER_SIZE) 
		return 1;
	memcpy(res_buffer,block,block_size);
	if (fbr_next_block(rbs))
		return 1;
	block = fbr_get_ref(rbs);
	block_size = fbr_get_size(rbs);
	if (block_size != 10)
		return 1;
	memcpy(res_buffer + BUFFER_SIZE,block,block_size);
	return 0;
	}

int read_buffer_test_5_2(FReadBufferSet *rbs,char *res_buffer)
	{
	if (fbr_first_block(rbs))
		return 1;
	char *block = fbr_get_ref(rbs);
	int block_size = fbr_get_size(rbs);
	if (block_size != BUFFER_SIZE) 
		return 1;
	memcpy(res_buffer,block,BUFFER_SIZE - 20);
	fbr_shift_pos(rbs,BUFFER_SIZE - 20);
	char *block2 = fbr_get_key_ref(rbs);
	block_size = fbr_get_size(rbs);
	if (block2 == block + BUFFER_SIZE - 20 || block_size != 30)
		return 1;
	memcpy(res_buffer + BUFFER_SIZE - 20,block2,block_size);
	return 0;
	}

int read_buffer_test_5_3(FReadBufferSet *rbs,char *res_buffer)
	{
	if (fbr_first_block(rbs))
		return 1;
	char *block = fbr_get_ref(rbs);
	int block_size = fbr_get_size(rbs);
	if (block_size != BUFFER_SIZE) 
		return 1;
	if (fbr_first_block(rbs))
		return 1;
	char *block2 = fbr_get_ref(rbs);
	if (block2 != block) 
		return 1;
	block_size = fbr_get_size(rbs);
	if (block_size != BUFFER_SIZE) 
		return 1;
	memcpy(res_buffer,block,BUFFER_SIZE - 20);
	fbr_shift_pos(rbs,BUFFER_SIZE - 20);
	block2 = fbr_get_key_ref(rbs);
	block_size = fbr_get_size(rbs);
	if (block2 == block + BUFFER_SIZE - 20 || block_size != 30)
		return 1;
	memcpy(res_buffer + BUFFER_SIZE - 20,block2,block_size);
	return 0;
	}

int read_buffer_test_6(FReadBufferSet *rbs,char *res_buffer)
	{
	if (fbr_first_block(rbs))
		return 1;
	char *block = fbr_get_ref(rbs);
	int block_size = fbr_get_size(rbs);
	if (block_size != BUFFER_SIZE) 
		return 1;
	memcpy(res_buffer,block,block_size);
	if (fbr_next_block(rbs))
		return 1;
	block = fbr_get_ref(rbs);
	block_size = fbr_get_size(rbs);
	if (block_size != BUFFER_SIZE) 
		return 1;
	memcpy(res_buffer + BUFFER_SIZE,block,block_size);
	return fbr_next_block(rbs) ? 0 : 1;
	}

int read_buffer_test_7(FReadBufferSet *rbs,char *res_buffer)
	{
	if (fbr_first_block(rbs))
		return 1;
	char *block = fbr_get_ref(rbs);
	int block_size = fbr_get_size(rbs);
	if (block_size != BUFFER_SIZE) 
		return 1;
	memcpy(res_buffer,block,block_size);
	return 0;
	}

typedef int (*CReadBufferTest)(FReadBufferSet *rbs,char *buf);
typedef struct FReadBufferSetTestTg
	{
	char *name;
	int size;
	CReadBufferTest test;
	int res_size;
	} FReadBufferSetTest;

int read_buffer_tests(void)
	{
	int i,rv = 0;
	FReadBufferSet *rbs;
	char result[BUFFER_SIZE * 3];

	FReadBufferSetTest tests[] = {
		{"read_buffer_1",-1,read_buffer_test_1,-1},
		{"read_buffer_2",0,read_buffer_test_2,0},
		{"read_buffer_3_1",BUFFER_SIZE - 10,read_buffer_test_3_1,BUFFER_SIZE - 10},
		{"read_buffer_3_2",BUFFER_SIZE - 10,read_buffer_test_3_2,BUFFER_SIZE - 10},
		{"read_buffer_4_1",BUFFER_SIZE,read_buffer_test_4_1,BUFFER_SIZE},
		{"read_buffer_4_2",BUFFER_SIZE,read_buffer_test_4_2,BUFFER_SIZE},
		{"read_buffer_5_1",BUFFER_SIZE + 10,read_buffer_test_5_1,BUFFER_SIZE + 10},
		{"read_buffer_5_2",BUFFER_SIZE + 10,read_buffer_test_5_2,BUFFER_SIZE + 10},
		{"read_buffer_5_3",BUFFER_SIZE + 10,read_buffer_test_5_3,BUFFER_SIZE + 10},
		{"read_buffer_6",BUFFER_SIZE * 2,read_buffer_test_6,BUFFER_SIZE * 2},
		{"read_buffer_7",BUFFER_SIZE * 3,read_buffer_test_7,BUFFER_SIZE},
		};
	unsigned tcnt = sizeof(tests) / sizeof(FReadBufferSetTest);
	for (i = 0; i < tcnt; i++)
		{
		memset(result,0,BUFFER_SIZE * 3);
		if (tests[i].size >= 0)
			create_test_file(tests[i].name,tests[i].size);
		rbs = fbr_create(tests[i].name);
		if (tests[i].test(rbs,result))
			{
			printf("Test %s failed\n",tests[i].name);
			rv = 1;
			}
		else if (tests[i].size >= 0 && test_pattern(result,tests[i].res_size))
			{
			printf("Test %s failed: pattern broken\n",tests[i].name);
			rv = 1;
			}
		fbr_finish(rbs);
		if (tests[i].size >= 0)
			unlink(tests[i].name);
		}
	return rv;
	}

// FWriteBufferSet
// 1 Файл пуст
//(2) fbw_check_space + запись WRITE_BUFFER_GROW + commit
// 2_1 Осталось больше-равно WRITE_BUFFER_GROW
// 2_2 Осталось меньше WRITE_BUFFER_GROW
//(3) fbw_commit
// 3_1 Заполнено меньше BUFFER_SIZE
//(3.2) Заполнено больше-равно BUFFER_SIZE
// 3_2_1 Заполнено целое число дисковых страниц.
// 3_2_2 Заполнено нецелое число дисковых страниц

char *make_pattern_buffer(int size)
	{
	if (!size)
		return NULL;
	char *buf = malloc(size);
	int pos = 0;
	while (pos + 11 <= size)
		strncpy(&buf[pos],pattern,11), pos += 11;
	if (pos < size)
		strncpy(&buf[pos],pattern,size - pos);
	return buf;
	}

int test_pattern_file(char *filename,int size)
	{
	int fsize = file_size(filename);
	if (fsize != size)
		return 1;
	if (!size)
		return 0;
	char *buf = malloc(size);
	FILE *f = fopen(filename,"r");
	if (fread(buf,1,size,f) != size)
		return fclose(f),free(buf),1;
	fclose(f);
	int i;
	for (i = 0; i <= size - 11; i += 11)
		if (strncmp(&buf[i],pattern,11))
			return free(buf),1;
	if (strncmp(&buf[i],pattern,size - i))
		return free(buf),1;
	free(buf);
	return 0;
	}

int write_buffer_test_1(FWriteBufferSet *wbs,char *buf)
	{
	return 0;
	}

int write_buffer_test_2_1(FWriteBufferSet *wbs,char *buf)
	{
	char *pos = fbw_get_ref(wbs);
	memcpy(pos,buf,10);
	fbw_shift_pos(wbs,10);
	fbw_check_space(wbs);
	pos = fbw_get_ref(wbs);
	memcpy(pos,&buf[10],WRITE_BUFFER_GROW);
	fbw_shift_pos(wbs,WRITE_BUFFER_GROW);
	fbw_commit(wbs);
	return 0;
	}

int write_buffer_test_2_2(FWriteBufferSet *wbs,char *buf)
	{
	char *pos = fbw_get_ref(wbs);
	memcpy(pos,buf,BUFFER_SIZE + 10);
	fbw_shift_pos(wbs,BUFFER_SIZE + 10);
	fbw_check_space(wbs);
	pos = fbw_get_ref(wbs);
	memcpy(pos,&buf[BUFFER_SIZE + 10],WRITE_BUFFER_GROW);
	fbw_shift_pos(wbs,WRITE_BUFFER_GROW);
	fbw_commit(wbs);
	return 0;
	}

int write_buffer_test_3_1(FWriteBufferSet *wbs,char *buf)
	{
	char *pos = fbw_get_ref(wbs);
	memcpy(pos,buf,10);
	fbw_shift_pos(wbs,10);
	fbw_commit(wbs);
	return 0;
	}

int write_buffer_test_3_2_1(FWriteBufferSet *wbs,char *buf)
	{
	char *pos = fbw_get_ref(wbs);
	memcpy(pos,buf,BUFFER_SIZE + DISK_PAGE_BYTES);
	fbw_shift_pos(wbs,BUFFER_SIZE + DISK_PAGE_BYTES);
	fbw_commit(wbs);
	return 0;
	}

int write_buffer_test_3_2_2(FWriteBufferSet *wbs,char *buf)
	{
	char *pos = fbw_get_ref(wbs);
	memcpy(pos,buf,BUFFER_SIZE + DISK_PAGE_BYTES + 10);
	fbw_shift_pos(wbs,BUFFER_SIZE + DISK_PAGE_BYTES + 10);
	fbw_commit(wbs);
	return 0;
	}

typedef int (*CWriteBufferTest)(FWriteBufferSet *wbs,char *buf);
typedef struct FWriteBufferSetTestTg
	{
	char *name;
	int size;
	CWriteBufferTest test;
	} FWriteBufferSetTest;

int write_buffer_tests(void)
	{
	int i,rv = 0;
	FWriteBufferSet *wbs;

	FWriteBufferSetTest tests[] = {
		{"write_buffer_1",0,write_buffer_test_1},
		{"write_buffer_2_1",10 + WRITE_BUFFER_GROW,write_buffer_test_2_1},
		{"write_buffer_2_2",BUFFER_SIZE + 10 + WRITE_BUFFER_GROW,write_buffer_test_2_2},
		{"write_buffer_3_1",10,write_buffer_test_3_1},
		{"write_buffer_3_2_1",BUFFER_SIZE + DISK_PAGE_BYTES,write_buffer_test_3_2_1},
		{"write_buffer_3_2_2",BUFFER_SIZE + DISK_PAGE_BYTES + 10,write_buffer_test_3_2_2},
		};
	unsigned tcnt = sizeof(tests) / sizeof(FWriteBufferSetTest);
	for (i = 0; i < tcnt; i++)
		{
		char *to_write = make_pattern_buffer(tests[i].size);
		wbs = fbw_create(tests[i].name);
		int res = tests[i].test(wbs,to_write);
		fbw_finish(wbs);
		if (res)
			{
			printf("Test %s failed\n",tests[i].name);
			rv = 1;
			}
		else if (tests[i].size >= 0 && test_pattern_file(tests[i].name,tests[i].size))
			{
			printf("Test %s failed: pattern broken\n",tests[i].name);
			rv = 1;
			}
		unlink(tests[i].name);
		if (to_write)
			free(to_write);
		}
	return rv;
	}

int main(void)
	{
	return read_buffer_tests() + write_buffer_tests();
	}