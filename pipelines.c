/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#include "index.h"
#include "codec.h"
#include "pipelines.h"

#define TWO_CORE_CHAIN_SIZE 24
#define TWO_CORE_CHAINS_COUNT 4

#define CACHE_ALIGNED_CHAR(A) (CACHE_LINE_SIZE * ((A)/CACHE_LINE_SIZE + (((A)%CACHE_LINE_SIZE)?1:0)))

typedef struct FProcessChainTg
	{
	unsigned char values[TWO_CORE_CHAIN_SIZE][CACHE_ALIGNED_CHAR(MAX_VALUE_SOURCE)] __attribute__ ((aligned (CACHE_LINE_SIZE)));;
	FTransformData tdata[TWO_CORE_CHAIN_SIZE];
	unsigned ctdata;
	unsigned state; // 0 - filling, 1 - processing, 2 - error
	} __attribute__ ((aligned (CACHE_LINE_SIZE))) FProcessChain;

static inline void pchain_init(FProcessChain *pchain, unsigned diff_mark)
	{
	unsigned i;
	for (i = 0; i < TWO_CORE_CHAIN_SIZE; i++)
		{
		pchain->tdata[i].value_source = pchain->values[i];
		pchain->tdata[i].head.fields.chain_stop = 1;
		pchain->tdata[i].head.fields.diff_mark = diff_mark;
		pchain->tdata[i].use_phantom = 0;
		}
	pchain->ctdata = 0; 
	pchain->state = 0;
	}

static inline void pchain_clear_locks(FSingSet *index,FTransformData **tdatas,int res)
	{
	if (tdatas[1])
		lck_chainUnlock(index,tdatas[1]->hash);
	if (tdatas[2])
		lck_chainUnlock(index,tdatas[2]->hash);
	}

static inline int pchain_step(FSingSet *index,FTransformData **tdatas,CProcessParsedItem pcb,void *cb_param)
	{
	unsigned res;
	int got_lock = 1;
	if (tdatas[0])
		__builtin_prefetch(&index->hash_table[(tdatas[0]->hash >> 1) * KEYHEADS_IN_BLOCK]);
	if (tdatas[1])
		{
		if (tdatas[2])  // Deadlock prevention
			got_lock = (tdatas[1]->hash >> 1 == tdatas[2]->hash >> 1) ? 0 : lck_tryChainLock(index,tdatas[1]->hash);
		else
			lck_chainLock(index,tdatas[1]->hash);
		if (got_lock)
			{
			if ((res = (*pcb)(index,tdatas[1],cb_param)))
				{ // Закончили действие на этом шаге
				if (__builtin_expect(res & KS_ERROR,0))
					return pchain_clear_locks(index,tdatas,res),1;
				idx_op_finalize(index,tdatas[1],res);
				lck_chainUnlock(index,tdatas[1]->hash);
				tdatas[1] = NULL;
				}
			else // Элемент не найден в хеш-таблице, идем в цепочку
				__builtin_prefetch(regionPointer(index,*(tdatas[1]->chain_idx_ref),KEY_HEAD_SIZE));
			}
		}
	if (!tdatas[2])
		return 0;
	res = (*pcb)(index,tdatas[2],cb_param);
	if (__builtin_expect(res & KS_ERROR,0))
		return pchain_clear_locks(index,tdatas,res),1;
	idx_op_finalize(index,tdatas[2],res);
	if (got_lock)
		{
		lck_chainUnlock(index,tdatas[2]->hash); // Разблокируем цепочку
		return 0;
		}
	if (tdatas[2]->hash >> 1 != tdatas[1]->hash >> 1)
		{
		lck_chainUnlock(index,tdatas[2]->hash);
		lck_chainLock(index,tdatas[1]->hash);
		}
	if ((res = (*pcb)(index,tdatas[1],cb_param)))
		{ 
		if (__builtin_expect(res & KS_ERROR,0))
			{
			lck_memoryUnlock(index);
			lck_chainUnlock(index,tdatas[1]->hash);
			return 1;
			}
		idx_op_finalize(index,tdatas[1],res);
		lck_chainUnlock(index,tdatas[1]->hash);
		tdatas[1] = NULL;
		return 0;
		}
	__builtin_prefetch(regionPointer(index,*(tdatas[1]->chain_idx_ref),KEY_HEAD_SIZE));
	return 0;
	}

static inline int pchain_tail(FSingSet *index,FTransformData **tdatas,CProcessParsedItem pcb,void *cb_param)
	{
	tdatas[0] = NULL; // Нет новых данных
	if (pchain_step(index,tdatas,pcb,cb_param)) return 1;
	tdatas[2] = tdatas[1];
	tdatas[1] = tdatas[0] = NULL;
	if (pchain_step(index,tdatas,pcb,cb_param)) return 1;
	tdatas[2] = NULL;
	return 0;
	}

typedef struct FProcessThreadParamsTg
	{
	FSingSet *index;
	CProcessParsedItem pcb;
	FProcessChain *pchains;
	void *cb_param;
	} FProcessThreadParams;

void *process_thread(void *arg)
	{
	unsigned i,size,state;
	unsigned pcnum = 0;
	FProcessThreadParams *ptp = (FProcessThreadParams *)arg;
	FSingSet *index = ptp->index;
	CProcessParsedItem pcb = ptp->pcb;
	FProcessChain *pchains = ptp->pchains;
	FProcessChain *wchain;

	FTransformData *tdatas[3] = {NULL,NULL,NULL};
	unsigned *need_release = NULL;

	do 
		{
		wchain = &pchains[pcnum];
		if (!(state = __atomic_load_n(&wchain->state,__ATOMIC_ACQUIRE)))
			{ // Парсер запаздывает, пока разберем хвост
			if (need_release)
				{
				if (pchain_tail(index,tdatas,pcb,ptp->cb_param))
					return __atomic_store_n(need_release,2,__ATOMIC_RELEASE),(void *)1;
				__atomic_store_n(need_release,0,__ATOMIC_RELEASE);
				need_release = NULL;
				}
			while (!(state = __atomic_load_n(&wchain->state,__ATOMIC_ACQUIRE)))
				_mm_pause();
			}
		size = wchain->ctdata;
		i = 0;
		if (need_release)
			{ 
			tdatas[0] = size ? &wchain->tdata[0] : NULL;
			if (pchain_step(index,tdatas,pcb,ptp->cb_param))
				return __atomic_store_n(need_release,2,__ATOMIC_RELEASE),(void *)1;
			tdatas[2] = tdatas[1];
			tdatas[1] = tdatas[0];
			tdatas[0] = (size > 1) ? &wchain->tdata[1] : NULL;
			if (pchain_step(index,tdatas,pcb,ptp->cb_param))
				return __atomic_store_n(need_release,2,__ATOMIC_RELEASE),(void *)1;
			tdatas[2] = tdatas[1];
			tdatas[1] = tdatas[0];			
			__atomic_store_n(need_release,0,__ATOMIC_RELEASE);
			i = 2;
			}
		for (;i < size; i++)
			{
			tdatas[0] = &wchain->tdata[i];
			if (pchain_step(index,tdatas,pcb,ptp->cb_param))
				return __atomic_store_n(&wchain->state,2,__ATOMIC_RELEASE),(void *)1;
			tdatas[2] = tdatas[1];
			tdatas[1] = tdatas[0];
			}
		pcnum = (pcnum + 1) % TWO_CORE_CHAINS_COUNT;
		need_release = &wchain->state;
		}
	while(size);
	if (pchain_tail(index,tdatas,pcb,ptp->cb_param))
		return __atomic_store_n(&wchain->state,2,__ATOMIC_RELEASE),(void *)1;
	return (void *)0;
	}

int pl_pipeline2(FSingSet *index,CInitSource init_cb,CGetFromSource get_cb, void *data_source,CProcessParsedItem pcb,unsigned invert,void *cb_param)
	{
	int rv;
	unsigned diff_mark = index->head->state_flags & SF_DIFF_MARK;
	
	FProcessChain *pchains;
	if (posix_memalign((void **)&pchains,CACHE_LINE_SIZE,sizeof(FProcessChain) * 4))
		return 1;

	pchain_init(&pchains[0],diff_mark);
	pchain_init(&pchains[1],diff_mark);
	pchain_init(&pchains[2],diff_mark);
	pchain_init(&pchains[3],diff_mark);
	unsigned pcnum = 1, state;
	FProcessChain *wchain;
	FTransformData *tdata;

	FProcessThreadParams ptp = {index,pcb,pchains,cb_param};
	
	if (init_cb && (*init_cb)(data_source))
		return free(pchains),0;

	wchain = &pchains[0];
	tdata = &wchain->tdata[0];
	
	pthread_t proc_thread;

	while (get_cb(index,data_source,tdata,invert))
		{
		tdata->hash = index->hashtable_size;
		cd_encode(tdata);

		if (++wchain->ctdata == TWO_CORE_CHAIN_SIZE)
			break;
		tdata++;
		}
	if (!wchain->ctdata)
		return free(pchains),0;
	__atomic_store_n(&wchain->state,1,__ATOMIC_RELEASE);
	wchain = &pchains[1];
	tdata = &wchain->tdata[0];
	pthread_create(&proc_thread, NULL, process_thread, &ptp);
	while (get_cb(index,data_source,tdata,invert))
		{
		tdata->hash = index->hashtable_size;
		cd_encode(tdata);

		if (++wchain->ctdata == TWO_CORE_CHAIN_SIZE)
			{
			__atomic_store_n(&wchain->state,1,__ATOMIC_RELEASE);
			pcnum = (pcnum + 1) % TWO_CORE_CHAINS_COUNT;
			wchain = &pchains[pcnum];
			while ((state = __atomic_load_n(&wchain->state,__ATOMIC_ACQUIRE)) == 1)
				_mm_pause();
			if (state == 2)
				goto fp_parseFile2_exit;
			wchain->ctdata = 0;
			tdata = &wchain->tdata[0];
			}
		else
			tdata++;
		}
	__atomic_store_n(&wchain->state,1,__ATOMIC_RELEASE);
	if (wchain->ctdata)
		{
		pcnum = (pcnum + 1) % TWO_CORE_CHAINS_COUNT;
		wchain = &pchains[pcnum];
		while ((state = __atomic_load_n(&wchain->state,__ATOMIC_ACQUIRE)) == 1)
			_mm_pause();
		if (state == 2)
			goto fp_parseFile2_exit;
		wchain->ctdata = 0;
		__atomic_store_n(&wchain->state,1,__ATOMIC_RELEASE);
		}
fp_parseFile2_exit:
	pthread_join(proc_thread,(void**)&rv);
	return free(pchains),rv;
	}

#define SINGLE_CHAIN_SIZE 4

int pl_pipeline1(FSingSet *index,CInitSource init_cb,CGetFromSource get_cb, void *data_source,CProcessParsedItem pcb,unsigned invert,void *cb_param)
	{
	unsigned i,ctdata = 0 ;
	FTransformData tdata[SINGLE_CHAIN_SIZE];
	unsigned char *values;
	if (posix_memalign((void **)&values,CACHE_LINE_SIZE,SINGLE_CHAIN_SIZE * CACHE_ALIGNED_CHAR(MAX_VALUE_SOURCE)))
		return 1;	

	FTransformData *tdatas[3] = {&tdata[0],NULL,NULL};
	unsigned diff_mark = index->head->state_flags & SF_DIFF_MARK;

	for (i = 0; i < SINGLE_CHAIN_SIZE; i++)
		{
		tdata[i].value_source = &values[i * CACHE_ALIGNED_CHAR(MAX_VALUE_SOURCE)];
		tdata[i].head.fields.chain_stop = 1;
		tdata[i].head.fields.diff_mark = diff_mark;
		tdata[i].use_phantom = 0;
		}
	
	if (init_cb && (*init_cb)(data_source))
		return free(values),0;
	
	while (get_cb(index,data_source,tdatas[0],invert))
		{
		tdatas[0]->hash = index->hashtable_size;
		cd_encode(tdatas[0]);

		if (pchain_step(index,tdatas,pcb,cb_param))
			return free(values),1;
	
		ctdata = (ctdata + 1) % SINGLE_CHAIN_SIZE;
		tdatas[2] = tdatas[1];
		tdatas[1] = tdatas[0];
		tdatas[0] = &tdata[ctdata];
		}
	int rv = pchain_tail(index,tdatas,pcb,cb_param);
	free(values);
	return rv;
	}