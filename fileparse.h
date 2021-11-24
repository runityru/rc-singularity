/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#ifndef _FILEPARSE_H
#define _FILEPARSE_H

typedef struct FSingSetTg FSingSet;
typedef struct FTransformDataTg FTransformData;
typedef struct FReadBufferSetTg FReadBufferSet;

typedef struct FFileParseParamTg 
	{
	const FSingCSVFile *csv_format;
	FReadBufferSet *rbs;
	} FFileParseParam;

int fp_init(void *source);
int fp_get_next(FSingSet *index,void *source,FTransformData *tdata,int invert_operation);
int fp_countKeys(FReadBufferSet *sourceRbs,off_t file_size);


#endif