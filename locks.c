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
#include <sys/syscall.h>

#include "locks.h"
#include "cpages.h"
#include "index.h"

static uint32_t gettid(void)
	{
	return (uint32_t)syscall(SYS_gettid);
	}

void lck_init_locks(FSingSet *kvset)
	{
	pthread_mutexattr_t attr;
	switch (kvset->head->lock_mode)
		{
		case LM_PROTECTED:
		case LM_SIMPLE:
			pthread_mutexattr_init(&attr);
			if (!kvset->is_private)
				{ // Для межпроцессного
				pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
				pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
				}
		   pthread_mutex_init(&kvset->lock_set->process_lock, &attr); 
			pthread_mutexattr_destroy(&attr);
		}
	}

void lck_deinit_locks(FSingSet *kvset)
 // We should be under manual mutex lock
 	{
	struct timespec ts = {0,11000000};
	struct timespec ts2;
	struct timespec *req = &ts, *rem = &ts2, *swp;
	__atomic_store_n(&kvset->head->bad_states.states.deleted,1,__ATOMIC_SEQ_CST);
	switch (kvset->head->lock_mode)
		{
		case LM_PROTECTED:
			lck_protectWait(kvset);
		case LM_SIMPLE: // Destroying mutex
			while(nanosleep(req,rem) == EINTR) // 11мс wait. Processes waiting for mutex, should get timeout and discover deleted state
				swp = req, req = rem, rem = swp;
			pthread_mutex_unlock(&kvset->lock_set->process_lock);
			while (pthread_mutex_destroy(&kvset->lock_set->process_lock) == EBUSY); // It looks like EBUSY will not work for robust mutex
			return;
		case LM_FAST: // Just removing spinlock
			lck_unlock_ex(&kvset->lock_set->shex_lock);
		}
	}

void lck_lock_sh(FShExLock *lock)
	{
	FShExLock shex_lock,new_state;
	uint32_t tid = gettid();
lck_lock_sh_repeat:
	shex_lock.whole = __atomic_load_n(&lock->whole,__ATOMIC_RELAXED); 
lck_lock_sh_repeat2:
	if (shex_lock.exclusive_lock)
		{ 
		if (shex_lock.exclusive_lock == tid)
			return;
		_mm_pause(); 
		goto lck_lock_sh_repeat; 
		}
	new_state = shex_lock;
	new_state.shared_count++;
	if (!__atomic_compare_exchange_n(&lock->whole, &shex_lock.whole, new_state.whole, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
		{ _mm_pause(); goto lck_lock_sh_repeat2; }
	}

void lck_unlock_sh(FShExLock *lock)
	{
	if (__atomic_load_n(&lock->exclusive_lock,__ATOMIC_RELAXED) != gettid())
		__atomic_sub_fetch(&lock->shared_count,1,__ATOMIC_RELEASE);
	}

int lck_lock_ex(FSingSet *kvset)
	{
	BAD_STATES_CHECK(kvset);
	uint32_t tid = gettid();
	FShExLock shex_lock,new_state;
	shex_lock.whole = __atomic_load_n(&kvset->lock_set->shex_lock.whole,__ATOMIC_RELAXED); 
	if (shex_lock.exclusive_lock == tid)
		return ERROR_IMPOSSIBLE_OPERATION;
lck_lock_ex_repeat:
	while(shex_lock.exclusive_lock)
		_mm_pause(),shex_lock.whole = __atomic_load_n(&kvset->lock_set->shex_lock.whole,__ATOMIC_RELAXED);
	new_state = shex_lock;
	new_state.exclusive_lock = tid;
	if (!__atomic_compare_exchange_n(&kvset->lock_set->shex_lock.whole, &shex_lock.whole, new_state.whole, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
		{ _mm_pause(); goto lck_lock_ex_repeat; }
	while(shex_lock.shared_count)
		_mm_pause(),shex_lock.whole = __atomic_load_n(&kvset->lock_set->shex_lock.whole,__ATOMIC_RELAXED);
	return 0;
	}

static int _try_ex(FSingSet *kvset)
	{
	BAD_STATES_CHECK(kvset);
	uint32_t tid = gettid();
	FShExLock shex_lock,new_state;
	shex_lock.whole = __atomic_load_n(&kvset->lock_set->shex_lock.whole,__ATOMIC_RELAXED); 
	if (shex_lock.exclusive_lock == tid)
		return ERROR_IMPOSSIBLE_OPERATION;
	if (shex_lock.exclusive_lock)
		return RESULT_LOCKED;
	new_state = shex_lock;
	new_state.exclusive_lock = tid;
	if (!__atomic_compare_exchange_n(&kvset->lock_set->shex_lock.whole, &shex_lock.whole, new_state.whole, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
		return RESULT_LOCKED;
	while(shex_lock.shared_count)
		_mm_pause(),shex_lock.whole = __atomic_load_n(&kvset->lock_set->shex_lock.whole,__ATOMIC_RELAXED);
	return 0;
	}

static inline int _common_mutexLock(FSingSet *kvset)
	{
	int res;
	struct timespec ts = {0,1000000};

lck_processLock_retry:
	BAD_STATES_CHECK(kvset);
	// (!) If we lost CPU for 9ms in this point, mutex can be deleted
	int err = pthread_mutex_timedlock(&kvset->lock_set->process_lock,&ts);
	switch(err)
		{
		case ETIMEDOUT:
			goto lck_processLock_retry;
		case EINVAL: // Can occur if mutex was deleted
			if (__atomic_load_n(&kvset->head->bad_states.states.deleted,__ATOMIC_SEQ_CST))
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
				if (cp_flush(kvset) < 0)
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
				{
				pthread_mutex_unlock(&kvset->lock_set->process_lock);
				goto lck_processLock_retry; 
				}
			return 0;	
		}
	return ERROR_INTERNAL;
	}

static inline int _mutexTry(FSingSet *kvset)
	{
	int res;

_mutexTry_retry:
	BAD_STATES_CHECK(kvset);
	int err = pthread_mutex_trylock(&kvset->lock_set->process_lock);
	switch(err)
		{
		case EINVAL: // Can occur if mutex was deleted
			if (__atomic_load_n(&kvset->head->bad_states.states.deleted,__ATOMIC_SEQ_CST))
				goto _mutexTry_retry;
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
				if (cp_flush(kvset) < 0)
					return pthread_mutex_unlock(&kvset->lock_set->process_lock), ERROR_SYNC_FAILED;
				}
			else if ((res = idx_revert(kvset)) < 0)
				{
				if (res == ERROR_INTERNAL)
					kvset->head->bad_states.states.corrupted = 1; // No disk load was made, set is still readable
				pthread_mutex_unlock(&kvset->lock_set->process_lock);
				return ERROR_DATA_CORRUPTED;
				}
		case EBUSY:
			return RESULT_LOCKED;
		case 0:
			if (__atomic_load_n(&kvset->head->bad_states.states.deleted,__ATOMIC_RELAXED))
				{
				pthread_mutex_unlock(&kvset->lock_set->process_lock);
				goto _mutexTry_retry; 
				}
			return 0;	
		}
	return ERROR_INTERNAL;
	}

int lck_processLock(FSingSet *kvset)
	{
	if (kvset->read_only)
		{
		BAD_STATES_CHECK(kvset);
		return ERROR_IMPOSSIBLE_OPERATION;
		}
	switch (kvset->head->lock_mode)
		{
		case LM_FAST:
			BAD_STATES_CHECK(kvset);
			lck_lock_sh(&kvset->lock_set->shex_lock);
			return 0;
		case LM_SIMPLE:
			if (kvset->protect_lock.manual_locked) // kvset can't be deleted while manual locked
				return (kvset->head->bad_states.states.corrupted) ? ERROR_DATA_CORRUPTED : 0;
			return _common_mutexLock(kvset);
		case LM_NONE:
			BAD_STATES_CHECK(kvset);
			return 0;
		}
	// LM_PROTECTED
	FProtectLock protect_lock,new_state;
lck_processLock_repeat:
	protect_lock.whole = __atomic_load_n(&kvset->protect_lock.whole,__ATOMIC_RELAXED); // Lighter spin until manual lock;
lck_processLock_repeat2:
	BAD_STATES_CHECK(kvset);
	if (!protect_lock.manual_locked)
		{ _mm_pause(); goto lck_processLock_repeat; }
	new_state = protect_lock;
	new_state.locks_count++;
	if (!__atomic_compare_exchange_n(&kvset->protect_lock.whole, &protect_lock.whole, new_state.whole, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
		{ _mm_pause(); goto lck_processLock_repeat2; }
	return 0;
	}

int lck_processUnlock(FSingSet *kvset,int op_result,int autorevert)
	{
	switch(kvset->head->lock_mode)
		{
		case LM_NONE:
		case LM_READ_ONLY:
			return op_result;
		case LM_PROTECTED:
			__atomic_sub_fetch(&kvset->protect_lock.locks_count,1,__ATOMIC_RELEASE);
			return op_result;			
		case LM_FAST:
			lck_unlock_sh(&kvset->lock_set->shex_lock);
			return op_result;
		}
	// LM_SIMPLE
	if (kvset->protect_lock.manual_locked) 
		return op_result; 
	int res = 0;
	if (op_result >= 0)
		res = cp_flush(kvset);
	else if (autorevert)
		res = idx_revert(kvset); // We do not set data_currupted here, if revert fail before actual job start
	pthread_mutex_unlock(&kvset->lock_set->process_lock);
	return (res < 0) ? res : op_result;
	}

int lck_manualLock(FSingSet *kvset)
	{
	if (kvset->read_only)
		{
		BAD_STATES_CHECK(kvset);
		return ERROR_IMPOSSIBLE_OPERATION;
		}
	switch(kvset->head->lock_mode)
		{
		case LM_NONE:
			BAD_STATES_CHECK(kvset);
			return ERROR_IMPOSSIBLE_OPERATION;
		case LM_FAST:
			return lck_lock_ex(kvset);
		}
	if (kvset->protect_lock.manual_locked)
		return ERROR_IMPOSSIBLE_OPERATION;
	int res = _common_mutexLock(kvset);
	if (!res)
		__atomic_store_n(&kvset->protect_lock.manual_locked,gettid(),__ATOMIC_SEQ_CST); // Or we can lose this if this thread switch to other core before unlock
	return res;
	}

int lck_manualPresent(FSingSet *kvset)
	{
	if (kvset->read_only)
		return 0;
	switch(kvset->head->lock_mode)
		{
		case LM_PROTECTED:
			return (__atomic_load_n(&kvset->protect_lock.manual_locked,__ATOMIC_RELAXED) == gettid()) ? 1 : 0;
		case LM_FAST:
			return (kvset->lock_set->shex_lock.exclusive_lock == gettid()) ? 1 : 0;
		case LM_SIMPLE:
			return kvset->protect_lock.manual_locked;
		}
	return 0;
	}

int lck_manualTry(FSingSet *kvset)
	{
	if (kvset->read_only)
		{
		BAD_STATES_CHECK(kvset);
		return ERROR_IMPOSSIBLE_OPERATION;
		}
	switch(kvset->head->lock_mode)
		{
		case LM_NONE:
			BAD_STATES_CHECK(kvset);
			return ERROR_IMPOSSIBLE_OPERATION;
		case LM_FAST:
			return _try_ex(kvset);
		}
	if (kvset->protect_lock.manual_locked)
		return ERROR_IMPOSSIBLE_OPERATION;
	int res = _mutexTry(kvset);
	if (!res)
		__atomic_store_n(&kvset->protect_lock.manual_locked,gettid(),__ATOMIC_SEQ_CST); // Or we can lose this if this thread switch to other core before unlock
	return res;
	}

void lck_protectWait(FSingSet *kvset)
	{
	FProtectLock protect_lock;
	protect_lock.whole = __atomic_and_fetch(&kvset->protect_lock.whole,0xFFFFFFFFLL,__ATOMIC_SEQ_CST); 
	if (protect_lock.locks_count)
		{
		_mm_pause();
		while (__atomic_load_n(&kvset->protect_lock.locks_count,__ATOMIC_RELAXED))
			_mm_pause();
		}
	}


int lck_manualUnlock(FSingSet *kvset,int commit,uint32_t *saved)
	{
	if (kvset->read_only)
		return ERROR_IMPOSSIBLE_OPERATION;
	switch(kvset->head->lock_mode)
		{
		case LM_NONE:
			return ERROR_IMPOSSIBLE_OPERATION;
		case LM_PROTECTED:
			if (__atomic_load_n(&kvset->protect_lock.manual_locked,__ATOMIC_RELAXED) != gettid())
				return ERROR_IMPOSSIBLE_OPERATION; // There can't be our tid anyway
			lck_protectWait(kvset);
			break; // We are still under mutex, but other threads are stopped, so we can perform disk sync
		case LM_FAST:
			if (kvset->lock_set->shex_lock.exclusive_lock != gettid())
				return ERROR_IMPOSSIBLE_OPERATION; // We should check if manual lock is removing by same thread
			break;
		case LM_SIMPLE:
			if (!kvset->protect_lock.manual_locked)
				return ERROR_IMPOSSIBLE_OPERATION;
			kvset->protect_lock.manual_locked = 0;
		}
	int res;
	if (commit)
		{
		res = cp_flush(kvset);
		if (res >= 0)
			{
			if (saved)
				*saved = res;
			res = 0;
			}
		}
	else if (kvset->head->use_flags & UF_NOT_PERSISTENT)
		res = ERROR_IMPOSSIBLE_OPERATION;
	else
		res = idx_revert(kvset);
	if (kvset->head->lock_mode == LM_FAST)
		lck_unlock_ex(&kvset->lock_set->shex_lock);
	else
		pthread_mutex_unlock(&kvset->lock_set->process_lock);
	return res;
	}

// Блокировка цепочки
void _lck_chainLock(FSingSet *index,unsigned hash)
	{
	hash >>= 1;
	unsigned num = hash / 64, bitmask = 1LL << (hash % 64);
	while(__atomic_fetch_or(&index->lock_set->hash_locks[num],bitmask,__ATOMIC_ACQUIRE) & bitmask)
		_mm_pause();
	FORMATTED_LOG_LOCKS("Chain spinlock %d setted\n",hash << 1);
	}

// Попытка блокировки цепочки
int _lck_tryChainLock(FSingSet *index,unsigned hash)
	{
	hash >>= 1;
	unsigned num = hash / 64, bitmask = 1LL << (hash % 64);
	int res = (__atomic_fetch_or(&index->lock_set->hash_locks[num],bitmask,__ATOMIC_ACQUIRE) & bitmask) ? 0 : 1;
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
	}

// Wait until readers started before this point finish their work
void lck_waitForReaders(FLockSet *locks,unsigned del_in_chain)
	{
	FRWLock lock;
	locks->del_in_chain = del_in_chain;
	lock.fullValue = __atomic_add_fetch(&locks->rw_lock.fullValue,0x100000000LL,__ATOMIC_SEQ_CST);
	unsigned cnum = 1 - (lock.parts.sequence & 1);
	unsigned short rcnt = lock.parts.counters.counter[cnum];
	if (!rcnt || rcnt == 0xFFFF) return;
	unsigned rmask = 0xFFFF << (16 * cnum);
	unsigned result,tryCnt = 0;
	while ((result = __atomic_load_n(&locks->rw_lock.parts.counters.both,__ATOMIC_SEQ_CST) & rmask) && tryCnt < RW_TIMEOUT * 1000)
		{ // м.б. RELAXED? Компилятор не использует lock здесь
		_mm_pause();
		tryCnt++;
		}
	if (result)
		__atomic_and_fetch(&locks->rw_lock.parts.counters.both,(unsigned)~rmask,__ATOMIC_SEQ_CST);
	}

// Wait until readers started before this point finish their work and prevent new readers from start
// Should be called under exclusive write lock only
void lck_fullReaderLock(FLockSet *locks)
	{
	FRWLock lock,newstate;
	lock.fullValue = __atomic_load_n(&locks->rw_lock.fullValue,__ATOMIC_RELAXED);
lck_fullReadersLock_retry:
	newstate = lock;
	newstate.parts.sequence++;
	unsigned cnum = newstate.parts.sequence & 1;
	newstate.parts.counters.counter[cnum] = 0xFFFF;
	if (!__atomic_compare_exchange_n(&locks->rw_lock.fullValue, &lock.fullValue, newstate.fullValue, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
		goto lck_fullReadersLock_retry;

	cnum = 1 - cnum;
	unsigned rmask = 0xFFFF << (16 * cnum);
	unsigned result,tryCnt = 0;
	while ((result = __atomic_load_n(&locks->rw_lock.parts.counters.both,__ATOMIC_SEQ_CST) & rmask) && tryCnt < RW_TIMEOUT * 1000)
		{ 
		_mm_pause();
		tryCnt++;
		}
	if (result)
		{ // Here we should increase sequence second time, for timeouted readers can restore
		newstate.parts.sequence++;
		newstate.parts.counters.counter[cnum] = 0xFFFF;
		__atomic_store_n(&locks->rw_lock.fullValue,newstate.fullValue,__ATOMIC_SEQ_CST);
		return;
		}
	__atomic_store_n(&locks->rw_lock.parts.counters.both,0xFFFFFFFF,__ATOMIC_RELAXED); // put 0xFFFF in second counter too for switching counters by lck_waitForReaders
	}

void lck_fullReaderUnlock(FLockSet *locks)
	{
	__atomic_store_n(&locks->rw_lock.parts.counters.both,0,__ATOMIC_RELEASE);
	}

static inline int _check_reader_lock(FRWLock lock,FReaderLock *rlock)
	{
	if (!(lock.parts.counters.counter[rlock->readerSeq & 1])) return 0;
	if (rlock->readerSeq != lock.parts.sequence && rlock->readerSeq + 1 != lock.parts.sequence) return 0;
	return 1;
	}

void lck_readerLock(FLockSet *locks,FReaderLock *rlock)
	{
	FRWLock lock,newstate;
	const static struct timespec full_wait = {0,200};

	if (rlock->keeped) return;
	lock.fullValue = __atomic_load_n(&locks->rw_lock.fullValue,__ATOMIC_RELAXED); // We omit one LOCK prefix in exchage for possible exra iteration
lck_readerLock_retry:
	while (lock.parts.counters.counter[lock.parts.sequence & 1] == 0xFFFF)
		{
		nanosleep(&full_wait,NULL); // Full reader lock is sign of revert disk io, we should wait some time
		lock.fullValue = __atomic_load_n(&locks->rw_lock.fullValue,__ATOMIC_RELAXED);
		}
	newstate = lock;
	newstate.parts.counters.counter[lock.parts.sequence & 1]++;
	if (!__atomic_compare_exchange_n(&locks->rw_lock.fullValue, &lock.fullValue, newstate.fullValue, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
		goto lck_readerLock_retry;
	rlock->readerSeq = newstate.parts.sequence;
	}

int lck_readerCheck(FLockSet *locks,FReaderLock *rlock)
	{
	FRWLock lock;
	lock.fullValue = __atomic_load_n(&locks->rw_lock.fullValue,__ATOMIC_RELAXED); // This call is for infinite loop breaking, no LOCK is ok
	return _check_reader_lock(lock,rlock);
	}

// This function remove reader lock, and if lock should be keeped, atomically set new one to let single writer operation pass.
// But if writer want full reader lock, function does not keep the lock.
// It return 1 on success, and 0 if reader lock was destroyed by timeout.
int lck_readerUnlock(FLockSet *locks,FReaderLock *rlock)
	{
	FRWLock lock,newstate;
	
	lock.fullValue = __atomic_load_n(&locks->rw_lock.fullValue,__ATOMIC_SEQ_CST);
	if (!_check_reader_lock(lock,rlock))
		return rlock->keeped = 0,0;
	newstate = lock;
	if (rlock->keep)
		{
		rlock->keeped = 1;
		if (rlock->readerSeq == lock.parts.sequence)
			return 1; // If there are no pending write we leave
		if (lock.parts.counters.counter[lock.parts.sequence & 1] == 0xFFFF)
			rlock->keeped = 0; // If write wants full reader lock, we do not keep the lock
		else
			newstate.parts.counters.counter[lock.parts.sequence & 1]++; // Otherwise we keep the lock
		}
	newstate.parts.counters.counter[rlock->readerSeq & 1]--;
	while (!__atomic_compare_exchange_n(&locks->rw_lock.fullValue, &lock.fullValue, newstate.fullValue, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
		{
		if (!_check_reader_lock(lock,rlock))
			return rlock->keeped = 0,0; 
		newstate = lock;
		if (rlock->keep) 
			newstate.parts.counters.counter[lock.parts.sequence & 1]++; // If we are here, we have pending write and it doesn't want full reader lock
		newstate.parts.counters.counter[rlock->readerSeq & 1]--;
		}
	rlock->readerSeq = newstate.parts.sequence;
	return 1;
	}

// This function sychronize lockfree chain dump with deletion. 
// It works like lck_readerUnlock, but if lock should be keeped and writer wait for deletion in specified chain or want full reader lock, 
// lock is not switched and writer stay locked.
int lck_readerUnlockCond(FLockSet *locks,FReaderLock *rlock,unsigned work_in_chain)
	{
	FRWLock lock,newstate;
	
	lock.fullValue = __atomic_load_n(&locks->rw_lock.fullValue,__ATOMIC_SEQ_CST);
	if (!_check_reader_lock(lock,rlock))
		return rlock->keeped = 0,0;
	newstate = lock;
	if (rlock->keep)
		{
		rlock->keeped = 1;
		if (rlock->readerSeq == lock.parts.sequence || work_in_chain == __atomic_load_n(&locks->del_in_chain,__ATOMIC_RELAXED)
				|| newstate.parts.counters.counter[lock.parts.sequence & 1] == 0xFFFF)
			return 1;
		newstate.parts.counters.counter[lock.parts.sequence & 1]++;
		}
	newstate.parts.counters.counter[rlock->readerSeq & 1]--;
	while (!__atomic_compare_exchange_n(&locks->rw_lock.fullValue, &lock.fullValue, newstate.fullValue, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
		{
		if (!_check_reader_lock(lock,rlock))
			return rlock->keeped = 0,0;
		newstate = lock;
		if (rlock->keep)
			newstate.parts.counters.counter[lock.parts.sequence & 1]++;
		newstate.parts.counters.counter[rlock->readerSeq & 1]--;
		}
	return 1;
	}

