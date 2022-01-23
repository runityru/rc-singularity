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

// sing_lock_W
// 1 Успешно LM_SIMPLE
// 2 Успешно LM_PROTECTED
// 3 Успешно LM_FAST
// 4 Ошибка LM_NONE
// 5 Ошибка LM_READ_ONLY
// 6 Подключение только для чтения
// 7 Повторное подключение LM_PROTECTED
// 8 Повторное подключение LM_FAST
// 9 Набор удален

int lockW_test_success(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_lock_W(index);
	if (!rv)
		sing_unlock_commit(index,NULL);
	return rv;
	}

int lockW_test_fail_ImpOp(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_lock_W(index);
	if (!rv)
		sing_unlock_commit(index,NULL);
	return (rv == SING_ERROR_IMPOSSIBLE_OPERATION)?0:1;
	}

int lockW_test_fail_relock(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_lock_W(index);
	if (rv) return rv;
	rv = sing_lock_W(index);
	sing_unlock_commit(index,NULL);
	return (rv == SING_ERROR_IMPOSSIBLE_OPERATION)?0:1;
	}

int lockW_test_9(FSingSet *index,int *res_mem,element_type prep_data)
	{
	test_delete_set("lockW_9");
	int rv = sing_lock_W(index);
	if (!rv)
		sing_unlock_commit(index,NULL);
	return (rv == SING_ERROR_CONNECTION_LOST)?0:1;
	}

// sing_unlock_commit
// 1 Успешно LM_SIMPLE
// 2 Успешно LM_PROTECTED
// 3 Успешно LM_FAST
// 4 Набор не был заблокирован LM_SIMPLE
// 5 Набор не был заблокирован LM_FAST
// 6 Набор не был заблокирован LM_NONE

int unlock_commit_test_success(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_lock_W(index);
	if (rv)
		return rv;
	return sing_unlock_commit(index,NULL);
	}

int unlock_commit_test_fail_ImpOp(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_unlock_commit(index,NULL);
	return (rv == SING_ERROR_IMPOSSIBLE_OPERATION)?0:1;
	}

// sing_unlock_revert
// 1 Успешно LM_SIMPLE
// 2 Успешно LM_PROTECTED
// 3 Успешно LM_FAST
// 4 Набор не был заблокирован LM_SIMPLE
// 5 Набор не был заблокирован LM_FAST
// 6 Набор не был заблокирован LM_NONE
// 7 У набора нет дисковой копии

int unlock_revert_test_success(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char vbuf[10];
	unsigned vsize = 10;
	int rv = sing_lock_W(index);
	if (rv)
		return rv;
	int set_res = sing_set_key(index,single_key,"newval",9);
	if ((rv = sing_unlock_revert(index)))
		return rv;
	if (set_res) 
		return set_res;
	rv = sing_get_value(index,single_key,vbuf,&vsize);
	if (rv)
		return rv;
	if (vsize != strlen(single_value) + 1 || strcmp(vbuf,single_value))
		return 1;
	return 0;
	}

int unlock_revert_test_fail_ImpOp(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = sing_unlock_revert(index);
	return (rv == SING_ERROR_IMPOSSIBLE_OPERATION)?0:1;
	}

int unlock_revert_test_7(FSingSet *index,int *res_mem,element_type prep_data)
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

// sing_revert
// 1 Успешно SING_LM_FAST, ключ заменен
// 2 Успешно SING_LM_NONE, ключ добавлен
// 3 Неуспешно, режим SING_LM_SIMPLE
// 4 Неуспешно, режим SING_LM_PROTECTED
// 5 Неуспешно, режим SING_LM_READ_ONLY
// 6 Неуспешно, подключение SING_CF_READER
// 7 Неуспешно, набор без дисковой копии
// 8 Неуспешно, набор под ручной блокировкой
// 9 Неуспешно, набор удален

int sing_revert_test_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv;
	char vbuf[10];
	unsigned vsize = 10;
	int set_res = sing_set_key(index,single_key,"newval",9);
	if ((rv = sing_revert(index)))
		return rv;
	if (set_res) 
		return set_res;
	rv = sing_get_value(index,single_key,vbuf,&vsize);
	if (rv)
		return rv;
	if (vsize != strlen(single_value) + 1 || strcmp(vbuf,single_value))
		return 1;
	return 0;
	}

int sing_revert_test_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv;
	char vbuf[10];
	unsigned vsize = 10;
	int set_res = sing_set_key(index,single_key,"newval",9);
	if ((rv = sing_revert(index)))
		return rv;
	if (set_res) 
		return set_res;
	rv = sing_get_value(index,single_key,vbuf,&vsize);
	return (rv == SING_RESULT_KEY_NOT_FOUND) ? 0 : 1;
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
		{"lockW_1",NULL,lockW_test_success,SING_LM_SIMPLE,0},
		{"lockW_2",NULL,lockW_test_success,SING_LM_PROTECTED,0},
		{"lockW_3",NULL,lockW_test_success,SING_LM_FAST,0},
		{"lockW_4",NULL,lockW_test_fail_ImpOp,SING_LM_NONE,0},
		{"lockW_5",NULL,lockW_test_fail_ImpOp,SING_LM_READ_ONLY,0},
		{"lockW_6",NULL,lockW_test_fail_ImpOp,SING_LM_FAST,SING_CF_READER},
		{"lockW_7",NULL,lockW_test_fail_relock,SING_LM_PROTECTED,0},
		{"lockW_8",NULL,lockW_test_fail_relock,SING_LM_FAST,0},
		{"lockW_9",NULL,lockW_test_9,SING_LM_SIMPLE,0},

		{"unlock_commit_1",NULL,unlock_commit_test_success,SING_LM_SIMPLE,0},
		{"unlock_commit_2",NULL,unlock_commit_test_success,SING_LM_PROTECTED,0},
		{"unlock_commit_3",NULL,unlock_commit_test_success,SING_LM_FAST,0},
		{"unlock_commit_4",NULL,unlock_commit_test_fail_ImpOp,SING_LM_SIMPLE,0},
		{"unlock_commit_5",NULL,unlock_commit_test_fail_ImpOp,SING_LM_FAST,0},
		{"unlock_commit_6",NULL,unlock_commit_test_fail_ImpOp,SING_LM_NONE,0},

		{"unlock_revert_1",one_key_value_prep,unlock_revert_test_success,SING_LM_SIMPLE,0},
		{"unlock_revert_2",one_key_value_prep,unlock_revert_test_success,SING_LM_PROTECTED,0},
		{"unlock_revert_3",one_key_value_prep,unlock_revert_test_success,SING_LM_FAST,0},
		{"unlock_revert_4",NULL,unlock_revert_test_fail_ImpOp,SING_LM_SIMPLE,0},
		{"unlock_revert_5",NULL,unlock_revert_test_fail_ImpOp,SING_LM_FAST,0},
		{"unlock_revert_6",NULL,unlock_revert_test_fail_ImpOp,SING_LM_NONE,0},
		{"unlock_revert_7",NULL,unlock_revert_test_7,SING_LM_FAST,SING_UF_NOT_PERSISTENT},

		{"sing_flush_4_1",NULL,sing_flush_test_success,SING_LM_FAST,0},
		{"sing_flush_4_2",NULL,sing_flush_test_success,SING_LM_NONE,0},
		{"sing_flush_4_3",NULL,sing_flush_test_imp_op,SING_LM_SIMPLE,0},
		{"sing_flush_4_4",NULL,sing_flush_test_imp_op,SING_LM_PROTECTED,0},
		{"sing_flush_4_5",NULL,sing_flush_test_imp_op,SING_LM_READ_ONLY,0},
		{"sing_flush_4_6",NULL,sing_flush_test_imp_op,SING_LM_NONE,SING_CF_READER},
		{"sing_flush_4_7",NULL,sing_flush_test_imp_op,SING_LM_NONE,SING_UF_NOT_PERSISTENT},
		{"sing_flush_4_8",NULL,sing_flush_test_4_8,SING_LM_NONE,0},
		{"sing_flush_4_9",NULL,sing_flush_test_4_9,SING_LM_NONE,0},
		{"sing_revert_5_1",one_key_value_prep,sing_revert_test_1,SING_LM_FAST,0},
		{"sing_revert_5_2",NULL,sing_revert_test_2,SING_LM_NONE,0},
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
