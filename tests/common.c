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
	if (!(index = idx_create_set(test_data->name,0,0,config)))
		goto test_error;
	element_type prep_data = test_data->prep ? (*test_data->prep)(index,&reserved) : 0;
	idx_creation_done(index,LM_NONE);
	if ((*(test_data->test))(index,&reserved,prep_data))
		goto test_error;
	if (cp_dirty_mask_check(index))
		goto test_error;
	if (idx_check_all(index,reserved))
		goto test_error;
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