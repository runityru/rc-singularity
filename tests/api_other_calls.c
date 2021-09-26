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
		};
	unsigned tcnt = sizeof(tests) / sizeof(FTestData);
	for (i = 0; i < tcnt; i++)
		{
		if (run_test(&tests[i]))
			rv = 1;
		}
	return rv;
	}
