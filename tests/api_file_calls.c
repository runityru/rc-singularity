/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "../rc_singularity.h"
#include "common.h"

void generate_csv_keys(char *filename,unsigned start,unsigned keys_cnt,unsigned valnum)
	{
	unsigned i;
	FILE *f = fopen(filename,"a");
	for (i = start; i < start + keys_cnt; i++)
		fprintf(f,"k%d\tval%d\n",i,valnum);
	fclose(f);
	}

element_type diff_prep(FSingSet *index,int *res_mem)
	{
	char key[20];
	int i;
	for(i = 0; i < 5000; i++)
		{
		sprintf(key,"k%d",i);
		if (sing_set_key(index,key,"val0",4))
			return 1;
		}
	return 0;
	}

#define OP_SUB 1
#define OP_ADD 2
#define OP_OLD 4
#define OP_REPLACE 8

int check_diff_result(char *filename,unsigned val1,unsigned val2,
			unsigned src_start,unsigned src_cnt,unsigned eq_start,unsigned eq_cnt,unsigned ne_cnt)
	{
	FILE *f = fopen(filename,"r");
	char op;
	unsigned keynum,valnum;
	unsigned i;
	int rv = 1;
	unsigned cnt = src_start + src_cnt;
	if (eq_start + eq_cnt + ne_cnt > cnt)
		cnt = eq_start + eq_cnt + ne_cnt;
	unsigned start = src_start;
	if (eq_start < src_start)
		start = eq_start;
	int *states = (int *)calloc(cnt,sizeof(int));
	for (i = src_start; i < src_start + src_cnt; i++)
		states[i] = OP_SUB;
	for (i = eq_start; i < eq_start + eq_cnt; i++)
		states[i] = states[i] ? 0 : OP_ADD;
	for (;i < eq_start + eq_cnt + ne_cnt; i++)
		states[i] = states[i] ? OP_OLD + OP_REPLACE : OP_ADD;

	while(fscanf(f,"%cK%d\tval%d\n",&op,&keynum,&valnum) != EOF)
		{
		if (keynum >= cnt)
			goto check_diff_result_exit;
		switch(op)
			{
			case '+': 
				if (valnum != val2)
					goto check_diff_result_exit;
				states[keynum] -= OP_ADD;
				break;
			case '-':
				if (valnum != val1)
					goto check_diff_result_exit;
				states[keynum] -= OP_SUB;
				break;
			case '!':
				if (valnum != val1)
					goto check_diff_result_exit;
				states[keynum] -= OP_OLD;
				break;
			case '=':
				if (valnum != val2)
					goto check_diff_result_exit;
				states[keynum] -= OP_REPLACE;
				break;
			}
		}
	for (i = start; i < cnt; i++)
		if (states[i])
			goto check_diff_result_exit;
	rv = 0;
check_diff_result_exit:
	free(states);
	fclose(f);
	return rv;
	}

int diff_replace_test(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char key[20],value[20];
	unsigned vsize = 20;
	int i,rv = 1, res;
	unlink("diff_test");
	generate_csv_keys("diff_test",4999,1,1);
	FSingCSVFile compare_file = {2,"diff_test",0,'\t'}; 
	if ((res = sing_diff_replace_file(index,&compare_file,"diff_test_result")))
		return res;
	for(i = 0; i < 4999; i++)
		{
		sprintf(key,"k%d",i);
		if (sing_key_present(index,key) != SING_RESULT_KEY_NOT_FOUND)
			goto diff_replace_test_exit;
		}
	res = sing_get_value(index,"k4999",value,&vsize);
	if (res || vsize != 4 || strncmp("val1",value,4))
		goto diff_replace_test_exit;
	if (!check_diff_result("diff_test_result",0,1,0,5000,4999,0,1))
		rv = 0;
diff_replace_test_exit:
	unlink("diff_test");
	unlink("diff_test_result");
	return rv;
	}

int diff_replace_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char key[20],value[20];
	unsigned vsize;
	int i,rv = 1, res;
	unlink("diff_test");
	generate_csv_keys("diff_test",500,2500,0);
	generate_csv_keys("diff_test",3000,3000,1);
	FSingCSVFile compare_file = {2,"diff_test",0,'\t'}; 
	if ((res = sing_diff_replace_file(index,&compare_file,"diff_test_result")))
		return res;
	for(i = 500; i < 3000; i++)
		{
		sprintf(key,"k%d",i);
		vsize = 20;
		res = sing_get_value(index,key,value,&vsize);
		if (res || vsize != 4 || strncmp("val0",value,4))
			goto diff_replace_test_3_exit;
		}
	for(i = 3000; i < 6000; i++)
		{
		sprintf(key,"k%d",i);
		vsize = 20;
		res = sing_get_value(index,key,value,&vsize);
		if (res || vsize != 4 || strncmp("val1",value,4))
			goto diff_replace_test_3_exit;
		}
	if (!check_diff_result("diff_test_result",0,1,0,5000,500,2500,3000))
		rv = 0;
diff_replace_test_3_exit:
	unlink("diff_test");
	unlink("diff_test_result");
	return rv;
	}

int diff_test(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char key[20],value[20];
	unsigned vsize = 20;
	int i,rv = 1, res;
	unlink("diff_test");
	generate_csv_keys("diff_test",4999,1,1);
	FSingCSVFile compare_file = {2,"diff_test",0,'\t'}; 
	if ((res = sing_diff_file(index,&compare_file,"diff_test_result")))
		return res;
	for(i = 0; i < 5000; i++)
		{
		sprintf(key,"k%d",i);
		res = sing_get_value(index,key,value,&vsize);
		if (res || vsize != 4 || strncmp("val0",value,4))
			goto diff_test_exit;
		}
	if (!check_diff_result("diff_test_result",0,1,0,5000,4999,0,1))
		rv = 0;
diff_test_exit:
	unlink("diff_test");
	unlink("diff_test_result");
	return rv;
	}

int diff_test_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char key[20],value[20];
	unsigned vsize;
	int i,rv = 1, res;
	unlink("diff_test");
	generate_csv_keys("diff_test",500,2500,0);
	generate_csv_keys("diff_test",3000,3000,1);
	FSingCSVFile compare_file = {2,"diff_test",0,'\t'}; 
	if ((res = sing_diff_file(index,&compare_file,"diff_test_result")))
		return res;
	for(i = 0; i < 5000; i++)
		{
		sprintf(key,"k%d",i);
		vsize = 20;
		res = sing_get_value(index,key,value,&vsize);
		if (res || vsize != 4 || strncmp("val0",value,4))
			goto diff_test_3_exit;
		}
	for(i = 5000; i < 6000; i++)
		{
		sprintf(key,"k%d",i);
		if(sing_key_present(index,key) != SING_RESULT_KEY_NOT_FOUND)
			goto diff_test_3_exit;
		}
	if (!check_diff_result("diff_test_result",0,1,0,5000,500,2500,3000))
		rv = 0;
diff_test_3_exit:
	unlink("diff_test");
	unlink("diff_test_result");
	return rv;
	}

int intersect_test(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char key[20],value[20];
	unsigned vsize = 20;
	int i,rv = 1, res;
	unlink("intersect_test");
	generate_csv_keys("intersect_test",2500,3000,1);
	FSingCSVFile compare_file = {2,"intersect_test",0,'\t'}; 
	if ((res = sing_intersect_file(index,&compare_file)))
		return res;
	for(i = 0; i < 2500; i++)
		{
		sprintf(key,"k%d",i);
		if(sing_key_present(index,key) != SING_RESULT_KEY_NOT_FOUND)
			goto intersect_test_exit;
		}
	for(i = 2500; i < 5000; i++)
		{
		sprintf(key,"k%d",i);
		res = sing_get_value(index,key,value,&vsize);
		if (res || vsize != 4 || strncmp("val0",value,4))
			goto intersect_test_exit;
		}
	for(i = 5000; i < 5500; i++)
		{
		sprintf(key,"k%d",i);
		if(sing_key_present(index,key) != SING_RESULT_KEY_NOT_FOUND)
			goto intersect_test_exit;
		}
	rv = 0;
intersect_test_exit:
	unlink("intersect_test");
	return rv;
	}

int intersect_replace_test(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char key[20],value[20];
	unsigned vsize = 20;
	int i,rv = 1, res;
	unlink("intersect_test");
	generate_csv_keys("intersect_test",2500,3000,1);
	FSingCSVFile compare_file = {2,"intersect_test",0,'\t'}; 
	if ((res = sing_intersect_replace_file(index,&compare_file)))
		return res;
	for(i = 0; i < 2500; i++)
		{
		sprintf(key,"k%d",i);
		if(sing_key_present(index,key) != SING_RESULT_KEY_NOT_FOUND)
			goto intersect_test_exit;
		}
	for(i = 2500; i < 5000; i++)
		{
		sprintf(key,"k%d",i);
		res = sing_get_value(index,key,value,&vsize);
		if (res || vsize != 4 || strncmp("val1",value,4))
			goto intersect_test_exit;
		}
	for(i = 5000; i < 5500; i++)
		{
		sprintf(key,"k%d",i);
		if((res = sing_key_present(index,key)) != SING_RESULT_KEY_NOT_FOUND)
			goto intersect_test_exit;
		}
	rv = 0;
intersect_test_exit:
	unlink("intersect_test");
	return rv;
	}

int main(void)
	{
	int rv = 0;
	unsigned i;

	FTestData tests[] = {
		{"intersect_replace_1",diff_prep,intersect_replace_test,SING_LM_NONE,0},
		{"diff_replace_1",diff_prep,diff_replace_test,SING_LM_NONE,0},
		{"diff_replace_2",diff_prep,diff_replace_test,SING_LM_NONE,SING_UF_COUNTERS},
		{"diff_replace_3",diff_prep,diff_replace_test_3,SING_LM_NONE,SING_UF_COUNTERS},
		{"diff_1",diff_prep,diff_test,SING_LM_NONE,0},
		{"diff_2",diff_prep,diff_test,SING_LM_NONE,SING_UF_COUNTERS},
		{"diff_3",diff_prep,diff_test_3,SING_LM_NONE,SING_UF_COUNTERS},
		{"intersect_1",diff_prep,intersect_test,SING_LM_NONE,0},
		{"intersect_2",diff_prep,intersect_test,SING_LM_NONE,SING_UF_COUNTERS},
		{"intersect_replace_2",diff_prep,intersect_replace_test,SING_LM_NONE,SING_UF_COUNTERS},
		};
	unsigned tcnt = sizeof(tests) / sizeof(FTestData);
	for (i = 0; i < tcnt; i++)
		{
		if (run_test(&tests[i]))
			rv = 1;
		}

	return rv;
	}