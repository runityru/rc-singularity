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

void *deletion_thread(void *param)
	{
	FSingSet *index = sing_link_set((char *)param,0,NULL);
	sing_delete_set(index);
	return (void*) 0;
	}

static void test_delete_set(char *index_name)
	{
	int *rv;
	pthread_t del_thread;
	pthread_create(&del_thread, NULL, deletion_thread, index_name);
	pthread_join(del_thread,(void**)&rv);
	}

element_type one_key_value_prep(FSingSet *index,int *res_mem)
	{
	if (sing_set_key(index,"key","oldvalue",9))
		return 1;
	return 0;
	}

//(1) sing_lock_W
// 1.1 Успешно
// 1.2 Набор только для чтения
// 1.3 Подключение только для чтения
// 1.4 Повторное подключение 
// 1.5 Набор удален

int lockW_test_1_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_lock_W(index);
	if (!rv)
		sing_unlock_commit(index,NULL);
	return rv;
	}

int lockW_test_1_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_lock_W(index);
	if (!rv)
		sing_unlock_commit(index,NULL);
	return (rv == SING_ERROR_IMPOSSIBLE_OPERATION)?0:1;
	}

int lockW_test_1_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_lock_W(index);
	if (rv) return rv;
	rv = sing_lock_W(index);
	sing_unlock_commit(index,NULL);
	return (rv == SING_ERROR_IMPOSSIBLE_OPERATION)?0:1;
	}

int lockW_test_1_5(FSingSet *index,int *res_mem,element_type prep_data)
	{
	test_delete_set("lockW_1_5");
	int rv = sing_lock_W(index);
	if (!rv)
		sing_unlock_commit(index,NULL);
	return (rv == SING_ERROR_CONNECTION_LOST)?0:1;
	}

//(2) sing_unlock_commit
// 2.1 Успешно
// 2.2 Набор не был заблокирован

int unlock_commit_test_2_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_lock_W(index);
	if (rv)
		return rv;
	return sing_unlock_commit(index,NULL);
	}

int unlock_commit_test_2_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_unlock_commit(index,NULL);
	return (rv == SING_ERROR_IMPOSSIBLE_OPERATION)?0:1;
	}

//(3) sing_unlock_revert
// 3.1 Успешно
// 3.2 Набор не был заблокирован
// 3.3 У набора нет дисковой копии

int unlock_revert_test_3_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char vbuf[10];
	unsigned vsize = 10;
	int rv = sing_lock_W(index);
	if (rv)
		return rv;
	int set_res = sing_set_key(index,"key","newval",9);
	if ((rv = sing_unlock_revert(index)))
		return rv;
	if (set_res != SING_RESULT_KEY_PRESENT) 
		return set_res;
	rv = sing_get_value(index,"key",vbuf,&vsize);
	if (rv)
		return rv;
	if (vsize != 9 || strcmp(vbuf,"oldvalue"))
		return 1;
	return 0;
	}

int unlock_revert_test_3_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_unlock_revert(index);
	return (rv == SING_ERROR_IMPOSSIBLE_OPERATION)?0:1;
	}

int unlock_revert_test_3_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_lock_W(index);
	if (rv)
		return rv;
	rv = sing_unlock_revert(index);
	sing_unlock_commit(index,NULL);
	return (rv == SING_ERROR_IMPOSSIBLE_OPERATION)?0:1;
	}

//(4) sing_flush
// 4.1 Успешно SING_LM_FAST
// 4.2 Успешно SING_LM_NONE
// 4.3 Неуспешно, режим SING_LM_SIMPLE
// 4.4 Неуспешно, режим SING_LM_PROTECTED
// 4.5 Неуспешно, режим SING_LM_READ_ONLY
// 4.6 Неуспешно, подключение SING_CF_LOW_FD_READER
// 4.7 Неуспешно, набор без дисковой копии
// 4.8 Неуспешно, набор под ручной блокировкой
// 4.9 Неуспешно, набор удален

int sing_flush_test_success(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return sing_flush(index,NULL);
	}

int sing_flush_test_imp_op(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (sing_flush(index,NULL) == SING_ERROR_IMPOSSIBLE_OPERATION)?0:1;
	}

int sing_flush_test_4_8(FSingSet *index,int *res_mem,element_type prep_data)
	{
	if (sing_lock_W(index))
		return 0;
	int res = sing_flush(index,NULL);
	if (sing_unlock_revert(index))
		return 0;
	return (res == SING_ERROR_IMPOSSIBLE_OPERATION)?0:1;
	}

int sing_flush_test_4_9(FSingSet *index,int *res_mem,element_type prep_data)
	{
	test_delete_set("sing_flush_4_9");
	return (sing_flush(index,NULL) == SING_ERROR_CONNECTION_LOST)?0:1;
	}

//(5) sing_revert
// 5.1 Успешно SING_LM_FAST
// 5.2 Успешно SING_LM_NONE
// 5.3 Неуспешно, режим SING_LM_SIMPLE
// 5.4 Неуспешно, режим SING_LM_PROTECTED
// 5.5 Неуспешно, режим SING_LM_READ_ONLY
// 5.6 Неуспешно, подключение SING_CF_LOW_FD_READER
// 5.7 Неуспешно, набор без дисковой копии
// 5.8 Неуспешно, набор под ручной блокировкой
// 5.9 Неуспешно, набор удален

int sing_revert_test_success(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv;
	char vbuf[10];
	unsigned vsize = 10;
	int set_res = sing_set_key(index,"key","newval",9);
	if ((rv = sing_revert(index)))
		return rv;
	if (set_res != SING_RESULT_KEY_PRESENT) 
		return set_res;
	rv = sing_get_value(index,"key",vbuf,&vsize);
	if (rv)
		return rv;
	if (vsize != 9 || strcmp(vbuf,"oldvalue"))
		return 1;
	return 0;
	}

int sing_revert_test_imp_op(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (sing_revert(index) == SING_ERROR_IMPOSSIBLE_OPERATION)?0:1;
	}

int sing_revert_test_5_8(FSingSet *index,int *res_mem,element_type prep_data)
	{
	if (sing_lock_W(index))
		return 0;
	int res = sing_revert(index);
	if (sing_unlock_revert(index))
		return 0;
	return (res == SING_ERROR_IMPOSSIBLE_OPERATION)?0:1;
	}

int sing_revert_test_5_9(FSingSet *index,int *res_mem,element_type prep_data)
	{
	test_delete_set("sing_revert_5_9");
	return (sing_revert(index) == SING_ERROR_CONNECTION_LOST)?0:1;
	}

//(6) sing_get_value_cb, sing_get_value_cb_n
// 6.1 Успешно sing_get_value_cb
// 6.2 Успешно sing_get_value_cb_n
// 6.3 Невозможный ключ
// 6.4 Набр удален

void *test_allocator(unsigned size)
	{
	return malloc(size);
	}

int sing_get_value_cb_test_6_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char *value;
	unsigned vsize;

	int res = sing_get_value_cb_n(index,"key",3,test_allocator,(void **)&value,&vsize);
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

int main(void)
	{
	int rv = 0;
	unsigned i;

	FTestData tests[] = {
		{"lockW_1_1",NULL,lockW_test_1_1,SING_LM_NONE,0},
		{"lockW_1_2",NULL,lockW_test_1_2,SING_LM_READ_ONLY,0},
		{"lockW_1_3",NULL,lockW_test_1_2,SING_LM_NONE,SING_CF_READER},
		{"lockW_1_4",NULL,lockW_test_1_4,SING_LM_NONE,0},
		{"lockW_1_5",NULL,lockW_test_1_5,SING_LM_NONE,0},
		{"unlock_commit_2_1",NULL,unlock_commit_test_2_1,SING_LM_NONE,0},
		{"unlock_commit_2_1",NULL,unlock_commit_test_2_1,SING_LM_NONE,0},
		{"unlock_revert_3_1",one_key_value_prep,unlock_revert_test_3_1,SING_LM_NONE,0},
		{"unlock_revert_3_2",NULL,unlock_revert_test_3_2,SING_LM_NONE,0},
		{"unlock_revert_3_3",NULL,unlock_revert_test_3_3,SING_LM_NONE,SING_UF_NOT_PERSISTENT},
		{"sing_flush_4_1",NULL,sing_flush_test_success,SING_LM_FAST,0},
		{"sing_flush_4_2",NULL,sing_flush_test_success,SING_LM_NONE,0},
		{"sing_flush_4_3",NULL,sing_flush_test_imp_op,SING_LM_SIMPLE,0},
		{"sing_flush_4_4",NULL,sing_flush_test_imp_op,SING_LM_PROTECTED,0},
		{"sing_flush_4_5",NULL,sing_flush_test_imp_op,SING_LM_READ_ONLY,0},
		{"sing_flush_4_6",NULL,sing_flush_test_imp_op,SING_LM_NONE,SING_CF_READER},
		{"sing_flush_4_7",NULL,sing_flush_test_imp_op,SING_LM_NONE,SING_UF_NOT_PERSISTENT},
		{"sing_flush_4_8",NULL,sing_flush_test_4_8,SING_LM_NONE,0},
		{"sing_flush_4_9",NULL,sing_flush_test_4_9,SING_LM_NONE,0},
		{"sing_revert_5_1",one_key_value_prep,sing_revert_test_success,SING_LM_FAST,0},
		{"sing_revert_5_2",one_key_value_prep,sing_revert_test_success,SING_LM_NONE,0},
		{"sing_revert_5_3",NULL,sing_revert_test_imp_op,SING_LM_SIMPLE,0},
		{"sing_revert_5_4",NULL,sing_revert_test_imp_op,SING_LM_PROTECTED,0},
		{"sing_revert_5_5",NULL,sing_revert_test_imp_op,SING_LM_READ_ONLY,0},
		{"sing_revert_5_6",NULL,sing_revert_test_imp_op,SING_LM_NONE,SING_CF_READER},
		{"sing_revert_5_7",NULL,sing_revert_test_imp_op,SING_LM_NONE,SING_UF_NOT_PERSISTENT},
		{"sing_revert_5_8",NULL,sing_revert_test_5_8,SING_LM_NONE,0},
		{"sing_revert_5_9",NULL,sing_revert_test_5_9,SING_LM_NONE,0},
		{"sing_get_value_cb_6_1",one_key_value_prep,sing_get_value_cb_test_6_1,SING_LM_NONE,0},
		{"sing_get_value_cb_6_2",one_key_value_prep,sing_get_value_cb_test_6_2,SING_LM_NONE,0},
		{"sing_get_value_cb_6_3",one_key_value_prep,sing_get_value_cb_test_6_3,SING_LM_NONE,0},
		{"sing_get_value_cb_6_4",one_key_value_prep,sing_get_value_cb_test_6_4,SING_LM_NONE,0},
		};
	unsigned tcnt = sizeof(tests) / sizeof(FTestData);
	for (i = 0; i < tcnt; i++)
		{
		if (run_test(&tests[i]))
			rv = 1;
		}
	return rv;
	}
/*
int main(void)
	{
	int i,res,rv = 1;
	FSingConfig *config = sing_config_get_default();
	if (!config)
		return printf("Test sing_config_get_default failed\n"),1;

	FSingSet *kvset = sing_create_set("api_test",NULL,0,0,SING_LM_NONE,config);
	sing_delete_config(config);

	if (!kvset)
		return printf("Test sing_create_set failed with message: %s\n",sing_config_get_error(config)),1;

	const char *keys[7] = {"key1","key2","key3","key4","key5","key6","key@"};
	char *values[5] = {"some string 1","some string 2","some string 3","some string 4","some string 5"};

	if ((res = sing_lock_W(kvset)))
		return printf("Test sing_lock_W failed with error: %d\n",res),1;

	for (i = 0; i < 5; i++)
		{
		if ((res = sing_set_key(kvset,keys[i],(void *)values[i],strlen(values[i]) + 1)))
			{ printf("Test sing_set_key failed with error: %d\n",res); sing_unlock_revert(kvset); goto main_exit; }
		}

	if ((res = sing_unlock_commit(kvset,NULL)))
		{ printf("Test sing_unlock_commit failed with error: %d\n",res); goto main_exit; }

	res = sing_set_key(kvset,keys[4],(void *)"other string",strlen("other string") + 1);

	if (res != SING_RESULT_KEY_PRESENT)
		{ printf("Test sing_set_key with present key failed with error: %d\n",res); goto main_exit; }
		
	char vbuf[128];
	unsigned vsize = 128;
	if ((res = sing_get_value(kvset,keys[4],vbuf,&vsize)))
		{ printf("Test sing_get_value failed with error: %d\n",res); goto main_exit; }

	if (vsize != strlen("other string") + 1)
		{ printf("Test sing_get_value failed: values size is %d\n",vsize); goto main_exit; }

	if (strcmp("other string",vbuf))
		{ printf("Test sing_get_value failed: value is %s\n",vbuf); goto main_exit; }

	if ((res = sing_revert(kvset)))
		{ printf("Test sing_revert failed with error: %d\n",res); goto main_exit; }

	vsize = 128;
	if ((res = 	sing_get_value(kvset,keys[4],vbuf,&vsize)))
		{ printf("Test sing_get_value failed with error: %d\n",res); goto main_exit; }

	if (vsize != strlen(values[4]) + 1)
		{ printf("Test sing_revert failed: values size is %d\n",vsize); goto main_exit; }

	if (strcmp(values[4],vbuf))
		{ printf("Test sing_revert failed: value is %s\n",vbuf); goto main_exit; }

	void *vstore[7] = {&vbuf[0],&vbuf[20],&vbuf[40],&vbuf[60],&vbuf[80],&vbuf[100],&vbuf[120]};
	unsigned vsizes[7] = {20,20,20,20,20,20,8};
	int results[7];
	res = sing_get_values(kvset,keys,7,vstore,vsizes,results);
	if (res != 5)
		{ printf("Test sing_get_values failed with result: %d\n",res); goto main_exit; }

	for (i = 0; i < 5; i++)
		{
		if (results[i])
			{ printf("Test sing_get_values failed: result %d is %d\n",i,results[i]); goto main_exit; }
		if (vsizes[i] != strlen(values[i]) + 1)
			{ printf("Test sing_get_values failed: value %d size is %d\n",i,vsizes[i]); goto main_exit; }
		if (strcmp(values[i],&vbuf[i * 20]))
			{ printf("Test sing_revert failed: value %d is %s\n",i,&vbuf[i * 20]); goto main_exit; }
		}
	if (results[5] != SING_RESULT_KEY_NOT_FOUND)
		{ printf("Test sing_get_values failed: result 5 is %d\n",results[5]); goto main_exit; }
	if (vsizes[5] != 0)
		{ printf("Test sing_get_values failed: value 5 size is %d\n",vsizes[5]); goto main_exit; }
	if (results[6] != SING_RESULT_IMPOSSIBLE_KEY)
		{ printf("Test sing_get_values failed: result 6 is %d\n",results[6]); goto main_exit; }
	if (vsizes[6] != 0)
		{ printf("Test sing_get_values failed: value 6 size is %d\n",vsizes[6]); goto main_exit; }


	rv = 0;
main_exit:
	sing_delete_set(kvset);
	return rv;
	}
*/