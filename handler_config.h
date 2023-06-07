/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#ifndef _HANDLER_CONFIG_H
#define _HANDLER_CONFIG_H

#include <stdint.h>
#include "rc_singularity.h"

// -r key, reset
#define CF_RESET 1

// -f key, restore from file if not exists
#define CF_BACKUP 2

// -u, unload from memory after completion
#define CF_UNLOAD 16

// -z key, print duration
#define CF_DURATION 32

// w option, write lock
#define CF_WRITELOCK 64

// r option, read lock
#define CF_READLOCK 128

// -y flag, print memory size
#define CF_MEMSIZE 1024

// --check
#define CF_CHECK 4096

// --destroy
#define CF_DESTROY 8192

typedef enum {
	SO_None,		   // noop
	SO_Dump,		   // -m key
	SO_PrintKey,	// -M key
	SO_Size,		   // -q key
	SO_MaxRead,		// noop, read/write boundary
	SO_Process,		// -p key, default
	SO_Sub,			// -s key,
	SO_Erase,		// -e key,
	SO_SetKey,		// -A key
	SO_DelKey,		// -S key
	SO_EraseKey,	// -E key
	SO_Diff,		   // -d key
	} EOperation;
	
typedef enum {
	VM_String,		// default
	VM_Int,		   // -n option
	VM_Float,		// -f option
	VM_Hex,		   // -x option
	VM_Double,	   // -d option
	VM_Empty		   // -e option
	} EValueMode;

#define REMOVE_KEY_FROM_VALS 0x80000000

typedef struct FKeyOperationTg
	{
	char *key;						// key for -a, -e and -c keys
	char *value;					// value for -a key
	EValueMode vmode;				// options of -a key
	} FKeyOperation;

// n optiond in -d key
#define OF_SHAREREPLACE 1

// f optiond in -d key
#define OF_FILEREPLACE 2

typedef struct FOperationTg
	{
	EOperation operation;		// operation
	unsigned flags;
	char *result_file;			// file for storing operation output defined in -o key
	union {
		FSingCSVFile file_op;
		FKeyOperation key_op;
		};
	} FOperation;

typedef struct FHandlerConfigTg
	{
	FSingConfig *base_config;
	char *indexname;
	uint32_t cores;				// -i key, cpu cores number

	uint32_t flags;				// used CF_ keys and options
	uint32_t use_flags;			// SING_UF_... flags
	uint32_t conn_flags;			// SING_CF_... flags
	uint32_t hashtable_size;	// size of hash table for -r / -f keys
	FSingCSVFile reset_data;
	uint64_t source_size;		// stored source file size for -r/-f keys for use with -y flag;

	FOperation *operations;
	unsigned ops_cnt;
	int diff_operation;			// diff present in operations
	} FHandlerConfig;

FHandlerConfig *get_config(int argc, char *argv[],char *errbuf);
void clear_config(FHandlerConfig *cfg);

#endif