/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include <immintrin.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "filebuf.h"
#include "codec.h"
#include "index.h"
#include "fileparse.h"

int fp_init(void *source)
	{
	return fbr_first_block(((FFileParseParam *)source)->rbs);
	}

int fp_get_next(FSingSet *index,void *source,FTransformData *tdata,int invert_operation)
	{
	const FSingCSVFile *csv_format = ((FFileParseParam *)source)->csv_format;
	FReadBufferSet *rbs = ((FFileParseParam *)source)->rbs;

	int keylen,has_val,i;
	uint64_t col_mask,key_mask = 1LL << csv_format->key_col_num;
	char share_delimiter = index->head->delimiter;
	char *key_start;
	char csym;

	if (rbs->eof)
		return 0;
_get_next_pair_repeat:
	tdata->value_size = 0;
	tdata->use_phantom = 0;
	has_val = 0;
	for (col_mask = 1LL; col_mask != key_mask; col_mask <<= 1)
		{
		if (col_mask & csv_format->val_col_mask)
			{ // We should store this column
			if (has_val && tdata->value_size < MAX_VALUE_SOURCE)
				tdata->value_source[tdata->value_size++] = share_delimiter;
			has_val = 1;
			while ((csym = fbr_get_sym(rbs)) != csv_format->delimiter)
				{
				if (fbr_inc_pos(rbs)) return 0;
				if (csym == '\r' || csym == '\n')
					goto _get_next_pair_grep_eol;
				if (tdata->value_size < MAX_VALUE_SOURCE)
					tdata->value_source[tdata->value_size++] = csym;
				}
			if (fbr_inc_pos(rbs)) return 0;
			continue;
			}
		// We should skip this column
		while ((csym = fbr_get_sym(rbs)) != csv_format->delimiter)
			{
			if (fbr_inc_pos(rbs)) return 0;
			if (csym == '\r' || csym == '\n')
				goto _get_next_pair_grep_eol;
			}
		if (fbr_inc_pos(rbs)) return 0;
		}

	if (cd_opscan(fbr_get_sym(rbs),tdata,invert_operation))
		{
		if (fbr_inc_pos(rbs)) return 0;
		if (tdata->operation == OP_OLD)
			goto _get_next_pair_to_eol;
		}
	key_start = fbr_get_key_ref(rbs);
	if (!(keylen = index->transform(key_start,MAX_KEY_SOURCE,tdata)))
		goto _get_next_pair_error;
	if(fbr_shift_pos(rbs,keylen)) 
		return 1;

	if (col_mask & csv_format->val_col_mask)
		{
		if (has_val && tdata->value_size < MAX_VALUE_SOURCE)
			tdata->value_source[tdata->value_size++] = share_delimiter;
		has_val = 1;
		int tocpy = (MAX_VALUE_SOURCE - tdata->value_size < keylen) ? MAX_VALUE_SOURCE - tdata->value_size : keylen;
		if (tocpy)
			{
			memcpy(&tdata->value_source[tdata->value_size],key_start,tocpy);
			tdata->value_size += tocpy;
			}
		}
	col_mask <<= 1;

	csym = fbr_get_sym(rbs);
	if (csym == csv_format->delimiter)
		{ 
_get_next_pair_repeat_after_key:
		if (fbr_inc_pos(rbs)) return 1;
		if (col_mask & csv_format->val_col_mask)
			{
			if (has_val && tdata->value_size < MAX_VALUE_SOURCE)
				tdata->value_source[tdata->value_size++] = share_delimiter;
			has_val = 1;
			while ((csym = fbr_get_sym(rbs)) != csv_format->delimiter)
				{
				if (csym == '\r' || csym == '\n')
					{
					do
						fbr_inc_pos(rbs);
					while (fbr_get_sym(rbs) == '\r' || fbr_get_sym(rbs) == '\n');
					return 1;
					}
				if (tdata->value_size < MAX_VALUE_SOURCE)
					tdata->value_source[tdata->value_size++] = csym;
				if (fbr_inc_pos(rbs)) return 1;
				}
			goto _get_next_pair_repeat_after_key;
			}
		while ((csym = fbr_get_sym(rbs)) != csv_format->delimiter)
			{
			if (csym == '\r' || csym == '\n')
				{
				do
					fbr_inc_pos(rbs);
				while (fbr_get_sym(rbs) == '\r' || fbr_get_sym(rbs) == '\n');
				return 1;
				}
			if (fbr_inc_pos(rbs)) return 1;
			}
		goto _get_next_pair_repeat_after_key;
		}
	if (csym == '\n' || csym == '\r')
		{
		do
			fbr_inc_pos(rbs);
		while (fbr_get_sym(rbs) == '\r' || fbr_get_sym(rbs) == '\n');
		return 1;
		}

_get_next_pair_error:
	if (index->conn_flags & CF_PARSE_ERRORS)
		{
		for (i = 0; key_start[i] != '\r' && key_start[i] != '\n' &&  key_start[i] && i < MAX_KEY_SOURCE; i++);
		fprintf(stderr,"Bad key in line %.*s\n",i,key_start);
		if(fbr_shift_pos(rbs,i)) 
			return 0;
		}
_get_next_pair_to_eol:	
	while (fbr_get_sym(rbs) != '\r' && fbr_get_sym(rbs) != '\n')
		if (fbr_inc_pos(rbs)) return 0;
	if (fbr_inc_pos(rbs)) return 0;
_get_next_pair_grep_eol:
	while (fbr_get_sym(rbs) == '\r' || fbr_get_sym(rbs) == '\n')
		if (fbr_inc_pos(rbs)) return 0;
	goto _get_next_pair_repeat;
	}
	
int fp_countKeys(FReadBufferSet *sourceRbs,off_t file_size)
	{
	int cnt = 0,pos = 0,lastpos = 1;
	if (fbr_first_block(sourceRbs)) return 0;
	char *block = fbr_get_ref(sourceRbs);
	int size = fbr_get_size(sourceRbs);
	
fp_countKeys_repeat:
	while (block[pos] != '\r' && block[pos] != '\n')
		{
		pos++;
		if (pos >= size)
			return (int)((uint64_t)file_size * cnt / lastpos);
		}
	cnt++;
	pos++;
	while (block[pos] == '\r' || block[pos] == '\n')
		{
		pos++;
		if (pos >= size)
			return (int)((uint64_t)file_size * cnt / lastpos);
		}
	lastpos = pos;
	goto fp_countKeys_repeat;
	}
