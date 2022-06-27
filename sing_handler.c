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
			case SO_SetKey:
			case SO_DelKey:
			case SO_PrintKey:
				break;
			case SO_Size:
				printf ("%d\n",sing_total_count(shmIndex));
				break;
			case SO_Diff:
				res = (opdata->flags & OF_SHAREREPLACE) ? 
							sing_diff_replace_file(shmIndex,&opdata->file_op,opdata->result_file) : sing_diff_file(shmIndex,&opdata->file_op,opdata->result_file);
				if (opdata->flags & OF_FILEREPLACE)
					source_replacement = opdata->file_op.filename;
				break;
			case SO_Dump: res = sing_dump(shmIndex,opdata->result_file,opdata->flags); break;
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
		fprintf (stderr,"%s\n",sing_config_get_error(config->base_config)); 
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