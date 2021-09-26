/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "../rc_singularity.h"
#include "common.h"

// sing_get_value_cb, sing_get_value_cb_n
// 1 Успешно sing_get_value_cb
// 2 Успешно sing_get_value_cb_n
// 3 Невозможный ключ
// 4 Набор удален
// 5 Ключ не найден
// 6 Коллбек не смог выделить память

void *test_allocator(unsigned size)
	{ return malloc(size); }

void *null_allocator(unsigned size)
	{ return NULL; }

int sing_get_value_cb_test_6_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *value;
	unsigned vsize;

	int res = sing_get_value_cb(index,"key",test_allocator,(void **)&value,&vsize);
	if (res) return res;
	if (!value) return 1;
	res = (strcmp(value,"oldvalue") || vsize != 9)? 1 : 0;
	free(value);
	return res;
	}

int sing_get_value_cb_test_6_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *value;
	unsigned vsize;

	int res = sing_get_value_cb_n(index,"keyandkey",3,test_allocator,(void **)&value,&vsize);
	if (res) return res;
	if (!value) return 1;
	res = (strcmp(value,"oldvalue") || vsize != 9)? 1 : 0;
	free(value);
	return res;
	}

int sing_get_value_cb_test_6_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	void *value;
	unsigned vsize;

	int res = sing_get_value_cb(index,"key#",test_allocator,&value,&vsize);
	if (res != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (value != NULL) return 1;
	if (vsize != 0) return 1;
	return 0;
	}

int sing_get_value_cb_test_6_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	void *value;
	unsigned vsize;

	test_delete_set("sing_get_value_cb_6_4");
	return (sing_get_value_cb(index,"key",test_allocator,&value,&vsize) == SING_ERROR_CONNECTION_LOST)?0:1;
	}

int sing_get_value_cb_test_6_5(FSingSet *index,int *res_mem,element_type prep_data)
	{
	void *value;
	unsigned vsize;

	int res = sing_get_value_cb(index,"keynotfound",test_allocator,&value,&vsize);
	if (res != SING_RESULT_KEY_NOT_FOUND) return 1;
	if (value != NULL) return 1;
	if (vsize != 0) return 1;
	return 0;
	}

int sing_get_value_cb_test_6_6(FSingSet *index,int *res_mem,element_type prep_data)
	{
	void *value;
	unsigned vsize;

	return (sing_get_value_cb(index,"key",null_allocator,&value,&vsize) == SING_ERROR_NO_MEMORY)? 0 : 1;
	}

//(7) sing_get_values_cb, sing_get_values_cb_n
// 7.1 Успешно sing_get_value_cb (+ невозможный ключ, +ключ не найден)
// 7.2 Успешно sing_get_value_cb_n (+ невозможный ключ, +ключ не найден)
// 7.3 Набор удален
// 7.4 Коллбек не смог выделить память

int sing_get_values_cb_test_7_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *vstore[7];
	unsigned vsizes[7];
	int results[7];
	int i;

	int res = sing_get_values_cb(index,multi_keys,7,test_allocator,(void **)vstore,vsizes,results);
	if (res != 5) return 1;

	for (i = 0; i < 5; i++)
		{
		if (results[i]) return 1;
		if (vsizes[i] != strlen(multi_values[i]) + 1) return 1;
		if (strcmp(multi_values[i],vstore[i])) return 1;
		free(vstore[i]);
		}
	if (results[5] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (vsizes[5] != 0 || vstore[5] != NULL) return 1;
	if (results[6] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (vsizes[6] != 0 || vstore[6] != NULL) return 1;
	return 0;
	}

int sing_get_values_cb_test_7_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *vstore[7];
	unsigned ksizes[7],vsizes[7];
	int results[7];
	int i;

	for (i = 0; i < 7; i++)
		ksizes[i] = strlen(multi_keys[i]);

	int res = sing_get_values_cb_n(index,multi_keys_long,ksizes,7,test_allocator,(void **)vstore,vsizes,results);
	if (res != 5) return 1;

	for (i = 0; i < 5; i++)
		{
		if (results[i]) return 1;
		if (vsizes[i] != strlen(multi_values[i]) + 1) return 1;
		if (strcmp(multi_values[i],vstore[i])) return 1;
		free(vstore[i]);
		}
	if (results[5] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (vsizes[5] != 0 || vstore[5] != NULL) return 1;
	if (results[6] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (vsizes[6] != 0 || vstore[6] != NULL) return 1;
	return 0;
	}

int sing_get_values_cb_test_7_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *vstore[7];
	unsigned vsizes[7];
	int results[7];

	test_delete_set("sing_get_values_cb_7_3");
	int res = sing_get_values_cb(index,multi_keys,7,test_allocator,(void **)vstore,vsizes,results);
	return (res == SING_ERROR_CONNECTION_LOST) ? 0 : 1;
	}

int sing_get_values_cb_test_7_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *vstore[7];
	unsigned vsizes[7];
	int results[7];
	int i;

	int res = sing_get_values_cb(index,multi_keys,7,null_allocator,(void **)vstore,vsizes,results);
	if (res != SING_ERROR_NO_MEMORY) return 1;
	for (i = 0; i < 7; i++)
		{
		if (results[i] != SING_ERROR_NO_MEMORY)	return 1;
		if (vsizes[i] != 0 || vstore[i] != NULL) return 1;
		}
	return 0;
	}

//(8) sing_get_value, sing_get_value_n
// 8.1 Успешно sing_get_value
// 8.2 Успешно sing_get_value_n
// 8.3 Невозможный ключ
// 8.4 Набор удален
// 8.5 Ключ не найден
// 8.6 Недостаточный размер буфера

int sing_get_value_test_8_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char value[20];
	unsigned vsize = 20;

	int res = sing_get_value(index,"key",&value,&vsize);
	if (res) return res;
	return (strcmp(value,"oldvalue") || vsize != 9)? 1 : 0;
	}

int sing_get_value_test_8_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char value[20];
	unsigned vsize = 20;

	int res = sing_get_value_n(index,"keyandkey",3,&value,&vsize);
	if (res) return res;
	return (strcmp(value,"oldvalue") || vsize != 9)? 1 : 0;
	}

int sing_get_value_test_8_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char value[20];
	unsigned vsize = 20;

	int res = sing_get_value(index,"key#",&value,&vsize);
	if (res != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (vsize != 0) return 1;
	return 0;
	}

int sing_get_value_test_8_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char value[20];
	unsigned vsize = 20;

	test_delete_set("sing_get_value_8_4");
	return (sing_get_value(index,"key",&value,&vsize) == SING_ERROR_CONNECTION_LOST)?0:1;
	}

int sing_get_value_test_8_5(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char value[20];
	unsigned vsize = 20;

	int res = sing_get_value(index,"keynotfound",&value,&vsize);
	if (res != SING_RESULT_KEY_NOT_FOUND) return 1;
	if (vsize != 0) return 1;
	return 0;
	}

int sing_get_value_test_8_6(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char value[20];
	unsigned vsize = 3;

	int res = sing_get_value(index,"key",&value,&vsize);
	if (res != SING_RESULT_SMALL_BUFFER) return 1;
	return (strcmp(value,"old") || vsize != 9)? 1 : 0;
	}

//(9) sing_get_values_cb, sing_get_values_n
// 9.1 Успешно sing_get_value_cb (+ невозможный ключ, +ключ не найден)
// 9.2 Успешно sing_get_value_cb_n (+ невозможный ключ, +ключ не найден)
// 9.3 Набор удален

int sing_get_values_test_9_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char vbuf[128];
	void *vstore[7] = {&vbuf[0],&vbuf[20],&vbuf[40],&vbuf[60],&vbuf[120],&vbuf[80],&vbuf[100]};
	unsigned vsizes[7] = {20,20,20,20,8,20,20};
	int results[7];
	int i;

	int res = sing_get_values(index,multi_keys,7,vstore,vsizes,results);
	if (res != 4) return 1;

	for (i = 0; i < 4; i++)
		{
		if (results[i]) return 1;
		if (vsizes[i] != strlen(multi_values[i]) + 1) return 1;
		if (strcmp(multi_values[i],&vbuf[i * 20])) return 1;
		}
	if (results[4] != SING_RESULT_SMALL_BUFFER) return 1;
	if (vsizes[4] != strlen(multi_values[4]) + 1) return 1;
	if (strncmp(multi_values[4],&vbuf[120],8)) return 1;

	if (results[5] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (vsizes[5] != 0) return 1;
	if (results[6] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (vsizes[6] != 0) return 1;
	return 0;
	}

int sing_get_values_test_9_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char vbuf[128];
	void *vstore[7] = {&vbuf[0],&vbuf[20],&vbuf[40],&vbuf[60],&vbuf[120],&vbuf[80],&vbuf[100]};
	unsigned vsizes[7] = {20,20,20,20,8,20,20};
	unsigned ksizes[7];
	int results[7];
	int i;

	for (i = 0; i < 7; i++)
		ksizes[i] = strlen(multi_keys[i]);

	int res = sing_get_values_n(index,multi_keys_long,ksizes,7,(void **)vstore,vsizes,results);
	if (res != 4) return 1;

	for (i = 0; i < 4; i++)
		{
		if (results[i]) return 1;
		if (vsizes[i] != strlen(multi_values[i]) + 1) return 1;
		if (strcmp(multi_values[i],vstore[i])) return 1;
		}
	if (results[4] != SING_RESULT_SMALL_BUFFER) return 1;
	if (vsizes[4] != strlen(multi_values[4]) + 1) return 1;
	if (strncmp(multi_values[4],&vbuf[120],8)) return 1;

	if (results[5] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (vsizes[5] != 0) return 1;
	if (results[6] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (vsizes[6] != 0) return 1;
	return 0;
	}

int sing_get_values_test_9_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char vbuf[128];
	void *vstore[7] = {&vbuf[0],&vbuf[20],&vbuf[40],&vbuf[60],&vbuf[120],&vbuf[80],&vbuf[100]};
	unsigned vsizes[7] = {20,20,20,20,8,20,20};
	int results[7];

	test_delete_set("sing_get_values_9_3");
	int res = sing_get_values(index,multi_keys,7,(void **)vstore,vsizes,results);
	return (res == SING_ERROR_CONNECTION_LOST) ? 0 : 1;
	}

//(10) sing_key_present, sing_key_present_n
// 10.1 Успешно sing_key_present
// 10.2 Успешно sing_key_present_n
// 10.3 Невозможный ключ
// 10.4 Набор удален
// 10.5 Ключ не найден

int key_present_test_10_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return sing_key_present(index,"key");
	}

int key_present_test_10_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return sing_key_present_n(index,"keyandkey",3);
	}

int key_present_test_10_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (sing_key_present(index,"key#") == SING_RESULT_IMPOSSIBLE_KEY) ? 0 : 1;
	}

int key_present_test_10_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	test_delete_set("key_present_10_4");
	return (sing_key_present(index,"key") == SING_ERROR_CONNECTION_LOST)?0:1;
	}

int key_present_test_10_5(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (sing_key_present(index,"keynotfound") == SING_RESULT_KEY_NOT_FOUND) ? 0 : 1;
	}

int main(void)
	{
	int rv = 0;
	unsigned i;

	FTestData tests[] = {
		{"sing_get_value_cb_6_1",one_key_value_prep,sing_get_value_cb_test_6_1,SING_LM_NONE,0},
		{"sing_get_value_cb_6_2",one_key_value_prep,sing_get_value_cb_test_6_2,SING_LM_NONE,0},
		{"sing_get_value_cb_6_3",one_key_value_prep,sing_get_value_cb_test_6_3,SING_LM_NONE,0},
		{"sing_get_value_cb_6_4",one_key_value_prep,sing_get_value_cb_test_6_4,SING_LM_NONE,0},
		{"sing_get_value_cb_6_5",one_key_value_prep,sing_get_value_cb_test_6_5,SING_LM_NONE,0},
		{"sing_get_value_cb_6_6",one_key_value_prep,sing_get_value_cb_test_6_6,SING_LM_NONE,0},
		{"sing_get_values_cb_7_1",many_key_value_prep,sing_get_values_cb_test_7_1,SING_LM_NONE,0},
		{"sing_get_values_cb_7_2",many_key_value_prep,sing_get_values_cb_test_7_2,SING_LM_NONE,0},
		{"sing_get_values_cb_7_3",many_key_value_prep,sing_get_values_cb_test_7_3,SING_LM_NONE,0},
		{"sing_get_values_cb_7_4",many_key_value_prep,sing_get_values_cb_test_7_4,SING_LM_NONE,0},
		{"sing_get_value_8_1",one_key_value_prep,sing_get_value_test_8_1,SING_LM_NONE,0},
		{"sing_get_value_8_2",one_key_value_prep,sing_get_value_test_8_2,SING_LM_NONE,0},
		{"sing_get_value_8_3",one_key_value_prep,sing_get_value_test_8_3,SING_LM_NONE,0},
		{"sing_get_value_8_4",one_key_value_prep,sing_get_value_test_8_4,SING_LM_NONE,0},
		{"sing_get_value_8_5",one_key_value_prep,sing_get_value_test_8_5,SING_LM_NONE,0},
		{"sing_get_value_8_6",one_key_value_prep,sing_get_value_test_8_6,SING_LM_NONE,0},
		{"sing_get_values_9_1",many_key_value_prep,sing_get_values_test_9_1,SING_LM_NONE,0},
		{"sing_get_values_9_2",many_key_value_prep,sing_get_values_test_9_2,SING_LM_NONE,0},
		{"sing_get_values_9_3",many_key_value_prep,sing_get_values_test_9_3,SING_LM_NONE,0},
		{"key_present_10_1",one_key_value_prep,key_present_test_10_1,SING_LM_NONE,0},
		{"key_present_10_2",one_key_value_prep,key_present_test_10_2,SING_LM_NONE,0},
		{"key_present_10_3",one_key_value_prep,key_present_test_10_3,SING_LM_NONE,0},
		{"key_present_10_4",one_key_value_prep,key_present_test_10_4,SING_LM_NONE,0},
		{"key_present_10_5",one_key_value_prep,key_present_test_10_5,SING_LM_NONE,0},
		};
	unsigned tcnt = sizeof(tests) / sizeof(FTestData);
	for (i = 0; i < tcnt; i++)
		{
		if (run_test(&tests[i]))
			rv = 1;
		}
	return rv;
	}
