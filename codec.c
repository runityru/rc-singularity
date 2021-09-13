/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

// Tiny codec for domain names, variable names and so on. Case independent, lat + num + '_' + '-' + '.'
// Was made from more complex one so may be strange somewhere
// Codec code: TC

#include <string.h>

#include "codec.h"
#include "stdint.h"

const unsigned char RES_SIZES [RES_TABLE_SIZE] = {
			 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
			 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5, 5,
			 5, 5, 5, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 8,
			 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9,10,10,10,10,10,
			10,11,11,11,11,11,11,12,12,12,12,12,12,13,13,13,
			13,13,13,14,14,14,14,14,14,15,15,15,15,15,15,16,
			16,16,16,16,16,17,17,17,17,17,17,18,18,18,18,18,
			18,19,19,19,19,19,19,20,20,20,20,20,20,21,21,21,
			21,21,21,22,22,22,22,22,22,23,23,23,23,23,23,24,
			24,24,24,24,24,25,25,25,25,25,25,26,26,26,26,26,
			26,27,27,27,27,27,27,28,28,28,28,28,28,31,31,31,
			31,31,31,32,32,32,32,32,32,33,33,33,33,33,33,34,
			34,34,34,34,34,35,35,35,35,35,35,36,36,36,36,36,
			36,37,37,37,37,37,37,38,38,38,38,38,38,39,39,39,
			39,39,39,40,40,40,40,40,40,41,41,41,41,41,41,42,
			42,42,42,42,42,43,43,43,43,43,43,44,44,44,44,44
		};

static inline void process_ext(FTransformData *tdata)
	{
	unsigned i,npos = 0;
	unsigned hsum = tdata->transformed_key[0] + tdata->transformed_key[1] * 40 + tdata->transformed_key[2] * 40 * 40 + tdata->transformed_key[3] * 40 * 40 * 40;
	unsigned size = tdata->trans_key_size;
	
	tdata->head.fields.data0 = hsum;

	for (i = 4; i + 6 <= size;)
		{
		tdata->key_rest[npos] = tdata->transformed_key[i++];
		tdata->key_rest[npos] += tdata->transformed_key[i++] * 40;
		tdata->key_rest[npos] += tdata->transformed_key[i++] * 40 * 40; 
		tdata->key_rest[npos] += tdata->transformed_key[i++] * 40 * 40 * 40; 
		tdata->key_rest[npos] += tdata->transformed_key[i++] * 40 * 40 * 40 * 40; 
		tdata->key_rest[npos] += tdata->transformed_key[i++] * 40 * 40 * 40 * 40 * 40; 
		hsum = HASH_FUNC(hsum,tdata->key_rest[npos]);
		npos++;
		}
		
	tdata->key_rest[npos] = 0; // Т.к. у нас есть лишний элемент в хвосте то это безопасно
	switch (size - i)
		{
		case 5:
			tdata->key_rest[npos]  = tdata->transformed_key[--size] * 40 * 40 * 40 * 40;
		case 4:
			tdata->key_rest[npos] += tdata->transformed_key[--size] * 40 * 40 * 40;
		case 3:
			tdata->key_rest[npos] += tdata->transformed_key[--size] * 40 * 40;
		case 2:
			tdata->key_rest[npos] += tdata->transformed_key[--size] * 40;
		case 1:
			tdata->key_rest[npos] += tdata->transformed_key[--size];
			hsum = HASH_FUNC(hsum,tdata->key_rest[npos]);
		}
	tdata->hash = HASH_TO_HTSIZE(hsum,tdata->hash);
	}
	
void cd_encode(FTransformData *tdata)
	{
	if (tdata->value_size)
		tdata->head.fields.has_value = 1;
	unsigned hash = 0, val;
	switch (tdata->trans_key_size)
		{
		case 10:
			hash = tdata->transformed_key[9] * 40 * 40 * 40 * 40 * 40;
		case 9:
			hash += tdata->transformed_key[8] * 40 * 40 * 40 * 40;
		case 8:
			hash += tdata->transformed_key[7] * 40 * 40 * 40;
		case 7:
			hash += tdata->transformed_key[6] * 40 * 40;
		case 6:
			hash += tdata->transformed_key[5] * 40;
		case 5: 
			hash += tdata->transformed_key[4];
			val = tdata->transformed_key[0] + tdata->transformed_key[1] * 40 + tdata->transformed_key[2] * 40 * 40 + tdata->transformed_key[3] * 40 * 40 * 40;
			
			tdata->head.fields.data0 = val;
			tdata->hash = HASH_TO_HTSIZE(HASH_FUNC(val,hash),tdata->hash);
	
			if (!tdata->value_size)
				tdata->head.fields.extra = hash;
			else
				tdata->key_rest[0] = hash;
			return;
		case 4:
			hash = tdata->transformed_key[3] * 40 * 40 * 40;
		case 3:
			hash += tdata->transformed_key[2] * 40 * 40;
		case 2:
			hash += tdata->transformed_key[1] * 40;
		case 1: 
			hash += tdata->transformed_key[0];
			tdata->head.fields.data0 = hash;
			tdata->hash = HASH_TO_HTSIZE(hash,tdata->hash);
		case 0: return;
		}
	process_ext(tdata); // Все размещаем во внешней области
	}
	
const char CODE_SYMS[40] = {
	0  ,'E','A','N','I','O','R','T','S','L',
	'C','U','D','H','M','G','B','W','K','F',
	'V','Y','P','X','J','Q','Z','-','_','.',
	'0','1','2','3','4','5','6','7','8','9'
	};
	
int cd_decode(char *outBuf,const FKeyHead *head,const element_type *key_rest)
	{
	int dpos = 0;
	unsigned work,key_size,sym;
	
	key_size = head->size;
	work = head->data0;
	
	sym = work % 40, work /= 40;
	if (!sym) return 0;
	outBuf[0] = CODE_SYMS[sym];
	sym = work % 40, work /= 40;
	if (!sym) return 1;
	outBuf[1] = CODE_SYMS[sym];
	sym = work % 40, work /= 40;
	if (!sym) return 2;
	outBuf[2] = CODE_SYMS[sym];
	if (!work) return 3;
	outBuf[3] = CODE_SYMS[work];
	
	if (key_size == 1 && !head->has_value)
		{
		work = head->extra;
		sym = work % 40, work /= 40;
		if (!sym) return 4;
		outBuf[4] = CODE_SYMS[sym];
		sym = work % 40, work /= 40;
		if (!sym) return 5;
		outBuf[5] = CODE_SYMS[sym];
		sym = work % 40, work /= 40;
		if (!sym) return 6;
		outBuf[6] = CODE_SYMS[sym];
		sym = work % 40, work /= 40;
		if (!sym) return 7;
		outBuf[7] = CODE_SYMS[sym];
		sym = work % 40, work /= 40;
		if (!sym) return 8;
		outBuf[8] = CODE_SYMS[sym];
		if (!work) return 9;
		outBuf[9] = CODE_SYMS[work];
		return 10;
		}

	int add = 4;
	while(key_size--)
		{
		work = key_rest[dpos];
		sym = work % 40, work /= 40;
		if (!sym) return add;
		outBuf[add++] = CODE_SYMS[sym];
		sym = work % 40, work /= 40;
		if (!sym) return add;
		outBuf[add++] = CODE_SYMS[sym];
		sym = work % 40, work /= 40;
		if (!sym) return add;
		outBuf[add++] = CODE_SYMS[sym];
		sym = work % 40, work /= 40;
		if (!sym) return add;
		outBuf[add++] = CODE_SYMS[sym];
		sym = work % 40, work /= 40;
		if (!sym) return add;
		outBuf[add++] = CODE_SYMS[sym];
		if (!work) return add;
		outBuf[add++] = CODE_SYMS[work];
		dpos++;
		}	
	return add;
	}
	
const unsigned char CHAR_CODES [256] = {
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 27, 29,  0, // - .
	30, 31, 32, 33, 34, 35, 36, 37, 38, 39,  0,  0,  0,  0,  0,  0, // 0 - 9
	 0,  2, 16, 10, 12,  1, 19, 15, 13,  4, 24, 18,  9, 14,  3,  5, // . A - O
	22, 25,  6,  8,  7, 11, 20, 17, 23, 21, 26,  0,  0,  0,  0, 28, // P - Z _
	 0,  2, 16, 10, 12,  1, 19, 15, 13,  4, 24, 18,  9, 14,  3,  5, // . a - o
	22, 25,  6,  8,  7, 11, 20, 17, 23, 21, 26,  0,  0,  0,  0,  0, // p - z
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	};

int cd_transform(const char *buffer,unsigned src_max_size,FTransformData *tdata)
	{
	int i;
	unsigned char *result = tdata->transformed_key, nc;
	if (src_max_size > MAX_KEY_SOURCE)
		src_max_size = MAX_KEY_SOURCE;
	
	for (i = 0; i < src_max_size && (nc = CHAR_CODES[(unsigned char)buffer[i]]); i++)
		result[i] = nc;

	tdata->trans_key_size = i;
	tdata->head.data.size_and_value = 1 + (RES_SIZES[i] << 1); 
	tdata->chain_idx_ref = NULL; // Ссылка на цепочку пока не инициализирована
	tdata->old_key_rest_size = 0;
	tdata->old_value_size = 0;

#ifdef LOG_OPERATION
	memcpy(tdata->key_source,buffer,i);
	tdata->key_source[i] = 0;
#endif
	return i;
	}

