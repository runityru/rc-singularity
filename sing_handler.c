/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <stddef.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <aio.h>
#include <unistd.h>
#include <signal.h>

#include "handler_config.h"
#include "rc_singularity.h"

static double getdiff(struct timespec *t1, struct timespec *t2)
	{
	int ds = t2->tv_sec - t1->tv_sec;
	return ds + (double)t2->tv_nsec / 1000000000.0 - (double)t1->tv_nsec / 1000000000.0;
	}

static off_t file_size(char *filename)
	{
	struct stat st;
	if (!filename || !filename[0] || stat(filename, &st) != 0 || !S_ISREG(st.st_mode))
		return -1;
	return st.st_size;
	}

static void *_cb_allocator(unsigned size)
	{ return malloc(size); }

static int key_set(FSingSet *kvset,FKeyOperation *key_op)
	{
	switch (key_op->vmode)
		{
		case VM_Int: return sing_set_key32i(kvset,key_op->key,atoi(key_op->value));
		case VM_Float: return sing_set_key32f(kvset,key_op->key,strtof(key_op->value,NULL));
		case VM_Double: return sing_set_key64d(kvset,key_op->key,atof(key_op->value));
		case VM_Empty: return sing_set_key(kvset,key_op->key,NULL,0);
		case VM_Hex:
			{
			char *value = key_op->value;
			if (value[0] == '0' && (value[1] & 0xDF) == 'X')
				value += 2;
			int len = strlen(value);
			unsigned char *result = (unsigned char *)malloc((len + 1) / 2);
			if (!result)
				return SING_ERROR_NO_MEMORY;
			unsigned pos = 0;
			if (len & 1)
				{
				len--;
				sscanf(value, "%1hhx", result);
				value++;
				pos = 1;
				}
			for(;len;len -= 2, pos++, value += 2)
				sscanf(value, "%2hhx", &result[pos]);
			int res = sing_set_key(kvset,key_op->key,result,pos);
			free(result);
			return res;
			}
		default:	return sing_set_key(kvset,key_op->key,key_op->value,strlen(key_op->value));
		}
	}

static int key_output(FSingSet *kvset,FKeyOperation *key_op)
	{
	int res;
	switch (key_op->vmode)
		{
		case VM_Int:
			{
			int val;
			if (!(res = sing_get_value32i(kvset,key_op->key,&val)))
				printf("%s: %d\n",key_op->key,val);
			return res;
			}
		case VM_Float:
			{
			float val;
			if (!(res = sing_get_value32f(kvset,key_op->key,&val)))
				printf("%s: %f\n",key_op->key,val);
			return res;
			}
		case VM_Double:
			{
			double val;
			if (!(res = sing_get_value64d(kvset,key_op->key,&val)))
				printf("%s: %f\n",key_op->key,val);
			return res;
			}
		case VM_Hex:
			{
			unsigned char *value = NULL;
			unsigned vsize,i;
			if (!(res = sing_get_value_cb(kvset,key_op->key,_cb_allocator,(void **)&value,&vsize)))
				{
				printf("%s: ",key_op->key);
				for(i = 0; i < vsize; i++)
					printf("%02X", value[i]);
				printf("\n");
				}
			return res;
			}
		default:
			{
			char *value = NULL;
			unsigned vsize;
			if (!(res = sing_get_value_cb(kvset,key_op->key,_cb_allocator,(void **)&value,&vsize)))
				printf("%s: %.*s\n",key_op->key,vsize,value);
			if (value)
				free(value);
			}
		}
	return res;
	}

int main(int argc, char *argv[])
	{
	struct timespec ts,ts2;
	int rv = 1;
	int reset = 0;
	FSingSet *shmIndex = NULL;
	FHandlerConfig *config;
	off_t filesize = 0;

	char errBuf[512] = {0};

	if (!(config = get_config(argc,argv,errBuf)))
		return 1;

	char *source_replacement = NULL;

	if (config->flags & CF_DURATION)
		clock_gettime(CLOCK_REALTIME,&ts);
	
	if ((config->flags & CF_RESET) || (!(shmIndex = sing_link_set(config->indexname,config->conn_flags,config->base_config)) && (config->flags & CF_BACKUP)))
		reset = 1;

	if (reset) 
		{
		char *reset_file = config->reset_data.filename;
		if (reset_file)
			{
			filesize = file_size(reset_file);
			if (filesize == -1)
				{ // Файла нет
				if (!config->ops_cnt || config->operations[0].operation != SO_Diff || !(config->operations[0].flags & OF_FILEREPLACE))
					{ 
					fprintf(stderr,"Source file %s not found\n",reset_file); 
					goto exit;
					}
				// Если первая операция это диф с заменой, то возьмем файл оттуда
				if ((filesize = file_size(config->operations[0].file_op.filename)) == -1)
					{ 
					fprintf(stderr,"Neither source file %s nor compare file %s not found\n",config->reset_data.filename,config->operations[0].file_op.filename); 
					goto exit; 
					}
				config->operations[0].operation = SO_Dump;
				config->reset_data = config->operations[0].file_op;
				config->operations[0].file_op.filename = NULL;
				reset_file = config->reset_data.filename;
				}
			shmIndex = sing_create_set(config->indexname,&config->reset_data,config->hashtable_size,config->conn_flags | config->use_flags,0,config->base_config);
			}
		else
			shmIndex = sing_create_set(config->indexname,NULL,config->hashtable_size,config->conn_flags | config->use_flags,0,config->base_config);
		}

	if (!shmIndex)
		{ fprintf (stderr,"%s\n",sing_config_get_error(config->base_config)); goto exit; }

	unsigned i,lm = sing_get_mode(shmIndex),w_op = 0;
	for (i = 0; i < config->ops_cnt; i++)
		{
		FOperation *opdata = &config->operations[i];
		int res = 0;
		if (opdata->operation > SO_MaxRead)
			{
			w_op = 1;
			if (lm == SING_LM_PROTECTED && sing_lock_W(shmIndex))
				goto exit;
			}
		switch (opdata->operation)
			{
			case SO_Process: res = sing_add_file(shmIndex,&opdata->file_op); break;
			case SO_Sub: res = sing_sub_file(shmIndex,&opdata->file_op); break;
			case SO_Erase: res = sing_remove_file(shmIndex,&opdata->file_op); break;
			case SO_SetKey: res = key_set(shmIndex,&opdata->key_op); break;
			case SO_DelKey: res = sing_del_key(shmIndex,opdata->key_op.key); break;
			case SO_EraseKey: res = sing_del_full(shmIndex,opdata->key_op.key); break;
			case SO_PrintKey: res = key_output(shmIndex,&opdata->key_op); break;
			case SO_Size:
				printf ("%d\n",sing_total_count(shmIndex));
				break;
			case SO_Diff:
				res = (opdata->flags & OF_SHAREREPLACE) ? 
							sing_diff_replace_file(shmIndex,&opdata->file_op,opdata->result_file) : sing_diff_file(shmIndex,&opdata->file_op,opdata->result_file);
				if (opdata->flags & OF_FILEREPLACE)
					source_replacement = opdata->file_op.filename;
				break;
			case SO_Dump: res = sing_dump(shmIndex,opdata->result_file); break;
			case SO_MaxRead:
			case SO_None:
				break;
			}
		if (res & SING_ERROR)
			{
			switch(lm)
				{
				case SING_LM_PROTECTED: if (opdata->operation > SO_MaxRead) sing_unlock_revert(shmIndex); break;
				case SING_LM_FAST: case SING_LM_NONE: if (w_op) sing_revert(shmIndex); break;
				}
			goto exit;
			}
		if (lm == SING_LM_PROTECTED && opdata->operation > SO_MaxRead && !sing_unlock_commit(shmIndex,NULL))
			goto exit;
		}
	
	if ((config->flags & CF_CHECK) && sing_check_set(shmIndex))
		{ 
		fprintf (stderr,"%s\n",sing_get_error(shmIndex)); 
		if (w_op && (lm == SING_LM_FAST || lm == SING_LM_NONE))
			sing_revert(shmIndex);
		goto exit; 
		}

	if (w_op)
		{
		int res = sing_flush(shmIndex,NULL);
		if (res && res != SING_ERROR_IMPOSSIBLE_OPERATION)
			goto exit;
		}
	rv = 0;
exit:
	
	if (!rv && config->flags & CF_DURATION)
		{
		clock_gettime(CLOCK_REALTIME,&ts2);
		printf("Done in %.3f\n",getdiff(&ts,&ts2));
		}
	
	if (!rv && source_replacement)
		rename(source_replacement,config->reset_data.filename);

	if (shmIndex)
		{
		if (!rv && (config->flags & CF_MEMSIZE))
			{
			uint32_t size = sing_get_memsize(shmIndex);
			if ((config->flags & CF_RESET) && filesize > 0)
				printf("Memory usage %d Kb (%.2f of source)\n",size,((double)size) / (double)(filesize / 1024));
			else
				printf("Memory usage %d Kb\n",size);
			}
		if (!rv)
			{
			if (config->flags & CF_DESTROY)
				sing_delete_set(shmIndex),shmIndex = NULL;
			else if (config->flags & CF_UNLOAD)
				sing_unload_set(shmIndex),shmIndex = NULL;
			}
		if (shmIndex)
			sing_unlink_set(shmIndex);
		}

	clear_config(config);

	return rv;
	}