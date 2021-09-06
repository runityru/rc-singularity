/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#ifndef _TESTS_COMMON_H
#define _TESTS_COMMON_H

#include "../defines.h"

typedef struct FSingSetTg FSingSet;

int collision_search(unsigned hashtable_size,unsigned needed_hash,unsigned needed_cnt,char **words);

typedef element_type (*prepfunc)(FSingSet *index,int *res_mem);
typedef int (*testfunc)(FSingSet *index,int *res_mem,element_type prep_data);

typedef struct FTestDataTg {
	char *name;
	prepfunc prep;
	testfunc test;
	unsigned lock_mode;
	unsigned flags;
	} FTestData;

int run_test(FTestData *test_data);

typedef struct FTransformDataTg FTransformData;

void test_make_tdata(FSingSet *index,char *key_source,int vsize,unsigned char *value,FTransformData *tdata);
void test_process_res(FSingSet *index,int res,FTransformData *tdata);
int test_add_key(FSingSet *index,char *key_source,int vsize,unsigned char *value);
int test_del_key(FSingSet *index,char *key_source);

#endif