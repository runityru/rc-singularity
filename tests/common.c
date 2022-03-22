/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "../index.h"
#include "../cpages.h"
#include "../config.h"
#include "../rc_singularity.h"
#include "common.h"

int collision_search(unsigned hashtable_size,unsigned needed_hash,unsigned needed_cnt,char **words)
	{
	unsigned i;
	char letters[36];
	char word[10];
	int cnt = 0,num = 0;
	FTransformData tdata;

	tdata.value_source = NULL;
	tdata.use_phantom = 0;
	tdata.head.fields.chain_stop = 1;
	tdata.head.fields.diff_mark = 0;

	for (i = 0; i < 26; i++)
		letters[i] = 'a' + i;
	for (i = 0; i < 10; i++)
		letters[i + 26] = '0' + i;

	while (cnt < 20)
		{
		int pos = 0;
		int cnum = num++;
		do 
			{
			word[pos++] = letters[cnum % 36];
			cnum /= 36;
			if (pos >= 9)
				return cnt;
			}
		while (cnum);
		word[pos] = 0;

		cd_transform(word,MAX_KEY_SOURCE,&tdata);
		tdata.hash = hashtable_size;
		cd_encode(&tdata);
		if (tdata.hash == needed_hash)
			{
			strcpy(words[cnt],word);
			cnt++;
			}
		}
	return cnt;
	}

const char * const single_key = "key1";
const char * const single_value = "oldvalue";

element_type one_key_value_prep(FSingSet *index,int *res_mem)
	{
	if (sing_set_key(index,single_key,(void *)single_value,strlen(single_value) + 1))
		return 1;
	return 0;
	}

const char *multi_keys[7] = {"key1","key22","key333","key444","key5555","key66666","key@"};
const char *multi_keys_long[7] = {"key1long","key22long","key333long","key4444long","key55555long","key666666long","key@long"};
char *multi_values[5] = {"some string 1","some string 22","some string 333","some string 4444","some string 55555"};

element_type many_key_value_prep(FSingSet *index,int *res_mem)
	{
	int i;
	for(i = 0; i < 5; i++)
		if (sing_set_key(index,multi_keys[i],multi_values[i],strlen(multi_values[i]) + 1))
			return 1;
	return 0;
	}

void *deletion_thread(void *param)
	{
	FSingSet *index = sing_link_set((char *)param,0,NULL);
	sing_delete_set(index);
	return (void*) 0;
	}

void test_delete_set(char *index_name)
	{
	int *rv;
	pthread_t del_thread;
	pthread_create(&del_thread, NULL, deletion_thread, index_name);
	pthread_join(del_thread,(void**)&rv);
	}

int run_test(FTestData *test_data)
	{
	int rv = 1,reserved = 0;
	FSingSet *index = NULL;
	FSingConfig *config = sing_config_get_default();
	if (!config)
		return 1;
	if (!(index = idx_create_set(test_data->name,0,test_data->flags,config)))
		goto test_error;
	element_type prep_data = test_data->prep ? (*test_data->prep)(index,&reserved) : 0;
	idx_creation_done(index,test_data->lock_mode);
	if ((*(test_data->test))(index,&reserved,prep_data))
		goto test_error;
	if (!index->head->bad_states.states.deleted)
		{
		if (cp_dirty_mask_check(index))
			goto test_error;
		if (idx_check_all(index,reserved))
			goto test_error;
		}
	else 
		index = NULL;
	rv = 0;
test_error:
	if (rv)
		printf("Test %s failed: %s\n",test_data->name,index ? sing_get_error(index) : sing_config_get_error(config));
	if (index)
		sing_delete_set(index);
	if (config)
		sing_delete_config(config);
	return rv;
	}
