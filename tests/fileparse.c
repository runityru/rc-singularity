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

typedef struct FParseTestTg {
	char *line;
	int key_num;
	uint64_t vmask;
	int key_present;
	char *value;
	} FParseTest;

int run_parse_test(FParseTest *test_data)
	{
	int rv = 1;

	FILE *f = fopen("parse_test","w");
	fputs(test_data->line,f);
	fclose(f);
	char *error_msg = "";

	FSingSet *index = NULL;
	FSingConfig *config = sing_config_get_default();
	if (!config)
		return 1;
	FSingCSVFile filedesc;
	filedesc.filename = "parse_test";
	filedesc.delimiter = '\t';
	filedesc.key_col_num = test_data->key_num;
	filedesc.val_col_mask = test_data->vmask;

	if (!(index = sing_create_set("parse_test",&filedesc,0,SING_UF_NOT_PERSISTENT,SING_LM_NONE,config)))
		goto test_error;

	char value[20];
	unsigned vsize = 20;
	int res = sing_get_value(index,"key",value,&vsize);
	if (test_data->key_present && res)
		{ error_msg = "key absent"; goto test_error; }
	if (!test_data->key_present && res != SING_RESULT_KEY_NOT_FOUND)
		{ error_msg = "key present"; goto test_error; }
	if (!test_data->value)
		{
		if (vsize)
			{ error_msg = "value present"; goto test_error; }
		}
	else if (vsize != strlen(test_data->value) || strncmp(value,test_data->value,vsize))
		{ 
		error_msg = "value is incorrect"; 
		goto test_error; 
		}

	rv = 0;
test_error:
	unlink("parse_test");
	if (rv)
		printf("Test parse_test failed on line %s: %s\n",test_data->line,error_msg);
	if (index)
		sing_delete_set(index);
	if (config)
		sing_delete_config(config);
	return rv;
	}

int main(void)
	{
	int rv = 0;
	unsigned i;

	FParseTest test_lines[] = {
		{"v1",1,0LL,0,NULL},
		{"v1\n",1,0LL,0,NULL},
		{"v1\r\n",1,0LL,0,NULL},
		{"v1\tkey",1,0LL,1,NULL},
		{"v1\tkey\n",1,0LL,1,NULL},
		{"v1\tkey\r\n",1,0LL,1,NULL},
		{"v1\tkey",1,1LL,1,"v1"},
		{"v1\tkey\n",1,2LL,1,"key"},
		{"v1\tkey\r\n",1,3LL,1,"v1\tkey"},
		{"v1\tkey\tv2",1,0LL,1,NULL},
		{"v1\tkey\tv2\n",1,0LL,1,NULL},
		{"v1\tkey\tv2\r\n",1,0LL,1,NULL},
		{"v1\tkey\tv2",1,4LL,1,"v2"},
		{"v1\tkey\tv2\n",1,6LL,1,"key\tv2"},
		{"v1\tkey\tv2\r\n",1,0xFFFFLL,1,"v1\tkey\tv2"},
		};

	unsigned tcnt = sizeof(test_lines) / sizeof(FParseTest);
	for (i = 0; i < tcnt; i++)
		{
		if (run_parse_test(&test_lines[i]))
			rv = 1;
		}
	return rv;
	}