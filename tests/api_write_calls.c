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

// sing_add_keys, sing_add_keys_n
// 1 Успешно sing_add_keys (+ невозможный ключ, +ключ имеется)
// 2 Успешно sing_add_keys_n (+ невозможный ключ, +ключ имеется)
// 3 Набор удален

int sing_add_keys_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *values[7] = {multi_values[0],multi_values[1],multi_values[2],multi_values[3],"","",multi_values[6]};
	unsigned vsizes[7]; 
	int results[7];
	int i;

	for (i = 0; i < 7; i++)
		vsizes[i] = strlen(values[i]) + 1;

	int res = sing_add_keys(index,multi_keys,7,(const void **)values,vsizes,results);
	if (res != 5) return 1;

	if (results[0] != SING_RESULT_KEY_PRESENT) return 1;
	for (i = 1; i < 5; i++)
		if (results[i]) return 1;

	if (results[i++] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (results[i]) return 1;
	return 0;
	}

int sing_add_keys_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *values[7] = {multi_values[0],multi_values[1],multi_values[2],multi_values[3],"","",multi_values[6]};
	unsigned vsizes[7]; 
	int results[7];
	unsigned ksizes[7];
	int i;

	for (i = 0; i < 7; i++)
		ksizes[i] = strlen(multi_keys[i]);
	for (i = 0; i < 7; i++)
		vsizes[i] = strlen(values[i]) + 1;

	int res = sing_add_keys_n(index,multi_keys_long,ksizes,7,(const void **)values,vsizes,results);
	if (res != 5) return 1;

	if (results[0] != SING_RESULT_KEY_PRESENT) return 1;
	for (i = 1; i < 5; i++)
		if (results[i]) return 1;

	if (results[i++] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (results[i]) return 1;
	return 0;
	}

int sing_add_keys_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *values[7] = {"","","","","","",""};
	unsigned vsizes[7] = {1,1,1,1,1,1,1}; 
	int results[7];

	test_delete_set("sing_add_keys_3");
	int res = sing_add_keys(index,multi_keys,7,(const void **)values,vsizes,results);
	return (res == SING_ERROR_CONNECTION_LOST) ? 0 : 1;
	}

// sing_set_keys, sing_set_keys_n
// 1 Успешно sing_add_keys (+ невозможный ключ, +ключ имеется)
// 2 Успешно sing_add_keys_n (+ невозможный ключ, +ключ имеется)
// 3 Набор удален

int sing_set_keys_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *values[7] = {"new_val",multi_values[1],multi_values[2],multi_values[3],"","",multi_values[6]};
	unsigned vsizes[7]; 
	int results[7];
	int i;

	for (i = 0; i < 7; i++)
		vsizes[i] = strlen(values[i]) + 1;

	int res = sing_set_keys(index,multi_keys,7,(const void **)values,vsizes,results);
	if (res != 6) return 1;

	for (i = 0; i < 5; i++)
		if (results[i]) return 1;

	if (results[i++] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (results[i]) return 1;
	return 0;
	}

int sing_set_keys_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *values[7] = {"new_val",multi_values[1],multi_values[2],multi_values[3],"","",multi_values[6]};
	unsigned vsizes[7]; 
	int results[7];
	unsigned ksizes[7];
	int i;

	for (i = 0; i < 7; i++)
		ksizes[i] = strlen(multi_keys[i]);

	for (i = 0; i < 7; i++)
		vsizes[i] = strlen(values[i]) + 1;

	int res = sing_set_keys_n(index,multi_keys_long,ksizes,7,(const void **)values,vsizes,results);
	if (res != 6) return 1;

	for (i = 0; i < 5; i++)
		if (results[i]) return 1;

	if (results[i++] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (results[i]) return 1;
	return 0;
	}

int sing_set_keys_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *values[7] = {"","","","","","",""};
	unsigned vsizes[7] = {1,1,1,1,1,1,1}; 
	int results[7];

	test_delete_set("sing_set_keys_3");
	int res = sing_set_keys(index,multi_keys,7,(const void **)values,vsizes,results);
	return (res == SING_ERROR_CONNECTION_LOST) ? 0 : 1;
	}

// sing_del_keys, sing_del_keys_n
// 1 Успешно sing_del_keys (+ невозможный ключ, +ключ не найден)
// 2 Успешно sing_del_keys_n (+ невозможный ключ, +ключ не найден)
// 3 Набор удален

int sing_del_keys_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int results[7];
	int i;

	int res = sing_del_keys(index,multi_keys,7,results);
	if (res != 5) return 1;

	for (i = 0; i < 4; i++)
		if (results[i]) return 1;

	if (results[i++] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (results[i++] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (results[i]) return 1;
	return 0;
	}

int sing_del_keys_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int results[7];
	unsigned ksizes[7];
	int i;

	for (i = 0; i < 7; i++)
		ksizes[i] = strlen(multi_keys[i]);

	int res = sing_del_keys_n(index,multi_keys,ksizes,7,results);
	if (res != 5) return 1;

	for (i = 0; i < 4; i++)
		if (results[i]) return 1;

	if (results[i++] != SING_RESULT_KEY_NOT_FOUND)	return 1;
	if (results[i++] != SING_RESULT_IMPOSSIBLE_KEY) return 1;
	if (results[i]) return 1;
	return 0;
	}

int sing_del_keys_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int results[7];

	test_delete_set("sing_del_keys_3");
	int res = sing_del_keys(index,multi_keys,7,results);
	return (res == SING_ERROR_CONNECTION_LOST) ? 0 : 1;
	}

int main(void)
	{
	int rv = 0;
	unsigned i;

	FTestData tests[] = {
		{"sing_add_keys_1",one_key_value_prep,sing_add_keys_test_1,SING_LM_NONE,0},
		{"sing_add_keys_2",one_key_value_prep,sing_add_keys_test_2,SING_LM_NONE,0},
		{"sing_add_keys_3",one_key_value_prep,sing_add_keys_test_3,SING_LM_NONE,0},
		{"sing_set_keys_1",one_key_value_prep,sing_set_keys_test_1,SING_LM_NONE,0},
		{"sing_set_keys_2",one_key_value_prep,sing_set_keys_test_2,SING_LM_NONE,0},
		{"sing_set_keys_3",one_key_value_prep,sing_set_keys_test_3,SING_LM_NONE,0},
		{"sing_del_keys_1",many_key_value_prep,sing_del_keys_test_1,SING_LM_NONE,0},
		{"sing_del_keys_2",many_key_value_prep,sing_del_keys_test_2,SING_LM_NONE,0},
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