/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#ifndef _KEYHEAD_H
#define _KEYHEAD_H

#include <stdint.h>

#include "defines.h"

typedef struct FKeyHeadTg
	{
	element_type space_used:1; // Признак использования заголовка, 1. Заполняется в cd_trasform.
	element_type size:6;       // Число дополнительных элементов в ключе. Заполняется в cd_trasform.
	element_type has_value:1;  // Есть ли данные. Заполняется в cd_trasform. 
	element_type chain_stop:1; // Это последний заголовок в цепочке. Заполняется при создании структуры в 1 (т.к. если структура будет добавлена, заголовок станет последним)
										// Невалиден в хештаблице, тк невозможно поддерживать консистентным
	element_type diff_or_phantom_mark:1;  // Служебная отметка
	element_type data0:(ELEMENT_SIZE * 8 - 10);     // Первая часть ключа. Заполняется в cd_encode.
	
	element_type extra;    // Заполняется в cd_encode. Тут может быть ссылка на продолжение ключа и значение или короткое продолжение ключа (до 4 байт)
	} FKeyHead;
	
#define KEY_HEAD_SIZE (sizeof(FKeyHead) / ELEMENT_SIZE)

// Нулевая ссылка на заголовок ключа. Отличается от ZERO_REF т.к. должна обеспечивать различие между занятым и незанятым заголовком в блоке.
// Т.к. размер заголовков кратен двум, это достигается нулевым младшим битом.
//#define KH_ZERO_REF (ZERO_REF & ~(KEY_HEAD_SIZE - 1))
#define KH_ZERO_REF 0

// Для доступа по именам ссылок
typedef struct
	{
	element_type next;	// next стоит раньше prev, чтобы его установкой атомарно сбрасывался space_used
	element_type prev;
	} FKeyHeadLinks;

// Для доступа по номерам ссылок
typedef struct
	{
	element_type links[2];
	} FKeyHeadLinksArray;
	
// Для доступа к байту размера и наличия значения (оптимизация битовых операций)
typedef struct
	{
	unsigned char size_and_value;
	unsigned char _reserv[ELEMENT_SIZE - 1];
	element_type extra;
	} FKeyHeadData;
	
// Битовые флаги использования заголовка
typedef struct
	{
	element_type space_used:1;
	element_type _reserv0:(ELEMENT_SIZE * 8 - 1);
	element_type reserved:1;
	element_type _reserv1:(ELEMENT_SIZE * 8 - 1);
	} FKeyHeadSpace;
	
typedef union FKeyHeadGeneralTg
	{
	uint64_t whole;
	FKeyHead fields;
	FKeyHeadLinks links;
	FKeyHeadLinksArray links_array;
	FKeyHeadSpace space;
	FKeyHeadData data;
	} FKeyHeadGeneral;
	
#define KEYHEADS_IN_BLOCK (CACHE_LINE_SIZE / sizeof(FKeyHead))

// Размер блока заголовков в элементах
#define KH_BLOCK_SIZE (sizeof(FKeyHead) / ELEMENT_SIZE * KEYHEADS_IN_BLOCK)
#define KH_BLOCK_LAST (KEYHEADS_IN_BLOCK - 1)

#define KH_BLOCK_IDX(A) ((A) & ~(CACHE_LINE_SIZE / ELEMENT_SIZE - 1))
#define KH_BLOCK_NUM(A) (((A) & (CACHE_LINE_SIZE / ELEMENT_SIZE - 1)) >> 1)

typedef struct FSingSetTg FSingSet;

void kh_expand_chain_up(FSingSet *index,FKeyHeadGeneral *hblock,unsigned hb_idx,unsigned header_num,FKeyHead *data);
void kh_expand_chain_down(FSingSet *index,FKeyHeadGeneral *hblock,unsigned hb_idx,unsigned header_num,FKeyHead *data);

FKeyHeadGeneral *kh_alloc_one(FSingSet *index,element_type *ref);
FKeyHeadGeneral *kh_alloc_block(FSingSet *index,unsigned size,element_type *ref);
FKeyHeadGeneral *kh_alloc_full_block(FSingSet *index,element_type *ref);

void kh_free_block(FSingSet *index,FKeyHeadGeneral *kh,element_type block_idx,unsigned size);
void kh_free_last_from_chain(FSingSet *index,FKeyHeadGeneral *kh,element_type block_idx);

typedef struct FCheckDataTg FCheckData;
// Добавляет к *free_size размер суммарный размер больших дырок в страницах заголовков
int check_head_holes_chains(FSingSet *index,FCheckData *check_data);

#endif