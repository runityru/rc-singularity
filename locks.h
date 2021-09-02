/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#ifndef _LOCKS_H
#define _LOCKS_H

#include <immintrin.h>
#include <pthread.h>
#include "utils.h"

#ifndef RW_TIMEOUT
#define RW_TIMEOUT 500
#endif

typedef struct FSingSetTg FSingSet;

typedef unsigned long long lockmask_t;

typedef union FRWLockCountersTg
	{
	unsigned short counter[2];
	unsigned both;
	} FRWLockCounters;

typedef struct FRWLockPartsTg
	{
	FRWLockCounters counters;
	unsigned sequence;
	} FRWLockParts;

typedef union FRWLockTg
	{
	FRWLockParts parts;
	lockmask_t fullValue;
	} FRWLock;

typedef struct FReaderLockTg
 	{
	unsigned short keep;
	unsigned short keeped;
 	unsigned readerSeq;
	} FReaderLock;
	
typedef struct FLockSetTg
	{
	FRWLock rw_lock __attribute__ ((aligned (8)));
	unsigned memory_lock;
	unsigned global_lock; 
	pthread_mutex_t process_lock __attribute__ ((aligned (8)));
	uint64_t hash_locks[] __attribute__ ((aligned (8)));
	} FLockSet;

void lck_init_locks(FSingSet *index);

void lck_deinit_locks(FSingSet *index);

// Автоблокировка записи на уровне процесса
int lck_processLock(FSingSet *index);

// Снятие блокировки записи на уровне процесса
int lck_processUnlock(FSingSet *index,int op_result);

// Ручная блокировка записи на уровне процесса
int lck_manualLock(FSingSet *index);

// Снятие блокировки записи на уровне процесса
int lck_manualUnlock(FSingSet *index,int commit,uint32_t *saved);

void _lck_chainLock(FSingSet *index,unsigned hash);

// Попытка блокировки цепочки
int _lck_tryChainLock(FSingSet *index,unsigned hash);

// Разблокировка цепочки
void _lck_chainUnlock(FSingSet *index,unsigned hash);

// Блокировка работы с памятью (устанавливает memory_lock)
static inline void _lck_memoryLock(FLockSet *locks)
	{
	FORMATTED_LOG_LOCKS("Memory spinlock setting\n");
	while (__atomic_exchange_n(&locks->memory_lock,(unsigned)1,__ATOMIC_ACQUIRE)) 
		_mm_pause();  // Межпотоковый спинлок
	FORMATTED_LOG_LOCKS("Memory spinlock setted\n");
	}

// Ожидает переключения читателей (увеличивает seq_lock и ожидает обнуления старой битмаски)
void lck_waitForReaders(FLockSet *locks);

// Снятие блокировки работы с памятью (снимает memory_lock)
static inline void _lck_memoryUnlock(FLockSet *locks)
	{
	FORMATTED_LOG_LOCKS("Memory spinlock removing\n");
	__atomic_store_n(&locks->memory_lock,0,__ATOMIC_RELEASE);
	FORMATTED_LOG_LOCKS("Memory spinlock removed\n");
	}

static inline void lck_globalLock(FLockSet *locks)
	{
	while (__atomic_exchange_n(&locks->global_lock,(unsigned)1,__ATOMIC_ACQUIRE)) 
		_mm_pause();  // Межпотоковый спинлок
	}

static inline void lck_globalUnlock(FLockSet *locks)
	{
	__atomic_store_n(&locks->global_lock,0,__ATOMIC_RELEASE);
	}

// Ставит блокировку читателя
void lck_readerLock(FLockSet *locks,FReaderLock *rlock);

// Проверяет валидность блокировки читателя (1 - все норм, 0 - была снята)
int lck_readerCheck(FLockSet *locks,FReaderLock *rlock);

// Снимает блокировку читателя (1 - все норм, 0 - была снята)
int lck_readerUnlock(FLockSet *locks,FReaderLock *rlock);

#endif