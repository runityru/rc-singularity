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

#define BAD_STATES_CHECK(KVSET)  do { if ((KVSET)->head->bad_states.some_present) \
		{ \
		while(__atomic_load_n(&(KVSET)->head->bad_states.states.deleted,__ATOMIC_RELAXED)) \
			if (idx_relink_set((KVSET))) return ERROR_CONNECTION_LOST; \
		if ((KVSET)->head->bad_states.states.corrupted) return ERROR_DATA_CORRUPTED; \
		} } while(0)

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

// Shared/Exclusive spinlock for writers
// Exclusive lock obtained in case of:
// - manual W or RW locks in SING_LM_FAST and SING_LM_NONE
// - revert data from disk copy
// Shared lock obtained in case of:
// - normal write call in SING_LM_PROTECTED, SING_LM_FAST and SING_LM_NONE

typedef union FShExLockTg
 	{
	struct {
		uint32_t shared_count;
 		uint32_t exclusive_lock;
		};
	uint64_t whole;
	} FShExLock;

void lck_lock_sh(FShExLock *lock);

void lck_unlock_sh(FShExLock *lock);

int lck_lock_ex(FSingSet *index);
static inline void lck_unlock_ex(FShExLock *lock)
	{ __atomic_store_n(&lock->exclusive_lock,0,__ATOMIC_RELEASE); }

typedef struct FReaderLockTg
 	{
	unsigned short keep;
	unsigned short keeped;
 	unsigned readerSeq;
	} FReaderLock;

#define READER_LOCK_INIT {0,0,0}
	
typedef struct FLockSetTg
	{
	FRWLock rw_lock __attribute__ ((aligned (8)));
	FShExLock shex_lock __attribute__ ((aligned (8)));
	unsigned del_in_chain;
	unsigned memory_lock;
	unsigned marks_lock;
	unsigned padding;
	pthread_mutex_t process_lock __attribute__ ((aligned (8)));
	uint64_t hash_locks[] __attribute__ ((aligned (8)));
	} FLockSet;

void lck_init_locks(FSingSet *index);

void lck_deinit_locks(FSingSet *index);

// Автоблокировка записи на уровне процесса
int lck_processLock(FSingSet *kvset);

// Снятие блокировки записи на уровне процесса
int lck_processUnlock(FSingSet *kvset,int op_result,int autorevert);

// Ручная блокировка записи на уровне процесса
int lck_manualLock(FSingSet *kvset);

int lck_manualTry(FSingSet *kvset);

int lck_manualPresent(FSingSet *kvset);

void lck_protectWait(FSingSet *kvset);

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

#define LCK_NO_DELETION 0xFFFFFFFF
// Ожидает переключения читателей (увеличивает seq_lock и ожидает обнуления старой битмаски)
void lck_waitForReaders(FLockSet *locks,unsigned del_in_chain);

// Снятие блокировки работы с памятью (снимает memory_lock)
static inline void _lck_memoryUnlock(FLockSet *locks)
	{
	FORMATTED_LOG_LOCKS("Memory spinlock removing\n");
	__atomic_store_n(&locks->memory_lock,0,__ATOMIC_RELEASE);
	FORMATTED_LOG_LOCKS("Memory spinlock removed\n");
	}

static inline void lck_marksLock(FLockSet *locks)
	{
	while (__atomic_exchange_n(&locks->marks_lock,(unsigned)1,__ATOMIC_ACQUIRE)) 
		_mm_pause();  // Межпотоковый спинлок
	}

static inline void lck_marksUnlock(FLockSet *locks)
	{
	__atomic_store_n(&locks->marks_lock,0,__ATOMIC_RELEASE);
	}

void lck_fullReaderLock(FLockSet *locks);

void lck_fullReaderUnlock(FLockSet *locks);

// Ставит блокировку читателя
void lck_readerLock(FLockSet *locks,FReaderLock *rlock);

// Проверяет валидность блокировки читателя (1 - все норм, 0 - была снята)
int lck_readerCheck(FLockSet *locks,FReaderLock *rlock);

// Снимает блокировку читателя (1 - все норм, 0 - была снята)
int lck_readerUnlock(FLockSet *locks,FReaderLock *rlock);

int lck_readerUnlockCond(FLockSet *locks,FReaderLock *rlock,unsigned work_in_chain);

#endif