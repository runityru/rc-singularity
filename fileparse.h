/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#ifndef _FILEPARSE_H
#define _FILEPARSE_H

typedef struct FSingConfigTg FSingConfig;
typedef struct FSingSetTg FSingSet;
typedef struct FTransformDataTg FTransformData;
typedef struct FReadBufferSetTg FReadBufferSet;

typedef int (*processParsedItem)(FSingSet *index,FTransformData *tdata,void *param) __attribute__((regparm(3)));
typedef void (*parsedError)(char *buf);

int fp_countKeys(FReadBufferSet *sourceRbs,off_t file_size);

typedef int (*fileParseFunc)(FSingSet *,const FSingCSVFile *,FReadBufferSet *,processParsedItem,parsedError,unsigned,void *);

int fp_parseFile(FSingSet *index,const FSingCSVFile *csv_format,FReadBufferSet *sourceRbs,processParsedItem pcb,parsedError ecb,unsigned invert,void *cb_param);
int fp_parseFile2(FSingSet *index,const FSingCSVFile *csv_format,FReadBufferSet *sourceRbs,processParsedItem pcb,parsedError ecb,unsigned invert,void *cb_param);

// Standart handler for errors in keys - output to stderr
void std_parse_error(char *buf);

#endif