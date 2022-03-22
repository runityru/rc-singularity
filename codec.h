/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

// Tiny codec for domain names, variable names and so on. Case independent, lat + num + '_' + '-' + '.'
// Was made from more complex one so may be strange somewhere
// Codec code: TC

#ifndef _CODEC_H
#define _CODEC_H

#include "defines.h"
#include "keyhead.h"
#include "allocator.h"

// Размер таблицы для вычисления размера ключа по размеру исходных данных
#define RES_TABLE_SIZE 256

// Таблица должна быть больше или равна исходному размеру ключа.
#if RES_TABLE_SIZE <= MAX_KEY_SOURCE 
	#error Incomplete size table
#endif

#define HASH_FUNC(A,B) ((A) * 1111111 + (B))

#define HASH_TO_HTSIZE(A,B) ((A) % (B))

#define OP_ADD 0
#define OP_DEL 1
#define OP_MODIFY 2
#define OP_OLD 3

#define OP_DEL_MASK 1

typedef struct FTransformDataTg
	{
	unsigned hash; 						// Хеш. Заполняется перед cd_encode размером хеш-таблицы, после вызова там хеш
	unsigned trans_key_size;			// Число символов в трансформированном ключе. Заполняется в cd_transform
	unsigned value_size;					// Размер данных в value_source. Заполняется перед вызовом cd_transform
	unsigned short operation;			// Операция с ключом. Заполняется в вызове cd_transform
	unsigned short use_phantom;		// Обрабатывать фантомный ключ как обычный
	unsigned char *value_source;		// Ссылка на данные в источнике. Заполняется перед вызовом cd_transform
	FKeyHeadGeneral head;				// Заголовок ключа. Копируется при успешном добавлении
// 32 байта для 32-х битной версии
	element_type *chain_idx_ref;		// Ссылка на ссылку на продолжение цепочки. Инитится в cd_transform. Заполняется в idx_key_try_set
	element_type old_key_rest;			// Индекс старого тела ключа при удалении
	unsigned old_key_rest_size;		// Размер старого тела ключа при удалении
	void *old_value;						// Ссылка на старое значение
	uint32_t old_value_size;			// Размер старого значения
	uint32_t res_num;						// Номер в массиве результатов для множественной обработки
// 64 байта для 32-х битной версии
	element_type key_rest[CACHE_ALIGNED_MAX_KEY_SIZE];				// Место для битового сжатия ключа
	unsigned char transformed_key[CACHE_ALIGNED_MAX_KEY_SOURCE];	// Место для трансформации алфавита ключа
#ifdef LOG_OPERATION
	char key_source[MAX_KEY_SOURCE + 1]; // Исходные данные ключа
#endif
	} __attribute__ ((aligned (CACHE_LINE_SIZE))) FTransformData;
	
// Поиск префикса операции перед именем ключа
static inline int cd_opscan(char sym,FTransformData *tdata,int invert_operation)
	{
	int rv = 0;
	switch(sym)
		{
		case '+': tdata->operation = OP_ADD; rv = 1; break;
		case '=': tdata->operation = OP_MODIFY; rv = 1; break;
		case '-': tdata->operation = OP_DEL; rv = 1; break;
		case '!': tdata->operation = OP_OLD; rv = 1; break;
		default: tdata->operation = OP_ADD;
		}
	tdata->operation = (tdata->operation + invert_operation) % 4;
	return rv;
	}

// Функция преобразования исходного текстового ключа в промежуточный буфер. Двухшаговое преобразование удобно,
// потому что перекодировку символов можно выполнять в том же switch, в котором определяется граница и 
// валидность ключа при разборе файла со списком. 
// Возвращает число обработанных символов в буфере
int cd_transform(const char *buffer,unsigned src_max_size,FTransformData *tdata);

// Функция кодирования ключа. Выполняет битовое сжатие, считает хеш и формирует заголовок
void cd_encode(FTransformData *tdata);
// Функция раскодирования значения ключа. Используется при diff списков
int cd_decode(char *outBuf,const FKeyHead *head,const element_type *key_rest);
	
#endif