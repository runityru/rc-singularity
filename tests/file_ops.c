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
	FILE *f;

	f = fopen(filename,"a");
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
			goto diff_test_exit;
		}
	res = sing_get_value(index,"k4999",value,&vsize);
	if (res || vsize != 4 || strncmp("val1",value,4))
		goto diff_test_exit;

	rv = 0;
diff_test_exit:
	unlink("diff_test");
	unlink("diff_test_result");
	return rv;
	}

int diff_replace_test_1_3(FSingSet *index,int *res_mem,element_type prep_data)
	{
	char key[20],value[20];
	unsigned vsize;
	int i,rv = 1, res;
	unlink("diff_test");
	generate_csv_keys("diff_test",0,2500,0);
	generate_csv_keys("diff_test",2500,2500,1);
	FSingCSVFile compare_file = {2,"diff_test",0,'\t'}; 
	if ((res = sing_diff_replace_file(index,&compare_file,"diff_test_result")))
		return res;
	for(i = 0; i < 2500; i++)
		{
		sprintf(key,"k%d",i);
		vsize = 20;
		res = sing_get_value(index,key,value,&vsize);
		if (res || vsize != 4 || strncmp("val0",value,4))
			goto diff_test_exit;
		}
	for(i = 2500; i < 5000; i++)
		{
		sprintf(key,"k%d",i);
		vsize = 20;
		res = sing_get_value(index,key,value,&vsize);
		if (res || vsize != 4 || strncmp("val1",value,4))
			goto diff_test_exit;
		}
	rv = 0;
diff_test_exit:
	unlink("diff_test");
	unlink("diff_test_result");
	return rv;
	}



int main(void)
	{
	int rv = 0;
	unsigned i;

	FTestData tests[] = {
		{"diff_1_1",diff_prep,diff_replace_test,SING_LM_NONE,0},
		{"diff_1_2",diff_prep,diff_replace_test,SING_LM_NONE,SING_UF_COUNTERS},
		{"diff_1_3",diff_prep,diff_replace_test_1_3,SING_LM_NONE,SING_UF_COUNTERS},
		};
	unsigned tcnt = sizeof(tests) / sizeof(FTestData);
	for (i = 0; i < tcnt; i++)
		{
		if (run_test(&tests[i]))
			rv = 1;
		}
	return rv;
	}