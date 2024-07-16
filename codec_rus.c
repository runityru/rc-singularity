/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

// Tiny codec for domain names, variable names and so on. Case independent, lat + num + '_' + '-' + '.'
// Was made from more complex one so may be strange somewhere
// Codec code: TC

#include <string.h>
#include <stdint.h>

#include "codec.h"

#define ALPHA_POWER 47
#define ALPHA_POWER2 (ALPHA_POWER * ALPHA_POWER)
#define ALPHA_POWER3 (ALPHA_POWER2 * ALPHA_POWER)
#define ALPHA_POWER4 (ALPHA_POWER2 * ALPHA_POWER2)
#define ALPHA_POWER5 (ALPHA_POWER2 * ALPHA_POWER3)

static const unsigned char RES_SIZES_RUS [RES_TABLE_SIZE] = {
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
	
static const char CODE_SYMS_RUS[ALPHA_POWER][2] = {
	{0,0},
	{0xD0,0x9E}, {0xD0,0x95}, {0xD0,0x90}, {0xD0,0x98},
	{0xD0,0x9D}, {0xD0,0xA2}, {0xD0,0xA1}, {0xD0,0xA0},
	{0xD0,0x92}, {0xD0,0x9B}, {0xD0,0x9A},	{0xD0,0x9C},
	{0xD0,0x94}, {0xD0,0x9F}, {0xD0,0xA3},	{0xD0,0xAF},
	{0xD0,0xAB}, {0xD0,0xAC}, {0xD0,0x93}, {0xD0,0x97},
	{0xD0,0x91}, {0xD0,0xA7}, {0xD0,0x99},	{0xD0,0xA5},
	{0xD0,0x96}, {0xD0,0xA8}, {0xD0,0xAE}, {0xD0,0xA6},
	{0xD0,0xA9}, {0xD0,0xAD}, {0xD0,0xA4}, {0xD0,0xAA},
	{0xD0,0x81},
	{'-',0},{'_',0},{'.',0},
	{'0',0},{'1',0},{'2',0},{'3',0},{'4',0},{'5',0},{'6',0},{'7',0},{'8',0},{'9',0}
	};
	
int cd_decode(char *outBuf,const FKeyHead *head,const element_type *key_rest)
	{
	int dpos = 0, tpos = 0;
	unsigned work,key_size,sym;
	
	key_size = head->size;
	work = head->data0;
	
	sym = work % ALPHA_POWER, work /= ALPHA_POWER;
	if (!sym) return tpos;
	outBuf[tpos++] = CODE_SYMS_RUS[sym][0];
	if (CODE_SYMS_RUS[sym][1])
		outBuf[tpos++] = CODE_SYMS_RUS[sym][1];

	sym = work % ALPHA_POWER, work /= ALPHA_POWER;
	if (!sym) return tpos;
	outBuf[tpos++] = CODE_SYMS_RUS[sym][0];
	if (CODE_SYMS_RUS[sym][1])
		outBuf[tpos++] = CODE_SYMS_RUS[sym][1];

	if (!work) return tpos;
	outBuf[tpos++] = CODE_SYMS_RUS[work][0];
	if (CODE_SYMS_RUS[work][1])
		outBuf[tpos++] = CODE_SYMS_RUS[work][1];
	
	if (key_size == 1 && !head->has_value)
		{
		work = head->extra;
		sym = work % ALPHA_POWER, work /= ALPHA_POWER;
		if (!sym) return tpos;
		outBuf[tpos++] = CODE_SYMS_RUS[sym][0];
		if (CODE_SYMS_RUS[sym][1])
			outBuf[tpos++] = CODE_SYMS_RUS[sym][1];

		sym = work % ALPHA_POWER, work /= ALPHA_POWER;
		if (!sym) return tpos;
		outBuf[tpos++] = CODE_SYMS_RUS[sym][0];
		if (CODE_SYMS_RUS[sym][1])
			outBuf[tpos++] = CODE_SYMS_RUS[sym][1];

		sym = work % ALPHA_POWER, work /= ALPHA_POWER;
		if (!sym) return tpos;
		outBuf[tpos++] = CODE_SYMS_RUS[sym][0];
		if (CODE_SYMS_RUS[sym][1])
			outBuf[tpos++] = CODE_SYMS_RUS[sym][1];

		sym = work % ALPHA_POWER, work /= ALPHA_POWER;
		if (!sym) return tpos;
		outBuf[tpos++] = CODE_SYMS_RUS[sym][0];
		if (CODE_SYMS_RUS[sym][1])
			outBuf[tpos++] = CODE_SYMS_RUS[sym][1];

		if (!work) return tpos;
		outBuf[tpos++] = CODE_SYMS_RUS[work][0];
		if (CODE_SYMS_RUS[sym][1])
			outBuf[tpos++] = CODE_SYMS_RUS[work][1];
		return tpos;
		}

	while(key_size--)
		{
		work = key_rest[dpos];
		sym = work % ALPHA_POWER, work /= ALPHA_POWER;
		if (!sym) return tpos;
		outBuf[tpos++] = CODE_SYMS_RUS[sym][0];
		if (CODE_SYMS_RUS[sym][1])
			outBuf[tpos++] = CODE_SYMS_RUS[sym][1];

		sym = work % ALPHA_POWER, work /= ALPHA_POWER;
		if (!sym) return tpos;
		outBuf[tpos++] = CODE_SYMS_RUS[sym][0];
		if (CODE_SYMS_RUS[sym][1])
			outBuf[tpos++] = CODE_SYMS_RUS[sym][1];

		sym = work % ALPHA_POWER, work /= ALPHA_POWER;
		if (!sym) return tpos;
		outBuf[tpos++] = CODE_SYMS_RUS[sym][0];
		if (CODE_SYMS_RUS[sym][1])
			outBuf[tpos++] = CODE_SYMS_RUS[sym][1];
		
		sym = work % ALPHA_POWER, work /= ALPHA_POWER;
		if (!sym) return tpos;
		outBuf[tpos++] = CODE_SYMS_RUS[sym][0];
		if (CODE_SYMS_RUS[sym][1])
			outBuf[tpos++] = CODE_SYMS_RUS[sym][1];
		
		if (!work) return tpos;
		outBuf[tpos++] = CODE_SYMS_RUS[work][0];
		if (CODE_SYMS_RUS[sym][1])
			outBuf[tpos++] = CODE_SYMS_RUS[work][1];
		dpos++;
		}	
	return tpos;
	}
	
typedef enum ERusLettersTg {
	RL_EMPTY,
	RL_O,	RL_E,	RL_A,	RL_I,
	RL_N,	RL_T,	RL_S,	RL_R,
	RL_V,	RL_L,	RL_K,	RL_M,
	RL_D,	RL_P,	RL_U,	RL_YA,
	RL_YI,RL_SS,RL_G,	RL_Z,
	RL_B, RL_CH,RL_Y,	RL_H,
	RL_J, RL_SH,RL_YU,RL_C,
	RL_SCH,RL_EE,RL_F,RL_HS,
	RL_YO,
	RL_DASH, RL_UNDER, RL_DOT,
	RL_0, RL_1, RL_2, RL_3, RL_4, RL_5, RL_6, RL_7, RL_8, RL_9
} ERusLetters;

int cd_transform(const char *buffer,unsigned src_max_size,FTransformData *tdata)
	{
	int i,tlen = 0;
	unsigned char *result = tdata->transformed_key, nc;
	if (src_max_size > MAX_KEY_SOURCE)
		src_max_size = MAX_KEY_SOURCE;
	
	for (i = 0; i < src_max_size && (nc = (unsigned char)buffer[i]); i++)
		{
		switch(nc)
			{
			case 0xD0:
				switch ((unsigned char)buffer[++i])
					{
					case 0x81: result[tlen++] = RL_YO; break; // Ё
					case 0x90: case 0xB0: result[tlen++] = RL_A; break; // А,a
					case 0x91: case 0xB1: result[tlen++] = RL_B; break; // Б,б
					case 0x92: case 0xB2: result[tlen++] = RL_V; break; // В,в
					case 0x93: case 0xB3: result[tlen++] = RL_G; break; // Г,г
					case 0x94: case 0xB4: result[tlen++] = RL_D; break; // Д,д
					case 0x95: case 0xB5: result[tlen++] = RL_E; break; // Е,е
					case 0x96: case 0xB6: result[tlen++] = RL_J; break; // Ж,ж
					case 0x97: case 0xB7: result[tlen++] = RL_Z; break; // З,з
					case 0x98: case 0xB8: result[tlen++] = RL_I; break; // И,и
					case 0x99: case 0xB9: result[tlen++] = RL_Y; break; // Й,й
					case 0x9A: case 0xBA: result[tlen++] = RL_K; break; // К,к
					case 0x9B: case 0xBB: result[tlen++] = RL_L; break; // Л,л
					case 0x9C: case 0xBC: result[tlen++] = RL_M; break; // М,м
					case 0x9D: case 0xBD: result[tlen++] = RL_N; break; // Н,н
					case 0x9E: case 0xBE: result[tlen++] = RL_O; break; // О,о
					case 0x9F: case 0xBF: result[tlen++] = RL_P; break; // П,п
					case 0xA0: result[tlen++] = RL_R; break; // Р
					case 0xA1: result[tlen++] = RL_S; break; // С
					case 0xA2: result[tlen++] = RL_T; break; // Т
					case 0xA3: result[tlen++] = RL_U; break; // У
					case 0xA4: result[tlen++] = RL_F; break; // Ф
					case 0xA5: result[tlen++] = RL_H; break; // Х
					case 0xA6: result[tlen++] = RL_C; break; // Ц
					case 0xA7: result[tlen++] = RL_CH; break; // Ч
					case 0xA8: result[tlen++] = RL_SH; break; // Ш
					case 0xA9: result[tlen++] = RL_SCH; break; // Щ
					case 0xAA: result[tlen++] = RL_HS; break; // Ъ
					case 0xAB: result[tlen++] = RL_YI; break; // Ы
					case 0xAC: result[tlen++] = RL_SS; break; // Ь
					case 0xAD: result[tlen++] = RL_EE; break; // Э
					case 0xAE: result[tlen++] = RL_YU; break; // Ю
					case 0xAF: result[tlen++] = RL_YA; break; // Я
					default:
						goto cd_transform_exit;
					}
				break;
			case 0xD1:
				switch ((unsigned char)buffer[++i])
					{
					case 0x80: result[tlen++] = RL_R; break; // р
					case 0x81: result[tlen++] = RL_S; break; // с
					case 0x82: result[tlen++] = RL_T; break; // т
					case 0x83: result[tlen++] = RL_U; break; // у
					case 0x84: result[tlen++] = RL_F; break; // ф
					case 0x85: result[tlen++] = RL_H; break; // х
					case 0x86: result[tlen++] = RL_C; break; // ц
					case 0x87: result[tlen++] = RL_CH; break; // ч
					case 0x88: result[tlen++] = RL_SH; break; // ш
					case 0x89: result[tlen++] = RL_SCH; break; // щ
					case 0x8A: result[tlen++] = RL_HS; break; // ъ
					case 0x8B: result[tlen++] = RL_YI; break; // ы
					case 0x8C: result[tlen++] = RL_SS; break; // ь
					case 0x8D: result[tlen++] = RL_EE; break; // э
					case 0x8E: result[tlen++] = RL_YU; break; // ю
					case 0x8F: result[tlen++] = RL_YA; break; // я
					case 0x91: result[tlen++] = RL_YO; break; // ё
					default:
						goto cd_transform_exit;
					}
				break;
			case '-': result[tlen++] = RL_DASH; break;	// -
			case '_': result[tlen++] = RL_UNDER; break;	// _
			case '.': result[tlen++] = RL_DOT; break;	// .
			case '0': result[tlen++] = RL_0; break;	// 0
			case '1': result[tlen++] = RL_1; break;	// 1
			case '2': result[tlen++] = RL_2; break;	// 2
			case '3': result[tlen++] = RL_3; break;	// 3
			case '4': result[tlen++] = RL_4; break;	// 4
			case '5': result[tlen++] = RL_5; break;	// 5
			case '6': result[tlen++] = RL_6; break;	// 6
			case '7': result[tlen++] = RL_7; break;	// 7
			case '8': result[tlen++] = RL_8; break;	// 8
			case '9': result[tlen++] = RL_9; break;	// 9
			default:
				goto cd_transform_exit;
			}
		}

cd_transform_exit:

	tdata->trans_key_size = tlen;
	tdata->head.data.size_and_value = 1 + (RES_SIZES_RUS[tlen] << 1); 
	tdata->chain_idx_ref = NULL; // Ссылка на цепочку пока не инициализирована
	tdata->old_key_rest_size = 0;
	tdata->old_value_size = 0;

#ifdef LOG_OPERATION
	memcpy(tdata->key_source,buffer,i);
	tdata->key_source[i] = 0;
#endif
	return i;
	}
