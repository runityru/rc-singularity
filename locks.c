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

#define BAD_STATES_CHECK(KVSET)  do { if ((KVSET)->head->bad_states.some_present) \
		{ \
		while(__atomic_load_n(&(KVSET)->head->bad_states.states.deleted,__ATOMIC_RELAXED)) \
			if (idx_relink_set((KVSET))) return ERROR_CONNECTION_LOST; \
		if ((KVSET)->head->bad_states.states.corrupted) return ERROR_DATA_CORRUPTED; \
		} } while(0)

static inline int _common_processLock(FSingSet *kvset)
	{
	int res;
	struct timespec ts = {0,1000000};

lck_processLock_retry:
	BAD_STATES_CHECK(kvset);
	// (!) If we lost CPU for 9ms in this point twice in a row, mutex can be deleted
	int err = pthread_mutex_timedlock(&kvset->lock_set->process_lock,&ts);
	switch(err)
		{
		case ETIMEDOUT:
			goto lck_processLock_retry;
		case EINVAL: // Can occur if mutex was deleted
			if (__atomic_load_n(&kvset->head->bad_states.states.deleted,__ATOMIC_RELAXED))
				goto lck_processLock_retry;
			return ERROR_INTERNAL;
		case EOWNERDEAD:
			pthread_mutex_consistent(&kvset->lock_set->process_lock);
			if (kvset->head->use_flags & UF_NOT_PERSISTENT)
				{
				kvset->head->bad_states.states.corrupted = 1; // Light failure, set is readable
				pthread_mutex_unlock(&kvset->lock_set->process_lock);
				return ERROR_DATA_CORRUPTED;
				}
			if (kvset->head->wip)
				{
				if (idx_flush(kvset) < 0)
					return pthread_mutex_unlock(&kvset->lock_set->process_lock), ERROR_SYNC_FAILED;
				}
			else if ((res = idx_revert(kvset)) < 0)
				{
				if (res == ERROR_INTERNAL)
					kvset->head->bad_states.states.corrupted = 1; // No disk load was made, set is still readable
				pthread_mutex_unlock(&kvset->lock_set->process_lock);
				return ERROR_DATA_CORRUPTED;
				}
		case 0:
			if (__atomic_load_n(&kvset->head->bad_states.states.deleted,__ATOMIC_RELAXED))
				goto lck_processLock_retry; 
			return 0;	
		}
	return ERROR_INTERNAL;
	}

int lck_processLock(FSingSet *kvset)
	{
	if (kvset->manual_locked) // kvset can't be deleted while locked by mutex
		return (kvset->head->bad_states.states.corrupted) ? ERROR_DATA_CORRUPTED : 0;
	if (kvset->read_only)
		{
		BAD_STATES_CHECK(kvset);
		return ERROR_IMPOSSIBLE_OPERATION;
		}
	if (!kvset->head->use_mutex) 
		{ // No mutex autolock, exept in LM_SIMPLE
		BAD_STATES_CHECK(kvset);
		return 0;
		} 
	return _common_processLock(kvset);
	}

int lck_processUnlock(FSingSet *index,int op_result,int autorevert)
	{
	if (!index->head->use_mutex || index->manual_locked) 
		return op_result; 
	
	if (index->head->check_mutex)
		while (__atomic_load_n(&index->locks_count,__ATOMIC_ACQUIRE))
			_mm_pause();
	int res = 0;
	if (op_result >= 0)
		res = idx_flush(index);
	else if (autorevert)
		res = idx_revert(index); // We do not set data_currupted here, if revert fail before actual job start
	pthread_mutex_unlock(&index->lock_set->process_lock);
	return (res < 0) ? res : op_result;
	}

int lck_manualLock(FSingSet *kvset)
	{
	if (kvset->read_only || kvset->manual_locked)
		{
		BAD_STATES_CHECK(kvset);
		return ERROR_IMPOSSIBLE_OPERATION;
		}

	int res = _common_processLock(kvset);
	if (!res) kvset->manual_locked = 1;
	return res;
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
	int res;
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
	if (index->head->use_flags & UF_NOT_PERSISTENT)
		res = ERROR_IMPOSSIBLE_OPERATION;
	else
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