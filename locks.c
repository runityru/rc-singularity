/*
 * Copyright: (C) LLC Hosting Community (RU-CENTER)
 * Author: Evgeny Buyevich
 * Contact email: singularity@nic.ru
 */
#include <immintrin.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <sched.h>
#include <errno.h>
#include <pthread.h>

#include "locks.h"
#include "cpages.h"
#include "index.h"

void lck_init_locks(FSingSet *index)
	{
	pthread_mutexattr_t attr;
	if (index->head->read_only)
		return;
   pthread_mutexattr_init(&attr);
	if (index->mem_index_fname)
		{ // Для межпроцессного
		pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
		pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
		}
   pthread_mutex_init(&index->lock_set->process_lock, &attr); 
	pthread_mutexattr_destroy(&attr);
	}

void lck_deinit_locks(FSingSet *index)
 // Эта схема в каком-то случае все таки может привести к неопред. поведению, 
 // но мы не можем использовать счетчик пытающихся захватить мутекс для безопасного его удаления (иначе мутекс становится бессмысленным).
	{
	struct timespec ts = {0,10000000};
	struct timespec ts2;
	struct timespec *req = &ts, *rem = &ts2, *swp;
	if (index->head->read_only)
		return;
	__atomic_store_n(&index->head->bad_states.states.deleted,1,__ATOMIC_SEQ_CST);
	int err = pthread_mutex_lock(&index->lock_set->process_lock);
	switch(err)
		{
		case EOWNERDEAD:
			pthread_mutex_consistent(&index->lock_set->process_lock);
		case 0:
			while(nanosleep(req,rem) == EINTR) // Ждем 10мс. За это время все кто ждал мутекса должны потаймаутиться
				swp = req, req = rem, rem = swp;
			pthread_mutex_unlock(&index->lock_set->process_lock);
		}
	while (pthread_mutex_destroy(&index->lock_set->process_lock) == EBUSY); // Для robust EBUSY похоже что не возвращается, на всякий случай
	}

int lck_processLock(FSingSet *index)
	{
	struct timespec ts = {0,1000000};
	if (index->head->bad_states.some_present)
		{
		while(__atomic_load_n(&index->head->bad_states.states.deleted,__ATOMIC_RELAXED))
			if (idx_relink_set(index))
				return ERROR_CONNECTION_LOST;
		if (index->head->bad_states.states.corrupted)
			return ERROR_DATA_CORRUPTED;
		}
	if (index->read_only)
		return ERROR_IMPOSSIBLE_OPERATION;
	if (!index->head->use_mutex || index->manual_locked)
		return 0; // Автоблокировки мутекса только в LM_SIMPLE

lck_processLock_retry:
	while(__atomic_load_n(&index->head->bad_states.states.deleted,__ATOMIC_RELAXED))
		if (idx_relink_set(index))
			return ERROR_CONNECTION_LOST;
	// (!) Если здесь мы как-то два раза подряд потеряем CPU на 9мс, мутекс может быть удален. Пока не знаю как сделать надежнее
	int err = pthread_mutex_timedlock(&index->lock_set->process_lock,&ts);
	switch(err)
		{
		case ETIMEDOUT:
			goto lck_processLock_retry;
		case EINVAL: // Возможно при захвате удаленного мутекса
			if (__atomic_load_n(&index->head->bad_states.states.deleted,__ATOMIC_RELAXED))
				goto lck_processLock_retry;
			return ERROR_INTERNAL;
		case EOWNERDEAD:
			pthread_mutex_consistent(&index->lock_set->process_lock);
			if (index->head->use_flags & UF_NOT_PERSISTENT)
				return pthread_mutex_unlock(&index->lock_set->process_lock),ERROR_DATA_CORRUPTED;
			if (index->head->wip)
				{
				if (idx_flush(index) < 0)
					return pthread_mutex_unlock(&index->lock_set->process_lock), ERROR_SYNC_FAILED;
				}
			else if (idx_revert(index) < 0)
				return pthread_mutex_unlock(&index->lock_set->process_lock),ERROR_DATA_CORRUPTED;
		case 0:
			if (__atomic_load_n(&index->head->bad_states.states.deleted,__ATOMIC_RELAXED))
				goto lck_processLock_retry; 
			return 0;	
		}
	return ERROR_INTERNAL;
	}

int lck_processUnlock(FSingSet *index,int op_result)
	{
	if (!index->head->use_mutex || index->manual_locked) 
		return op_result; 
	
	if (index->head->check_mutex)
		while (__atomic_load_n(&index->locks_count,__ATOMIC_ACQUIRE))
			_mm_pause();
	int res = (op_result >= 0) ? idx_flush(index) : idx_revert(index);
	pthread_mutex_unlock(&index->lock_set->process_lock);
	return (res < 0) ? res : op_result;
	}

int lck_manualLock(FSingSet *index)
	{
	struct timespec ts = {0,1000000};
	if (index->head->bad_states.some_present)
		{
		while(__atomic_load_n(&index->head->bad_states.states.deleted,__ATOMIC_RELAXED))
			if (idx_relink_set(index))
				return ERROR_CONNECTION_LOST;
		if (index->head->bad_states.states.corrupted)
			return ERROR_DATA_CORRUPTED;
		}
	if (index->read_only || index->manual_locked) // manual_locked - защита от повторного лока в одном потоке
		return ERROR_IMPOSSIBLE_OPERATION;

lck_manualLock_retry:
	while(__atomic_load_n(&index->head->bad_states.states.deleted,__ATOMIC_RELAXED))
		if (idx_relink_set(index))
			return ERROR_CONNECTION_LOST;
	// (!) Если здесь мы как-то два раза подряд потеряем CPU на 9мс, мутекс может быть удален. Пока не знаю как сделать надежнее
	int err = pthread_mutex_timedlock(&index->lock_set->process_lock,&ts);
	switch(err)
		{
		case ETIMEDOUT:
			goto lck_manualLock_retry;
		case EINVAL:
			if (__atomic_load_n(&index->head->bad_states.states.deleted,__ATOMIC_RELAXED))
				goto lck_manualLock_retry;
			return ERROR_INTERNAL;
		case EOWNERDEAD:
			pthread_mutex_consistent(&index->lock_set->process_lock);
			if (index->head->use_flags & UF_NOT_PERSISTENT)
				goto lck_processLock_corrupted;
			if (index->head->wip)
				{
				if (idx_flush(index) < 0)
					{
					pthread_mutex_unlock(&index->lock_set->process_lock);
					return ERROR_SYNC_FAILED;
					}
				}
			else if (idx_revert(index) < 0)
				{
lck_processLock_corrupted:
				pthread_mutex_unlock(&index->lock_set->process_lock);
				return ERROR_DATA_CORRUPTED;
				}				
		case 0:
			if (__atomic_load_n(&index->head->bad_states.states.deleted,__ATOMIC_RELAXED))
				goto lck_manualLock_retry; 
			index->manual_locked = 1;
			return 0;	
		}
	return ERROR_INTERNAL;
	}

int lck_manualUnlock(FSingSet *index,int commit,uint32_t *saved)
	{
	if (!index->manual_locked)
		return ERROR_IMPOSSIBLE_OPERATION; // В том же потоке, что и блокировка. Разблокировка в другом - неопр. поведение
	if (index->head->check_mutex)
		{
		__atomic_store_n(&index->manual_locked,0,__ATOMIC_SEQ_CST);
		while (__atomic_load_n(&index->locks_count,__ATOMIC_SEQ_CST))
			_mm_pause();
		}
	else
		index->manual_locked = 0;
	int res = 0;
	if (commit)
		{
		res = idx_flush(index);
		pthread_mutex_unlock(&index->lock_set->process_lock);
		if (res < 0)
			return res;
		if (saved) 
			*saved = res;
		return 0;
		}
	res = idx_revert(index);
	pthread_mutex_unlock(&index->lock_set->process_lock);
	return res;
	}

// Блокировка цепочки
void _lck_chainLock(FSingSet *index,unsigned hash)
	{
	if (index->head->check_mutex)
		{
lck_chainLock_repeat:
		while (!__atomic_load_n(&index->manual_locked,__ATOMIC_RELAXED))
			_mm_pause();
		__atomic_fetch_add(&index->locks_count,1,__ATOMIC_SEQ_CST);
		if (!__atomic_load_n(&index->manual_locked,__ATOMIC_SEQ_CST))
			{
			__atomic_fetch_sub(&index->locks_count,1,__ATOMIC_SEQ_CST);
			goto lck_chainLock_repeat;
			}
		}
	hash >>= 1;
	unsigned num = hash / 64, bitmask = 1LL << (hash % 64);
	while(__atomic_fetch_or(&index->lock_set->hash_locks[num],bitmask,__ATOMIC_ACQUIRE) & bitmask)
		_mm_pause();
	FORMATTED_LOG_LOCKS("Chain spinlock %d setted\n",hash << 1);
	}

// Попытка блокировки цепочки
int _lck_tryChainLock(FSingSet *index,unsigned hash)
	{
	if (index->head->check_mutex)
		{
lck_chainLock_repeat:
		while (!__atomic_load_n(&index->manual_locked,__ATOMIC_RELAXED))
			_mm_pause();
		__atomic_fetch_add(&index->locks_count,1,__ATOMIC_SEQ_CST);
		if (!__atomic_load_n(&index->manual_locked,__ATOMIC_SEQ_CST))
			{
			__atomic_fetch_sub(&index->locks_count,1,__ATOMIC_SEQ_CST);
			goto lck_chainLock_repeat;
			}
		}
	hash >>= 1;
	unsigned num = hash / 64, bitmask = 1LL << (hash % 64);
	int res = (__atomic_fetch_or(&index->lock_set->hash_locks[num],bitmask,__ATOMIC_ACQUIRE) & bitmask) ? 0 : 1;
	if (index->head->check_mutex && !res)
		__atomic_fetch_sub(&index->locks_count,1,__ATOMIC_RELEASE);
	FORMATTED_LOG_LOCKS("Chain spinlock %d try lock, result %d\n",hash << 1,res);
	return res;
	}

// Разблокировка цепочки
void _lck_chainUnlock(FSingSet *index,unsigned hash)
	{
	hash >>= 1;
	unsigned num = hash / 64, bitmask = ~(1LL << (hash % 64));
	__atomic_and_fetch(&index->lock_set->hash_locks[num],bitmask,__ATOMIC_RELEASE);
	FORMATTED_LOG_LOCKS("Chain spinlock %d removed\n",hash << 1);
	if (index->head->check_mutex)
		__atomic_fetch_sub(&index->locks_count,1,__ATOMIC_RELEASE);
	}

void lck_waitForReaders(FLockSet *locks)
	{
	// Переключаем маску и увеличиваем seq_lock
	unsigned seq_num = __atomic_add_fetch(&locks->rw_lock.parts.sequence,(unsigned)1,__ATOMIC_SEQ_CST);
	unsigned rmask = 0xFFFF << (16 - (seq_num & 1) * 16);
	unsigned result; 
	
	unsigned tryCnt = 0;
	while (tryCnt < RW_TIMEOUT * 1000 && (result = __atomic_load_n(&locks->rw_lock.parts.counters.both,__ATOMIC_SEQ_CST) & rmask))
		{ // м.б. RELAXED? Компилятор не использует lock здесь
		_mm_pause();
		tryCnt++;
		}
	if (result)
		{
//		printf("reader lock forced reset %d\n",result);
		__atomic_and_fetch(&locks->rw_lock.parts.counters.both,(unsigned)~rmask,__ATOMIC_SEQ_CST);
		}
	}

void lck_readerLock(FLockSet *locks,FReaderLock *rlock)
	{
	FRWLock lock,newstate;

	if (rlock->keeped) return;
	lock.fullValue = __atomic_load_n(&locks->rw_lock.fullValue,__ATOMIC_RELAXED); // Получим приблизительное состояние блокировок
lck_readerLock_retry:
	newstate = lock;
	newstate.parts.counters.counter[lock.parts.sequence & 1]++;
	if (!__atomic_compare_exchange_n(&locks->rw_lock.fullValue, &lock.fullValue, newstate.fullValue, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
		goto lck_readerLock_retry;
	rlock->readerSeq = lock.parts.sequence;
	}
	
static inline int _check_reader_lock(FRWLock *lock,FReaderLock *rlock)
	{
	if (!(lock->parts.counters.counter[rlock->readerSeq & 1])) return 0;
	if (rlock->readerSeq != lock->parts.sequence && rlock->readerSeq + 1 != lock->parts.sequence) return 0;
	return 1;
	}

int lck_readerCheck(FLockSet *locks,FReaderLock *rlock)
	{
	FRWLock lock;
	lock.fullValue = __atomic_load_n(&locks->rw_lock.fullValue,__ATOMIC_RELAXED); // Эта функция исп. только для предупреждения зацикливания, т.о. актуальность не очень важна
	return _check_reader_lock(&lock,rlock);
	}

int lck_readerUnlock(FLockSet *locks,FReaderLock *rlock)
	{
	FRWLock lock,newstate;
	
	rlock->keeped = 0;
	lock.fullValue = __atomic_load_n(&locks->rw_lock.fullValue,__ATOMIC_SEQ_CST);
	if (!_check_reader_lock(&lock,rlock))
		return 0;
	if (rlock->keep && rlock->readerSeq == lock.parts.sequence)
		{
		rlock->keeped = 1;
		return 1;
		}
	newstate = lock;
	newstate.parts.counters.counter[rlock->readerSeq & 1]--;
	while (!__atomic_compare_exchange_n(&locks->rw_lock.fullValue, &lock.fullValue, newstate.fullValue, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
		{
		if (!_check_reader_lock(&lock,rlock))
			return 0;
		newstate = lock;
		newstate.parts.counters.counter[rlock->readerSeq & 1]--;
		}
	return 1;
	}