/*
 * Copyright (C) �Hostcomm� LLC
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
// 1_1 ������� sing_get_value_cb
// 1_2 �������, ����� � ���������, ���� ���������� � ���������, ������ ����������
// 2 ������� sing_get_value_cb_n
// 3 ����������� ����
// 4 ����� ������
// 5 ������� �� ���� �������� ������
// 6_1 ���� �� ������
// 6_2 ���� �� ������, ����� � ���������, ���� ������, ������ ����������


void *test_allocator(unsigned size)
	{ return malloc(size); }

void *null_allocator(unsigned size)
	{ return NULL; }

int sing_get_value_cb_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *value;
	unsigned vsize;

	int res = sing_get_value_cb(index,single_key,test_allocator,(void **)&value,&vsize);
	if (res) return res;
	if (!value) return 1;
	res = (strcmp(value,single_value) || vsize != strlen(single_value) + 1)? 1 : 0;
	free(value);
	return res;
	}

int sing_get_value_cb_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *value;
	unsigned vsize;
	char key[20];

	strcpy(key,single_key);
	strcat(key,"andkey");

	int res = sing_get_value_cb_n(index,key,strlen(single_key),test_allocator,(void **)&value,&vsize);
	if (res) return res;
	if (!value) return 1;
	res = (strcmp(value,single_value) || vsize != strlen(single_value) + 1)? 1 : 0;
	free(value);
	return res;
	}

int sing_get_value_cb_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	void *value;
	unsigned vsize;

	int res = sing_get_value_cb(index,"key#",test_allocator,&value,&vsize);
	if (res != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (value != NULL) return 1;
	if (vsize != 0) return 1;
	return 0;
	}

int sing_get_value_cb_test_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	void *value;
	unsigned vsize;

	test_delete_set("sing_get_value_cb_4");
	return (sing_get_value_cb(index,single_key,test_allocator,&value,&vsize) == SING_ERROR_CONNECTION_LOST)?0:1;
	}

int sing_get_value_cb_test_5(FSingSet *index,int *res_mem,element_type prep_data)
	{
	void *value;
	unsigned vsize;

	return (sing_get_value_cb(index,single_key,null_allocator,&value,&vsize) == SING_ERROR_NO_MEMORY)? 0 : 1;
	}

int sing_get_value_cb_test_6(FSingSet *index,int *res_mem,element_type prep_data)
	{
	void *value;
	unsigned vsize;

	int res = sing_get_value_cb(index,"keynotfound",test_allocator,&value,&vsize);
	if (res != SING_RESULT_KEY_NOT_FOUND) return 1;
	if (value != NULL) return 1;
	if (vsize != 0) return 1;
	return 0;
	}

// sing_get_phantom_cb, sing_get_phantom_cb_n
// 1_1 ����� � ���������, �������, ���� ������, ������ ���������
// 1_2 ����� � ���������, �������, ���� ���������� � ���������, ������ ���������
// 2 ����� � ���������, ���� ������ ����������, ������ ���������
// 3 ����� ��� ��������

int sing_get_phantom_cb_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *value;
	unsigned vsize;

	int res = sing_get_phantom_cb(index,single_key,test_allocator,(void **)&value,&vsize);
	if (res) return res;
	if (!value) return 1;
	res = (strcmp(value,single_phantom) || vsize != strlen(single_phantom) + 1)? 1 : 0;
	free(value);
	return res;
	}

int sing_get_phantom_cb_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	void *value;
	unsigned vsize;

	int res = sing_get_phantom_cb(index,single_key,test_allocator,&value,&vsize);
	if (res != SING_RESULT_KEY_NOT_FOUND) return 1;
	if (value != NULL) return 1;
	if (vsize != 0) return 1;
	return 0;
	}

int sing_get_phantom_cb_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	void *value;
	unsigned vsize;

	int res = sing_get_phantom_cb(index,single_key,test_allocator,&value,&vsize);
	if (res != SING_ERROR_IMPOSSIBLE_OPERATION) return 1;
	if (value != NULL) return 1;
	if (vsize != 0) return 1;
	return 0;
	}

// sing_get_values_cb, sing_get_values_cb_n
// 1 ������� sing_get_values_cb (+ ����������� ����, +���� �� ������, +���� �� ������ ����� ��������)
// 2 ������� sing_get_values_cb_n (+ ����������� ����, +���� �� ������, +���� �� ������ ����� �������� � ������ � ���������)
// 3 ����� ������
// 4 ������� �� ���� �������� ������

int sing_get_values_cb_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *vstore[TEST_MULTIKEYS_CNT];
	unsigned vsizes[TEST_MULTIKEYS_CNT];
	int results[TEST_MULTIKEYS_CNT];
	int i;

	int res = sing_get_values_cb(index,multi_keys,TEST_MULTIKEYS_CNT,test_allocator,(void **)vstore,vsizes,results);
	if (res != 5) return 1;

	for (i = 0; i < 4; i++)
		{
		if (results[i]) return 1;
		if (vsizes[i] != strlen(multi_values[i]) + 1) return 1;
		if (strcmp(multi_values[i],vstore[i])) return 1;
		free(vstore[i]);
		}
	if (results[i] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (vsizes[i] != 0 || vstore[i] != NULL) return 1;
	i++;
	if (results[i] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (vsizes[i] != 0 || vstore[i] != NULL) return 1;
	i++;
	if (results[i] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (vsizes[i] != 0 || vstore[i] != NULL) return 1;
	i++;
	if (results[i]) return 1;
	if (vsizes[i] != strlen(multi_values[i]) + 1) return 1;
	if (strcmp(multi_values[i],vstore[i])) return 1;
	free(vstore[i]);
	return 0;
	}

int sing_get_values_cb_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *vstore[TEST_MULTIKEYS_CNT];
	unsigned ksizes[TEST_MULTIKEYS_CNT],vsizes[TEST_MULTIKEYS_CNT];
	int results[TEST_MULTIKEYS_CNT];
	int i;

	for (i = 0; i < TEST_MULTIKEYS_CNT; i++)
		ksizes[i] = strlen(multi_keys[i]);

	int res = sing_get_values_cb_n(index,multi_keys_long,ksizes,TEST_MULTIKEYS_CNT,test_allocator,(void **)vstore,vsizes,results);
	if (res != 5) return 1;

	for (i = 0; i < 4; i++)
		{
		if (results[i]) return 1;
		if (vsizes[i] != strlen(multi_values[i]) + 1) return 1;
		if (strcmp(multi_values[i],vstore[i])) return 1;
		free(vstore[i]);
		}
	if (results[i] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (vsizes[i] != 0 || vstore[i] != NULL) return 1;
	i++;
	if (results[i] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (vsizes[i] != 0 || vstore[i] != NULL) return 1;
	i++;
	if (results[i] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (vsizes[i] != 0 || vstore[i] != NULL) return 1;
	i++;
	if (results[i]) return 1;
	if (vsizes[i] != strlen(multi_values[i]) + 1) return 1;
	if (strcmp(multi_values[i],vstore[i])) return 1;
	free(vstore[i]);
	return 0;
	}

int sing_get_values_cb_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *vstore[TEST_MULTIKEYS_CNT];
	unsigned vsizes[TEST_MULTIKEYS_CNT];
	int results[TEST_MULTIKEYS_CNT];

	test_delete_set("sing_get_values_cb_3");
	int res = sing_get_values_cb(index,multi_keys,TEST_MULTIKEYS_CNT,test_allocator,(void **)vstore,vsizes,results);
	return (res == SING_ERROR_CONNECTION_LOST) ? 0 : 1;
	}

int sing_get_values_cb_test_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *vstore[TEST_MULTIKEYS_CNT];
	unsigned vsizes[TEST_MULTIKEYS_CNT];
	int results[TEST_MULTIKEYS_CNT];
	int i;

	int res = sing_get_values_cb(index,multi_keys,TEST_MULTIKEYS_CNT,null_allocator,(void **)vstore,vsizes,results);
	if (res != SING_ERROR_NO_MEMORY) return 1;
	for (i = 0; i < TEST_MULTIKEYS_CNT; i++)
		{
		if (results[i] != SING_ERROR_NO_MEMORY) return 1;
		if (vsizes[i] != 0 || vstore[i] != NULL) return 1;
		}
	return 0;
	}

// sing_get_value, sing_get_value_n
// 1_1 ������� sing_get_value
// 1_2 ������� sing_get_value, ����� � ���������, ���� ���������� � ���������, ������ ����������
// 2 ������� sing_get_value_n
// 3 ����������� ����
// 4 ����� ������
// 5 ������������� ������ ������
// 6_1 ���� �� ������
// 6_2 ���� �� ������, ����� � ���������, ���� ������, ������ ����������

int sing_get_value_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char value[20];
	unsigned vsize = 20;

	int res = sing_get_value(index,single_key,&value,&vsize);
	if (res) return res;
	return (strcmp(value,single_value) || vsize != strlen(single_value) + 1)? 1 : 0;
	}

int sing_get_value_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char value[20];
	unsigned vsize = 20;
	char key[20];

	strcpy(key,single_key);
	strcat(key,"andkey");
	int res = sing_get_value_n(index,key,strlen(single_key),&value,&vsize);
	if (res) return res;
	return (strcmp(value,single_value) || vsize != strlen(single_value) + 1)? 1 : 0;
	}

int sing_get_value_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char value[20];
	unsigned vsize = 20;

	int res = sing_get_value(index,"key#",&value,&vsize);
	if (res != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (vsize != 0) return 1;
	return 0;
	}

int sing_get_value_test_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char value[20];
	unsigned vsize = 20;

	test_delete_set("sing_get_value_4");
	return (sing_get_value(index,single_key,&value,&vsize) == SING_ERROR_CONNECTION_LOST)?0:1;
	}

int sing_get_value_test_5(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char value[20];
	unsigned vsize = 3;

	int res = sing_get_value(index,single_key,&value,&vsize);
	if (res != SING_RESULT_SMALL_BUFFER) return 1;
	return (strncmp(value,single_value,3) || vsize != strlen(single_value) + 1)? 1 : 0;
	}

int sing_get_value_test_6(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char value[20];
	unsigned vsize = 20;

	int res = sing_get_value(index,"keynotfound",&value,&vsize);
	if (res != SING_RESULT_KEY_NOT_FOUND) return 1;
	if (vsize != 0) return 1;
	return 0;
	}

// sing_get_phantom, sing_get_phantom_n
// 1_1 ����� � ���������, �������, ���� ������, ������ ���������
// 1_2 ����� � ���������, �������, ���� ���������� � ���������, ������ ���������
// 2 ����� � ���������, ���� ������ ����������, ������ ���������
// 3 ����� ��� ��������

int sing_get_phantom_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char value[20];
	unsigned vsize = 20;

	int res = sing_get_phantom(index,single_key,(void *)value,&vsize);
	if (res) return res;
	res = (strcmp(value,single_phantom) || vsize != strlen(single_phantom) + 1)? 1 : 0;
	return res;
	}

int sing_get_phantom_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char value[20];
	unsigned vsize = 20;

	int res = sing_get_phantom(index,single_key,(void *)value,&vsize);
	if (res != SING_RESULT_KEY_NOT_FOUND) return 1;
	if (vsize != 0) return 1;
	return 0;
	}

int sing_get_phantom_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char value[20];
	unsigned vsize = 20;

	int res = sing_get_phantom(index,single_key,(void *)value,&vsize);
	if (res != SING_ERROR_IMPOSSIBLE_OPERATION) return 1;
	if (vsize != 0) return 1;
	return 0;
	}

// sing_get_values, sing_get_values_n
// 1 ������� sing_get_values (+ ����������� ����, +���� �� ������, +���� �� ������ ����� ��������)
// 2 ������� sing_get_values_n (+ ����������� ����, +���� �� ������, +���� �� ������ ����� �������� � ������ � ���������)
// 3 ����� ������

int sing_get_values_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char vbuf[148];
	void *vstore[TEST_MULTIKEYS_CNT] = {&vbuf[0],&vbuf[20],&vbuf[40],&vbuf[60],&vbuf[80],&vbuf[100],&vbuf[120],&vbuf[140]};
	unsigned vsizes[TEST_MULTIKEYS_CNT] = {20,20,20,20,20,20,20,8};
	int results[TEST_MULTIKEYS_CNT];
	int i;

	int res = sing_get_values(index,multi_keys,TEST_MULTIKEYS_CNT,vstore,vsizes,results);
	if (res != 5) return 1;

	for (i = 0; i < 4; i++)
		{
		if (results[i]) return 1;
		if (vsizes[i] != strlen(multi_values[i]) + 1) return 1;
		if (strcmp(multi_values[i],&vbuf[i * 20])) return 1;
		}
	if (results[i] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (vsizes[i] != 0) return 1;
	i++;
	if (results[i] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (vsizes[i] != 0) return 1;
	i++;
	if (results[i] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (vsizes[i] != 0) return 1;
	i++;
	if (results[i] != SING_RESULT_SMALL_BUFFER) return 1;
	if (vsizes[i] != strlen(multi_values[i]) + 1) return 1;
	if (strncmp(multi_values[i],&vbuf[140],8)) return 1;
	return 0;
	}

int sing_get_values_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char vbuf[148];
	void *vstore[TEST_MULTIKEYS_CNT] = {&vbuf[0],&vbuf[20],&vbuf[40],&vbuf[60],&vbuf[80],&vbuf[100],&vbuf[120],&vbuf[140]};
	unsigned vsizes[TEST_MULTIKEYS_CNT] = {20,20,20,20,20,20,20,8};
	unsigned ksizes[TEST_MULTIKEYS_CNT];
	int results[TEST_MULTIKEYS_CNT];
	int i;

	for (i = 0; i < TEST_MULTIKEYS_CNT; i++)
		ksizes[i] = strlen(multi_keys[i]);

	int res = sing_get_values_n(index,multi_keys_long,ksizes,TEST_MULTIKEYS_CNT,(void **)vstore,vsizes,results);
	if (res != 5) return 1;

	for (i = 0; i < 4; i++)
		{
		if (results[i]) return 1;
		if (vsizes[i] != strlen(multi_values[i]) + 1) return 1;
		if (strcmp(multi_values[i],vstore[i])) return 1;
		}
	if (results[i] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (vsizes[i] != 0) return 1;
	i++;
	if (results[i] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (vsizes[i] != 0) return 1;
	i++;
	if (results[i] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (vsizes[i] != 0) return 1;
	i++;
	if (results[i] != SING_RESULT_SMALL_BUFFER) return 1;
	if (vsizes[i] != strlen(multi_values[i]) + 1) return 1;
	if (strncmp(multi_values[i],&vbuf[140],8)) return 1;
	return 0;
	}

int sing_get_values_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char vbuf[128];
	void *vstore[TEST_MULTIKEYS_CNT] = {&vbuf[0],&vbuf[20],&vbuf[40],&vbuf[60],&vbuf[120],&vbuf[80],&vbuf[100]};
	unsigned vsizes[TEST_MULTIKEYS_CNT] = {20,20,20,20,8,20,20};
	int results[TEST_MULTIKEYS_CNT];

	test_delete_set("sing_get_values_3");
	int res = sing_get_values(index,multi_keys,TEST_MULTIKEYS_CNT,(void **)vstore,vsizes,results);
	return (res == SING_ERROR_CONNECTION_LOST) ? 0 : 1;
	}

// sing_get_values_simple, sing_get_values_simple_n
// 1 ������� sing_get_value_simple (+ ����������� ����, +���� �� ������, +���� �� ������ ����� ��������, +��������� �����)
// 2 ������� sing_get_value_simple_n (+ ����������� ����, +���� �� ������, +���� �� ������ ����� �������� � ������ � ���������, +��������� �����)
// 3 ����� ������

int sing_get_values_simple_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char vbuf[128];
	char *vstore[TEST_MULTIKEYS_CNT] = {&vbuf[0],NULL,NULL,NULL,NULL,NULL,NULL,NULL};
	unsigned vsizes[TEST_MULTIKEYS_CNT] = {50};
	int results[TEST_MULTIKEYS_CNT];
	int i;

	int res = sing_get_values_simple(index,multi_keys,TEST_MULTIKEYS_CNT,(void *)vstore,vsizes,results);
	if (res != 3) return 1;

	for (i = 0; i < 3; i++)
		{
		if (results[i]) return 1;
		if (vsizes[i] != strlen(multi_values[i]) + 1) return 1;
		if (strcmp(multi_values[i],vstore[i])) return 1;
		}
	if (results[i] != SING_RESULT_SMALL_BUFFER) return 1;
	if (vsizes[i] != strlen(multi_values[i]) + 1) return 1;
	if (vstore[i] != NULL) return 1;
	i++;
	if (results[i] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (vsizes[i] != 0 || vstore[i] != NULL) return 1;
	i++;
	if (results[i] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (vsizes[i] != 0 || vstore[i] != NULL) return 1;
	i++;
	if (results[i] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (vsizes[i] != 0 || vstore[i] != NULL) return 1;
	i++;
	if (results[i] != SING_RESULT_SMALL_BUFFER) return 1;
	if (vsizes[i] != strlen(multi_values[i]) + 1) return 1;
	if (vstore[i] != NULL) return 1;
	return 0;
	}

int sing_get_values_simple_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char vbuf[128];
	char *vstore[TEST_MULTIKEYS_CNT] = {&vbuf[0],NULL,NULL,NULL,NULL,NULL,NULL};
	unsigned vsizes[TEST_MULTIKEYS_CNT] = {50};
	unsigned ksizes[TEST_MULTIKEYS_CNT];
	int results[TEST_MULTIKEYS_CNT];
	int i;

	for (i = 0; i < TEST_MULTIKEYS_CNT; i++)
		ksizes[i] = strlen(multi_keys[i]);

	int res = sing_get_values_simple_n(index,multi_keys_long,ksizes,TEST_MULTIKEYS_CNT,(void *)vstore,vsizes,results);
	if (res != 3) return 1;

	for (i = 0; i < 3; i++)
		{
		if (results[i]) return 1;
		if (vsizes[i] != strlen(multi_values[i]) + 1) return 1;
		if (strcmp(multi_values[i],vstore[i])) return 1;
		}
	if (results[i] != SING_RESULT_SMALL_BUFFER) return 1;
	if (vsizes[i] != strlen(multi_values[i]) + 1) return 1;
	if (vstore[i] != NULL) return 1;
	i++;
	if (results[i] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (vsizes[i] != 0 || vstore[i] != NULL) return 1;
	i++;
	if (results[i] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (vsizes[i] != 0 || vstore[i] != NULL) return 1;
	i++;
	if (results[i] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (vsizes[i] != 0 || vstore[i] != NULL) return 1;
	i++;
	if (results[i] != SING_RESULT_SMALL_BUFFER) return 1;
	if (vsizes[i] != strlen(multi_values[i]) + 1) return 1;
	if (vstore[i] != NULL) return 1;
	return 0;
	}

int sing_get_values_simple_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char vbuf[128];
	char *vstore[TEST_MULTIKEYS_CNT] = {&vbuf[0],NULL,NULL,NULL,NULL,NULL,NULL};
	unsigned vsizes[TEST_MULTIKEYS_CNT] = {50};
	int results[TEST_MULTIKEYS_CNT];

	test_delete_set("sing_get_values_simple_3");
	int res = sing_get_values_simple(index,multi_keys,TEST_MULTIKEYS_CNT,(void *)vstore,vsizes,results);
	return (res == SING_ERROR_CONNECTION_LOST) ? 0 : 1;
	}

// sing_get_values_same, sing_get_values_same_n
// 1 ������� sing_get_value_same (+ ����������� ����, +���� �� ������, +���� �� ������ ����� ��������)
// 2 ������� sing_get_value_simple_n (+ ����������� ����, +���� �� ������, +���� �� ������ ����� �������� � ������ � ���������)
// 3 ����� ������

int sing_get_values_same_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char vbuf[TEST_MULTIKEYS_CNT * 4];
	int results[TEST_MULTIKEYS_CNT];
	int i;

	int res = sing_get_values_same(index,multi_keys,TEST_MULTIKEYS_CNT,(void *)vbuf,4,results);
	if (res != 5) return 1;

	for (i = 0; i < 4; i++)
		{
		if (results[i]) return 1;
		if (strncmp(multi_values[i],&vbuf[i * 4],4)) return 1;
		}

	if (results[i] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (vbuf[i * 4] != 0 || vbuf[i * 4 + 1] != 0 || vbuf[i * 4 + 2] != 0 || vbuf[i * 4 + 3] != 0) return 1;
	i++;
	if (results[i] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (vbuf[i * 4] != 0 || vbuf[i * 4 + 1] != 0 || vbuf[i * 4 + 2] != 0 || vbuf[i * 4 + 3] != 0) return 1;
	i++;
	if (results[i] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (vbuf[i * 4] != 0 || vbuf[i * 4 + 1] != 0 || vbuf[i * 4 + 2] != 0 || vbuf[i * 4 + 3] != 0) return 1;
	i++;
	if (results[i]) return 1;
	if (strncmp(multi_values[i],&vbuf[i * 4],4)) return 1;
	return 0;
	}

int sing_get_values_same_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char vbuf[TEST_MULTIKEYS_CNT * 4];
	int results[TEST_MULTIKEYS_CNT];
	unsigned ksizes[TEST_MULTIKEYS_CNT];
	int i;

	for (i = 0; i < TEST_MULTIKEYS_CNT; i++)
		ksizes[i] = strlen(multi_keys[i]);

	int res = sing_get_values_same_n(index,multi_keys,ksizes,TEST_MULTIKEYS_CNT,(void *)vbuf,4,results);
	if (res != 5) return 1;

	for (i = 0; i < 4; i++)
		{
		if (results[i]) return 1;
		if (strncmp(multi_values[i],&vbuf[i * 4],4)) return 1;
		}

	if (results[i] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (vbuf[i * 4] != 0 || vbuf[i * 4 + 1] != 0 || vbuf[i * 4 + 2] != 0 || vbuf[i * 4 + 3] != 0) return 1;
	i++;
	if (results[i] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (vbuf[i * 4] != 0 || vbuf[i * 4 + 1] != 0 || vbuf[i * 4 + 2] != 0 || vbuf[i * 4 + 3] != 0) return 1;
	i++;
	if (results[i] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (vbuf[i * 4] != 0 || vbuf[i * 4 + 1] != 0 || vbuf[i * 4 + 2] != 0 || vbuf[i * 4 + 3] != 0) return 1;
	i++;
	if (results[i]) return 1;
	if (strncmp(multi_values[i],&vbuf[i * 4],4)) return 1;
	return 0;
	}

int sing_get_values_same_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char vbuf[TEST_MULTIKEYS_CNT * 4];
	int results[TEST_MULTIKEYS_CNT];

	test_delete_set("sing_get_values_same_3");
	int res = sing_get_values_same(index,multi_keys,TEST_MULTIKEYS_CNT,(void *)vbuf,4,results);
	return (res == SING_ERROR_CONNECTION_LOST) ? 0 : 1;
	}

// sing_key_present, sing_key_present_n
// 1_1 ������� sing_key_present
// 1_2 ������� sing_key_present, ����� � ���������, ���� ���������� � ���������, ������ ����������
// 2 ������� sing_key_present_n
// 3 ����������� ����
// 4 ����� ������
// 5_1 ���� �� ������
// 5_2 ���� �� ������, ����� � ���������, ���� ������, ������ ����������

int sing_key_present_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return sing_key_present(index,single_key);
	}

int sing_key_present_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char key[20];

	strcpy(key,single_key);
	strcat(key,"andkey");
	return sing_key_present_n(index,key,strlen(single_key));
	}

int sing_key_present_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (sing_key_present(index,"key#") == SING_RESULT_IMPOSSIBLE_KEY) ? 0 : 1;
	}

int sing_key_present_test_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	test_delete_set("sing_key_present_4");
	return (sing_key_present(index,single_key) == SING_ERROR_CONNECTION_LOST)?0:1;
	}

int sing_key_present_test_5_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (sing_key_present(index,"keynotfound") == SING_RESULT_KEY_NOT_FOUND) ? 0 : 1;
	}

int sing_key_present_test_5_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (sing_key_present(index,single_key) == SING_RESULT_KEY_NOT_FOUND) ? 0 : 1;
	}

// sing_phantom_present, sing_phantom_present_n
// 1_1 ����� � ���������, �������, ���� ������, ������ ���������
// 1_2 ����� � ���������, �������, ���� ���������� � ���������, ������ ���������
// 2 ����� � ���������, ���� ������ ����������, ������ ���������
// 3 ����� ��� ��������

int sing_phantom_present_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return sing_phantom_present(index,single_key);
	}

int sing_phantom_present_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int res = sing_phantom_present(index,single_key);
	if (res != SING_RESULT_KEY_NOT_FOUND) return 1;
	return 0;
	}

int sing_phantom_present_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int res = sing_phantom_present(index,single_key);
	if (res != SING_ERROR_IMPOSSIBLE_OPERATION) return 1;
	return 0;
	}

// sing_keys_present, sing_keys_present_n
// 1 ������� sing_keys_present (+����������� ����, +���� �� ������, +���� �� ������ ����� ��������)
// 2 ������� sing_keys_present_n (+����������� ����, +���� �� ������, +���� �� ������ ����� �������� � ������ � ���������)
// 3 ����� ������

int sing_keys_present_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int results[TEST_MULTIKEYS_CNT];
	int i;

	int res = sing_keys_present(index,multi_keys,TEST_MULTIKEYS_CNT,results);
	if (res != 5) return 1;

	for (i = 0; i < 4; i++)
		if (results[i]) return 1;

	if (results[i++] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (results[i++] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (results[i++] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (results[i]) return 1;
	return 0;
	}

int sing_keys_present_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int results[TEST_MULTIKEYS_CNT];
	unsigned ksizes[TEST_MULTIKEYS_CNT];
	int i;

	for (i = 0; i < TEST_MULTIKEYS_CNT; i++)
		ksizes[i] = strlen(multi_keys[i]);

	int res = sing_keys_present_n(index,multi_keys,ksizes,TEST_MULTIKEYS_CNT,results);
	if (res != 5) return 1;

	for (i = 0; i < 4; i++)
		if (results[i]) return 1;

	if (results[i++] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (results[i++] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (results[i++] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (results[i]) return 1;
	return 0;
	}

int sing_keys_present_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int results[TEST_MULTIKEYS_CNT];

	test_delete_set("sing_keys_present_3");
	int res = sing_keys_present(index,multi_keys,TEST_MULTIKEYS_CNT,results);
	return (res == SING_ERROR_CONNECTION_LOST) ? 0 : 1;
	}

// sing_value_equal, sing_value_equal_n
// 1_1 ������� sing_value_equal
// 1_2 ������� sing_value_equal, ����� � ���������, ���� ���������� � ���������, ������ ����������
// 2 ������� sing_value_equal_n
// 3 ����������� ����
// 4 ����� ������
// 5 �������� ����������
// 6_1 ���� �� ������
// 6_2 ���� �� ������, ����� � ���������, ���� ������

int sing_value_equal_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return sing_value_equal(index,single_key,single_value,strlen(single_value) + 1);
	}

int sing_value_equal_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char key[20];

	strcpy(key,single_key);
	strcat(key,"andkey");
	return sing_value_equal_n(index,key,strlen(single_key),single_value,strlen(single_value) + 1);
	}

int sing_value_equal_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (sing_value_equal(index,"key#",single_value,strlen(single_value) + 1) == SING_RESULT_IMPOSSIBLE_KEY) ? 0 : 1;
	}

int sing_value_equal_test_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	test_delete_set("sing_value_equal_4");
	return (sing_value_equal(index,single_key,single_value,strlen(single_value) + 1) == SING_ERROR_CONNECTION_LOST)?0:1;
	}

int sing_value_equal_test_5(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (sing_value_equal(index,single_key,single_value,strlen(single_value) - 1) == SING_RESULT_VALUE_DIFFER) ? 0 : 1;
	}

int sing_value_equal_test_6_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (sing_value_equal(index,"keynotfound",single_value,strlen(single_value) + 1) == SING_RESULT_KEY_NOT_FOUND) ? 0 : 1;
	}

int sing_value_equal_test_6_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (sing_value_equal(index,single_key,single_phantom,strlen(single_phantom) + 1) == SING_RESULT_KEY_NOT_FOUND) ? 0 : 1;
	}

// sing_phantom_equal, sing_phantom_equal_n
// 1_1 ����� � ���������, �������, ���� ������, ������ ���������
// 1_2 ����� � ���������, �������, ���� ���������� � ���������, ������ ���������
// 2 ����� � ���������, ���� ������ ����������, ������ ���������
// 3 ����� ��� ��������
// 4 ����� � ���������, ���� ������, ������ ���������, �������� ����������

int sing_phantom_equal_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return sing_phantom_equal(index,single_key,single_phantom,strlen(single_phantom) + 1);
	}

int sing_phantom_equal_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int res = sing_phantom_equal(index,single_key,single_phantom,strlen(single_phantom) + 1);
	if (res != SING_RESULT_KEY_NOT_FOUND) return 1;
	return 0;
	}

int sing_phantom_equal_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int res = sing_phantom_equal(index,single_key,single_phantom,strlen(single_phantom) + 1);
	if (res != SING_ERROR_IMPOSSIBLE_OPERATION) return 1;
	return 0;
	}

int sing_phantom_equal_test_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int res = sing_phantom_equal(index,single_key,single_phantom,strlen(single_phantom) - 1);
	if (res != SING_RESULT_VALUE_DIFFER) return 1;
	return 0;
	}

// sing_values_equal, sing_values_equal_n
// 1 ������� sing_values_equal (+ ����������� ����, +���� �� ������, +���� �� ������ ����� ��������, +�������� ����������)
// 2 ������� sing_values_equal_n (+ ����������� ����, +���� �� ������, +���� �� ������ ����� �������� � ������, +�������� ����������)
// 3 ����� ������

int sing_values_equal_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *values[TEST_MULTIKEYS_CNT] = {multi_values[0],multi_values[1],multi_values[2],multi_values[3],"","","differ_value"};
	unsigned vsizes[TEST_MULTIKEYS_CNT] = {0,0,0,0,1,1,13}; 
	int results[TEST_MULTIKEYS_CNT];
	int i;

	for (i = 0; i < 4; i++)
		vsizes[i] = strlen(values[i]) + 1;

	int res = sing_values_equal(index,multi_keys,TEST_MULTIKEYS_CNT,(const void **)values,vsizes,results);
	if (res != 4) return 1;

	for (i = 0; i < 4; i++)
		if (results[i]) return 1;

	if (results[i++] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (results[i++] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (results[i++] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (results[i] != SING_RESULT_VALUE_DIFFER) return 1;
	return 0;
	}

int sing_values_equal_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *values[TEST_MULTIKEYS_CNT] = {multi_values[0],multi_values[1],multi_values[2],multi_values[3],"","","differ_value"};
	unsigned vsizes[TEST_MULTIKEYS_CNT] = {0,0,0,0,1,1,13}; 
	int results[TEST_MULTIKEYS_CNT];
	unsigned ksizes[TEST_MULTIKEYS_CNT];
	int i;

	for (i = 0; i < TEST_MULTIKEYS_CNT; i++)
		ksizes[i] = strlen(multi_keys[i]);

	for (i = 0; i < 4; i++)
		vsizes[i] = strlen(values[i]) + 1;

	int res = sing_values_equal_n(index,multi_keys,ksizes,TEST_MULTIKEYS_CNT,(const void **)values,vsizes,results);
	if (res != 4) return 1;

	for (i = 0; i < 4; i++)
		if (results[i]) return 1;

	if (results[i++] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (results[i++] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (results[i++] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (results[i] != SING_RESULT_VALUE_DIFFER) return 1;
	return 0;
	}

int sing_values_equal_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *values[TEST_MULTIKEYS_CNT] = {multi_values[0],multi_values[1],multi_values[2],multi_values[3],"","","differ_value"};
	unsigned vsizes[TEST_MULTIKEYS_CNT] = {0,0,0,0,13,1,1}; 
	int results[TEST_MULTIKEYS_CNT];

	test_delete_set("sing_values_equal_3");
	int res = sing_values_equal(index,multi_keys,TEST_MULTIKEYS_CNT,(const void **)values,vsizes,results);
	return (res == SING_ERROR_CONNECTION_LOST) ? 0 : 1;
	}

int main(void)
	{
	int rv = 0;
	unsigned i;

	FTestData tests[] = {
		{"sing_get_value_cb_1_1",one_key_value_prep,sing_get_value_cb_test_1,SING_LM_NONE,0},
		{"sing_get_value_cb_1_2",one_key_both_prep,sing_get_value_cb_test_1,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_get_value_cb_2",one_key_value_prep,sing_get_value_cb_test_2,SING_LM_NONE,0},
		{"sing_get_value_cb_3",one_key_value_prep,sing_get_value_cb_test_3,SING_LM_NONE,0},
		{"sing_get_value_cb_4",one_key_value_prep,sing_get_value_cb_test_4,SING_LM_NONE,0},
		{"sing_get_value_cb_5",one_key_value_prep,sing_get_value_cb_test_5,SING_LM_NONE,0},
		{"sing_get_value_cb_6_1",one_key_value_prep,sing_get_value_cb_test_6,SING_LM_NONE,0},
		{"sing_get_value_cb_6_2",one_key_phantom_prep,sing_get_value_cb_test_6,SING_LM_NONE,SING_UF_PHANTOM_KEYS},

		{"sing_get_phantom_cb_1_1",one_key_phantom_prep,sing_get_phantom_cb_test_1,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_get_phantom_cb_1_2",one_key_both_prep,sing_get_phantom_cb_test_1,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_get_phantom_cb_2",one_key_value_prep,sing_get_phantom_cb_test_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_get_phantom_cb_3",one_key_value_prep,sing_get_phantom_cb_test_3,SING_LM_NONE,0},

		{"sing_get_values_cb_1",many_key_value_prep,sing_get_values_cb_test_1,SING_LM_NONE,0},
		{"sing_get_values_cb_2",many_key_value_prep,sing_get_values_cb_test_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_get_values_cb_3",many_key_value_prep,sing_get_values_cb_test_3,SING_LM_NONE,0},
		{"sing_get_values_cb_4",many_key_value_prep,sing_get_values_cb_test_4,SING_LM_NONE,0},

		{"sing_get_value_1_1",one_key_value_prep,sing_get_value_test_1,SING_LM_NONE,0},
		{"sing_get_value_1_2",one_key_both_prep,sing_get_value_test_1,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_get_value_2",one_key_value_prep,sing_get_value_test_2,SING_LM_NONE,0},
		{"sing_get_value_3",one_key_value_prep,sing_get_value_test_3,SING_LM_NONE,0},
		{"sing_get_value_4",one_key_value_prep,sing_get_value_test_4,SING_LM_NONE,0},
		{"sing_get_value_5",one_key_value_prep,sing_get_value_test_5,SING_LM_NONE,0},
		{"sing_get_value_6_1",one_key_value_prep,sing_get_value_test_6,SING_LM_NONE,0},
		{"sing_get_value_6_2",one_key_phantom_prep,sing_get_value_test_6,SING_LM_NONE,SING_UF_PHANTOM_KEYS},

		{"sing_get_phantom_1_1",one_key_phantom_prep,sing_get_phantom_test_1,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_get_phantom_1_2",one_key_both_prep,sing_get_phantom_test_1,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_get_phantom_2",one_key_value_prep,sing_get_phantom_test_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_get_phantom_3",one_key_value_prep,sing_get_phantom_test_3,SING_LM_NONE,0},

		{"sing_get_values_1",many_key_value_prep,sing_get_values_test_1,SING_LM_NONE,0},
		{"sing_get_values_2",many_key_value_prep,sing_get_values_test_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_get_values_3",many_key_value_prep,sing_get_values_test_3,SING_LM_NONE,0},

		{"sing_get_values_simple_1",many_key_value_prep,sing_get_values_simple_test_1,SING_LM_NONE,0},
		{"sing_get_values_simple_2",many_key_value_prep,sing_get_values_simple_test_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_get_values_simple_3",many_key_value_prep,sing_get_values_simple_test_3,SING_LM_NONE,0},

		{"sing_get_values_same_1",many_key_value_prep,sing_get_values_same_test_1,SING_LM_NONE,0},
		{"sing_get_values_same_2",many_key_value_prep,sing_get_values_same_test_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_get_values_same_3",many_key_value_prep,sing_get_values_same_test_3,SING_LM_NONE,0},

		{"sing_key_present_1_1",one_key_value_prep,sing_key_present_test_1,SING_LM_NONE,0},
		{"sing_key_present_1_2",one_key_both_prep,sing_key_present_test_1,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_key_present_2",one_key_value_prep,sing_key_present_test_2,SING_LM_NONE,0},
		{"sing_key_present_3",one_key_value_prep,sing_key_present_test_3,SING_LM_NONE,0},
		{"sing_key_present_4",one_key_value_prep,sing_key_present_test_4,SING_LM_NONE,0},
		{"sing_key_present_5_1",one_key_value_prep,sing_key_present_test_5_1,SING_LM_NONE,0},
		{"sing_key_present_5_2",one_key_phantom_prep,sing_key_present_test_5_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},

		{"sing_phantom_present_1_1",one_key_phantom_prep,sing_phantom_present_test_1,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_phantom_present_1_2",one_key_both_prep,sing_phantom_present_test_1,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_phantom_present_2",one_key_value_prep,sing_phantom_present_test_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_phantom_present_3",one_key_value_prep,sing_phantom_present_test_3,SING_LM_NONE,0},

		{"sing_keys_present_1",many_key_value_prep,sing_keys_present_test_1,SING_LM_NONE,0},
		{"sing_keys_present_2",many_key_value_prep,sing_keys_present_test_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_keys_present_3",many_key_value_prep,sing_keys_present_test_3,SING_LM_NONE,0},

		{"sing_value_equal_1_1",one_key_value_prep,sing_value_equal_test_1,SING_LM_NONE,0},
		{"sing_value_equal_1_2",one_key_both_prep,sing_value_equal_test_1,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_value_equal_2",one_key_value_prep,sing_value_equal_test_2,SING_LM_NONE,0},
		{"sing_value_equal_3",one_key_value_prep,sing_value_equal_test_3,SING_LM_NONE,0},
		{"sing_value_equal_4",one_key_value_prep,sing_value_equal_test_4,SING_LM_NONE,0},
		{"sing_value_equal_5",one_key_value_prep,sing_value_equal_test_5,SING_LM_NONE,0},
		{"sing_value_equal_6_1",one_key_value_prep,sing_value_equal_test_6_1,SING_LM_NONE,0},
		{"sing_value_equal_6_2",one_key_phantom_prep,sing_value_equal_test_6_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},

		{"sing_phantom_equal_1_1",one_key_phantom_prep,sing_phantom_equal_test_1,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_phantom_equal_1_2",one_key_both_prep,sing_phantom_equal_test_1,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_phantom_equal_2",one_key_value_prep,sing_phantom_equal_test_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_phantom_equal_3",one_key_value_prep,sing_phantom_equal_test_3,SING_LM_NONE,0},
		{"sing_phantom_equal_4",one_key_phantom_prep,sing_phantom_equal_test_4,SING_LM_NONE,SING_UF_PHANTOM_KEYS},

		{"sing_values_equal_1",many_key_value_prep,sing_values_equal_test_1,SING_LM_NONE,0},
		{"sing_values_equal_2",many_key_value_prep,sing_values_equal_test_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_values_equal_3",many_key_value_prep,sing_values_equal_test_3,SING_LM_NONE,0},
		};
	unsigned tcnt = sizeof(tests) / sizeof(FTestData);
	for (i = 0; i < tcnt; i++)
		{
		if (run_test(&tests[i]))
			rv = 1;
		}
	return rv;
	}
