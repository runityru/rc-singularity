/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#ifndef _CONFIG_H
#define _CONFIG_H

#include <stdint.h>

#ifndef _SING_CSV_FILE
#define _SING_CSV_FILE
typedef struct FSingCSVFileTg
	{
	uint64_t val_col_mask;		// -v key, value columns mask
	char *filename;				// name of file for operation
	uint32_t key_col_num;		// -k key, key column number
	char delimiter; 				// -t key, csv delimiter
	} FSingCSVFile;
#endif

#define CF_MULTICORE_PARSE 0x100
#define CF_PARSE_ERRORS 0x200
#define CF_FULL_LOAD 0x400
#define CF_READER 0x800
#define CF_STAY_LOCKED 0x1000

#define CF_MASK 0xFF00

typedef struct FSingConfigTg
	{
	char column_delimiter;		// in-share value column delimiter
	char padding[3];
	unsigned connect_flags;
	char *base_location; 		// base persistent files location
	char last_error[512];
	} FSingConfig;

void cnf_set_error(FSingConfig *config,const char *message);
void cnf_set_formatted_error(FSingConfig *config,const char *format,...);

FSingConfig *sing_config_get_default(void);
FSingConfig *sing_config_get_empty(void);
const char *sing_config_get_error(FSingConfig *config);
void sing_config_set_connection_flags(FSingConfig *config,unsigned flags);
void sing_config_set_value_delimiter(FSingConfig *config,char delimiter);
void sing_config_set_base_path(FSingConfig *config,const char *base_path);
void sing_delete_config(FSingConfig *config);

#endif

