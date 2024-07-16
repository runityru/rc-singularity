/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

// Tiny codec for binary keys in symbols range 1..63 - cd_transform just copying data
// Codec code: TC

#include <string.h>
#include <stdint.h>

#include "codec.h"

#define ALPHA_POWER 64
#define ALPHA_POWER2 (ALPHA_POWER * ALPHA_POWER)
#define ALPHA_POWER3 (ALPHA_POWER2 * ALPHA_POWER)
#define ALPHA_POWER4 (ALPHA_POWER2 * ALPHA_POWER2)
#define ALPHA_POWER5 (ALPHA_POWER2 * ALPHA_POWER3)

static const unsigned char RES_SIZES_64 [RES_TABLE_SIZE] = {
			 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3,
			 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6, 6,
			 6, 6, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 9, 9, 9, 9,
			 9,10,10,10,10,10,11,11,11,11,11,12,12,12,12,12,
			13,13,13,13,13,14,14,14,14,14,15,15,15,15,15,16,
			16,16,16,16,17,17,17,17,17,18,18,18,18,18,19,19,
			19,19,19,20,20,20,20,20,21,21,21,21,21,22,22,22,
			22,22,23,23,23,23,23,24,24,24,24,24,25,25,25,25,
			25,26,26,26,26,26,27,27,27,27,27,28,28,28,28,28,
			29,29,29,29,29,30,30,30,30,30,31,31,31,31,31,32,
			32,32,32,32,33,33,33,33,33,34,34,34,34,34,35,35,
			35,35,35,36,36,36,36,36,37,37,37,37,37,38,38,38,
			38,38,39,39,39,39,39,40,40,40,40,40,41,41,41,41,
			41,42,42,42,42,42,43,43,43,43,43,44,44,44,44,44,
			45,45,45,45,45,46,46,46,46,46,47,47,47,47,47,48,
			48,48,48,48,49,49,49,49,49,50,50,50,50,50,51,51
		};

static inline void process_ext(FTransformData *tdata)
	{
	unsigned i,npos = 0;
	unsigned hsum = tdata->transformed_key[0] + tdata->transformed_key[1] * ALPHA_POWER + tdata->transformed_key[2] * ALPHA_POWER2;
	unsigned size = tdata->trans_key_size;
	
	tdata->head.fields.data0 = hsum;

	for (i = 3; i + 5 <= size;)
		{
		tdata->key_rest[npos] = tdata->transformed_key[i++];
		tdata->key_rest[npos] += tdata->transformed_key[i++] * ALPHA_POWER;
		tdata->key_rest[npos] += tdata->transformed_key[i++] * ALPHA_POWER2; 
		tdata->key_rest[npos] += tdata->transformed_key[i++] * ALPHA_POWER3; 
		tdata->key_rest[npos] += tdata->transformed_key[i++] * ALPHA_POWER4; 
		hsum = HASH_FUNC(hsum,tdata->key_rest[npos]);
		npos++;
		}
		
	tdata->key_rest[npos] = 0; // Т.к. у нас есть лишний элемент в хвосте то это безопасно
	switch (size - i)
		{
		case 4: tdata->key_rest[npos] += tdata->transformed_key[--size] * ALPHA_POWER3;
		case 3: tdata->key_rest[npos] += tdata->transformed_key[--size] * ALPHA_POWER2;
		case 2: tdata->key_rest[npos] += tdata->transformed_key[--size] * ALPHA_POWER;
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
		case 8:
			hash += tdata->transformed_key[7] * ALPHA_POWER4;
		case 7:
			hash += tdata->transformed_key[6] * ALPHA_POWER3;
		case 6:
			hash += tdata->transformed_key[5] * ALPHA_POWER2;
		case 5:
			hash += tdata->transformed_key[4] * ALPHA_POWER;
		case 4: 
			hash += tdata->transformed_key[3];
			val = tdata->transformed_key[0] + tdata->transformed_key[1] * ALPHA_POWER + tdata->transformed_key[2] * ALPHA_POWER2;
			
			tdata->head.fields.data0 = val;
			tdata->hash = HASH_TO_HTSIZE(HASH_FUNC(val,hash),tdata->hash);
			tdata->head.fields.extra = tdata->key_rest[0] = hash; // Putting key tail to both possible places
			return;
		case 3:
			hash += tdata->transformed_key[2] * ALPHA_POWER2;
		case 2:
			hash += tdata->transformed_key[1] * ALPHA_POWER;
		case 1: 
			hash += tdata->transformed_key[0];
			tdata->head.fields.data0 = hash;
			tdata->hash = HASH_TO_HTSIZE(hash,tdata->hash);
		case 0: return;
		}
	process_ext(tdata); // Все размещаем во внешней области
	}
	
int cd_decode(char *outBuf,const FKeyHead *head,const element_type *key_rest)
	{
	int dpos = 0, tpos = 0;
	unsigned work,key_size,sym;
	
	key_size = head->size;
	work = head->data0;
	
	sym = work % ALPHA_POWER, work /= ALPHA_POWER;
	if (!sym) return tpos;
	outBuf[tpos++] = sym;

	sym = work % ALPHA_POWER, work /= ALPHA_POWER;
	if (!sym) return tpos;
	outBuf[tpos++] = sym;

	if (!work) return tpos;
	outBuf[tpos++] = sym;
	
	if (key_size == 1 && !head->has_value)
		{
		work = head->extra;
		sym = work % ALPHA_POWER, work /= ALPHA_POWER;
		if (!sym) return tpos;
		outBuf[tpos++] = sym;

		sym = work % ALPHA_POWER, work /= ALPHA_POWER;
		if (!sym) return tpos;
		outBuf[tpos++] = sym;

		sym = work % ALPHA_POWER, work /= ALPHA_POWER;
		if (!sym) return tpos;
		outBuf[tpos++] = sym;

		sym = work % ALPHA_POWER, work /= ALPHA_POWER;
		if (!sym) return tpos;
		outBuf[tpos++] = sym;

		if (!work) return tpos;
		outBuf[tpos++] = work;
		return tpos;
		}

	while(key_size--)
		{
		work = key_rest[dpos];
		sym = work % ALPHA_POWER, work /= ALPHA_POWER;
		if (!sym) return tpos;
		outBuf[tpos++] = sym;

		sym = work % ALPHA_POWER, work /= ALPHA_POWER;
		if (!sym) return tpos;
		outBuf[tpos++] = sym;

		sym = work % ALPHA_POWER, work /= ALPHA_POWER;
		if (!sym) return tpos;
		outBuf[tpos++] = sym;
		
		sym = work % ALPHA_POWER, work /= ALPHA_POWER;
		if (!sym) return tpos;
		outBuf[tpos++] = sym;
		
		if (!work) return tpos;
		outBuf[tpos++] = sym;
		dpos++;
		}	
	return tpos;
	}
	
int cd_transform(const char *buffer,unsigned src_max_size,FTransformData *tdata)
	{
	int i,tlen = 0;
	unsigned char *result = tdata->transformed_key, nc;
	if (src_max_size > MAX_KEY_SOURCE)
		src_max_size = MAX_KEY_SOURCE;
	
	for (i = 0; i < src_max_size && (nc = (unsigned char)buffer[i]); i++)
		result[tlen++] = nc;

	tdata->trans_key_size = tlen;
	tdata->head.data.size_and_value = 1 + (RES_SIZES_64[tlen] << 1); 
	tdata->chain_idx_ref = NULL; // Ссылка на цепочку пока не инициализирована
	tdata->old_key_rest_size = 0;
	tdata->old_value_size = 0;

#ifdef LOG_OPERATION
	memcpy(tdata->key_source,buffer,i);
	tdata->key_source[i] = 0;
#endif
	return i;
	}
