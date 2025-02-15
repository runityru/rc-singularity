/*
 * Copyright (C) �Hostcomm� LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#ifndef _TESTS_COMMON_H
#define _TESTS_COMMON_H

#include "../defines.h"

typedef struct FSingSetTg FSingSet;

int collision_search(unsigned hashtable_size,unsigned needed_hash,unsigned needed_cnt,char **words);

#define TEST_MULTIKEYS_CNT 8
extern const char *multi_keys[TEST_MULTIKEYS_CNT];
extern const char *multi_keys_long[TEST_MULTIKEYS_CNT];
extern char *multi_values[TEST_MULTIKEYS_CNT];
extern const char * const single_key;
extern const char * const single_value;
extern const char * const single_phantom;

element_type one_key_value_prep(FSingSet *index,int *res_mem);
element_type one_key_phantom_prep(FSingSet *index,int *res_mem);
element_type one_key_both_prep(FSingSet *index,int *res_mem);

element_type many_key_value_prep(FSingSet *index,int *res_mem);

typedef element_type (*prepfunc)(FSingSet *index,int *res_mem);
typedef int (*testfunc)(FSingSet *index,int *res_mem,element_type prep_data);

typedef struct FTestDataTg {
	char *name;
	prepfunc prep;
	testfunc test;
	unsigned lock_mode;
	unsigned flags;
	} FTestData;

void test_delete_set(char *index_name);

int run_test(FTestData *test_data);

#endif