/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#ifndef _PIPELINES_H
#define _PIPELINES_H

#include "index.h"

typedef int (*CProcessParsedItem)(FSingSet *index,FTransformData *tdata,void *param);
typedef int (*CInitSource)(void *param);
typedef int (*CGetFromSource)(FSingSet *index,void *param,FTransformData *tdata,int invert_operation);

int pl_pipeline1(FSingSet *index,CInitSource init_cb,CGetFromSource get_cb, void *data_source,CProcessParsedItem pcb,unsigned invert,void *cb_param);
int pl_pipeline2(FSingSet *index,CInitSource init_cb,CGetFromSource get_cb, void *data_source,CProcessParsedItem pcb,unsigned invert,void *cb_param);

#define PL_COUNT_UNKNOWN 0xFFFFFFFF

static inline int pl_pipeline(FSingSet *index,CInitSource init_cb,CGetFromSource get_cb, void *data_source,CProcessParsedItem pcb,unsigned invert,void *cb_param,unsigned count)
	{ 
	return (count > 100 && (index->conn_flags & CF_MULTICORE_PARSE)) ? 
																	pl_pipeline2(index,init_cb,get_cb,data_source,pcb,invert,cb_param) :
																	pl_pipeline1(index,init_cb,get_cb,data_source,pcb,invert,cb_param);
	}

#endif