/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
	   
#include "config.h"

FSingConfig *sing_config_get_empty(void)
	{
	FSingConfig *config = (FSingConfig *)calloc(1,sizeof(FSingConfig));
	if (!config) 
		return NULL; 
	config->column_delimiter = '\t';
	return config;
	}

char *strtrunc(char *src)
	{
	char *rv;
	int i = 0, j = strlen(src) - 1;

strt_rep:	
	switch(src[i])
		{
		case 0: return NULL;
		case ' ': case '\t': case '\r': case '\n': i++; goto strt_rep;
		}
	rv = &src[i];
strt_rep2:
	switch(src[j])
		{
		case ' ': case '\t': case '\r': case '\n': j--; goto strt_rep2;
		}	
	src[j+1] = 0;
	return rv;
	}

FSingConfig *sing_config_get_default(void)
	{
	FILE *sfh;
	char linebuf[512];
	char *k,*v;
	
	FSingConfig *config = sing_config_get_empty();
	if (!config) 
		return NULL; 

	if (!(sfh = fopen("/etc/rc-singularity/singularity.cnf","r"))) 
		return config;

	while(fgets(linebuf,512,sfh))
		{
		if (!(v = strchr(linebuf,':'))) continue;
		*v = 0;
		k = strtrunc(linebuf);
		v = strtrunc(v + 1);
		if (!*v)
			continue;
		if (!strcmp(k,"db_dir"))
			{
			int sl = strlen(v);
			config->base_location = (char *)malloc(sl + 2);
			strcpy(config->base_location,v);
			if (config->base_location[sl - 1] != '/')
				config->base_location[sl] = '/',config->base_location[sl+1] = 0;
			}
		if (!strcmp(k,"column_delimiter"))
			{
			if (!strcmp(v,"\\t"))
				config->column_delimiter = '\t';
			else
				config->column_delimiter = v[0];
			}
		}
	fclose(sfh);
	return config;
	}

void cnf_set_error(FSingConfig *config,const char *message)
	{
	strncpy(config->last_error,message,511);
	}
	
void cnf_set_formatted_error(FSingConfig *config,const char *format,...)
	{
	va_list args;
	va_start (args, format);
	vsnprintf (config->last_error,511,format, args);
	va_end (args);
	}

void sing_config_set_connection_flags(FSingConfig *config,unsigned flags)
	{ config->connect_flags = flags; }

void sing_config_set_value_delimiter(FSingConfig *config,char delimiter)
	{ config->column_delimiter = delimiter; }

void sing_config_set_base_path(FSingConfig *config,const char *base_path)
	{
	if (config->base_location) free(config->base_location);
	config->base_location = base_path ? strdup(base_path) : NULL; 
	}

const char *sing_config_get_error(FSingConfig *config)
	{ return config->last_error; }

void sing_delete_config(FSingConfig *config)
	{
	if (config->base_location) free(config->base_location);

	free(config);
	}
	
