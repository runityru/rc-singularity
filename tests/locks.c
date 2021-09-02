/*
 * Copyright (C) �Hostcomm� LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "../index.h"
#include "../cpages.h"
#include "../config.h"
#include "../utils.h"
#include "../rc_singularity.h"

#define TEST_SIZE 100000

int stopTest = 0;

static void *fill_thread(void *param)
	{
	char *errbuf = param;
	unsigned i;
	void *rv = (void *)1;
	int res;

	FSingSet *index = sing_link_set("locks_test",0,NULL);
	if (!index)
		{
		sprintf(errbuf,"failed to link set lock_test"); 
		return (void *)1;
		}

	for(i = 0; i < TEST_SIZE * 10; i++)
		{
		if ((res = sing_set_key32u(index,"testkey",i)) && res != RESULT_KEY_PRESENT)
			{ sprintf(errbuf,"key testkey set failed, result %d",res); goto fill_thread_exit; }
		}
	rv = (void *)0; 
fill_thread_exit:
	stopTest = 1;
	sing_unlink_set(index);
	return rv;
	}

int locks_test_reader(void)
	{
	int rv = 0,reserved = 0;
	void *t1res;
	char fill_error[512];
	FSingSet *index = NULL;
	FSingConfig *config = sing_config_get_default();
	if (!config)
		return 1;
	pthread_t req_t;
	if (!(index = sing_create_set("locks_test",NULL,17003,0,LM_NONE,config)))
		{
		printf("Test %s failed: %s\n","locks_test_reader",sing_config_get_error(config));
		sing_delete_config(config);
		return 1;
		}
	__atomic_store_n(&stopTest,0,__ATOMIC_SEQ_CST);
	sing_set_key32u(index,"testkey",0);
	cp_full_flush(index);
	pthread_create(&req_t,NULL,fill_thread,fill_error);
	unsigned ptestkey = 0,testkey;
	int res;
	while(!__atomic_load_n(&stopTest,__ATOMIC_ACQUIRE))
		{
		if((res = sing_get_value32u(index,"testkey",&testkey)))
			{
			printf("Test %s failed: key testkey get failed, result %d, last known value %d\n","locks_test_reader",res,ptestkey);
			rv = 1;
			break;
			}
		ptestkey = testkey;
		}
	pthread_join(req_t,&t1res);
	if ((int64_t)t1res)
		printf ("Test %s failed: %s\n","locks_test_reader",fill_error), rv = 1;
	if (idx_check_all(index,reserved))
		printf("Test %s failed: %s\n","locks_test_reader",sing_get_error(index)), rv = 1; 
	sing_delete_set(index);
	sing_delete_config(config);
	return rv;
	}

static char *req_data = "abcdefghijklnmopqrstuvwxyz";
const static char *proc_data = "ABCDEFGHIJKLNMOPQRSTUVWXYZ";
const static struct timespec idle_wait = {0,500000};
unsigned volatile use_manual_locks = 1;

static void *request_thread(void *param)
	{
	char *errbuf = param;
	FSingSet *index = sing_link_set("locks_test",0,NULL);
	if (!index)
		{
		sprintf(errbuf,"failed to link set lock_test"); 
		return (void *)1;
		}
	char keyname[12];
	char proc_result[30];
	unsigned i,lastproc = 0,nproc,vsize;

	int res;

	for (i = 1; i <= 10; i++)
		{
		sprintf(keyname,"k%d",i);
		if ((res = sing_set_key(index,keyname,req_data,(i % 26) + 1)))
			{ 
			sprintf(errbuf,"key %s set failed, result %d",keyname,res); 
			goto request_thread_exit; 
			}
		}
	if (use_manual_locks && (res = sing_lock_W(index)))
		{ 
		sprintf(errbuf,"Manual lock failed, result %d",res); 
		goto request_thread_exit; 
		}
	for (i = 11; i <= TEST_SIZE; i++)
		{
		sprintf(keyname,"k%d",i);
		if ((res = sing_set_key(index,keyname,req_data,(i % 26) + 1)))
			{ 
			sprintf(errbuf,"key %s set failed, result %d",keyname,res); 
			goto request_thread_exit_unlock; 
			}
		}
	if (use_manual_locks && (res = sing_unlock_commit(index,NULL)))
		{ 
		sprintf(errbuf,"Manual unlock failed, result %d",res); 
		goto request_thread_exit; 
		}
	if ((res = sing_set_key32u(index,"lastreq",i - 1)) != RESULT_KEY_PRESENT)
		{ sprintf(errbuf,"key lastreq set failed, result %d",res); goto request_thread_exit; }
	do 
		{
		while(!(res = sing_get_value32u(index,"lastproc",&nproc)) && nproc == lastproc && !__atomic_load_n(&stopTest,__ATOMIC_ACQUIRE))
			nanosleep(&idle_wait,NULL);
		if (stopTest)
			goto request_thread_exit;
		if (res)
			{ sprintf(errbuf,"key lastproc get failed, result %d",res); goto request_thread_exit; }
		if (use_manual_locks && (res = sing_lock_W(index)))
			{ 
			sprintf(errbuf,"Manual lock failed, result %d",res); 
			goto request_thread_exit; 
			}
		for (i = lastproc + 1; i <= nproc; i++)
			{
			sprintf(keyname,"k%d",i);
			vsize = 30;
			if ((res = sing_get_value(index,keyname,proc_result,&vsize)))
				{ sprintf(errbuf,"key %s get failed, result %d",keyname,res); goto request_thread_exit_unlock; }
			if (vsize != (i % 26) + 1)
				{ sprintf(errbuf,"processed key %s length do not match",keyname); goto request_thread_exit_unlock; }
			if (strncmp(proc_result,proc_data,vsize))
				{ sprintf(errbuf,"processed key %s do not match",keyname); goto request_thread_exit_unlock; }
			if (sing_del_key(index,keyname))
				{ sprintf(errbuf,"key %s deletion failed",keyname); goto request_thread_exit_unlock; }
			}
		if (use_manual_locks && (res = sing_unlock_commit(index,NULL)))
			{ 
			sprintf(errbuf,"Manual unlock failed, result %d",res); 
			goto request_thread_exit; 
			}
		lastproc = nproc;
		}
	while(lastproc != TEST_SIZE);
	stopTest = 1;
	sing_unlink_set(index);
	return (void *)0;

request_thread_exit_unlock:
	if (use_manual_locks) 
		sing_unlock_revert(index);
request_thread_exit:
	stopTest = 1;
	sing_unlink_set(index);
	return (void *)1;
	}

static void *process_thread(void *param)
	{
	char *errbuf = param;
	FSingSet *index = sing_link_set("locks_test",0,NULL);
	if (!index)
		{
		sprintf(errbuf,"failed to link set lock_test"); 
		return (void *)1;
		}
	unsigned i,j,lastreq = 0,nreq,vsize;
	char keyname[12];
	char data[30];
	int res;
	do
		{
		while(!(res = sing_get_value32u(index,"lastreq",&nreq)) && nreq == lastreq && !__atomic_load_n(&stopTest,__ATOMIC_ACQUIRE))
			nanosleep(&idle_wait,NULL);
		if (nreq > lastreq + 100)
			nreq = lastreq + 100;
		if (use_manual_locks && (res = sing_lock_W(index)))
			{ sprintf(errbuf,"Manual lock failed, result %d",res); goto process_thread_exit; }
		for (i = lastreq + 1; i <= nreq; i++)
			{
			sprintf(keyname,"k%d",i);
			vsize = 30;
			if ((res = sing_get_value(index,keyname,data,&vsize)))
				{ sprintf(errbuf,"key %s get failed, result %d",keyname,res); goto process_thread_exit_unlock; }
			if (vsize != (i % 26) + 1)
				{ sprintf(errbuf,"requested key %s length do not match",keyname); goto process_thread_exit_unlock; }
			if (strncmp(data,req_data,vsize))
				{ sprintf(errbuf,"requested key %s do not match",keyname); goto process_thread_exit_unlock; }
			for(j = 0; j < vsize; j++)
				data[j] = data[j] - ('a' - 'A');
			if ((res = sing_set_key(index,keyname,data,vsize)) != RESULT_KEY_PRESENT)
				{ sprintf(errbuf,"key %s set failed, result %d",keyname,res); goto process_thread_exit_unlock; }
			if ((res = sing_set_key32u(index,"lastproc",i)) != RESULT_KEY_PRESENT)
				{ sprintf(errbuf,"key lastproc set failed, result %d",res); goto process_thread_exit_unlock; }
			}
		if (use_manual_locks && (res = sing_unlock_commit(index,NULL)))
			{ sprintf(errbuf,"Manual unlock failed, result %d",res); goto process_thread_exit; }
		lastreq = nreq;
		}
	while (!__atomic_load_n(&stopTest,__ATOMIC_ACQUIRE));
	stopTest = 1;
	sing_unlink_set(index);
	return (void *)0;
process_thread_exit_unlock:
	if (use_manual_locks) 
		sing_unlock_revert(index);
process_thread_exit:
	stopTest = 1;
	sing_unlink_set(index);
	return (void *)1;
	}

int locks_test_lm_simple(void)
	{
	int rv = 0,reserved = 0;
	void *t1res,*t2res;
	char req_error[512],proc_error[512];
	FSingSet *index = NULL;
	FSingConfig *config = sing_config_get_default();
	if (!config)
		return 1;
	pthread_t req_t, prc_t;
	if (!(index = sing_create_set("locks_test",NULL,17003,0,LM_SIMPLE,config)))
		{
		printf("Test %s failed: %s\n","locks_test_lm_simpe",sing_config_get_error(config));
		sing_delete_config(config);
		return 1;
		}
	__atomic_store_n(&stopTest,0,__ATOMIC_SEQ_CST);
	sing_set_key32u(index,"lastreq",0);
	sing_set_key32u(index,"lastproc",0);
	cp_full_flush(index);
	pthread_create(&req_t,NULL,request_thread,req_error);
	pthread_create(&prc_t,NULL,process_thread,proc_error);
	pthread_join(req_t,&t1res);
	pthread_join(prc_t,&t2res);
	if ((int64_t)t1res)
		printf ("Test %s failed: %s\n","locks_test_lm_simple",req_error), rv = 1;
	if ((int64_t)t2res) 
		printf ("Test %s failed: %s\n","locks_test_lm_simple",proc_error), rv = 1;
	if (idx_check_all(index,reserved))
		printf("Test %s failed: %s\n","locks_test_lm_simple",sing_get_error(index)), rv = 1; 
	sing_delete_set(index);
	sing_delete_config(config);
	return rv;
	}

int locks_test_lm_fast(void)
	{
	int rv = 0,reserved = 0;
	void *t1res,*t2res;
	char req_error[512],proc_error[512];
	FSingSet *index = NULL;
	FSingConfig *config = sing_config_get_default();
	if (!config)
		return 1;
	pthread_t req_t, prc_t;
	if (!(index = sing_create_set("locks_test",NULL,17003,0,LM_FAST,config)))
		{
		printf("Test %s failed: %s\n","locks_test_lm_fast",sing_config_get_error(config));
		sing_delete_config(config);
		return 1;
		}
	__atomic_store_n(&stopTest,0,__ATOMIC_SEQ_CST);
	sing_set_key32u(index,"lastreq",0);
	sing_set_key32u(index,"lastproc",0);
	cp_full_flush(index);
	pthread_create(&req_t,NULL,request_thread,req_error);
	pthread_create(&prc_t,NULL,process_thread,proc_error);
	pthread_join(req_t,&t1res);
	pthread_join(prc_t,&t2res);
	if ((int64_t)t1res)
		printf ("Test %s failed: %s\n","locks_test_lm_fast",req_error), rv = 1;
	if ((int64_t)t2res) 
		printf ("Test %s failed: %s\n","locks_test_lm_fast",proc_error), rv = 1;
	if (idx_check_all(index,reserved))
		printf("Test %s failed: %s\n","locks_test_lm_fast",sing_get_error(index)), rv = 1; 
	sing_delete_set(index);
	sing_delete_config(config);
	return rv;
	}

int main(void)
	{
	int rv = 0;
	if (locks_test_reader())
		rv = 1;
	if (locks_test_lm_simple())
		rv = 1;
	if (locks_test_lm_fast())
		rv = 1;
	use_manual_locks = 0;
	if (locks_test_lm_fast())
		rv = 1;
	return rv;
	}