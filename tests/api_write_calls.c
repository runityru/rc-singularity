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

// sing_add_key, sing_add_key_n
// 1_1 Успешно sing_add_key
// 1_2 Успешно sing_add_key, набор с фантомами, есть фантомный ключ
// 2 Успешно sing_add_key_n
// 3_1 Ключ уже есть
// 3_2 Ключ уже есть, набор с фатомами, есть нормальный ключ
// 4 Невозможный ключ
// 5 Набор удален

int sing_add_key_test_1_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_add_key(index,single_key,(void *)single_value,strlen(single_value) + 1);
	return rv;
	}

int sing_add_key_test_1_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_add_key(index,single_key,(void *)single_value,strlen(single_value) + 1);
	return rv;
	}

int sing_add_key_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_add_key_n(index,single_key,strlen(single_key),(void *)single_value,strlen(single_value) + 1);
	return rv;
	}

int sing_add_key_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_add_key(index,single_key,(void *)single_value,strlen(single_value) + 1);
	return (rv == SING_RESULT_KEY_PRESENT) ? 0 : 1;
	}

int sing_add_key_test_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_add_key(index,"key#",NULL,0);
	return (rv == SING_RESULT_IMPOSSIBLE_KEY) ? 0 : 1;
	}

int sing_add_key_test_5(FSingSet *index,int *res_mem,element_type prep_data)
	{
	test_delete_set("sing_add_key_5");
	int res = sing_add_key(index,single_key,(void *)single_value,strlen(single_value) + 1);
	return (res == SING_ERROR_CONNECTION_LOST) ? 0 : 1;
	}

// sing_add_phantom, sing_add_phantom_n
// 1_1 Набор с фантомами, успешно, ключа нет
// 1_2 Набор с фантомами, успешно, ключ с нормальным значением
// 2 Набор с фантомами, ключ фантомный
// 3 Набор с фантомами, ключ с фантомным значением
// 4 Набор без фантомов

int sing_add_phantom_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_add_phantom(index,single_key,(void *)single_phantom,strlen(single_phantom) + 1);
	if (rv)
		return 1;

	char value[20];
	unsigned vsize = 20;
	rv = sing_get_phantom(index,single_key,(void *)value,&vsize);
	if (rv || vsize != strlen(single_phantom) + 1 || strcmp(value,single_phantom)) 
		return 2;
	return 0;
	}

int sing_add_phantom_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_add_phantom(index,single_key,(void *)single_phantom,strlen(single_phantom) + 1);
	if (rv != SING_RESULT_KEY_PRESENT)
		return 1;
	return 0;
	}

int sing_add_phantom_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_add_phantom(index,single_key,(void *)single_phantom,strlen(single_phantom) + 1);
	if (rv != SING_RESULT_KEY_PRESENT)
		return 1;
	return 0;
	}

int sing_add_phantom_test_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_add_phantom(index,single_key,(void *)single_phantom,strlen(single_phantom) + 1);
	if (rv != SING_ERROR_IMPOSSIBLE_OPERATION)
		return 1;
	return 0;
	}

// sing_add_keys, sing_add_keys_n
// 1 Успешно sing_add_keys (+ невозможный ключ, +ключ имеется)
// 2 Успешно sing_add_keys_n, набор с фантомами (+ невозможный ключ, +ключ имеется)
// 3 Набор удален

int sing_add_keys_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	unsigned vsizes[TEST_MULTIKEYS_CNT]; 
	int results[TEST_MULTIKEYS_CNT];
	int i;

	for (i = 0; i < TEST_MULTIKEYS_CNT; i++)
		vsizes[i] = multi_values[i] ? (strlen(multi_values[i]) + 1) : 0;

	int res = sing_add_keys(index,multi_keys,TEST_MULTIKEYS_CNT,(const void **)multi_values,vsizes,results);
	if (res != 6) return 1;

	if (results[0] != SING_RESULT_KEY_PRESENT) return 1;
	for (i = 1; i < 5; i++)
		if (results[i]) return 1;

	if (results[i++] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (results[i++]) return 1;
	if (results[i]) return 1;
	return 0;
	}

int sing_add_keys_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	unsigned vsizes[TEST_MULTIKEYS_CNT]; 
	int results[TEST_MULTIKEYS_CNT];
	unsigned ksizes[TEST_MULTIKEYS_CNT];
	int i;

	for (i = 0; i < TEST_MULTIKEYS_CNT; i++)
		{
		ksizes[i] = strlen(multi_keys[i]);
		vsizes[i] = multi_values[i] ? (strlen(multi_values[i]) + 1) : 0;
		}

	int res = sing_add_keys_n(index,multi_keys_long,ksizes,TEST_MULTIKEYS_CNT,(const void **)multi_values,vsizes,results);
	if (res != 6) return 1;

	if (results[0] != SING_RESULT_KEY_PRESENT) return 1;
	for (i = 1; i < 5; i++)
		if (results[i]) return 1;

	if (results[i++] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (results[i++]) return 1;
	if (results[i]) return 1;
	return 0;
	}

int sing_add_keys_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	unsigned vsizes[TEST_MULTIKEYS_CNT]; 
	int results[TEST_MULTIKEYS_CNT];
	int i;

	for (i = 0; i < TEST_MULTIKEYS_CNT; i++)
		vsizes[i] = multi_values[i] ? (strlen(multi_values[i]) + 1) : 0;

	test_delete_set("sing_add_keys_3");
	int res = sing_add_keys(index,multi_keys,TEST_MULTIKEYS_CNT,(const void **)multi_values,vsizes,results);
	return (res == SING_ERROR_CONNECTION_LOST) ? 0 : 1;
	}

// sing_set_key, sing_set_key_n
// 1_1 Успешно sing_set_key, ключа нет
// 1_2 Успешно sing_set_key, есть нормальное значение
// 2_1 Успешно sing_set_key_n, набор с фантомами, есть фантомный ключ
// 2_2 Успешно sing_set_key_n, набор с фантомами, есть нормальное и фатомное значение
// 3 Невозможный ключ
// 4 Набор удален

int sing_set_key_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_set_key(index,single_key,(void *)single_value,strlen(single_value) + 1);
	return rv;
	}

int sing_set_key_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_set_key_n(index,single_key,strlen(single_key),(void *)"new_value",strlen("new_value") + 1);
	if (rv)
		return rv;
	char value[20];
	unsigned vsize = 20;
	rv = sing_get_phantom(index,single_key,(void *)value,&vsize);
	if (rv || vsize != strlen(single_phantom) + 1 || strcmp(value,single_phantom)) 
		return 2;
	return 0;
	}

int sing_set_key_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_set_key(index,"key#",NULL,0);
	return (rv == SING_RESULT_IMPOSSIBLE_KEY) ? 0 : 1;
	}

int sing_set_key_test_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	test_delete_set("sing_set_key_4");
	int res = sing_set_key(index,single_key,(void *)single_value,strlen(single_value) + 1);
	return (res == SING_ERROR_CONNECTION_LOST) ? 0 : 1;
	}

// sing_set_phantom, sing_set_phantom_n
// 1_1 Набор с фантомами, успешно, ключа нет
// 1_2 Набор с фантомами, успешно, ключ с нормальным значением
// 1_3 Набор с фантомами, успешно, ключ фантомный
// 1_4 Набор с фантомами, успешно, ключ с фантомным значением
// 2 Набор без фантомов

int sing_set_phantom_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_set_phantom(index,single_key,(void *)single_phantom,strlen(single_phantom) + 1);
	if (rv)
		return 1;

	char value[20];
	unsigned vsize = 20;
	rv = sing_get_phantom(index,single_key,(void *)value,&vsize);
	if (rv || vsize != strlen(single_phantom) + 1 || strcmp(value,single_phantom)) 
		return 2;
	return 0;
	}

int sing_set_phantom_test_1_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_set_phantom(index,single_key,(void *)single_phantom,strlen(single_phantom) + 1);
	if (rv)
		return 1;

	char value[20];
	unsigned vsize = 20;
	rv = sing_get_phantom(index,single_key,(void *)value,&vsize);
	if (rv || vsize != strlen(single_phantom) + 1 || strcmp(value,single_phantom)) 
		return 2;
	return 0;
	}

int sing_set_phantom_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_set_phantom(index,single_key,(void *)single_phantom,strlen(single_phantom) + 1);
	if (rv != SING_ERROR_IMPOSSIBLE_OPERATION)
		return 1;
	return 0;
	}

// sing_set_keys, sing_set_keys_n
// 1 Успешно sing_add_keys (+ невозможный ключ, +ключ имеется)
// 2 Успешно sing_add_keys_n, набор с фантомами (+ невозможный ключ, +ключ имеется)
// 3 Набор удален

int sing_set_keys_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *values[TEST_MULTIKEYS_CNT] = {"new_val"};
	unsigned vsizes[TEST_MULTIKEYS_CNT]; 
	int results[TEST_MULTIKEYS_CNT];
	int i;

	for (i = 1; i < TEST_MULTIKEYS_CNT; i++)
		values[i] = multi_values[i];
	for (i = 0; i < TEST_MULTIKEYS_CNT; i++)
		vsizes[i] = values[i] ? (strlen(values[i]) + 1) : 0;

	int res = sing_set_keys(index,multi_keys,TEST_MULTIKEYS_CNT,(const void **)values,vsizes,results);
	if (res != 7) return 1;

	for (i = 0; i < 5; i++)
		if (results[i]) return 1;

	if (results[i++] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (results[i++]) return 1;
	if (results[i]) return 1;
	return 0;
	}

int sing_set_keys_test_2(FSingSet *kvset,int *res_mem,element_type prep_data)
	{
	char *values[TEST_MULTIKEYS_CNT] = {"new_val"};
	unsigned vsizes[TEST_MULTIKEYS_CNT]; 
	int results[TEST_MULTIKEYS_CNT];
	unsigned ksizes[TEST_MULTIKEYS_CNT];
	int i;

	for (i = 0; i < TEST_MULTIKEYS_CNT; i++)
		ksizes[i] = strlen(multi_keys[i]);

			// Here we need general allocation for key+val to avoid false mask error
			// since replaced val in first key has slab allocation, and moved to general with phantom addition
	for (i = 1; i < TEST_MULTIKEYS_CNT; i++)
		values[i] = multi_values[i] ? multi_values[i] : "some_long_value";

	for (i = 0; i < TEST_MULTIKEYS_CNT; i++)
		vsizes[i] = strlen(values[i]) + 1;

	int res = sing_set_keys_n(kvset,multi_keys_long,ksizes,TEST_MULTIKEYS_CNT,(const void **)values,vsizes,results);
	if (res != 7) return 1;

	for (i = 0; i < 5; i++)
		if (results[i]) return 1;

	if (results[i++] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (results[i++]) return 1;
	if (results[i]) return 1;

	char value[20];
	unsigned vsize = 20;

	sing_get_phantom_n(kvset,multi_keys_long[0],ksizes[0],&value,&vsize);
	if (vsize != strlen(single_value) + 1 || strncmp(value,single_value,vsize))
		return 1;
	vsize = 20;
	sing_get_value_n(kvset,multi_keys_long[0],ksizes[0],&value,&vsize);
	if (vsize != strlen(values[0]) + 1 || strncmp(value,values[0],vsize))
		return 1;

	return 0;
	}

int sing_set_keys_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	unsigned vsizes[TEST_MULTIKEYS_CNT]; 
	int results[TEST_MULTIKEYS_CNT];
	int i;

	for (i = 0; i < TEST_MULTIKEYS_CNT; i++)
		vsizes[i] = multi_values[i] ? (strlen(multi_values[i]) + 1) : 0;

	test_delete_set("sing_set_keys_3");
	int res = sing_set_keys(index,multi_keys,TEST_MULTIKEYS_CNT,(const void **)multi_values,vsizes,results);
	return (res == SING_ERROR_CONNECTION_LOST) ? 0 : 1;
	}

// sing_del_key, sing_del_key_n
// 1 Успешно sing_del_key, ключ с нормальным значением
// 2_1 Успешно sing_del_key_n, набор с фантомами, ключ с нормальным значением
// 2_2 Успешно sing_del_key_n, набор с фантомами, есть нормальное и фатомное значение
// 3_1 Ключ не найден 
// 3_2 Ключ не найден, набор с фантомами, ключ фантомный
// 4 Невозможный ключ
// 5 Набор удален

int sing_del_key_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_del_key(index,single_key);
	return rv;
	}

int sing_del_key_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_del_key_n(index,single_key,strlen(single_key));
	return rv;
	}

int sing_del_key_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_del_key_n(index,single_key,strlen(single_key));
	return (rv == SING_RESULT_KEY_NOT_FOUND) ? 0 : 1;
	}

int sing_del_key_test_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_del_key(index,"key#");
	return (rv == SING_RESULT_IMPOSSIBLE_KEY) ? 0 : 1;
	}

int sing_del_key_test_5(FSingSet *index,int *res_mem,element_type prep_data)
	{
	test_delete_set("sing_del_key_5");
	int res = sing_del_key(index,single_key);
	return (res == SING_ERROR_CONNECTION_LOST) ? 0 : 1;
	}

// sing_del_phantom, sing_del_phantom_n
// 1 Успешно sing_del_phantom, набор с фантомами, фантомный ключ
// 2 Успешно sing_del_phantom_n, набор с фантомами, есть нормальное и фантомное значение
// 3_1 Ключ не найден 
// 3_2 Нет фантомного значения
// 4 Набор без фантомов

int sing_del_phantom_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_del_phantom(index,single_key);
	return rv;
	}

int sing_del_phantom_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_del_phantom_n(index,single_key,strlen(single_key));
	return rv;
	}

int sing_del_phantom_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_del_phantom(index,single_key);
	return (rv == SING_RESULT_KEY_NOT_FOUND) ? 0 : 1;
	}

int sing_del_phantom_test_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_del_phantom(index,single_key);
	return (rv == SING_ERROR_IMPOSSIBLE_OPERATION) ? 0 : 1;
	}



// sing_del_full, sing_del_full_n
// 1 Успешно sing_del_full, ключ с нормальным значением
// 2_1 Успешно sing_del_full_n, набор с фантомами, фантомный ключ
// 2_2 Успешно sing_del_full_n, набор с фантомами, есть нормальное и фантомное значение
// 3 Ключ не найден 
// 4 Невозможный ключ
// 5 Набор удален

int sing_del_full_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_del_full(index,single_key);
	return rv;
	}

int sing_del_full_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_del_full_n(index,single_key,strlen(single_key));
	return rv;
	}

int sing_del_full_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_del_full_n(index,single_key,strlen(single_key));
	return (rv == SING_RESULT_KEY_NOT_FOUND) ? 0 : 1;
	}

int sing_del_full_test_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_del_full(index,"key#");
	return (rv == SING_RESULT_IMPOSSIBLE_KEY) ? 0 : 1;
	}

int sing_del_full_test_5(FSingSet *index,int *res_mem,element_type prep_data)
	{
	test_delete_set("sing_del_full_5");
	int res = sing_del_full(index,single_key);
	return (res == SING_ERROR_CONNECTION_LOST) ? 0 : 1;
	}

// sing_del_keys, sing_del_keys_n
// 1 Успешно sing_del_keys (+ невозможный ключ, +ключ не найден)
// 2 Успешно sing_del_keys_n (+ невозможный ключ, +ключ не найден)
// 3 Набор удален

int sing_del_keys_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int results[TEST_MULTIKEYS_CNT];
	int i;

	int res = sing_del_keys(index,multi_keys,TEST_MULTIKEYS_CNT,results);
	if (res != 5) return 1;

	for (i = 0; i < 4; i++)
		if (results[i]) return 1;

	if (results[i++] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (results[i++] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (results[i++] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (results[i]) return 1;
	return 0;
	}

int sing_del_keys_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int results[TEST_MULTIKEYS_CNT];
	unsigned ksizes[TEST_MULTIKEYS_CNT];
	int i;

	for (i = 0; i < TEST_MULTIKEYS_CNT; i++)
		ksizes[i] = strlen(multi_keys[i]);

	int res = sing_del_keys_n(index,multi_keys,ksizes,TEST_MULTIKEYS_CNT,results);
	if (res != 5) return 1;

	for (i = 0; i < 4; i++)
		if (results[i]) return 1;

	if (results[i++] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (results[i++] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (results[i++] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (results[i]) return 1;
	return 0;
	}

int sing_del_keys_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int results[TEST_MULTIKEYS_CNT];

	test_delete_set("sing_del_keys_3");
	int res = sing_del_keys(index,multi_keys,TEST_MULTIKEYS_CNT,results);
	return (res == SING_ERROR_CONNECTION_LOST) ? 0 : 1;
	}

int main(void)
	{
	int rv = 0;
	unsigned i;

	FTestData tests[] = {
		{"sing_add_key_1_1",NULL,sing_add_key_test_1_1,SING_LM_NONE,0},
		{"sing_add_key_1_2",one_key_phantom_prep,sing_add_key_test_1_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_add_key_2",NULL,sing_add_key_test_2,SING_LM_NONE,0},
		{"sing_add_key_3_1",one_key_value_prep,sing_add_key_test_3,SING_LM_NONE,0},
		{"sing_add_key_3_2",one_key_value_prep,sing_add_key_test_3,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_add_key_4",NULL,sing_add_key_test_4,SING_LM_NONE,0},
		{"sing_add_key_5",NULL,sing_add_key_test_5,SING_LM_NONE,0},

		{"sing_add_phantom_1_1",NULL,sing_add_phantom_test_1,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_add_phantom_1_2",one_key_value_prep,sing_add_phantom_test_1,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_add_phantom_2",one_key_phantom_prep,sing_add_phantom_test_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_add_phantom_3",one_key_both_prep,sing_add_phantom_test_3,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_add_phantom_4",NULL,sing_add_phantom_test_4,SING_LM_NONE,0},

		{"sing_add_keys_1",one_key_value_prep,sing_add_keys_test_1,SING_LM_NONE,0},
		{"sing_add_keys_2",one_key_value_prep,sing_add_keys_test_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_add_keys_3",one_key_value_prep,sing_add_keys_test_3,SING_LM_NONE,0},

		{"sing_set_key_1_1",NULL,sing_set_key_test_1,SING_LM_NONE,0},
		{"sing_set_key_1_2",one_key_value_prep,sing_set_key_test_1,SING_LM_NONE,0},
		{"sing_set_key_2_1",one_key_phantom_prep,sing_set_key_test_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_set_key_2_2",one_key_both_prep,sing_set_key_test_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_set_key_3",NULL,sing_set_key_test_3,SING_LM_NONE,0},
		{"sing_set_key_4",NULL,sing_set_key_test_4,SING_LM_NONE,0},

		{"sing_set_phantom_1_1",NULL,sing_set_phantom_test_1,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_set_phantom_1_2",one_key_value_prep,sing_set_phantom_test_1,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_set_phantom_1_3",one_key_phantom_prep,sing_set_phantom_test_1,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_set_phantom_1_4",one_key_both_prep,sing_set_phantom_test_1_4,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_set_phantom_2",NULL,sing_set_phantom_test_2,SING_LM_NONE,0},

		{"sing_set_keys_1",one_key_value_prep,sing_set_keys_test_1,SING_LM_NONE,0},
		{"sing_set_keys_2",one_key_value_prep,sing_set_keys_test_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_set_keys_3",one_key_value_prep,sing_set_keys_test_3,SING_LM_NONE,0},

		{"sing_del_key_1",one_key_value_prep,sing_del_key_test_1,SING_LM_NONE,0},
		{"sing_del_key_2_1",one_key_value_prep,sing_del_key_test_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_del_key_2_2",one_key_both_prep,sing_del_key_test_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_del_key_3_1",NULL,sing_del_key_test_3,SING_LM_NONE,0},
		{"sing_del_key_3_2",one_key_phantom_prep,sing_del_key_test_3,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_del_key_4",NULL,sing_del_key_test_4,SING_LM_NONE,0},
		{"sing_del_key_5",one_key_value_prep,sing_del_key_test_5,SING_LM_NONE,0},

		{"sing_del_phantom_1",one_key_phantom_prep,sing_del_phantom_test_1,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_del_phantom_2",one_key_both_prep,sing_del_phantom_test_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_del_phantom_3_1",NULL,sing_del_phantom_test_3,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_del_phantom_3_2",one_key_value_prep,sing_del_phantom_test_3,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_del_phantom_4",one_key_value_prep,sing_del_phantom_test_4,SING_LM_NONE,0},

		{"sing_del_full_1",one_key_value_prep,sing_del_full_test_1,SING_LM_NONE,0},
		{"sing_del_full_2_1",one_key_phantom_prep,sing_del_full_test_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_del_full_2_2",one_key_both_prep,sing_del_full_test_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_del_full_3",NULL,sing_del_full_test_3,SING_LM_NONE,0},
		{"sing_del_full_4",NULL,sing_del_full_test_4,SING_LM_NONE,0},
		{"sing_del_full_5",one_key_value_prep,sing_del_full_test_5,SING_LM_NONE,0},

		{"sing_del_keys_1",many_key_value_prep,sing_del_keys_test_1,SING_LM_NONE,0},
		{"sing_del_keys_2",many_key_value_prep,sing_del_keys_test_2,SING_LM_NONE,SING_UF_PHANTOM_KEYS},
		{"sing_del_keys_3",many_key_value_prep,sing_del_keys_test_3,SING_LM_NONE,0},
		};
	unsigned tcnt = sizeof(tests) / sizeof(FTestData);
	for (i = 0; i < tcnt; i++)
		{
		if (run_test(&tests[i]))
			rv = 1;
		}
	return rv;
	}