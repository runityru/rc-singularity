/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <stddef.h>
#include <stdio.h>

#include "../index.h"
#include "../cpages.h"
#include "../config.h"
#include "../rc_singularity.h"
#include "common.h"

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
			lck_waitForReaders(index->lock_set);
			idx_general_free(index,tdata->old_key_rest,tdata->old_key_rest_size);
			}
		lck_memoryUnlock(index);
		}
	}

int test_add_key(FSingSet *index,char *key_source,int vsize,unsigned char *value)
	{
	FTransformData tdata;

	test_make_tdata(index,key_source,vsize,value,&tdata);
	int rv = idx_key_try_set(index,&tdata);
	if (!rv)
		rv = idx_key_set(index,&tdata);
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