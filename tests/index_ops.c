/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "../defines.h"
#include "../index.h"
#include "../codec.h"
#include "../locks.h"

char *collisions[20];
char *collisions2[20];

#define TF_SET_PHANTOM 1
#define TF_USE_PHANTOM 2

void test_make_tdata(FSingSet *index,char *key_source,int vsize,unsigned char *value,FTransformData *tdata,unsigned flags)
	{
	tdata->value_source = value;
	tdata->value_size = vsize;
	tdata->head.fields.chain_stop = 1;
	tdata->head.fields.diff_or_phantom_mark = (flags & TF_SET_PHANTOM) ? 1 : 0;
	tdata->use_phantom = (flags & TF_USE_PHANTOM) ? 1 : 0;
	index->transform(key_source,MAX_KEY_SOURCE,tdata);
	tdata->hash = index->hashtable_size;
	cd_encode(tdata);
	}

void test_process_res(FSingSet *index,int res,FTransformData *tdata)
	{
	if (res & KS_CHANGED)
		{
		if (tdata->old_key_rest_size)
			{
			lck_waitForReaders(index->lock_set,LCK_NO_DELETION);
			idx_general_free(index,tdata->old_key_rest,tdata->old_key_rest_size);
			}
		lck_memoryUnlock(index);
		}
	}

int test_set_key(FSingSet *index,char *key_source,int vsize,unsigned char *value,unsigned flags)
	{
	FTransformData tdata = {0};

	test_make_tdata(index,key_source,vsize,value,&tdata,flags);
	int rv = idx_key_try_set(index,&tdata,KS_ADDED | KS_DELETED);
	if (!rv)
		rv = idx_key_set(index,&tdata,KS_ADDED | KS_DELETED);
	test_process_res(index,rv,&tdata);
	return rv;
	}

int test_del_key(FSingSet *index,char *key_source,unsigned flags)
	{
	FTransformData tdata;

	test_make_tdata(index,key_source,0,NULL,&tdata,flags);

	int rv = idx_key_del(index,&tdata);
	if (rv & KS_CHANGED)
		lck_memoryUnlock(index);
	return rv;
	}

int test_get_key(FSingSet *index,char *key_source,unsigned *value_dst_size,void *value_dst,unsigned flags)
	{
	FTransformData tdata = {0};
	FReaderLock rlock = READER_LOCK_INIT;

	test_make_tdata(index,key_source,0,NULL,&tdata,flags);
   return idx_key_get(index,&tdata,&rlock,value_dst,value_dst_size);
	}

// Размещение тела ключа (_alloc_and_set_rest)
//	(1) Ключ без тела
// 1_1 Нет значения
// 1_2 Есть значение
//(2) Ключ с телом
// 2_1 Нет значения
// 2_2 Есть значение

int alloc_rest_test_1_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = test_set_key(index,"a",0,NULL,0);
	return (rv & KS_ADDED)?0:1;
	}

int alloc_rest_test_1_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	uint32_t val = 0;
	int rv = test_set_key(index,"a",4,(unsigned char*)&val,0);
	return (rv & KS_ADDED)?0:1;
	}

int alloc_rest_test_2_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = test_set_key(index,"abcde",0,NULL,0);
	return (rv & KS_ADDED)?0:1;
	}

int alloc_rest_test_2_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	uint32_t val = 0;
	int rv = test_set_key(index,"abcde",4,(unsigned char*)&val,0);
	return (rv & KS_ADDED)?0:1;
	}

// Замена значения у ключа (_replace_value)
//(1) У старого нет значения
// 1_1 У нового нет значения
// 1_2 У нового есть значение
//(2) У старого есть значение
// 2_1 У нового нет значения
// 2_2 У нового есть значение, не совпадает
// 2_3 У нового есть значение, совпадает

element_type replace_value_prep_1(FSingSet *index,int *res_mem)
	{
	test_set_key(index,"abcde",0,NULL,0);
	return 0;
	}

int replace_value_test_1_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (test_set_key(index,"abcde",0,NULL,0) & KS_CHANGED)?1:0;
	}

int replace_value_test_1_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	uint32_t val = 0;
	return (test_set_key(index,"abcde",4,(unsigned char*)&val,0) & KS_CHANGED)?0:1;
	}

element_type replace_value_prep_2(FSingSet *index,int *res_mem)
	{
	uint32_t val = 0;
	test_set_key(index,"abcde",4,(unsigned char*)&val,0);
	return 0;
	}

int replace_value_test_2_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (test_set_key(index,"abcde",0,NULL,0) & KS_CHANGED)?0:1;
	}

int replace_value_test_2_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	uint32_t val = 1;
	return (test_set_key(index,"abcde",4,(unsigned char*)&val,0) & KS_CHANGED)?0:1;
	}

int replace_value_test_2_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	uint32_t val = 0;
	return (test_set_key(index,"abcde",4,(unsigned char*)&val,0) & KS_CHANGED)?1:0;
	}

// Замена значения у ключа в наборе с фантомами (_replace_phantom_value)
// 1 Удаление удаленного ключа
//(2) Замена обычного ключа
// 2_1 В старом было нормальное значение, но не было удаленного
// 2_2 В старом было нормальное значение, и было удаленное
// 2_3 В старом было не было значения
// 2_4 Значение совпадает со старым
// 2_5 В старом было нормальное значение, заменяем на пустое
// (3) Замена удаленного ключа на обычный
// 3_1 В удаленном было значение
// 3_2 В удаленном не было значения
// (4) Замена обычного ключа на удаленный
// 4_1 Значения не было
// 4_2 Было значение, но не было фантомного
// 4_3 Фантомное пустое, ключ длинный
// 4_4_1 Фантомное пустое, ключ в 1 элемент, есть значение в удаленном
// 4_4_2 Фантомное пустое, ключ в 1 элемент, нет значения в удаленном
// 4_5 Фантомное пустое, ключ в 0 элемент
// 4_6 Фантомное не пустое

element_type replace_phantom_value_prep_1(FSingSet *index,int *res_mem)
	{
	test_set_key(index,"abcde",0,NULL,TF_SET_PHANTOM);
	return 0;
	}

int replace_phantom_value_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int val = 0;
	int rv = test_set_key(index,"abcde",4,(unsigned char *)&val,TF_SET_PHANTOM);
	if (rv != KS_SUCCESS)
		return 1;
	unsigned vsize = 4;
	if (test_get_key(index,"abcde",&vsize,&val,0) != RESULT_KEY_NOT_FOUND)
		return 2;
	vsize = 4;
	if (test_get_key(index,"abcde",&vsize,&val,TF_USE_PHANTOM) || vsize)
		return 3;
	return 0;
	}

element_type replace_phantom_value_prep_2_1(FSingSet *index,int *res_mem)
	{
	int val = 0;
	test_set_key(index,"abcde",4,(unsigned char *)&val,0);
	return 0;
	}

int replace_phantom_value_test_2_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int val = 1;
	int rv = test_set_key(index,"abcde",4,(unsigned char *)&val,0);
	if (!(rv & KS_CHANGED))
		return 1;
	unsigned vsize = 4;
	val = 2;
	if (test_get_key(index,"abcde",&vsize,&val,0) || val != 1)
		return 2;
	vsize = 4, val = 2;
	if (test_get_key(index,"abcde",&vsize,&val,TF_USE_PHANTOM) || val != 0)
		return 3;
	return 0;
	}

element_type replace_phantom_value_prep_2_2(FSingSet *index,int *res_mem)
	{
	int val = 0;
	test_set_key(index,"abcde",0,NULL,0);
	test_set_key(index,"abcde",4,(unsigned char *)&val,0);
	return 0;
	}

int replace_phantom_value_test_2_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int val = 1;
	int rv = test_set_key(index,"abcde",4,(unsigned char *)&val,0);
	if (!(rv & KS_CHANGED))
		return 1;
	unsigned vsize = 4;
	val = 2;
	if (test_get_key(index,"abcde",&vsize,&val,0) || val != 1)
		return 2;
	vsize = 4;
	if (test_get_key(index,"abcde",&vsize,&val,TF_USE_PHANTOM) || vsize)
		return 3;
	return 0;
	}

element_type replace_phantom_value_prep_2_3(FSingSet *index,int *res_mem)
	{
	test_set_key(index,"abcde",0,NULL,0);
	return 0;
	}

int replace_phantom_value_test_2_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int val = 1;
	int rv = test_set_key(index,"abcde",4,(unsigned char *)&val,0);
	if (!(rv & KS_CHANGED))
		return 1;
	unsigned vsize = 4;
	val = 2;
	if (test_get_key(index,"abcde",&vsize,&val,0) || val != 1)
		return 2;
	vsize = 4;
	if (test_get_key(index,"abcde",&vsize,&val,TF_USE_PHANTOM) || vsize)
		return 3;
	return 0;
	}

element_type replace_phantom_value_prep_2_4(FSingSet *index,int *res_mem)
	{
	int val = 0;
	test_set_key(index,"abcde",4,(unsigned char *)&val,0);
	return 0;
	}

int replace_phantom_value_test_2_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int val = 0;
	int rv = test_set_key(index,"abcde",4,(unsigned char *)&val,0);
	if (rv & KS_CHANGED)
		return 1;
	unsigned vsize = 4;
	val = 2;
	if (test_get_key(index,"abcde",&vsize,&val,0) || val != 0)
		return 2;
	vsize = 4;
	if (test_get_key(index,"abcde",&vsize,&val,TF_USE_PHANTOM) != RESULT_KEY_NOT_FOUND)
		return 3;
	return 0;
	}

element_type replace_phantom_value_prep_2_5(FSingSet *index,int *res_mem)
	{
	int val = 1;
	test_set_key(index,"abcde",4,(unsigned char *)&val,0);
	return 0;
	}

int replace_phantom_value_test_2_5(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = test_set_key(index,"abcde",0,NULL,0);
	if (!(rv & KS_CHANGED))
		return 1;
	unsigned vsize = 4,val = 2;
	if (test_get_key(index,"abcde",&vsize,&val,0) || vsize)
		return 2;
	vsize = 4, val = 2;
	if (test_get_key(index,"abcde",&vsize,&val,TF_USE_PHANTOM) || vsize != 4 || val != 1)
		return 3;
	return 0;
	}

element_type replace_phantom_value_prep_3_1(FSingSet *index,int *res_mem)
	{
	int val = 0;
	test_set_key(index,"abcde",4,(unsigned char *)&val,TF_SET_PHANTOM);
	return 0;
	}

int replace_phantom_value_test_3_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int val = 0;
	int rv = test_set_key(index,"abcde",4,(unsigned char *)&val,0);
	return (rv & KS_CHANGED) ? 0 : 1;
	}

element_type replace_phantom_value_prep_3_2(FSingSet *index,int *res_mem)
	{
	test_set_key(index,"abcde",0,NULL,TF_SET_PHANTOM);
	return 0;
	}

int replace_phantom_value_test_3_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int val = 0;
	int rv = test_set_key(index,"abcde",4,(unsigned char *)&val,0);
	return (rv & KS_CHANGED) ? 0 : 1;
	}

element_type replace_phantom_value_prep_4_1(FSingSet *index,int *res_mem)
	{
	test_set_key(index,"abcde",0,NULL,0);
	return 0;
	}

element_type replace_phantom_value_prep_4_2(FSingSet *index,int *res_mem)
	{
	int val = 0;
	test_set_key(index,"abcde",4,(unsigned char *)&val,0);
	return 0;
	}

int replace_phantom_value_test_4_1(FSingSet *index,int *res_mem,element_type prep_data)
	{ // and 4_2
	int val = 0;
	int rv = test_set_key(index,"abcde",4,(unsigned char *)&val,TF_SET_PHANTOM);
	return (rv == (KS_MARKED | KS_SUCCESS)) ? 0 : 1;
	}

element_type replace_phantom_value_prep_4_3(FSingSet *index,int *res_mem)
	{
	int val = 0;
	test_set_key(index,"abcdefghijklm",0,NULL,0);
	test_set_key(index,"abcdefghijklm",4,(unsigned char *)&val,0);
	return 0;
	}

int replace_phantom_value_test_4_3(FSingSet *index,int *res_mem,element_type prep_data)
	{ 
	int val = 0;
	int rv = test_set_key(index,"abcdefghijklm",4,(unsigned char *)&val,TF_SET_PHANTOM);
	return (rv & KS_CHANGED) ? 0 : 1;
	}

element_type replace_phantom_value_prep_4_4(FSingSet *index,int *res_mem)
	{
	int val = 0;
	test_set_key(index,"abcdef",0,NULL,0);
	test_set_key(index,"abcdef",4,(unsigned char *)&val,0);
	return 0;
	}

int replace_phantom_value_test_4_4_1(FSingSet *index,int *res_mem,element_type prep_data)
	{ 
	int val = 0;
	int rv = test_set_key(index,"abcdef",4,(unsigned char *)&val,TF_SET_PHANTOM);
	if (!(rv & KS_CHANGED))
		return 1;
	unsigned vsize = 4;
	if (test_get_key(index,"abcdef",&vsize,&val,0) != RESULT_KEY_NOT_FOUND)
		return 2;
	vsize = 4;
	if (test_get_key(index,"abcdef",&vsize,&val,TF_USE_PHANTOM) || vsize)
		return 3;
	return 0;
	}

int replace_phantom_value_test_4_4_2(FSingSet *index,int *res_mem,element_type prep_data)
	{ 
	int val = 0;
	int rv = test_set_key(index,"abcdef",0,NULL,TF_SET_PHANTOM);
	if (!(rv & KS_CHANGED))
		return 1;
	unsigned vsize = 4;
	if (test_get_key(index,"abcdef",&vsize,&val,0) != RESULT_KEY_NOT_FOUND)
		return 2;
	vsize = 4;
	if (test_get_key(index,"abcdef",&vsize,&val,TF_USE_PHANTOM) || vsize)
		return 3;
	return 0;
	}

element_type replace_phantom_value_prep_4_5(FSingSet *index,int *res_mem)
	{
	int val = 0;
	test_set_key(index,"abc",0,NULL,0);
	test_set_key(index,"abc",4,(unsigned char *)&val,0);
	return 0;
	}

int replace_phantom_value_test_4_5(FSingSet *index,int *res_mem,element_type prep_data)
	{ 
	int val = 0;
	int rv = test_set_key(index,"abc",4,(unsigned char *)&val,TF_SET_PHANTOM);
	if (!(rv & KS_CHANGED))
		return 1;
	unsigned vsize = 4;
	if (test_get_key(index,"abc",&vsize,&val,0) != RESULT_KEY_NOT_FOUND)
		return 2;
	vsize = 4;
	if (test_get_key(index,"abc",&vsize,&val,TF_USE_PHANTOM) || vsize)
		return 3;
	return 0;
	}

element_type replace_phantom_value_prep_4_6(FSingSet *index,int *res_mem)
	{
	int val = 5;
	test_set_key(index,"abc",4,(unsigned char *)&val,0);
	test_set_key(index,"abc",0,NULL,0);
	return 0;
	}

int replace_phantom_value_test_4_6(FSingSet *index,int *res_mem,element_type prep_data)
	{ 
	int val = 0;
	int rv = test_set_key(index,"abc",4,(unsigned char *)&val,TF_SET_PHANTOM);
	if (!(rv & KS_CHANGED))
		return 1;
	unsigned vsize = 4;
	if (test_get_key(index,"abc",&vsize,&val,0) != RESULT_KEY_NOT_FOUND)
		return 2;
	vsize = 4;
	if (test_get_key(index,"abc",&vsize,&val,TF_USE_PHANTOM) || vsize != 4 || val != 5)
		return 3;
	return 0;
	}

// Попытка добавления ключа в хештаблицу (idx_key_try_set)
// 1 Ключ есть в хештаблице
// 2 Ключа нет в хештаблице, есть продолжение
// 3 Ключа нет в хештаблице, есть место в хештаблице
// 4 Ключа нет в хештаблице, нет места в хештаблице

int test_try_add_key(FSingSet *index,char *key_source,int vsize,unsigned char *value)
	{
	FTransformData tdata;

	test_make_tdata(index,key_source,vsize,value,&tdata,0);

	int rv = idx_key_try_set(index,&tdata,KS_ADDED | KS_DELETED);
	test_process_res(index,rv,&tdata);
	return rv;
	}

element_type key_try_set_prep_1(FSingSet *index,int *res_mem)
	{
	test_set_key(index,collisions[0],0,NULL,0);
	test_set_key(index,collisions[1],0,NULL,0);
	return 0;
	}

int key_try_set_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = test_try_add_key(index,collisions[prep_data],0,NULL);
	return (rv & KS_ADDED) ? 1: 0;
	}

element_type key_try_set_prep_2(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 8; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 8;
	}

int key_try_set_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = test_try_add_key(index,collisions[prep_data],0,NULL);
	return rv;
	}

element_type key_try_set_prep_3(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 6; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 6;
	}

int key_try_set_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (test_try_add_key(index,collisions[prep_data],0,NULL) & KS_ADDED) ? 0 : 1;
	}
 
 element_type key_try_set_prep_4(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 7; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 7;
	}

int key_try_set_test_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (test_try_add_key(index,collisions[prep_data],0,NULL) & KS_ADDED) ? 0 : 1;
	}

// Добавление ключа в цепочку после хеш-таблицы
//(1) Ключ есть
// 1_1 В начале блока
// 1_2 Второй в блоке
// 1_3 Восьмой в блоке
// 1_4 Первый в след. блоке
//(2) Ключа нет
// 2_1 Блок 8 заголовков
//(2.2) Блок менее 8 заголовков (два)
// 2_2_1 Блок начинается с первого, нулевой свободен
// 2_2_2 После блока пустое место
// 2_2_3 После блока другой блок, перед блоком пустое место
// 2_2_4 Перед блоком и после блока другие блоки

int key_set_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (test_set_key(index,collisions[prep_data],0,NULL,0) & KS_ADDED) ? 1 : 0;
	}

 element_type key_set_prep_1_1(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 9; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 7;
	}

 element_type key_set_prep_1_2(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 9; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 8;
	}

 element_type key_set_prep_1_3(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 15; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 14;
	}

 element_type key_set_prep_1_4(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 16; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 15;
	}

int key_set_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (test_set_key(index,collisions[prep_data],0,NULL,0) & KS_ADDED) ? 0 : 1;
	}

element_type key_set_prep_2_1(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 15; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 15;
	}

element_type key_set_prep_2_2_1(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 4; i++)
		{
		test_set_key(index,collisions[i],0,NULL,0);
		test_set_key(index,collisions2[i],0,NULL,0);
		}
	for (i = 4; i < 11; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	test_del_key(index,collisions2[3],0);
	return 11;
	}

element_type key_set_prep_2_2_2(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 3; i++)
		{
		test_set_key(index,collisions[i],0,NULL,0);
		test_set_key(index,collisions2[i],0,NULL,0);
		}
	for (i = 3; i < 6; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 6;
	}

element_type key_set_prep_2_2_3(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 4; i++)
		{
		test_set_key(index,collisions[i],0,NULL,0);
		test_set_key(index,collisions2[i],0,NULL,0);
		}
	test_set_key(index,collisions2[4],0,NULL,0);
	for (i = 4; i < 9; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 9;
	}

element_type key_set_prep_2_2_4(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 4; i++)
		{
		test_set_key(index,collisions[i],0,NULL,0);
		test_set_key(index,collisions2[i],0,NULL,0);
		}
	test_set_key(index,collisions2[4],0,NULL,0);
	for (i = 4; i < 10; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 10;
	}

// Удаление тела ключа (del_key_rest)
//(1) Ключ без тела
// 1_1 Нет значения
// 1_2 Есть значение
//(2) Ключ с телом
// 2_1 Нет значения
// 2_2 Есть значение

int del_key_rest_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (test_del_key(index,"a",0) & KS_DELETED)?0:1;
	}

element_type del_key_rest_prep_1_1(FSingSet *index,int *res_mem)
	{
	test_set_key(index,"a",0,NULL,0);
	return 0;
	}

element_type del_key_rest_prep_1_2(FSingSet *index,int *res_mem)
	{
	uint32_t val = 0;
	test_set_key(index,"a",4,(unsigned char*)&val,0);
	return 0;
	}

int del_key_rest_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (test_del_key(index,"abcde",0) & KS_DELETED)?0:1;
	}

element_type del_key_rest_prep_2_1(FSingSet *index,int *res_mem)
	{
	test_set_key(index,"abcde",0,NULL,0);
	return 0;
	}

element_type del_key_rest_prep_2_2(FSingSet *index,int *res_mem)
	{
	uint32_t val = 0;
	test_set_key(index,"abcde",4,(unsigned char*)&val,0);
	return 0;
	}

// Удаление ключа (idx_key_del)
//(1) Удаление ключа из хеш-таблицы, цепочка без продолжения
// 1_1 Ключ не последний
// 1_2 Ключ последний
//(2) Удаление ключа из хеш-таблицы, цепочка с продолжением
// 2_1 Продолжение цепочки 1 заголовок
// 2_2 Продолжение цепочки 2 заголовка
// 2_3 Продолжение цепочки полный блок и 2 заголовка
//(3) Удаление первого ключа после хеш-таблицы, 
// 3_1 Продолжение цепочки 1 заголовок
// 3_2 Продолжение цепочки 2 заголовка
//(4) Удаление второго ключа после хеш-таблицы
// 4_1 Он последний
// 4_2 Он предпоследний
// 4_3 В этом блоке 8 элементов
// 4_4 Есть следующий блок из 2 заголовков
// 4_5 Есть следующий блок из 3 заголовков
//	5 Удаление первого во втором блоке цепочки из 2-х заголовков 

int del_key_test(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = test_del_key(index,collisions[prep_data],0);
	return (rv & KS_DELETED)?0:1;
	}

element_type del_key_prep_1_1(FSingSet *index,int *res_mem)
	{
	test_set_key(index,collisions[0],0,NULL,0);
	test_set_key(index,collisions[1],0,NULL,0);
	return 0;
	}

element_type del_key_prep_1_2(FSingSet *index,int *res_mem)
	{
	test_set_key(index,collisions[0],0,NULL,0);
	test_set_key(index,collisions[1],0,NULL,0);
	return 1;
	}

element_type del_key_prep_2_1(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 8; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 1;
	}

element_type del_key_prep_2_2(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 9; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 1;
	}

element_type del_key_prep_2_3(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 16; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 1;
	}

element_type del_key_prep_3_1(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 8; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 7;
	}

element_type del_key_prep_3_2(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 9; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 7;
	}

element_type del_key_prep_4_1(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 9; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 8;
	}

element_type del_key_prep_4_2(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 10; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 8;
	}

element_type del_key_prep_4_3(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 15; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 8;
	}

element_type del_key_prep_4_4(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 16; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 8;
	}

element_type del_key_prep_4_5(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 17; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 8;
	}

element_type del_key_prep_5(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 17; i++)
		test_set_key(index,collisions[i],0,NULL,0);
	return 15;
	}

int main(void)
	{
	unsigned i;
//	char errbuf[512] = {0};
	char space[20][MAX_KEY_SOURCE + 1];
	char space2[20][MAX_KEY_SOURCE + 1];
	for (i = 0; i < 20; i++)
		collisions[i] = space[i];
	for (i = 0; i < 20; i++)
		collisions2[i] = space2[i];

	if (collision_search(512,0,20,collisions) != 20 || collision_search(512,1,20,collisions2) != 20)
		{
		puts("Failed to generate collisions\n");
		return 1;
		}

	int rv = 0;
	FTestData tests[] = {
		{"alloc_rest_1_1",NULL,alloc_rest_test_1_1,LM_NONE,0},
		{"alloc_rest_1_2",NULL,alloc_rest_test_1_2,LM_NONE,0},
		{"alloc_rest_2_1",NULL,alloc_rest_test_2_1,LM_NONE,0},
		{"alloc_rest_2_2",NULL,alloc_rest_test_2_2,LM_NONE,0},

		{"replace_value_1_1",replace_value_prep_1,replace_value_test_1_1,LM_NONE,0},
		{"replace_value_1_2",replace_value_prep_1,replace_value_test_1_2,LM_NONE,0},
		{"replace_value_2_1",replace_value_prep_2,replace_value_test_2_1,LM_NONE,0},
		{"replace_value_2_2",replace_value_prep_2,replace_value_test_2_2,LM_NONE,0},
		{"replace_value_2_3",replace_value_prep_2,replace_value_test_2_3,LM_NONE,0},

		{"replace_phantom_value_1",replace_phantom_value_prep_1,replace_phantom_value_test_1,LM_NONE,UF_PHANTOM_KEYS},
		{"replace_phantom_value_2_1",replace_phantom_value_prep_2_1,replace_phantom_value_test_2_1,LM_NONE,UF_PHANTOM_KEYS},
		{"replace_phantom_value_2_2",replace_phantom_value_prep_2_2,replace_phantom_value_test_2_2,LM_NONE,UF_PHANTOM_KEYS},
		{"replace_phantom_value_2_3",replace_phantom_value_prep_2_3,replace_phantom_value_test_2_3,LM_NONE,UF_PHANTOM_KEYS},
		{"replace_phantom_value_2_4",replace_phantom_value_prep_2_4,replace_phantom_value_test_2_4,LM_NONE,UF_PHANTOM_KEYS},
		{"replace_phantom_value_2_5",replace_phantom_value_prep_2_5,replace_phantom_value_test_2_5,LM_NONE,UF_PHANTOM_KEYS},
		{"replace_phantom_value_3_1",replace_phantom_value_prep_3_1,replace_phantom_value_test_3_1,LM_NONE,UF_PHANTOM_KEYS},
		{"replace_phantom_value_3_2",replace_phantom_value_prep_3_2,replace_phantom_value_test_3_2,LM_NONE,UF_PHANTOM_KEYS},
		{"replace_phantom_value_4_1",replace_phantom_value_prep_4_1,replace_phantom_value_test_4_1,LM_NONE,UF_PHANTOM_KEYS},
		{"replace_phantom_value_4_2",replace_phantom_value_prep_4_2,replace_phantom_value_test_4_1,LM_NONE,UF_PHANTOM_KEYS},
		{"replace_phantom_value_4_3",replace_phantom_value_prep_4_3,replace_phantom_value_test_4_3,LM_NONE,UF_PHANTOM_KEYS},
		{"replace_phantom_value_4_4_1",replace_phantom_value_prep_4_4,replace_phantom_value_test_4_4_1,LM_NONE,UF_PHANTOM_KEYS},
		{"replace_phantom_value_4_4_2",replace_phantom_value_prep_4_4,replace_phantom_value_test_4_4_2,LM_NONE,UF_PHANTOM_KEYS},
		{"replace_phantom_value_4_5",replace_phantom_value_prep_4_5,replace_phantom_value_test_4_5,LM_NONE,UF_PHANTOM_KEYS},
		{"replace_phantom_value_4_6",replace_phantom_value_prep_4_6,replace_phantom_value_test_4_6,LM_NONE,UF_PHANTOM_KEYS},

		{"key_try_set_1",key_try_set_prep_1,key_try_set_test_1,LM_NONE,0},
		{"key_try_set_2",key_try_set_prep_2,key_try_set_test_2,LM_NONE,0},
		{"key_try_set_3",key_try_set_prep_3,key_try_set_test_3,LM_NONE,0},
		{"key_try_set_4",key_try_set_prep_4,key_try_set_test_4,LM_NONE,0},
		{"key_set_1_1",key_set_prep_1_1,key_set_test_1,LM_NONE,0},
		{"key_set_1_2",key_set_prep_1_2,key_set_test_1,LM_NONE,0},
		{"key_set_1_3",key_set_prep_1_3,key_set_test_1,LM_NONE,0},
		{"key_set_1_4",key_set_prep_1_4,key_set_test_1,LM_NONE,0},
		{"key_set_2_1",key_set_prep_2_1,key_set_test_2,LM_NONE,0},
		{"key_set_2_2_1",key_set_prep_2_2_1,key_set_test_2,LM_NONE,0},
		{"key_set_2_2_2",key_set_prep_2_2_2,key_set_test_2,LM_NONE,0},
		{"key_set_2_2_3",key_set_prep_2_2_3,key_set_test_2,LM_NONE,0},
		{"key_set_2_2_4",key_set_prep_2_2_4,key_set_test_2,LM_NONE,0},
		{"del_key_rest_1_1",del_key_rest_prep_1_1,del_key_rest_test_1,LM_NONE,0},
		{"del_key_rest_1_2",del_key_rest_prep_1_2,del_key_rest_test_1,LM_NONE,0},
		{"del_key_rest_2_1",del_key_rest_prep_2_1,del_key_rest_test_2,LM_NONE,0},
		{"del_key_rest_2_2",del_key_rest_prep_2_2,del_key_rest_test_2,LM_NONE,0},
		{"del_key_1_1",del_key_prep_1_1,del_key_test,LM_NONE,0},
		{"del_key_1_2",del_key_prep_1_2,del_key_test,LM_NONE,0},
		{"del_key_2_1",del_key_prep_2_1,del_key_test,LM_NONE,0},
		{"del_key_2_2",del_key_prep_2_2,del_key_test,LM_NONE,0},
		{"del_key_2_3",del_key_prep_2_3,del_key_test,LM_NONE,0},
		{"del_key_3_1",del_key_prep_3_1,del_key_test,LM_NONE,0},
		{"del_key_3_2",del_key_prep_3_2,del_key_test,LM_NONE,0},
		{"del_key_4_1",del_key_prep_4_1,del_key_test,LM_NONE,0},
		{"del_key_4_2",del_key_prep_4_2,del_key_test,LM_NONE,0},
		{"del_key_4_3",del_key_prep_4_3,del_key_test,LM_NONE,0},
		{"del_key_4_4",del_key_prep_4_4,del_key_test,LM_NONE,0},
		{"del_key_4_5",del_key_prep_4_5,del_key_test,LM_NONE,0},
		{"del_key_5",del_key_prep_5,del_key_test,LM_NONE,0},
		};
	unsigned tcnt = sizeof(tests) / sizeof(FTestData);
	for (i = 0; i < tcnt; i++)
		{
		if (run_test(&tests[i]))
			rv = 1;
		}
	return rv;
	}