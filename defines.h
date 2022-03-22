/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#ifndef _SING_DEFINES_H
#define _SING_DEFINES_H

// For Visual Studio
#ifdef __INTELLISENSE__
#define __null 0
#endif

#include "logbin.h"

// Extern definitions and dependent

typedef unsigned element_type; // Adressed element

#define SYSTEM_SHM_PATH "/dev/shm/"

// Empty reference
#define ZERO_REF 0
// Invalid reference for testing
#define INVALID_REF 0xFFFFCDFF

// Empty page reference
#define NO_PAGE 0xFFFFFFFF

#define ELEMENT_SIZE sizeof(element_type)
#define ELEMENT_SIZE_BITS (ELEMENT_SIZE * 8)

#define CACHE_LINE_SIZE 64 

// Max key source size
#define MAX_KEY_SOURCE 255 

#define ALIGN_UP(VALUE,ALIGN) ((ALIGN) * ((VALUE) / (ALIGN) + ((VALUE) % (ALIGN) ? 1 : 0)))
#define CACHE_LINE_PADDING(NAME,SIZE) char NAME[ALIGN_UP((SIZE),CACHE_LINE_SIZE) - (SIZE)]
#define CACHE_ALIGNED_MAX_KEY_SOURCE ALIGN_UP(MAX_KEY_SOURCE,CACHE_LINE_SIZE)

// Number of addressed elements on page
#ifndef PAGE_SIZE
	#define PAGE_SIZE 0x10000
#endif

// Сдвиг для получения номера страницы из индекса элемента
#define PAGE_SHIFT (LOG_BIN_MACRO(PAGE_SIZE))
// Размер страницы должен быть степенью двойки, иначе вместо сдвигов будет использоваться деление
#if (1 << PAGE_SHIFT) != PAGE_SIZE
	#error Bad page size
#endif
// Размер страницы должен нацело делиться на 64 ( >= 64)
#if PAGE_SIZE < 64
	#error Bad page size
#endif
// Маска для получения положения на странице
#define OFFSET_MASK (PAGE_SIZE - 1)

// Максимальное число страниц
// По дефолту MAX_PAGE * PAGE_SIZE = 2^32 (для 32-битной версии)
#ifndef MAX_PAGES
	#define MAX_PAGES ((0xFFFFFFFF >> PAGE_SHIFT) + 1)
#endif

#define PAGES_MASK_SIZE (MAX_PAGES / 64 + ((MAX_PAGES % 64) ? 1 : 0))

// Размер страницы в байтах
#define PAGE_SIZE_BYTES (PAGE_SIZE * ELEMENT_SIZE)

// Размер порции обмена с диском. Желательно чтобы совпадал или был больше дисковой страницы
// 64 - число бит в маске измененных участков страницы
#define DISK_PAGE_SIZE (PAGE_SIZE / 64)

// Гарантия того, что кеш-линия полностью попадает в порцию обмена с диском
#if DISK_PAGE_SIZE % CACHE_LINE_SIZE
	#error Disk page is bad aligned
#endif

#define DISK_PAGE_BYTES (PAGE_SIZE_BYTES / 64)

// Max value size in elements (max value source size is calculated)
#define MAX_VALUE_SIZE 32768

#if MAX_VALUE_SIZE > PAGE_SIZE
	#error Value size is larger than page size 
#endif

// Счетчики числа ключей в блоках 
// 1024 ключа в одном блоке
#define HASH_COUNTER_REDUCE_BITS 10

#define HASH_TO_COUNTER(A) ((A) >> HASH_COUNTER_REDUCE_BITS)
#define COUNTER_TO_HASH(A) ((A) << HASH_COUNTER_REDUCE_BITS)

#define COUNTERS_SIZE(A) (HASH_TO_COUNTER(A) + 1)

#endif


