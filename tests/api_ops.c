/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#ifdef __INTELLISENSE__
#define __null 0
#endif

#include "../rc_singularity.h"

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