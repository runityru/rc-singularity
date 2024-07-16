/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "handler_config.h"

void help_message(void)
	{
	char *help_message = 
"\n"
"Usage: sing_handler [sharename] [options]\n"
"\n"
"  If sharename is not specified, '-r filename' option should be present. In this case\n" 
"  '-u' option, 'c' and 'n' flag of -r option will be in work even if not specified\n"
"\n"
"Options can be one of the following:\n"
"\n"
"  -A[e|n|f|d|x] key [value] - add key to share. In case of 'n' flag, value is treated as 4 byte integer,\n"
"                           if 'f' is used - as 4 byte float, if 'd' - as double, in case of 'x' - as ,\n"
"                           hexadecimal set of bytes, and as string otherhwise. 'e' flag mean there is no value.\n"
"  -E key                 - erase key from share (no phantom make)\n"
"  -S key                 - delete key from share\n"
"  -M[n|f|d|x] key        - read key from share. Print string 'key: value' to STDOUT.\n"
"  -q                     - print count of keys in share to STDOUT.\n"
"  -d[n[f]]	csv_file_desc - compare share content with csv file on key based basis and print result\n"
"                           to STDOUT in form of lines with '+' or '-' as first character. Share content\n" 
"                           will be replaced by content of filename if 'n' is used. With 'f' flag\n"
"                           if -r or -f option are used, file from theese options will be replaced\n"
"                           with file from -dnf option after finish. Programs have read access to share\n"
"                           during operation, but beware that share will first grow to union of current\n"
"                           and filename key sets and then shrink to filename key set.\n"
"                           Using '-g' option deny reading access to share during operation if this behaviour\n"
"                           is unacceptable.\n"
"  -m                       - print whole share content to STDOUT. Programs can have read and write\n"
"                           access to share during operation, so data consistence is not guaranteed.\n"
"  -p csv_file_desc	       - add file content (format of diff output from -d option can be used too) to share.\n"
"  -s csv_file_desc         - substract file content (format of diff output from -d option can be used too, \n"
"                           operations will be inversed in this case) from share.\n\n"
"  -e csv_file_desc         - erase keys from file (format of diff output from -d option can be used too, \n"
"                           but operation sign is ignored and all keys will be deleted) from share. This option\n"
"                           really delete keys, no make phantoms\n\n"
"accompanied with any of the following (-r,-f,-u can be used alone too):\n\n"
"  -u                     - unload share from memory after finish.\n"
"  -r[c][n][d] [csv_file_desc] - (re)create share from csv filename. Use 'c' to add some extra info\n"
"                           for faster bulk processing (options -d -m). If csv_file_desc is not specified\n"
"                           share will be cleared. Use 'n' to disable persistence for this share,\n"
"                           but beware that share with no persistence can not be recovered after\n"
"                           software failure during writing. Programs work with previous content\n"
"                           of share during share recreation.\n"
"                           Use 'd' option to allow phantom deleted keys. This is useful for merging diff\n"
"                           files without known initial state. But such shares can't be used with -d option.\n"
"  -f[c][n][d] csv_file_desc - same as -r (mutually exclusive) but share created only if not present. File\n"
"                           filename should be present except in case when -dnf key is used.\n"
"  -n number              - additional option for -r and -f for specifying approximated number of keys.\n"
"  -C codec               - additional option for -r and -f for cpecifying key hashing algoritm\n"
"  -j [tab|space|comma|   - additional option for -r and -f. This symbol will be used as a column separator\n"
"      semicolon|'char']    for value storing. Default is tab.\n"
"  -o filename            - print output to filename instead of STDOUT (little bit faster).\n"
"  -b dbdir               - use directory dbdir as a base path for disk persistent files\n" 
"                           instead one defined in /etc/shared_lists.cnf\n"
"  -i                     - utilize second CPU core for parsing if possible.\n"
"  -w                     - print warnings about parser errors to STDERR\n"
"  -y                     - print share size in memory in bytes and in percents if -r is used\n"
"  -z                     - print duration time when work is done (printed to STDOUT even with -o)\n"
"\n"
"csv_file_desc is description of CSV-like format in form:\n\n"
"  filename [csv_options]\n\n"
"where csv_options can be any combination of:\n\n"
"  -t [tab|space|...]     - recognize this symbol as column separator in file. Default is tab.\n"
"  -k column_num          - number of the key column in csv-like source files\n"
"  -v c_num1[,c_num2...]  - comma joined numbers of value columns for csv-like source files. Columns\n"
"                           will be concatenated with share separator symbol (-j key) in specified order\n"
"  -v rest                - all columns except the key one will be used as value\n"
"\n"
"Share can be checked on errors with key:\n"
"  --check\n"
"Share can be deleted from memory and disk files with key:\n"
"  --destroy\n"
"To look this message use\n"
"  -h                     - print this message and exit\n"
"\n"
"Examples:\n"
"  ./sing_handler -r file1 -d file2 -o resultfile\n"
"    - compare files file1 and file2 and print the difference to resultfile. No side effects but there\n"
"      should be enought memory for load union of file1 and file2 key sets. Some kind of diff utility.\n"
"\n"
"  ./sing_handler sharename -fc file1 -dnf file2 -o resultfile -u\n"
"    - compare file file2 with share sharename and print the difference to resultfile. Share will be\n"
"      created from file1 if not present or empty if file1 is not present too. file1 will be replaced\n"
"      with file2 after operation. Share rests on disk but removed from memory. Usefull for creating\n"
"      diffs when file2 content was changed\n"
"  ./sing_handler sharename -r sourcefile -k 1 -v 0,2 -t space\n"  
"    - create sharename from csv like sourcefile where space used as column separator, keys are in\n"
"      column 1 and values are space separated columns 0 and 2. This share rests in memory and on disk\n"
"      and should not be used for bulk operations in future, just for adding or deleting single keys.\n"
"      It is fast share initialization with big data set for using by some programs.\n"
"  ./sing_handler sharename file1\n"
"    - update sharename with diff data from file1. Nothing is locked, so it is just set of atomic\n"
"      deletion, insertion and modification.\n";
	puts(help_message);
	}

static char _get_delimiter(char *desc)
	{
	if (!strcmp("tab",desc)) return '\t';
	if (!strcmp("space",desc)) return ' ';
	if (!strcmp("semicolon",desc)) return ';';
	if (!strcmp("comma",desc)) return ',';
	return desc[0];
	}

static EOperation KEY_OPERATION[58] = {
	SO_SetKey,SO_None,SO_None,SO_None,SO_EraseKey,SO_None,SO_None,SO_None, // A-H
	SO_None,SO_None,SO_None,SO_None,SO_PrintKey,SO_None,SO_None,SO_None, // I-P
	SO_None,SO_None,SO_DelKey,SO_None,SO_None,SO_None,SO_None,SO_None, // Q-X
	SO_None,SO_None, // Y-Z
	SO_None,SO_None,SO_None,SO_None,SO_None,SO_None, // some chars
	SO_None,SO_None,SO_None,SO_Diff,SO_None,SO_None,SO_None,SO_None, // a-h
	SO_None,SO_None,SO_None,SO_None,SO_Dump,SO_None,SO_None,SO_Process, // i-p
	SO_Size,SO_None,SO_Sub,SO_None,SO_None,SO_None,SO_None,SO_None, // q-x
	SO_None,SO_None, // y-z
};
	
FHandlerConfig *get_config(int argc, char *argv[],char *errbuf)
	{
	int arg_num,opt_num;
	FHandlerConfig *rv = (FHandlerConfig *)calloc(1,sizeof(FHandlerConfig)); 
	if (!rv)
		return NULL;
	rv->reset_data.delimiter = '\t';

	if (!(rv->base_config = sing_config_get_default()))
		goto get_config_error;
	FSingCSVFile *c_csv_file = NULL;
	FOperation *opdata = NULL;
	EOperation c_op = 0;
	
	if (argc < 2)
		goto get_config_error;
	
	if (argv[1][0] != '-') // share name present
		rv->indexname = strdup(argv[1]),arg_num = 2;
	else
		arg_num = 1;
	
	while (arg_num < argc)
		{
		if (argv[arg_num][0] != '-')
			goto get_config_error;
		char nsym = argv[arg_num][1];
		if (nsym >= 'A' && nsym <= 'z' && (c_op = KEY_OPERATION[nsym - 'A']))
			{
			if (!rv->ops_cnt % 8)
				rv->operations = (FOperation *)realloc(rv->operations,sizeof(FOperation) * (rv->ops_cnt / 8 + 1) * 8);
			opdata = &rv->operations[rv->ops_cnt++];
			memset(opdata,0,sizeof(FOperation));
			opdata->file_op.delimiter = '\t';
			}
		switch (nsym)
			{
			case 'A': // -A key, set key
				c_csv_file = NULL;
				opdata->operation = SO_SetKey;
				for (opt_num = 2; argv[arg_num][opt_num] != 0; opt_num++)
					{
					switch (argv[arg_num][opt_num])
						{
						case 'n': opdata->key_op.vmode = VM_Int; break;
						case 'f': opdata->key_op.vmode = VM_Float; break;
						case 'd': opdata->key_op.vmode = VM_Double; break;
						case 'x': opdata->key_op.vmode = VM_Hex; break;
						case 'e': opdata->key_op.vmode = VM_Empty; break;
						case 0: opdata->key_op.vmode = VM_String; break;
						default: goto get_config_error;
						}
					}
				if (++arg_num >= argc) goto get_config_error;
				opdata->key_op.key = argv[arg_num++];
				if (opdata->key_op.vmode != VM_Empty)
					{
					if (arg_num >= argc) goto get_config_error;
					opdata->key_op.value = argv[arg_num++];
					}
				continue;
			case 'b': // -b key, base path
				if (++arg_num >= argc) goto get_config_error;
				sing_config_set_base_path(rv->base_config,argv[arg_num++]);
				continue;
			case 'M': // -M key, key output
				c_csv_file = NULL;
				opdata->operation = SO_PrintKey;
				for (opt_num = 2; argv[arg_num][opt_num] != 0; opt_num++)
					{
					switch (argv[arg_num][opt_num])
						{
						case 'n': opdata->key_op.vmode = VM_Int; break;
						case 'f': opdata->key_op.vmode = VM_Float; break;
						case 'd': opdata->key_op.vmode = VM_Double; break;
						case 'x': opdata->key_op.vmode = VM_Hex; break;
						case 0: opdata->key_op.vmode = VM_String; break;
						default: goto get_config_error;
						}
					}
				if (++arg_num >= argc) goto get_config_error;
				opdata->key_op.key = argv[arg_num++];
				continue;
			case 'E': // -E key, key erasing
			case 'S': // -S key, key deletion
				c_csv_file = NULL;
				opdata->operation = c_op;
				if (++arg_num >= argc) goto get_config_error;
				opdata->key_op.key = argv[arg_num++];
				continue;
			case 'd': // -d key, diff creation
				c_csv_file = &opdata->file_op;
				opdata->operation = SO_Diff;
				for (opt_num = 2; argv[arg_num][opt_num] != 0; opt_num++)
					{
					switch (argv[arg_num][opt_num])
						{
						case 'n':
							opdata->flags |= OF_SHAREREPLACE;
							continue;
						case 'f':
							opdata->flags |= OF_FILEREPLACE;
							continue;
						default: goto get_config_error;
						}
					}
				if ((opdata->flags & OF_FILEREPLACE) && !(opdata->flags & OF_SHAREREPLACE))
					goto get_config_error;
				if (++arg_num >= argc || argv[arg_num][0] == '-') goto get_config_error;
				c_csv_file->filename = argv[arg_num++];
				rv->diff_operation = (opdata->flags & OF_FILEREPLACE) ? 2 : 1;
				continue;
			case 'f': // -f key, create from csv file if not exists
				if (rv->flags & (CF_RESET | CF_BACKUP)) goto get_config_error;
				for (opt_num = 2; argv[arg_num][opt_num] != 0; opt_num++)
					{
					switch (argv[arg_num][opt_num])
						{
						case 'c':
							rv->use_flags |= SING_UF_COUNTERS;
							continue;
						case 'n':
							rv->use_flags |= SING_UF_NOT_PERSISTENT;
							continue;
						case 'd':
							rv->use_flags |= SING_UF_PHANTOM_KEYS;
							continue;
						default: goto get_config_error;
						}
					}
				rv->flags |= CF_BACKUP;
				if (++arg_num >= argc) goto get_config_error;
				c_csv_file = &rv->reset_data;
				c_csv_file->filename = argv[arg_num++];
				continue;
			case 'r': // -r key, reset share to file content
				if (rv->flags & (CF_RESET | CF_BACKUP)) goto get_config_error;
				for (opt_num = 2; argv[arg_num][opt_num] != 0; opt_num++)
					{
					switch (argv[arg_num][opt_num])
						{
						case 'c':
							rv->use_flags |= SING_UF_COUNTERS;
							continue;
						case 'n':
							rv->use_flags |= SING_UF_NOT_PERSISTENT;
							continue;
						case 'd':
							rv->use_flags |= SING_UF_PHANTOM_KEYS;
							continue;
						default: goto get_config_error;
						}
					}
				rv->flags |= CF_RESET;
				arg_num++;
				if (arg_num < argc && argv[arg_num][0] != '-')
					{
					c_csv_file = &rv->reset_data;
					c_csv_file->filename = argv[arg_num++];
					}
				continue;
			case 'i':
				rv->conn_flags |= SING_CF_MULTICORE_PARSE;
				arg_num++;
				continue;
			case 'j':
				if (++arg_num >= argc) goto get_config_error;
				sing_config_set_value_delimiter(rv->base_config,_get_delimiter(argv[arg_num++]));
				continue;
			case 'k': // -k key, key column number
				if (++arg_num >= argc) goto get_config_error;
				if (!c_csv_file) goto get_config_error;
				c_csv_file->key_col_num = atoi(argv[arg_num++]) | (c_csv_file->key_col_num & REMOVE_KEY_FROM_VALS);
				continue;
			case 'm': // -m key, dump whole share
			case 'q': // -q key, size output
				c_csv_file = NULL;
				opdata->operation = c_op;
				arg_num ++;
				continue;
			case 'n': // -n key, number of keys
				if (++arg_num >= argc || argv[arg_num][0] == '-') goto get_config_error;
				rv->hashtable_size = atoi(argv[arg_num++]);
				continue;
			case 'C': // -C key, key codec for created set
				if (++arg_num >= argc || argv[arg_num][0] == '-') goto get_config_error;
				rv->codec = strdup(argv[arg_num++]);
				continue;
			case 'o': // -o key, output file
				if (++arg_num >= argc || argv[arg_num][0] == '-') goto get_config_error;
				if (!opdata) goto get_config_error;
				opdata->result_file = argv[arg_num++];
				continue;
			case 'e': // -e key, keyset removing
			case 'p': // -p key, diff applying
			case 's': // -s key, diff applying
				c_csv_file = &opdata->file_op;
				opdata->operation = c_op;
				if (++arg_num >= argc || argv[arg_num][0] == '-') goto get_config_error;
				c_csv_file->filename = argv[arg_num++];
				continue;
			case 't':
				if (++arg_num >= argc) goto get_config_error;
				if (!c_csv_file) goto get_config_error;
				c_csv_file->delimiter = _get_delimiter(argv[arg_num++]);
				continue;
			case 'u': // -u key, unload share from memory after finish
				rv->flags |= CF_UNLOAD;
				arg_num ++;
				continue;
			case 'v': // -v key, value column numbers
				{
				int vpos = 0,numsize = 0,num;
				char numbuf[3];

				if (++arg_num >= argc) goto get_config_error;
				if (!c_csv_file) goto get_config_error;

				if (!strcmp("rest",argv[arg_num]))
					{
					c_csv_file->key_col_num |= REMOVE_KEY_FROM_VALS;
					c_csv_file->val_col_mask = 0xFFFFFFFF;
					arg_num++;
					continue;
					}					
				while (argv[arg_num][vpos])
					{
					if (argv[arg_num][vpos] == ',' && numsize)
						{
						numbuf[numsize] = 0;
						if ((num = atoi(numbuf)) > 63) goto get_config_error;
						c_csv_file->val_col_mask |= 1LL << num;
						numsize = 0;
						}
					else if (numsize >= 2)
						goto get_config_error;
					else
						{
						numbuf[numsize] = argv[arg_num][vpos];
						numsize++;
						}
					vpos++;
					}
				if (numsize)
					{
					numbuf[numsize] = 0;
					if ((num = atoi(numbuf)) > 31) goto get_config_error;
					c_csv_file->val_col_mask |= 1LL << num;
					}
				arg_num++;
				continue;
				}
			case 'w': // -y key, print memory usage
				rv->conn_flags |= SING_CF_PARSE_ERRORS;
				arg_num ++;
				continue;
			case 'y': // -y key, print memory usage
				rv->flags |= CF_MEMSIZE;
				arg_num ++;
				continue;
			case 'z': // -z key, print duration after finish
				rv->flags |= CF_DURATION;
				arg_num ++;
				continue;
			case '-':
				if (!strcmp(&argv[arg_num][2],"destroy"))
					rv->flags |= CF_DESTROY;
				else if (!strcmp(&argv[arg_num][2],"check"))
					rv->flags |= CF_CHECK;
				else if (!strcmp(&argv[arg_num][2],"check"))
					rv->flags |= CF_CHECK;
				else
					 goto get_config_error;
				arg_num ++;
				continue;
			}
		goto get_config_error;
		}
		
	if (!rv->indexname && !(rv->flags & CF_RESET))
		goto get_config_error;

	if (rv->diff_operation && (rv->use_flags & SING_UF_PHANTOM_KEYS))
		goto get_config_error;
	
	if (rv->diff_operation == 2)
		{ 
		if (!(rv->flags & (CF_BACKUP | CF_RESET))) 
			goto get_config_error;
		}

	if (rv->reset_data.key_col_num & REMOVE_KEY_FROM_VALS)
		{
		rv->reset_data.key_col_num -= REMOVE_KEY_FROM_VALS;
		rv->reset_data.val_col_mask &= ~(1LL << rv->reset_data.key_col_num);
		}

	unsigned i;
	for (i = 0 ; i < rv->ops_cnt; i++)
		if ((rv->operations[i].operation == SO_Diff || rv->operations[i].operation == SO_Process || rv->operations[i].operation == SO_Sub)
				&& (rv->operations[i].file_op.key_col_num & REMOVE_KEY_FROM_VALS))
			{
			rv->operations[i].file_op.key_col_num -= REMOVE_KEY_FROM_VALS;
			rv->operations[i].file_op.val_col_mask &= ~(1LL << rv->operations[i].file_op.key_col_num);
			}
	return rv;	
	
get_config_error:
	clear_config(rv);
	help_message(); 
	return NULL;
	}

void clear_config(FHandlerConfig *cfg)
	{
	if (cfg->base_config)
		sing_delete_config(cfg->base_config);
	if (cfg->indexname)
		free(cfg->indexname);
	if (cfg->codec)
		free(cfg->codec);
	free(cfg);
	}