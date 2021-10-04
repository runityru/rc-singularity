/*
 * Copyright (C) �Hostcomm� LLC
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

void test_make_tdata(FSingSet *index,char *key_source,int vsize,unsigned char *value,FTransformData *tdata)
	{
	tdata->value_source = value;
	tdata->value_size = vsize;
	tdata->head.fields.chain_stop = 1;
	tdata->head.fields.diff_mark = 0;
	cd_transform(key_source,MAX_KEY_SOURCE,tdata);
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

int test_set_key(FSingSet *index,char *key_source,int vsize,unsigned char *value)
	{
	FTransformData tdata;

	test_make_tdata(index,key_source,vsize,value,&tdata);
	int rv = idx_key_try_set(index,&tdata,KS_ADDED | KS_DELETED);
	if (!rv)
		rv = idx_key_set(index,&tdata,KS_ADDED | KS_DELETED);
	test_process_res(index,rv,&tdata);
	return rv;
	}

int test_del_key(FSingSet *index,char *key_source)
	{
	FTransformData tdata;

	test_make_tdata(index,key_source,0,NULL,&tdata);

	int rv = idx_key_del(index,&tdata);
	if (rv & KS_CHANGED)
		lck_memoryUnlock(index);
	return rv;
	}

//(1) ���������� ���� �����
//(1.1) ���� ��� ����
// 1_1_1 ��� ��������
// 1_1_2 ���� ��������
//(1.2) ���� � �����
// 1_2_1 ��� ��������
// 1_2_2 ���� ��������

int alloc_rest_test_1_1_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = test_set_key(index,"a",0,NULL);
	return (rv & KS_ADDED)?0:1;
	}

int alloc_rest_test_1_1_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	uint32_t val = 0;
	int rv = test_set_key(index,"a",4,(unsigned char*)&val);
	return (rv & KS_ADDED)?0:1;
	}

int alloc_rest_test_1_2_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = test_set_key(index,"abcde",0,NULL);
	return (rv & KS_ADDED)?0:1;
	}

int alloc_rest_test_1_2_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	uint32_t val = 0;
	int rv = test_set_key(index,"abcde",4,(unsigned char*)&val);
	return (rv & KS_ADDED)?0:1;
	}

//(2) ������ �������� � �����
//(2.1) � ������� ��� ��������
// 2_1_1 � ������ ��� ��������
// 2_1_2 � ������ ���� ��������
//(2.2) � ������� ���� ��������
// 2_2_1 � ������ ��� ��������
// 2_2_2 � ������ ���� ��������, �� ���������
// 2_2_3 � ������ ���� ��������, ���������

element_type replace_value_prep_2_1(FSingSet *index,int *res_mem)
	{
	test_set_key(index,"abcde",0,NULL);
	return 0;
	}

int replace_value_test_2_1_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (test_set_key(index,"abcde",0,NULL) & KS_CHANGED)?1:0;
	}

int replace_value_test_2_1_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	uint32_t val = 0;
	return (test_set_key(index,"abcde",4,(unsigned char*)&val) & KS_CHANGED)?0:1;
	}

element_type replace_value_prep_2_2(FSingSet *index,int *res_mem)
	{
	uint32_t val = 0;
	test_set_key(index,"abcde",4,(unsigned char*)&val);
	return 0;
	}

int replace_value_test_2_2_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (test_set_key(index,"abcde",0,NULL) & KS_CHANGED)?0:1;
	}

int replace_value_test_2_2_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	uint32_t val = 1;
	return (test_set_key(index,"abcde",4,(unsigned char*)&val) & KS_CHANGED)?0:1;
	}

int replace_value_test_2_2_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	uint32_t val = 0;
	return (test_set_key(index,"abcde",4,(unsigned char*)&val) & KS_CHANGED)?1:0;
	}

//(3) ������� ���������� ����� � ����������
// 3_1 ���� ���� � ����������
// 3_2 ����� ��� � ����������, ���� �����������
// 3_3 ����� ��� � ����������, ���� ����� � ����������
// 3_4 ����� ��� � ����������, ��� ����� � ����������

int test_try_add_key(FSingSet *index,char *key_source,int vsize,unsigned char *value)
	{
	FTransformData tdata;

	test_make_tdata(index,key_source,vsize,value,&tdata);

	int rv = idx_key_try_set(index,&tdata,KS_ADDED | KS_DELETED);
	test_process_res(index,rv,&tdata);
	return rv;
	}

element_type key_try_set_prep_3_1(FSingSet *index,int *res_mem)
	{
	test_set_key(index,collisions[0],0,NULL);
	test_set_key(index,collisions[1],0,NULL);
	return 0;
	}

int key_try_set_test_3_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = test_try_add_key(index,collisions[prep_data],0,NULL);
	return (rv & KS_ADDED) ? 1: 0;
	}

element_type key_try_set_prep_3_2(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 8; i++)
		test_set_key(index,collisions[i],0,NULL);
	return 8;
	}

int key_try_set_test_3_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = test_try_add_key(index,collisions[prep_data],0,NULL);
	return rv;
	}

element_type key_try_set_prep_3_3(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 6; i++)
		test_set_key(index,collisions[i],0,NULL);
	return 6;
	}

int key_try_set_test_3_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (test_try_add_key(index,collisions[prep_data],0,NULL) & KS_ADDED) ? 0 : 1;
	}
 
 element_type key_try_set_prep_3_4(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 7; i++)
		test_set_key(index,collisions[i],0,NULL);
	return 7;
	}

int key_try_set_test_3_4(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (test_try_add_key(index,collisions[prep_data],0,NULL) & KS_ADDED) ? 0 : 1;
	}

//(4) ���������� ����� � ������� ����� ���-�������
//(4.1) ���� ����
// 4_1_1 � ������ �����
// 4_1_2 ������ � �����
// 4_1_3 ������� � �����
// 4_1_4 ������ � ����. �����
//(4.2) ����� ���
// 4_2_1 ���� 8 ����������
//(4.2.2) ���� ����� 8 ���������� (���)
// 4_2_2_1 ���� ���������� � �������, ������� ��������
// 4_2_2_2 ����� ����� ������ �����
// 4_2_2_3 ����� ����� ������ ����, ����� ������ ������ �����
// 4_2_2_4 ����� ������ � ����� ����� ������ �����

int key_set_test_4_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (test_set_key(index,collisions[prep_data],0,NULL) & KS_ADDED) ? 1 : 0;
	}

 element_type key_set_prep_4_1_1(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 9; i++)
		test_set_key(index,collisions[i],0,NULL);
	return 7;
	}

 element_type key_set_prep_4_1_2(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 9; i++)
		test_set_key(index,collisions[i],0,NULL);
	return 8;
	}

 element_type key_set_prep_4_1_3(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 15; i++)
		test_set_key(index,collisions[i],0,NULL);
	return 14;
	}

 element_type key_set_prep_4_1_4(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 16; i++)
		test_set_key(index,collisions[i],0,NULL);
	return 15;
	}

int key_set_test_4_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (test_set_key(index,collisions[prep_data],0,NULL) & KS_ADDED) ? 0 : 1;
	}

element_type key_set_prep_4_2_1(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 15; i++)
		test_set_key(index,collisions[i],0,NULL);
	return 15;
	}

element_type key_set_prep_4_2_2_1(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 4; i++)
		{
		test_set_key(index,collisions[i],0,NULL);
		test_set_key(index,collisions2[i],0,NULL);
		}
	for (i = 4; i < 11; i++)
		test_set_key(index,collisions[i],0,NULL);
	test_del_key(index,collisions2[3]);
	return 11;
	}

element_type key_set_prep_4_2_2_2(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 3; i++)
		{
		test_set_key(index,collisions[i],0,NULL);
		test_set_key(index,collisions2[i],0,NULL);
		}
	for (i = 3; i < 6; i++)
		test_set_key(index,collisions[i],0,NULL);
	return 6;
	}

element_type key_set_prep_4_2_2_3(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 4; i++)
		{
		test_set_key(index,collisions[i],0,NULL);
		test_set_key(index,collisions2[i],0,NULL);
		}
	test_set_key(index,collisions2[4],0,NULL);
	for (i = 4; i < 9; i++)
		test_set_key(index,collisions[i],0,NULL);
	return 9;
	}

element_type key_set_prep_4_2_2_4(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 4; i++)
		{
		test_set_key(index,collisions[i],0,NULL);
		test_set_key(index,collisions2[i],0,NULL);
		}
	test_set_key(index,collisions2[4],0,NULL);
	for (i = 4; i < 10; i++)
		test_set_key(index,collisions[i],0,NULL);
	return 10;
	}

//(5) �������� ���� �����
//(5.1) ���� ��� ����
// 5_1_1 ��� ��������
// 5_1_2 ���� ��������
//(5.2) ���� � �����
// 5_2_1 ��� ��������
// 5_2_2 ���� ��������

int del_key_rest_test_5_1(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (test_del_key(index,"a") & KS_DELETED)?0:1;
	}

element_type del_key_rest_prep_5_1_1(FSingSet *index,int *res_mem)
	{
	test_set_key(index,"a",0,NULL);
	return 0;
	}

element_type del_key_rest_prep_5_1_2(FSingSet *index,int *res_mem)
	{
	uint32_t val = 0;
	test_set_key(index,"a",4,(unsigned char*)&val);
	return 0;
	}

int del_key_rest_test_5_2(FSingSet *index,int *res_mem,element_type prep_data)
	{
	return (test_del_key(index,"abcde") & KS_DELETED)?0:1;
	}

element_type del_key_rest_prep_5_2_1(FSingSet *index,int *res_mem)
	{
	test_set_key(index,"abcde",0,NULL);
	return 0;
	}

element_type del_key_rest_prep_5_2_2(FSingSet *index,int *res_mem)
	{
	uint32_t val = 0;
	test_set_key(index,"abcde",4,(unsigned char*)&val);
	return 0;
	}

//(6) �������� �����
//(6_1) �������� ����� �� ���-�������, ������� ��� �����������
// 6_1_1 ���� �� ���������
// 6_1_2 ���� ���������
//(6_2) �������� ����� �� ���-�������, ������� � ������������
// 6_2_1 ����������� ������� 1 ���������
// 6_2_2 ����������� ������� 2 ���������
// 6_2_3 ����������� ������� ������ ���� � 2 ���������
//(6_3) �������� ������� ����� ����� ���-�������, 
// 6_3_1 ����������� ������� 1 ���������
// 6_3_2 ����������� ������� 2 ���������
//(6_4) �������� ������� ����� ����� ���-�������
// 6_4_1 �� ���������
// 6_4_2 �� �������������
// 6_4_3 � ���� ����� 8 ���������
// 6_4_4 ���� ��������� ���� �� 2 ����������
// 6_4_5 ���� ��������� ���� �� 3 ����������
//	6_5 �������� ������� �� ������ ����� ������� �� 2-� ���������� 

int del_key_test(FSingSet *index,int *res_mem,element_type prep_data)
	{
	int rv = test_del_key(index,collisions[prep_data]);
	return (rv & KS_DELETED)?0:1;
	}

element_type del_key_prep_6_1_1(FSingSet *index,int *res_mem)
	{
	test_set_key(index,collisions[0],0,NULL);
	test_set_key(index,collisions[1],0,NULL);
	return 0;
	}

element_type del_key_prep_6_1_2(FSingSet *index,int *res_mem)
	{
	test_set_key(index,collisions[0],0,NULL);
	test_set_key(index,collisions[1],0,NULL);
	return 1;
	}

element_type del_key_prep_6_2_1(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 8; i++)
		test_set_key(index,collisions[i],0,NULL);
	return 1;
	}

element_type del_key_prep_6_2_2(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 9; i++)
		test_set_key(index,collisions[i],0,NULL);
	return 1;
	}

element_type del_key_prep_6_2_3(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 16; i++)
		test_set_key(index,collisions[i],0,NULL);
	return 1;
	}

element_type del_key_prep_6_3_1(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 8; i++)
		test_set_key(index,collisions[i],0,NULL);
	return 7;
	}

element_type del_key_prep_6_3_2(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 9; i++)
		test_set_key(index,collisions[i],0,NULL);
	return 7;
	}

element_type del_key_prep_6_4_1(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 9; i++)
		test_set_key(index,collisions[i],0,NULL);
	return 8;
	}

element_type del_key_prep_6_4_2(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 10; i++)
		test_set_key(index,collisions[i],0,NULL);
	return 8;
	}

element_type del_key_prep_6_4_3(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 15; i++)
		test_set_key(index,collisions[i],0,NULL);
	return 8;
	}

element_type del_key_prep_6_4_4(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 16; i++)
		test_set_key(index,collisions[i],0,NULL);
	return 8;
	}

element_type del_key_prep_6_4_5(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 17; i++)
		test_set_key(index,collisions[i],0,NULL);
	return 8;
	}

element_type del_key_prep_6_5(FSingSet *index,int *res_mem)
	{
	unsigned i;
	for (i = 0; i < 17; i++)
		test_set_key(index,collisions[i],0,NULL);
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
		{"alloc_rest_1_1_1",NULL,alloc_rest_test_1_1_1,LM_NONE,0},
		{"alloc_rest_1_1_2",NULL,alloc_rest_test_1_1_2,LM_NONE,0},
		{"alloc_rest_1_2_1",NULL,alloc_rest_test_1_2_1,LM_NONE,0},
		{"alloc_rest_1_2_2",NULL,alloc_rest_test_1_2_2,LM_NONE,0},
		{"replace_value_2_1_1",replace_value_prep_2_1,replace_value_test_2_1_1,LM_NONE,0},
		{"replace_value_2_1_2",replace_value_prep_2_1,replace_value_test_2_1_2,LM_NONE,0},
		{"replace_value_2_2_1",replace_value_prep_2_2,replace_value_test_2_2_1,LM_NONE,0},
		{"replace_value_2_2_2",replace_value_prep_2_2,replace_value_test_2_2_2,LM_NONE,0},
		{"replace_value_2_2_3",replace_value_prep_2_2,replace_value_test_2_2_3,LM_NONE,0},
		{"key_try_set_3_1",key_try_set_prep_3_1,key_try_set_test_3_1,LM_NONE,0},
		{"key_try_set_3_2",key_try_set_prep_3_2,key_try_set_test_3_2,LM_NONE,0},
		{"key_try_set_3_3",key_try_set_prep_3_3,key_try_set_test_3_3,LM_NONE,0},
		{"key_try_set_3_4",key_try_set_prep_3_4,key_try_set_test_3_4,LM_NONE,0},
		{"key_set_4_1_1",key_set_prep_4_1_1,key_set_test_4_1,LM_NONE,0},
		{"key_set_4_1_2",key_set_prep_4_1_2,key_set_test_4_1,LM_NONE,0},
		{"key_set_4_1_3",key_set_prep_4_1_3,key_set_test_4_1,LM_NONE,0},
		{"key_set_4_1_4",key_set_prep_4_1_4,key_set_test_4_1,LM_NONE,0},
		{"key_set_4_2_1",key_set_prep_4_2_1,key_set_test_4_2,LM_NONE,0},
		{"key_set_4_2_2_1",key_set_prep_4_2_2_1,key_set_test_4_2,LM_NONE,0},
		{"key_set_4_2_2_2",key_set_prep_4_2_2_2,key_set_test_4_2,LM_NONE,0},
		{"key_set_4_2_2_3",key_set_prep_4_2_2_3,key_set_test_4_2,LM_NONE,0},
		{"key_set_4_2_2_4",key_set_prep_4_2_2_4,key_set_test_4_2,LM_NONE,0},
		{"del_key_rest_5_1_1",del_key_rest_prep_5_1_1,del_key_rest_test_5_1,LM_NONE,0},
		{"del_key_rest_5_1_2",del_key_rest_prep_5_1_2,del_key_rest_test_5_1,LM_NONE,0},
		{"del_key_rest_5_2_1",del_key_rest_prep_5_2_1,del_key_rest_test_5_2,LM_NONE,0},
		{"del_key_rest_5_2_2",del_key_rest_prep_5_2_2,del_key_rest_test_5_2,LM_NONE,0},
		{"del_key_6_1_1",del_key_prep_6_1_1,del_key_test,LM_NONE,0},
		{"del_key_6_1_2",del_key_prep_6_1_2,del_key_test,LM_NONE,0},
		{"del_key_6_2_1",del_key_prep_6_2_1,del_key_test,LM_NONE,0},
		{"del_key_6_2_2",del_key_prep_6_2_2,del_key_test,LM_NONE,0},
		{"del_key_6_2_3",del_key_prep_6_2_3,del_key_test,LM_NONE,0},
		{"del_key_6_3_1",del_key_prep_6_3_1,del_key_test,LM_NONE,0},
		{"del_key_6_3_2",del_key_prep_6_3_2,del_key_test,LM_NONE,0},
		{"del_key_6_4_1",del_key_prep_6_4_1,del_key_test,LM_NONE,0},
		{"del_key_6_4_2",del_key_prep_6_4_2,del_key_test,LM_NONE,0},
		{"del_key_6_4_3",del_key_prep_6_4_3,del_key_test,LM_NONE,0},
		{"del_key_6_4_4",del_key_prep_6_4_4,del_key_test,LM_NONE,0},
		{"del_key_6_4_5",del_key_prep_6_4_5,del_key_test,LM_NONE,0},
		{"del_key_6_5",del_key_prep_6_5,del_key_test,LM_NONE,0},
		};
	unsigned tcnt = sizeof(tests) / sizeof(FTestData);
	for (i = 0; i < tcnt; i++)
		{
		if (run_test(&tests[i]))
			rv = 1;
		}
	return rv;
	}