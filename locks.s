	.file	"locks.c"
# GNU C11 (Ubuntu 7.4.0-1ubuntu1~18.04.1) version 7.4.0 (x86_64-linux-gnu)
#	compiled by GNU C version 7.4.0, GMP version 6.1.2, MPFR version 4.0.1, MPC version 1.1.0, isl version isl-0.19-GMP

# GGC heuristics: --param ggc-min-expand=100 --param ggc-min-heapsize=131072
# options passed:  -imultiarch x86_64-linux-gnu locks.c -mtune=generic
# -march=x86-64 -fverbose-asm -fstack-protector-strong -Wformat
# -Wformat-security
# options enabled:  -fPIC -fPIE -faggressive-loop-optimizations
# -fasynchronous-unwind-tables -fauto-inc-dec -fchkp-check-incomplete-type
# -fchkp-check-read -fchkp-check-write -fchkp-instrument-calls
# -fchkp-narrow-bounds -fchkp-optimize -fchkp-store-bounds
# -fchkp-use-static-bounds -fchkp-use-static-const-bounds
# -fchkp-use-wrappers -fcommon -fdelete-null-pointer-checks
# -fdwarf2-cfi-asm -fearly-inlining -feliminate-unused-debug-types
# -ffp-int-builtin-inexact -ffunction-cse -fgcse-lm -fgnu-runtime
# -fgnu-unique -fident -finline-atomics -fira-hoist-pressure
# -fira-share-save-slots -fira-share-spill-slots -fivopts
# -fkeep-static-consts -fleading-underscore -flifetime-dse
# -flto-odr-type-merging -fmath-errno -fmerge-debug-strings -fpeephole
# -fplt -fprefetch-loop-arrays -freg-struct-return
# -fsched-critical-path-heuristic -fsched-dep-count-heuristic
# -fsched-group-heuristic -fsched-interblock -fsched-last-insn-heuristic
# -fsched-rank-heuristic -fsched-spec -fsched-spec-insn-heuristic
# -fsched-stalled-insns-dep -fschedule-fusion -fsemantic-interposition
# -fshow-column -fshrink-wrap-separate -fsigned-zeros
# -fsplit-ivs-in-unroller -fssa-backprop -fstack-protector-strong
# -fstdarg-opt -fstrict-volatile-bitfields -fsync-libcalls -ftrapping-math
# -ftree-cselim -ftree-forwprop -ftree-loop-if-convert -ftree-loop-im
# -ftree-loop-ivcanon -ftree-loop-optimize -ftree-parallelize-loops=
# -ftree-phiprop -ftree-reassoc -ftree-scev-cprop -funit-at-a-time
# -funwind-tables -fverbose-asm -fzero-initialized-in-bss
# -m128bit-long-double -m64 -m80387 -malign-stringops
# -mavx256-split-unaligned-load -mavx256-split-unaligned-store
# -mfancy-math-387 -mfp-ret-in-387 -mfxsr -mglibc -mieee-fp
# -mlong-double-80 -mmmx -mno-sse4 -mpush-args -mred-zone -msse -msse2
# -mstv -mtls-direct-seg-refs -mvzeroupper

	.text
	.globl	lck_init_locks
	.type	lck_init_locks, @function
lck_init_locks:
.LFB3699:
	.cfi_startproc
	pushq	%rbp	#
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp	#,
	.cfi_def_cfa_register 6
	subq	$32, %rsp	#,
	movq	%rdi, -24(%rbp)	# index, index
# locks.c:19: 	{
	movq	%fs:40, %rax	#, tmp100
	movq	%rax, -8(%rbp)	# tmp100, D.24278
	xorl	%eax, %eax	# tmp100
# locks.c:21: 	if (index->head->read_only)
	movq	-24(%rbp), %rax	# index, tmp92
	movq	584(%rax), %rax	# index_9(D)->head, _1
	movl	40(%rax), %eax	# _1->read_only, _2
	testl	%eax, %eax	# _2
	jne	.L6	#,
# locks.c:23:    pthread_mutexattr_init(&attr);
	leaq	-12(%rbp), %rax	#, tmp93
	movq	%rax, %rdi	# tmp93,
	call	pthread_mutexattr_init@PLT	#
# locks.c:24: 	if (index->mem_index_fname)
	movq	-24(%rbp), %rax	# index, tmp94
	movq	32(%rax), %rax	# index_9(D)->mem_index_fname, _3
	testq	%rax, %rax	# _3
	je	.L4	#,
# locks.c:26: 		pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
	leaq	-12(%rbp), %rax	#, tmp95
	movl	$1, %esi	#,
	movq	%rax, %rdi	# tmp95,
	call	pthread_mutexattr_setrobust@PLT	#
# locks.c:27: 		pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
	leaq	-12(%rbp), %rax	#, tmp96
	movl	$1, %esi	#,
	movq	%rax, %rdi	# tmp96,
	call	pthread_mutexattr_setpshared@PLT	#
.L4:
# locks.c:29:    pthread_mutex_init(&index->lock_set->process_lock, &attr); 
	movq	-24(%rbp), %rax	# index, tmp97
	movq	608(%rax), %rax	# index_9(D)->lock_set, _4
	leaq	16(%rax), %rdx	#, _5
	leaq	-12(%rbp), %rax	#, tmp98
	movq	%rax, %rsi	# tmp98,
	movq	%rdx, %rdi	# _5,
	call	pthread_mutex_init@PLT	#
# locks.c:30: 	pthread_mutexattr_destroy(&attr);
	leaq	-12(%rbp), %rax	#, tmp99
	movq	%rax, %rdi	# tmp99,
	call	pthread_mutexattr_destroy@PLT	#
	jmp	.L1	#
.L6:
# locks.c:22: 		return;
	nop
.L1:
# locks.c:31: 	}
	movq	-8(%rbp), %rax	# D.24278, tmp101
	xorq	%fs:40, %rax	#, tmp101
	je	.L5	#,
	call	__stack_chk_fail@PLT	#
.L5:
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE3699:
	.size	lck_init_locks, .-lck_init_locks
	.globl	lck_deinit_locks
	.type	lck_deinit_locks, @function
lck_deinit_locks:
.LFB3700:
	.cfi_startproc
	pushq	%rbp	#
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp	#,
	.cfi_def_cfa_register 6
	subq	$96, %rsp	#,
	movq	%rdi, -88(%rbp)	# index, index
# locks.c:36: 	{
	movq	%fs:40, %rax	#, tmp117
	movq	%rax, -8(%rbp)	# tmp117, D.24280
	xorl	%eax, %eax	# tmp117
# locks.c:37: 	struct timespec ts = {0,10000000};
	movq	$0, -48(%rbp)	#, ts.tv_sec
	movq	$10000000, -40(%rbp)	#, ts.tv_nsec
# locks.c:39: 	struct timespec *req = &ts, *rem = &ts2, *swp;
	leaq	-48(%rbp), %rax	#, tmp101
	movq	%rax, -72(%rbp)	# tmp101, req
	leaq	-32(%rbp), %rax	#, tmp102
	movq	%rax, -64(%rbp)	# tmp102, rem
# locks.c:40: 	if (index->head->read_only)
	movq	-88(%rbp), %rax	# index, tmp103
	movq	584(%rax), %rax	# index_27(D)->head, _1
	movl	40(%rax), %eax	# _1->read_only, _2
	testl	%eax, %eax	# _2
	jne	.L17	#,
# locks.c:42: 	__atomic_store_n(&index->head->bad_states.states.deleted,1,__ATOMIC_SEQ_CST);
	movq	-88(%rbp), %rax	# index, tmp104
	movq	584(%rax), %rax	# index_27(D)->head, _3
	addq	$32, %rax	#, _4
	movl	$1, %edx	#, tmp105
	movl	%edx, (%rax)	#, tmp105,* _4
	mfence
# locks.c:43: 	int err = pthread_mutex_lock(&index->lock_set->process_lock);
	movq	-88(%rbp), %rax	# index, tmp106
	movq	608(%rax), %rax	# index_27(D)->lock_set, _5
	addq	$16, %rax	#, _6
	movq	%rax, %rdi	# _6,
	call	pthread_mutex_lock@PLT	#
	movl	%eax, -76(%rbp)	# tmp107, err
# locks.c:44: 	switch(err)
	movl	-76(%rbp), %eax	# err, err_30
	testl	%eax, %eax	# err_30
	je	.L13	#,
	cmpl	$130, %eax	#, err_30
	jne	.L10	#,
# locks.c:47: 			pthread_mutex_consistent(&index->lock_set->process_lock);
	movq	-88(%rbp), %rax	# index, tmp109
	movq	608(%rax), %rax	# index_27(D)->lock_set, _7
	addq	$16, %rax	#, _8
	movq	%rax, %rdi	# _8,
	call	pthread_mutex_consistent@PLT	#
# locks.c:49: 			while(nanosleep(req,rem) == EINTR) // Ждем 10мс. За это время все кто ждал мутекса должны потаймаутиться
	jmp	.L13	#
.L14:
# locks.c:50: 				swp = req, req = rem, rem = swp;
	movq	-72(%rbp), %rax	# req, tmp110
	movq	%rax, -56(%rbp)	# tmp110, swp
	movq	-64(%rbp), %rax	# rem, tmp111
	movq	%rax, -72(%rbp)	# tmp111, req
	movq	-56(%rbp), %rax	# swp, tmp112
	movq	%rax, -64(%rbp)	# tmp112, rem
.L13:
# locks.c:49: 			while(nanosleep(req,rem) == EINTR) // Ждем 10мс. За это время все кто ждал мутекса должны потаймаутиться
	movq	-64(%rbp), %rdx	# rem, tmp113
	movq	-72(%rbp), %rax	# req, tmp114
	movq	%rdx, %rsi	# tmp113,
	movq	%rax, %rdi	# tmp114,
	call	nanosleep@PLT	#
	cmpl	$4, %eax	#, _9
	je	.L14	#,
# locks.c:51: 			pthread_mutex_unlock(&index->lock_set->process_lock);
	movq	-88(%rbp), %rax	# index, tmp115
	movq	608(%rax), %rax	# index_27(D)->lock_set, _10
	addq	$16, %rax	#, _11
	movq	%rax, %rdi	# _11,
	call	pthread_mutex_unlock@PLT	#
.L10:
# locks.c:53: 	while (pthread_mutex_destroy(&index->lock_set->process_lock) == EBUSY); // Для robust EBUSY похоже что не возвращается, на всякий случай
	nop
.L15:
# locks.c:53: 	while (pthread_mutex_destroy(&index->lock_set->process_lock) == EBUSY); // Для robust EBUSY похоже что не возвращается, на всякий случай
	movq	-88(%rbp), %rax	# index, tmp116
	movq	608(%rax), %rax	# index_27(D)->lock_set, _12
	addq	$16, %rax	#, _13
	movq	%rax, %rdi	# _13,
	call	pthread_mutex_destroy@PLT	#
	cmpl	$16, %eax	#, _14
	je	.L15	#,
	jmp	.L7	#
.L17:
# locks.c:41: 		return;
	nop
.L7:
# locks.c:54: 	}
	movq	-8(%rbp), %rax	# D.24280, tmp118
	xorq	%fs:40, %rax	#, tmp118
	je	.L16	#,
	call	__stack_chk_fail@PLT	#
.L16:
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE3700:
	.size	lck_deinit_locks, .-lck_deinit_locks
	.type	_common_processLock, @function
_common_processLock:
.LFB3701:
	.cfi_startproc
	pushq	%rbp	#
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp	#,
	.cfi_def_cfa_register 6
	subq	$64, %rsp	#,
	movq	%rdi, -56(%rbp)	# kvset, kvset
# locks.c:64: 	{
	movq	%fs:40, %rax	#, tmp146
	movq	%rax, -8(%rbp)	# tmp146, D.24281
	xorl	%eax, %eax	# tmp146
# locks.c:66: 	struct timespec ts = {0,1000000};
	movq	$0, -32(%rbp)	#, ts.tv_sec
	movq	$1000000, -24(%rbp)	#, ts.tv_nsec
.L19:
# locks.c:69: 	BAD_STATES_CHECK(kvset);
	movq	-56(%rbp), %rax	# kvset, tmp123
	movq	584(%rax), %rax	# kvset_45(D)->head, _1
	movq	32(%rax), %rax	# _1->bad_states.some_present, _2
	testq	%rax, %rax	# _2
	je	.L20	#,
	jmp	.L21	#
.L23:
# locks.c:69: 	BAD_STATES_CHECK(kvset);
	movq	-56(%rbp), %rax	# kvset, tmp124
	movq	%rax, %rdi	# tmp124,
	call	idx_relink_set@PLT	#
	testl	%eax, %eax	# _3
	je	.L21	#,
# locks.c:69: 	BAD_STATES_CHECK(kvset);
	movl	$-2147483392, %eax	#, _35
	jmp	.L35	#
.L21:
# locks.c:69: 	BAD_STATES_CHECK(kvset);
	movq	-56(%rbp), %rax	# kvset, tmp125
	movq	584(%rax), %rax	# kvset_45(D)->head, _4
	addq	$32, %rax	#, _5
	movl	(%rax), %eax	#* _5, _6
	testl	%eax, %eax	# _6
	jne	.L23	#,
# locks.c:69: 	BAD_STATES_CHECK(kvset);
	movq	-56(%rbp), %rax	# kvset, tmp126
	movq	584(%rax), %rax	# kvset_45(D)->head, _7
	movl	36(%rax), %eax	# _7->bad_states.states.corrupted, _8
	testl	%eax, %eax	# _8
	je	.L20	#,
# locks.c:69: 	BAD_STATES_CHECK(kvset);
	movl	$-2147483136, %eax	#, _35
	jmp	.L35	#
.L20:
# locks.c:71: 	int err = pthread_mutex_timedlock(&kvset->lock_set->process_lock,&ts);
	movq	-56(%rbp), %rax	# kvset, tmp127
	movq	608(%rax), %rax	# kvset_45(D)->lock_set, _9
	leaq	16(%rax), %rdx	#, _10
	leaq	-32(%rbp), %rax	#, tmp128
	movq	%rax, %rsi	# tmp128,
	movq	%rdx, %rdi	# _10,
	call	pthread_mutex_timedlock@PLT	#
	movl	%eax, -40(%rbp)	# tmp129, err
# locks.c:72: 	switch(err)
	movl	-40(%rbp), %eax	# err, err_51
	cmpl	$22, %eax	#, err_51
	je	.L25	#,
	cmpl	$22, %eax	#, err_51
	jg	.L26	#,
	testl	%eax, %eax	# err_51
	je	.L27	#,
	jmp	.L24	#
.L26:
	cmpl	$110, %eax	#, err_51
	je	.L19	#,
	cmpl	$130, %eax	#, err_51
	je	.L29	#,
	jmp	.L24	#
.L25:
# locks.c:77: 			if (__atomic_load_n(&kvset->head->bad_states.states.deleted,__ATOMIC_RELAXED))
	movq	-56(%rbp), %rax	# kvset, tmp131
	movq	584(%rax), %rax	# kvset_45(D)->head, _11
	addq	$32, %rax	#, _12
	movl	(%rax), %eax	#* _12, _13
	testl	%eax, %eax	# _13
	je	.L30	#,
# locks.c:78: 				goto lck_processLock_retry;
	jmp	.L19	#
.L30:
# locks.c:79: 			return ERROR_INTERNAL;
	movl	$-2147482880, %eax	#, _35
	jmp	.L35	#
.L29:
# locks.c:81: 			pthread_mutex_consistent(&kvset->lock_set->process_lock);
	movq	-56(%rbp), %rax	# kvset, tmp132
	movq	608(%rax), %rax	# kvset_45(D)->lock_set, _14
	addq	$16, %rax	#, _15
	movq	%rax, %rdi	# _15,
	call	pthread_mutex_consistent@PLT	#
# locks.c:82: 			if (kvset->head->use_flags & UF_NOT_PERSISTENT)
	movq	-56(%rbp), %rax	# kvset, tmp133
	movq	584(%rax), %rax	# kvset_45(D)->head, _16
	movl	16(%rax), %eax	# _16->use_flags, _17
	andl	$1, %eax	#, _18
	testl	%eax, %eax	# _18
	je	.L31	#,
# locks.c:84: 				kvset->head->bad_states.states.corrupted = 1; // Light failure, set is readable
	movq	-56(%rbp), %rax	# kvset, tmp134
	movq	584(%rax), %rax	# kvset_45(D)->head, _19
	movl	$1, 36(%rax)	#, _19->bad_states.states.corrupted
# locks.c:85: 				pthread_mutex_unlock(&kvset->lock_set->process_lock);
	movq	-56(%rbp), %rax	# kvset, tmp135
	movq	608(%rax), %rax	# kvset_45(D)->lock_set, _20
	addq	$16, %rax	#, _21
	movq	%rax, %rdi	# _21,
	call	pthread_mutex_unlock@PLT	#
# locks.c:86: 				return ERROR_DATA_CORRUPTED;
	movl	$-2147483136, %eax	#, _35
	jmp	.L35	#
.L31:
# locks.c:88: 			if (kvset->head->wip)
	movq	-56(%rbp), %rax	# kvset, tmp136
	movq	584(%rax), %rax	# kvset_45(D)->head, _22
	movl	104(%rax), %eax	# _22->wip, _23
	testl	%eax, %eax	# _23
	je	.L32	#,
# locks.c:90: 				if (idx_flush(kvset) < 0)
	movq	-56(%rbp), %rax	# kvset, tmp137
	movq	%rax, %rdi	# tmp137,
	call	idx_flush@PLT	#
	testl	%eax, %eax	# _24
	jns	.L27	#,
# locks.c:91: 					return pthread_mutex_unlock(&kvset->lock_set->process_lock), ERROR_SYNC_FAILED;
	movq	-56(%rbp), %rax	# kvset, tmp138
	movq	608(%rax), %rax	# kvset_45(D)->lock_set, _25
	addq	$16, %rax	#, _26
	movq	%rax, %rdi	# _26,
	call	pthread_mutex_unlock@PLT	#
	movl	$-2147481344, %eax	#, _35
	jmp	.L35	#
.L32:
# locks.c:93: 			else if ((res = idx_revert(kvset)) < 0)
	movq	-56(%rbp), %rax	# kvset, tmp139
	movq	%rax, %rdi	# tmp139,
	call	idx_revert@PLT	#
	movl	%eax, -36(%rbp)	# tmp140, res
	cmpl	$0, -36(%rbp)	#, res
	jns	.L27	#,
# locks.c:95: 				if (res == ERROR_INTERNAL)
	cmpl	$-2147482880, -36(%rbp)	#, res
	jne	.L33	#,
# locks.c:96: 					kvset->head->bad_states.states.corrupted = 1; // No disk load was made, set is still readable
	movq	-56(%rbp), %rax	# kvset, tmp141
	movq	584(%rax), %rax	# kvset_45(D)->head, _27
	movl	$1, 36(%rax)	#, _27->bad_states.states.corrupted
.L33:
# locks.c:97: 				pthread_mutex_unlock(&kvset->lock_set->process_lock);
	movq	-56(%rbp), %rax	# kvset, tmp142
	movq	608(%rax), %rax	# kvset_45(D)->lock_set, _28
	addq	$16, %rax	#, _29
	movq	%rax, %rdi	# _29,
	call	pthread_mutex_unlock@PLT	#
# locks.c:98: 				return ERROR_DATA_CORRUPTED;
	movl	$-2147483136, %eax	#, _35
	jmp	.L35	#
.L27:
# locks.c:101: 			if (__atomic_load_n(&kvset->head->bad_states.states.deleted,__ATOMIC_RELAXED))
	movq	-56(%rbp), %rax	# kvset, tmp143
	movq	584(%rax), %rax	# kvset_45(D)->head, _30
	addq	$32, %rax	#, _31
	movl	(%rax), %eax	#* _31, _32
	testl	%eax, %eax	# _32
	je	.L34	#,
# locks.c:103: 				pthread_mutex_unlock(&kvset->lock_set->process_lock);
	movq	-56(%rbp), %rax	# kvset, tmp144
	movq	608(%rax), %rax	# kvset_45(D)->lock_set, _33
	addq	$16, %rax	#, _34
	movq	%rax, %rdi	# _34,
	call	pthread_mutex_unlock@PLT	#
# locks.c:104: 				goto lck_processLock_retry; 
	jmp	.L19	#
.L34:
# locks.c:106: 			return 0;	
	movl	$0, %eax	#, _35
	jmp	.L35	#
.L24:
# locks.c:108: 	return ERROR_INTERNAL;
	movl	$-2147482880, %eax	#, _35
.L35:
# locks.c:109: 	}
	movq	-8(%rbp), %rcx	# D.24281, tmp147
	xorq	%fs:40, %rcx	#, tmp147
	je	.L36	#,
# locks.c:109: 	}
	call	__stack_chk_fail@PLT	#
.L36:
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE3701:
	.size	_common_processLock, .-_common_processLock
	.globl	lck_processLock
	.type	lck_processLock, @function
lck_processLock:
.LFB3702:
	.cfi_startproc
	pushq	%rbp	#
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp	#,
	.cfi_def_cfa_register 6
	subq	$16, %rsp	#,
	movq	%rdi, -8(%rbp)	# kvset, kvset
# locks.c:113: 	if (kvset->manual_locked) // kvset can't be deleted while locked by mutex
	movq	-8(%rbp), %rax	# kvset, tmp111
	movl	572(%rax), %eax	# kvset_31(D)->manual_locked, _1
	testl	%eax, %eax	# _1
	je	.L38	#,
# locks.c:114: 		return (kvset->head->bad_states.states.corrupted) ? ERROR_DATA_CORRUPTED : 0;
	movq	-8(%rbp), %rax	# kvset, tmp112
	movq	584(%rax), %rax	# kvset_31(D)->head, _2
	movl	36(%rax), %eax	# _2->bad_states.states.corrupted, _3
	testl	%eax, %eax	# _3
	je	.L39	#,
# locks.c:114: 		return (kvset->head->bad_states.states.corrupted) ? ERROR_DATA_CORRUPTED : 0;
	movl	$-2147483136, %eax	#, _23
	jmp	.L41	#
.L39:
# locks.c:114: 		return (kvset->head->bad_states.states.corrupted) ? ERROR_DATA_CORRUPTED : 0;
	movl	$0, %eax	#, _23
	jmp	.L41	#
.L38:
# locks.c:115: 	if (kvset->read_only)
	movq	-8(%rbp), %rax	# kvset, tmp113
	movl	576(%rax), %eax	# kvset_31(D)->read_only, _4
	testl	%eax, %eax	# _4
	je	.L42	#,
# locks.c:117: 		BAD_STATES_CHECK(kvset);
	movq	-8(%rbp), %rax	# kvset, tmp114
	movq	584(%rax), %rax	# kvset_31(D)->head, _5
	movq	32(%rax), %rax	# _5->bad_states.some_present, _6
	testq	%rax, %rax	# _6
	je	.L43	#,
	jmp	.L44	#
.L45:
# locks.c:117: 		BAD_STATES_CHECK(kvset);
	movq	-8(%rbp), %rax	# kvset, tmp115
	movq	%rax, %rdi	# tmp115,
	call	idx_relink_set@PLT	#
	testl	%eax, %eax	# _7
	je	.L44	#,
# locks.c:117: 		BAD_STATES_CHECK(kvset);
	movl	$-2147483392, %eax	#, _23
	jmp	.L41	#
.L44:
# locks.c:117: 		BAD_STATES_CHECK(kvset);
	movq	-8(%rbp), %rax	# kvset, tmp116
	movq	584(%rax), %rax	# kvset_31(D)->head, _8
	addq	$32, %rax	#, _9
	movl	(%rax), %eax	#* _9, _10
	testl	%eax, %eax	# _10
	jne	.L45	#,
# locks.c:117: 		BAD_STATES_CHECK(kvset);
	movq	-8(%rbp), %rax	# kvset, tmp117
	movq	584(%rax), %rax	# kvset_31(D)->head, _11
	movl	36(%rax), %eax	# _11->bad_states.states.corrupted, _12
	testl	%eax, %eax	# _12
	je	.L43	#,
# locks.c:117: 		BAD_STATES_CHECK(kvset);
	movl	$-2147483136, %eax	#, _23
	jmp	.L41	#
.L43:
# locks.c:118: 		return ERROR_IMPOSSIBLE_OPERATION;
	movl	$-2147481600, %eax	#, _23
	jmp	.L41	#
.L42:
# locks.c:120: 	if (!kvset->head->use_mutex) 
	movq	-8(%rbp), %rax	# kvset, tmp118
	movq	584(%rax), %rax	# kvset_31(D)->head, _13
	movl	20(%rax), %eax	# _13->use_mutex, _14
	testl	%eax, %eax	# _14
	jne	.L46	#,
# locks.c:122: 		BAD_STATES_CHECK(kvset);
	movq	-8(%rbp), %rax	# kvset, tmp119
	movq	584(%rax), %rax	# kvset_31(D)->head, _15
	movq	32(%rax), %rax	# _15->bad_states.some_present, _16
	testq	%rax, %rax	# _16
	je	.L47	#,
	jmp	.L48	#
.L49:
# locks.c:122: 		BAD_STATES_CHECK(kvset);
	movq	-8(%rbp), %rax	# kvset, tmp120
	movq	%rax, %rdi	# tmp120,
	call	idx_relink_set@PLT	#
	testl	%eax, %eax	# _17
	je	.L48	#,
# locks.c:122: 		BAD_STATES_CHECK(kvset);
	movl	$-2147483392, %eax	#, _23
	jmp	.L41	#
.L48:
# locks.c:122: 		BAD_STATES_CHECK(kvset);
	movq	-8(%rbp), %rax	# kvset, tmp121
	movq	584(%rax), %rax	# kvset_31(D)->head, _18
	addq	$32, %rax	#, _19
	movl	(%rax), %eax	#* _19, _20
	testl	%eax, %eax	# _20
	jne	.L49	#,
# locks.c:122: 		BAD_STATES_CHECK(kvset);
	movq	-8(%rbp), %rax	# kvset, tmp122
	movq	584(%rax), %rax	# kvset_31(D)->head, _21
	movl	36(%rax), %eax	# _21->bad_states.states.corrupted, _22
	testl	%eax, %eax	# _22
	je	.L47	#,
# locks.c:122: 		BAD_STATES_CHECK(kvset);
	movl	$-2147483136, %eax	#, _23
	jmp	.L41	#
.L47:
# locks.c:123: 		return 0;
	movl	$0, %eax	#, _23
	jmp	.L41	#
.L46:
# locks.c:125: 	return _common_processLock(kvset);
	movq	-8(%rbp), %rax	# kvset, tmp123
	movq	%rax, %rdi	# tmp123,
	call	_common_processLock	#
.L41:
# locks.c:126: 	}
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE3702:
	.size	lck_processLock, .-lck_processLock
	.globl	lck_processUnlock
	.type	lck_processUnlock, @function
lck_processUnlock:
.LFB3703:
	.cfi_startproc
	pushq	%rbp	#
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp	#,
	.cfi_def_cfa_register 6
	subq	$32, %rsp	#,
	movq	%rdi, -24(%rbp)	# index, index
	movl	%esi, -28(%rbp)	# op_result, op_result
	movl	%edx, -32(%rbp)	# autorevert, autorevert
# locks.c:130: 	if (!index->head->use_mutex || index->manual_locked) 
	movq	-24(%rbp), %rax	# index, tmp98
	movq	584(%rax), %rax	# index_18(D)->head, _1
	movl	20(%rax), %eax	# _1->use_mutex, _2
	testl	%eax, %eax	# _2
	je	.L51	#,
# locks.c:130: 	if (!index->head->use_mutex || index->manual_locked) 
	movq	-24(%rbp), %rax	# index, tmp99
	movl	572(%rax), %eax	# index_18(D)->manual_locked, _3
	testl	%eax, %eax	# _3
	je	.L52	#,
.L51:
# locks.c:131: 		return op_result; 
	movl	-28(%rbp), %eax	# op_result, iftmp.1_12
	jmp	.L53	#
.L52:
# locks.c:133: 	if (index->head->check_mutex)
	movq	-24(%rbp), %rax	# index, tmp100
	movq	584(%rax), %rax	# index_18(D)->head, _4
	movl	28(%rax), %eax	# _4->check_mutex, _5
	testl	%eax, %eax	# _5
	je	.L54	#,
# locks.c:134: 		while (__atomic_load_n(&index->locks_count,__ATOMIC_ACQUIRE))
	jmp	.L55	#
.L56:
# /usr/lib/gcc/x86_64-linux-gnu/7/include/xmmintrin.h:1267:   __builtin_ia32_pause ();
	rep nop
.L55:
# locks.c:134: 		while (__atomic_load_n(&index->locks_count,__ATOMIC_ACQUIRE))
	movq	-24(%rbp), %rax	# index, tmp101
	addq	$568, %rax	#, _6
	movl	(%rax), %eax	#* _6, _7
	testl	%eax, %eax	# _7
	jne	.L56	#,
.L54:
# locks.c:136: 	int res = 0;
	movl	$0, -4(%rbp)	#, res
# locks.c:137: 	if (op_result >= 0)
	cmpl	$0, -28(%rbp)	#, op_result
	js	.L57	#,
# locks.c:138: 		res = idx_flush(index);
	movq	-24(%rbp), %rax	# index, tmp102
	movq	%rax, %rdi	# tmp102,
	call	idx_flush@PLT	#
	movl	%eax, -4(%rbp)	# tmp103, res
	jmp	.L58	#
.L57:
# locks.c:139: 	else if (autorevert)
	cmpl	$0, -32(%rbp)	#, autorevert
	je	.L58	#,
# locks.c:140: 		res = idx_revert(index); // We do not set data_currupted here, if revert fail before actual job start
	movq	-24(%rbp), %rax	# index, tmp104
	movq	%rax, %rdi	# tmp104,
	call	idx_revert@PLT	#
	movl	%eax, -4(%rbp)	# tmp105, res
.L58:
# locks.c:141: 	pthread_mutex_unlock(&index->lock_set->process_lock);
	movq	-24(%rbp), %rax	# index, tmp106
	movq	608(%rax), %rax	# index_18(D)->lock_set, _8
	addq	$16, %rax	#, _9
	movq	%rax, %rdi	# _9,
	call	pthread_mutex_unlock@PLT	#
# locks.c:142: 	return (res < 0) ? res : op_result;
	cmpl	$0, -4(%rbp)	#, res
	jns	.L59	#,
# locks.c:142: 	return (res < 0) ? res : op_result;
	movl	-4(%rbp), %eax	# res, iftmp.1_12
	jmp	.L53	#
.L59:
# locks.c:142: 	return (res < 0) ? res : op_result;
	movl	-28(%rbp), %eax	# op_result, iftmp.1_12
.L53:
# locks.c:143: 	}
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE3703:
	.size	lck_processUnlock, .-lck_processUnlock
	.globl	lck_manualLock
	.type	lck_manualLock, @function
lck_manualLock:
.LFB3704:
	.cfi_startproc
	pushq	%rbp	#
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp	#,
	.cfi_def_cfa_register 6
	subq	$32, %rsp	#,
	movq	%rdi, -24(%rbp)	# kvset, kvset
# locks.c:147: 	if (kvset->read_only || kvset->manual_locked)
	movq	-24(%rbp), %rax	# kvset, tmp99
	movl	576(%rax), %eax	# kvset_17(D)->read_only, _1
	testl	%eax, %eax	# _1
	jne	.L62	#,
# locks.c:147: 	if (kvset->read_only || kvset->manual_locked)
	movq	-24(%rbp), %rax	# kvset, tmp100
	movl	572(%rax), %eax	# kvset_17(D)->manual_locked, _2
	testl	%eax, %eax	# _2
	je	.L63	#,
.L62:
# locks.c:149: 		BAD_STATES_CHECK(kvset);
	movq	-24(%rbp), %rax	# kvset, tmp101
	movq	584(%rax), %rax	# kvset_17(D)->head, _3
	movq	32(%rax), %rax	# _3->bad_states.some_present, _4
	testq	%rax, %rax	# _4
	je	.L64	#,
	jmp	.L65	#
.L67:
# locks.c:149: 		BAD_STATES_CHECK(kvset);
	movq	-24(%rbp), %rax	# kvset, tmp102
	movq	%rax, %rdi	# tmp102,
	call	idx_relink_set@PLT	#
	testl	%eax, %eax	# _5
	je	.L65	#,
# locks.c:149: 		BAD_STATES_CHECK(kvset);
	movl	$-2147483392, %eax	#, _11
	jmp	.L66	#
.L65:
# locks.c:149: 		BAD_STATES_CHECK(kvset);
	movq	-24(%rbp), %rax	# kvset, tmp103
	movq	584(%rax), %rax	# kvset_17(D)->head, _6
	addq	$32, %rax	#, _7
	movl	(%rax), %eax	#* _7, _8
	testl	%eax, %eax	# _8
	jne	.L67	#,
# locks.c:149: 		BAD_STATES_CHECK(kvset);
	movq	-24(%rbp), %rax	# kvset, tmp104
	movq	584(%rax), %rax	# kvset_17(D)->head, _9
	movl	36(%rax), %eax	# _9->bad_states.states.corrupted, _10
	testl	%eax, %eax	# _10
	je	.L64	#,
# locks.c:149: 		BAD_STATES_CHECK(kvset);
	movl	$-2147483136, %eax	#, _11
	jmp	.L66	#
.L64:
# locks.c:150: 		return ERROR_IMPOSSIBLE_OPERATION;
	movl	$-2147481600, %eax	#, _11
	jmp	.L66	#
.L63:
# locks.c:153: 	int res = _common_processLock(kvset);
	movq	-24(%rbp), %rax	# kvset, tmp105
	movq	%rax, %rdi	# tmp105,
	call	_common_processLock	#
	movl	%eax, -4(%rbp)	# tmp106, res
# locks.c:154: 	if (!res) kvset->manual_locked = 1;
	cmpl	$0, -4(%rbp)	#, res
	jne	.L68	#,
# locks.c:154: 	if (!res) kvset->manual_locked = 1;
	movq	-24(%rbp), %rax	# kvset, tmp107
	movl	$1, 572(%rax)	#, kvset_17(D)->manual_locked
.L68:
# locks.c:155: 	return res;
	movl	-4(%rbp), %eax	# res, _11
.L66:
# locks.c:156: 	}
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE3704:
	.size	lck_manualLock, .-lck_manualLock
	.globl	lck_manualUnlock
	.type	lck_manualUnlock, @function
lck_manualUnlock:
.LFB3705:
	.cfi_startproc
	pushq	%rbp	#
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp	#,
	.cfi_def_cfa_register 6
	subq	$48, %rsp	#,
	movq	%rdi, -24(%rbp)	# index, index
	movl	%esi, -28(%rbp)	# commit, commit
	movq	%rdx, -40(%rbp)	# saved, saved
# locks.c:160: 	if (!index->manual_locked)
	movq	-24(%rbp), %rax	# index, tmp103
	movl	572(%rax), %eax	# index_23(D)->manual_locked, _1
	testl	%eax, %eax	# _1
	jne	.L70	#,
# locks.c:161: 		return ERROR_IMPOSSIBLE_OPERATION; // В том же потоке, что и блокировка. Разблокировка в другом - неопр. поведение
	movl	$-2147481600, %eax	#, _16
	jmp	.L71	#
.L70:
# locks.c:162: 	if (index->head->check_mutex)
	movq	-24(%rbp), %rax	# index, tmp104
	movq	584(%rax), %rax	# index_23(D)->head, _2
	movl	28(%rax), %eax	# _2->check_mutex, _3
	testl	%eax, %eax	# _3
	je	.L72	#,
# locks.c:164: 		__atomic_store_n(&index->manual_locked,0,__ATOMIC_SEQ_CST);
	movq	-24(%rbp), %rax	# index, tmp105
	addq	$572, %rax	#, _4
	movl	$0, %edx	#, tmp106
	movl	%edx, (%rax)	#, tmp106,* _4
	mfence
# locks.c:165: 		while (__atomic_load_n(&index->locks_count,__ATOMIC_SEQ_CST))
	jmp	.L73	#
.L74:
# /usr/lib/gcc/x86_64-linux-gnu/7/include/xmmintrin.h:1267:   __builtin_ia32_pause ();
	rep nop
.L73:
# locks.c:165: 		while (__atomic_load_n(&index->locks_count,__ATOMIC_SEQ_CST))
	movq	-24(%rbp), %rax	# index, tmp107
	addq	$568, %rax	#, _5
	movl	(%rax), %eax	#* _5, _6
	testl	%eax, %eax	# _6
	jne	.L74	#,
	jmp	.L75	#
.L72:
# locks.c:169: 		index->manual_locked = 0;
	movq	-24(%rbp), %rax	# index, tmp108
	movl	$0, 572(%rax)	#, index_23(D)->manual_locked
.L75:
# locks.c:171: 	if (commit)
	cmpl	$0, -28(%rbp)	#, commit
	je	.L76	#,
# locks.c:173: 		res = idx_flush(index);
	movq	-24(%rbp), %rax	# index, tmp109
	movq	%rax, %rdi	# tmp109,
	call	idx_flush@PLT	#
	movl	%eax, -4(%rbp)	# tmp110, res
# locks.c:174: 		pthread_mutex_unlock(&index->lock_set->process_lock);
	movq	-24(%rbp), %rax	# index, tmp111
	movq	608(%rax), %rax	# index_23(D)->lock_set, _7
	addq	$16, %rax	#, _8
	movq	%rax, %rdi	# _8,
	call	pthread_mutex_unlock@PLT	#
# locks.c:175: 		if (res < 0)
	cmpl	$0, -4(%rbp)	#, res
	jns	.L77	#,
# locks.c:176: 			return res;
	movl	-4(%rbp), %eax	# res, _16
	jmp	.L71	#
.L77:
# locks.c:177: 		if (saved) 
	cmpq	$0, -40(%rbp)	#, saved
	je	.L78	#,
# locks.c:178: 			*saved = res;
	movl	-4(%rbp), %edx	# res, res.2_9
	movq	-40(%rbp), %rax	# saved, tmp112
	movl	%edx, (%rax)	# res.2_9, *saved_36(D)
.L78:
# locks.c:179: 		return 0;
	movl	$0, %eax	#, _16
	jmp	.L71	#
.L76:
# locks.c:181: 	if (index->head->use_flags & UF_NOT_PERSISTENT)
	movq	-24(%rbp), %rax	# index, tmp113
	movq	584(%rax), %rax	# index_23(D)->head, _10
	movl	16(%rax), %eax	# _10->use_flags, _11
	andl	$1, %eax	#, _12
	testl	%eax, %eax	# _12
	je	.L79	#,
# locks.c:182: 		res = ERROR_IMPOSSIBLE_OPERATION;
	movl	$-2147481600, -4(%rbp)	#, res
	jmp	.L80	#
.L79:
# locks.c:184: 		res = idx_revert(index);
	movq	-24(%rbp), %rax	# index, tmp114
	movq	%rax, %rdi	# tmp114,
	call	idx_revert@PLT	#
	movl	%eax, -4(%rbp)	# tmp115, res
.L80:
# locks.c:185: 	pthread_mutex_unlock(&index->lock_set->process_lock);
	movq	-24(%rbp), %rax	# index, tmp116
	movq	608(%rax), %rax	# index_23(D)->lock_set, _13
	addq	$16, %rax	#, _14
	movq	%rax, %rdi	# _14,
	call	pthread_mutex_unlock@PLT	#
# locks.c:186: 	return res;
	movl	-4(%rbp), %eax	# res, _16
.L71:
# locks.c:187: 	}
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE3705:
	.size	lck_manualUnlock, .-lck_manualUnlock
	.globl	_lck_chainLock
	.type	_lck_chainLock, @function
_lck_chainLock:
.LFB3706:
	.cfi_startproc
	pushq	%rbp	#
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp	#,
	.cfi_def_cfa_register 6
	movq	%rdi, -24(%rbp)	# index, index
	movl	%esi, -28(%rbp)	# hash, hash
# locks.c:192: 	if (index->head->check_mutex)
	movq	-24(%rbp), %rax	# index, tmp103
	movq	584(%rax), %rax	# index_22(D)->head, _1
	movl	28(%rax), %eax	# _1->check_mutex, _2
	testl	%eax, %eax	# _2
	je	.L82	#,
# locks.c:194: lck_chainLock_repeat:
	nop
.L83:
# locks.c:195: 		while (!__atomic_load_n(&index->manual_locked,__ATOMIC_RELAXED))
	jmp	.L84	#
.L85:
# /usr/lib/gcc/x86_64-linux-gnu/7/include/xmmintrin.h:1267:   __builtin_ia32_pause ();
	rep nop
.L84:
# locks.c:195: 		while (!__atomic_load_n(&index->manual_locked,__ATOMIC_RELAXED))
	movq	-24(%rbp), %rax	# index, tmp104
	addq	$572, %rax	#, _3
	movl	(%rax), %eax	#* _3, _4
	testl	%eax, %eax	# _4
	je	.L85	#,
# locks.c:197: 		__atomic_fetch_add(&index->locks_count,1,__ATOMIC_SEQ_CST);
	movq	-24(%rbp), %rax	# index, tmp105
	addq	$568, %rax	#, _5
	lock addl	$1, (%rax)	#,,* _5
# locks.c:198: 		if (!__atomic_load_n(&index->manual_locked,__ATOMIC_SEQ_CST))
	movq	-24(%rbp), %rax	# index, tmp106
	addq	$572, %rax	#, _6
	movl	(%rax), %eax	#* _6, _7
	testl	%eax, %eax	# _7
	jne	.L82	#,
# locks.c:200: 			__atomic_fetch_sub(&index->locks_count,1,__ATOMIC_SEQ_CST);
	movq	-24(%rbp), %rax	# index, tmp107
	addq	$568, %rax	#, _8
	lock subl	$1, (%rax)	#,,* _8
# locks.c:201: 			goto lck_chainLock_repeat;
	jmp	.L83	#
.L82:
# locks.c:204: 	hash >>= 1;
	shrl	-28(%rbp)	# hash
# locks.c:205: 	unsigned num = hash / 64, bitmask = 1LL << (hash % 64);
	movl	-28(%rbp), %eax	# hash, tmp109
	shrl	$6, %eax	#, tmp108
	movl	%eax, -8(%rbp)	# tmp108, num
	movl	-28(%rbp), %eax	# hash, tmp110
	andl	$63, %eax	#, _9
	movl	$1, %edx	#, tmp111
	movl	%eax, %ecx	# _9, tmp122
	salq	%cl, %rdx	# tmp122, tmp111
	movq	%rdx, %rax	# tmp111, _10
	movl	%eax, -4(%rbp)	# _10, bitmask
# locks.c:206: 	while(__atomic_fetch_or(&index->lock_set->hash_locks[num],bitmask,__ATOMIC_ACQUIRE) & bitmask)
	jmp	.L86	#
.L88:
# /usr/lib/gcc/x86_64-linux-gnu/7/include/xmmintrin.h:1267:   __builtin_ia32_pause ();
	rep nop
.L86:
# locks.c:206: 	while(__atomic_fetch_or(&index->lock_set->hash_locks[num],bitmask,__ATOMIC_ACQUIRE) & bitmask)
	movl	-4(%rbp), %esi	# bitmask, _11
	movq	-24(%rbp), %rax	# index, tmp112
	movq	608(%rax), %rax	# index_22(D)->lock_set, _12
	movl	-8(%rbp), %edx	# num, tmp113
	addq	$6, %rdx	#, tmp114
	salq	$3, %rdx	#, tmp115
	addq	%rdx, %rax	# tmp115, tmp116
	leaq	8(%rax), %rdx	#, _13
	movq	(%rdx), %rax	#* _13, tmp119
.L87:
	movq	%rax, %rdi	# tmp117, _14
	movq	%rax, %rcx	# tmp117, tmp118
	orq	%rsi, %rcx	# _11, tmp118
	lock cmpxchgq	%rcx, (%rdx)	#, tmp118,* _13
	sete	%cl	#, tmp120
	testb	%cl, %cl	# tmp120
	je	.L87	#,
	movl	-4(%rbp), %eax	# bitmask, _15
	andq	%rdi, %rax	# _14, _16
	testq	%rax, %rax	# _16
	jne	.L88	#,
# locks.c:209: 	}
	nop
	popq	%rbp	#
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE3706:
	.size	_lck_chainLock, .-_lck_chainLock
	.globl	_lck_tryChainLock
	.type	_lck_tryChainLock, @function
_lck_tryChainLock:
.LFB3707:
	.cfi_startproc
	pushq	%rbp	#
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp	#,
	.cfi_def_cfa_register 6
	movq	%rdi, -24(%rbp)	# index, index
	movl	%esi, -28(%rbp)	# hash, hash
# locks.c:214: 	if (index->head->check_mutex)
	movq	-24(%rbp), %rax	# index, tmp109
	movq	584(%rax), %rax	# index_26(D)->head, _1
	movl	28(%rax), %eax	# _1->check_mutex, _2
	testl	%eax, %eax	# _2
	je	.L90	#,
# locks.c:216: lck_chainLock_repeat:
	nop
.L91:
# locks.c:217: 		while (!__atomic_load_n(&index->manual_locked,__ATOMIC_RELAXED))
	jmp	.L92	#
.L93:
# /usr/lib/gcc/x86_64-linux-gnu/7/include/xmmintrin.h:1267:   __builtin_ia32_pause ();
	rep nop
.L92:
# locks.c:217: 		while (!__atomic_load_n(&index->manual_locked,__ATOMIC_RELAXED))
	movq	-24(%rbp), %rax	# index, tmp110
	addq	$572, %rax	#, _3
	movl	(%rax), %eax	#* _3, _4
	testl	%eax, %eax	# _4
	je	.L93	#,
# locks.c:219: 		__atomic_fetch_add(&index->locks_count,1,__ATOMIC_SEQ_CST);
	movq	-24(%rbp), %rax	# index, tmp111
	addq	$568, %rax	#, _5
	lock addl	$1, (%rax)	#,,* _5
# locks.c:220: 		if (!__atomic_load_n(&index->manual_locked,__ATOMIC_SEQ_CST))
	movq	-24(%rbp), %rax	# index, tmp112
	addq	$572, %rax	#, _6
	movl	(%rax), %eax	#* _6, _7
	testl	%eax, %eax	# _7
	jne	.L90	#,
# locks.c:222: 			__atomic_fetch_sub(&index->locks_count,1,__ATOMIC_SEQ_CST);
	movq	-24(%rbp), %rax	# index, tmp113
	addq	$568, %rax	#, _8
	lock subl	$1, (%rax)	#,,* _8
# locks.c:223: 			goto lck_chainLock_repeat;
	jmp	.L91	#
.L90:
# locks.c:226: 	hash >>= 1;
	shrl	-28(%rbp)	# hash
# locks.c:227: 	unsigned num = hash / 64, bitmask = 1LL << (hash % 64);
	movl	-28(%rbp), %eax	# hash, tmp115
	shrl	$6, %eax	#, tmp114
	movl	%eax, -12(%rbp)	# tmp114, num
	movl	-28(%rbp), %eax	# hash, tmp116
	andl	$63, %eax	#, _9
	movl	$1, %edx	#, tmp117
	movl	%eax, %ecx	# _9, tmp132
	salq	%cl, %rdx	# tmp132, tmp117
	movq	%rdx, %rax	# tmp117, _10
	movl	%eax, -8(%rbp)	# _10, bitmask
# locks.c:228: 	int res = (__atomic_fetch_or(&index->lock_set->hash_locks[num],bitmask,__ATOMIC_ACQUIRE) & bitmask) ? 0 : 1;
	movl	-8(%rbp), %esi	# bitmask, _11
	movq	-24(%rbp), %rax	# index, tmp118
	movq	608(%rax), %rax	# index_26(D)->lock_set, _12
	movl	-12(%rbp), %edx	# num, tmp119
	addq	$6, %rdx	#, tmp120
	salq	$3, %rdx	#, tmp121
	addq	%rdx, %rax	# tmp121, tmp122
	leaq	8(%rax), %rdx	#, _13
	movq	(%rdx), %rax	#* _13, tmp125
.L94:
	movq	%rax, %rdi	# tmp123, _14
	movq	%rax, %rcx	# tmp123, tmp124
	orq	%rsi, %rcx	# _11, tmp124
	lock cmpxchgq	%rcx, (%rdx)	#, tmp124,* _13
	sete	%cl	#, tmp126
	testb	%cl, %cl	# tmp126
	je	.L94	#,
	movl	-8(%rbp), %eax	# bitmask, _15
	andq	%rdi, %rax	# _14, _16
	testq	%rax, %rax	# _16
	sete	%al	#, _17
	movzbl	%al, %eax	# _17, tmp127
	movl	%eax, -4(%rbp)	# tmp127, res
# locks.c:229: 	if (index->head->check_mutex && !res)
	movq	-24(%rbp), %rax	# index, tmp128
	movq	584(%rax), %rax	# index_26(D)->head, _18
	movl	28(%rax), %eax	# _18->check_mutex, _19
	testl	%eax, %eax	# _19
	je	.L95	#,
# locks.c:229: 	if (index->head->check_mutex && !res)
	cmpl	$0, -4(%rbp)	#, res
	jne	.L95	#,
# locks.c:230: 		__atomic_fetch_sub(&index->locks_count,1,__ATOMIC_RELEASE);
	movq	-24(%rbp), %rax	# index, tmp129
	addq	$568, %rax	#, _20
	lock subl	$1, (%rax)	#,,* _20
.L95:
# locks.c:232: 	return res;
	movl	-4(%rbp), %eax	# res, _38
# locks.c:233: 	}
	popq	%rbp	#
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE3707:
	.size	_lck_tryChainLock, .-_lck_tryChainLock
	.globl	_lck_chainUnlock
	.type	_lck_chainUnlock, @function
_lck_chainUnlock:
.LFB3708:
	.cfi_startproc
	pushq	%rbp	#
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp	#,
	.cfi_def_cfa_register 6
	movq	%rdi, -24(%rbp)	# index, index
	movl	%esi, -28(%rbp)	# hash, hash
# locks.c:238: 	hash >>= 1;
	shrl	-28(%rbp)	# hash
# locks.c:239: 	unsigned num = hash / 64, bitmask = ~(1LL << (hash % 64));
	movl	-28(%rbp), %eax	# hash, tmp97
	shrl	$6, %eax	#, tmp96
	movl	%eax, -8(%rbp)	# tmp96, num
	movl	-28(%rbp), %eax	# hash, tmp98
	andl	$63, %eax	#, _1
	movl	$1, %edx	#, tmp99
	movl	%eax, %ecx	# _1, tmp109
	salq	%cl, %rdx	# tmp109, tmp99
	movq	%rdx, %rax	# tmp99, _2
	notl	%eax	# tmp100
	movl	%eax, -4(%rbp)	# tmp100, bitmask
# locks.c:240: 	__atomic_and_fetch(&index->lock_set->hash_locks[num],bitmask,__ATOMIC_RELEASE);
	movl	-4(%rbp), %eax	# bitmask, _4
	movq	-24(%rbp), %rdx	# index, tmp101
	movq	608(%rdx), %rdx	# index_16(D)->lock_set, _5
	movl	-8(%rbp), %ecx	# num, tmp102
	addq	$6, %rcx	#, tmp103
	salq	$3, %rcx	#, tmp104
	addq	%rcx, %rdx	# tmp104, tmp105
	addq	$8, %rdx	#, _6
	lock andq	%rax, (%rdx)	#, _4,* _6
# locks.c:242: 	if (index->head->check_mutex)
	movq	-24(%rbp), %rax	# index, tmp106
	movq	584(%rax), %rax	# index_16(D)->head, _7
	movl	28(%rax), %eax	# _7->check_mutex, _8
	testl	%eax, %eax	# _8
	je	.L99	#,
# locks.c:243: 		__atomic_fetch_sub(&index->locks_count,1,__ATOMIC_RELEASE);
	movq	-24(%rbp), %rax	# index, tmp107
	addq	$568, %rax	#, _9
	lock subl	$1, (%rax)	#,,* _9
.L99:
# locks.c:244: 	}
	nop
	popq	%rbp	#
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE3708:
	.size	_lck_chainUnlock, .-_lck_chainUnlock
	.globl	lck_waitForReaders
	.type	lck_waitForReaders, @function
lck_waitForReaders:
.LFB3709:
	.cfi_startproc
	pushq	%rbp	#
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp	#,
	.cfi_def_cfa_register 6
	movq	%rdi, -24(%rbp)	# locks, locks
# locks.c:249: 	unsigned seq_num = __atomic_add_fetch(&locks->rw_lock.parts.sequence,(unsigned)1,__ATOMIC_SEQ_CST);
	movq	-24(%rbp), %rax	# locks, tmp94
	addq	$4, %rax	#, _1
	movl	$1, %edx	#, tmp95
	lock xaddl	%edx, (%rax)	#, tmp95,* _1
	leal	1(%rdx), %eax	#, tmp96
	movl	%eax, -8(%rbp)	# tmp96, seq_num
# locks.c:250: 	unsigned rmask = 0xFFFF << (16 - (seq_num & 1) * 16);
	movl	-8(%rbp), %eax	# seq_num, tmp97
	andl	$1, %eax	#, _2
	testl	%eax, %eax	# _2
	jne	.L101	#,
# locks.c:250: 	unsigned rmask = 0xFFFF << (16 - (seq_num & 1) * 16);
	movl	$-65536, %eax	#, iftmp.3_10
	jmp	.L102	#
.L101:
# locks.c:250: 	unsigned rmask = 0xFFFF << (16 - (seq_num & 1) * 16);
	movl	$65535, %eax	#, iftmp.3_10
.L102:
# locks.c:250: 	unsigned rmask = 0xFFFF << (16 - (seq_num & 1) * 16);
	movl	%eax, -4(%rbp)	# iftmp.3_10, rmask
# locks.c:253: 	unsigned tryCnt = 0;
	movl	$0, -12(%rbp)	#, tryCnt
# locks.c:254: 	while (tryCnt < RW_TIMEOUT * 1000 && (result = __atomic_load_n(&locks->rw_lock.parts.counters.both,__ATOMIC_SEQ_CST) & rmask))
	jmp	.L103	#
.L105:
# /usr/lib/gcc/x86_64-linux-gnu/7/include/xmmintrin.h:1267:   __builtin_ia32_pause ();
	rep nop
# locks.c:257: 		tryCnt++;
	addl	$1, -12(%rbp)	#, tryCnt
.L103:
# locks.c:254: 	while (tryCnt < RW_TIMEOUT * 1000 && (result = __atomic_load_n(&locks->rw_lock.parts.counters.both,__ATOMIC_SEQ_CST) & rmask))
	cmpl	$499999, -12(%rbp)	#, tryCnt
	ja	.L104	#,
# locks.c:254: 	while (tryCnt < RW_TIMEOUT * 1000 && (result = __atomic_load_n(&locks->rw_lock.parts.counters.both,__ATOMIC_SEQ_CST) & rmask))
	movq	-24(%rbp), %rax	# locks, _3
	movl	(%rax), %eax	#* _3, _4
	andl	-4(%rbp), %eax	# rmask, tmp99
	movl	%eax, -16(%rbp)	# tmp99, result
	cmpl	$0, -16(%rbp)	#, result
	jne	.L105	#,
.L104:
# locks.c:259: 	if (result)
	cmpl	$0, -16(%rbp)	#, result
	je	.L107	#,
# locks.c:262: 		__atomic_and_fetch(&locks->rw_lock.parts.counters.both,(unsigned)~rmask,__ATOMIC_SEQ_CST);
	movl	-4(%rbp), %eax	# rmask, tmp100
	notl	%eax	# tmp100
	movl	%eax, %edx	# tmp100, _5
	movq	-24(%rbp), %rax	# locks, _6
	lock andl	%edx, (%rax)	#, _5,* _6
.L107:
# locks.c:264: 	}
	nop
	popq	%rbp	#
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE3709:
	.size	lck_waitForReaders, .-lck_waitForReaders
	.type	_check_reader_lock, @function
_check_reader_lock:
.LFB3710:
	.cfi_startproc
	pushq	%rbp	#
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp	#,
	.cfi_def_cfa_register 6
	movq	%rdi, -8(%rbp)	# lock, lock
	movq	%rsi, -16(%rbp)	# rlock, rlock
# locks.c:268: 	if (!(lock.parts.counters.counter[rlock->readerSeq & 1])) return 0;
	movq	-16(%rbp), %rax	# rlock, tmp97
	movl	4(%rax), %eax	# rlock_11(D)->readerSeq, _1
	andl	$1, %eax	#, _2
	movl	%eax, %eax	# _2, tmp98
	movzwl	-8(%rbp,%rax,2), %eax	# lock.parts.counters.counter, _3
	testw	%ax, %ax	# _3
	jne	.L109	#,
# locks.c:268: 	if (!(lock.parts.counters.counter[rlock->readerSeq & 1])) return 0;
	movl	$0, %eax	#, _9
	jmp	.L110	#
.L109:
# locks.c:269: 	if (rlock->readerSeq != lock.parts.sequence && rlock->readerSeq + 1 != lock.parts.sequence) return 0;
	movq	-16(%rbp), %rax	# rlock, tmp99
	movl	4(%rax), %edx	# rlock_11(D)->readerSeq, _4
	movl	-4(%rbp), %eax	# lock.parts.sequence, _5
	cmpl	%eax, %edx	# _5, _4
	je	.L111	#,
# locks.c:269: 	if (rlock->readerSeq != lock.parts.sequence && rlock->readerSeq + 1 != lock.parts.sequence) return 0;
	movq	-16(%rbp), %rax	# rlock, tmp100
	movl	4(%rax), %eax	# rlock_11(D)->readerSeq, _6
	leal	1(%rax), %edx	#, _7
	movl	-4(%rbp), %eax	# lock.parts.sequence, _8
	cmpl	%eax, %edx	# _8, _7
	je	.L111	#,
# locks.c:269: 	if (rlock->readerSeq != lock.parts.sequence && rlock->readerSeq + 1 != lock.parts.sequence) return 0;
	movl	$0, %eax	#, _9
	jmp	.L110	#
.L111:
# locks.c:270: 	return 1;
	movl	$1, %eax	#, _9
.L110:
# locks.c:271: 	}
	popq	%rbp	#
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE3710:
	.size	_check_reader_lock, .-_check_reader_lock
	.globl	lck_readerLock
	.type	lck_readerLock, @function
lck_readerLock:
.LFB3711:
	.cfi_startproc
	pushq	%rbp	#
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp	#,
	.cfi_def_cfa_register 6
	subq	$48, %rsp	#,
	movq	%rdi, -40(%rbp)	# locks, locks
	movq	%rsi, -48(%rbp)	# rlock, rlock
# locks.c:274: 	{
	movq	%fs:40, %rax	#, tmp107
	movq	%rax, -8(%rbp)	# tmp107, D.24283
	xorl	%eax, %eax	# tmp107
# locks.c:277: 	if (rlock->keeped) return;
	movq	-48(%rbp), %rax	# rlock, tmp99
	movzwl	2(%rax), %eax	# rlock_17(D)->keeped, _1
	testw	%ax, %ax	# _1
	jne	.L119	#,
# locks.c:278: 	lock.fullValue = __atomic_load_n(&locks->rw_lock.fullValue,__ATOMIC_RELAXED); // We omit one LOCK prefix in exchage for possible exra iteration
	movq	-40(%rbp), %rax	# locks, _2
	movq	(%rax), %rax	#* _2, _3
	movq	%rax, -24(%rbp)	# _3, lock.fullValue
.L115:
# locks.c:280: 	newstate = lock;
	movq	-24(%rbp), %rax	# lock, tmp100
	movq	%rax, -16(%rbp)	# tmp100, newstate
# locks.c:281: 	newstate.parts.counters.counter[lock.parts.sequence & 1]++;
	movl	-20(%rbp), %eax	# lock.parts.sequence, _4
	andl	$1, %eax	#, _4
	movl	%eax, %edx	# _4, _5
	movl	%edx, %eax	# _5, tmp101
	movzwl	-16(%rbp,%rax,2), %eax	# newstate.parts.counters.counter, _6
	addl	$1, %eax	#, _8
	movl	%edx, %edx	# _5, tmp102
	movw	%ax, -16(%rbp,%rdx,2)	# _8, newstate.parts.counters.counter
# locks.c:282: 	if (!__atomic_compare_exchange_n(&locks->rw_lock.fullValue, &lock.fullValue, newstate.fullValue, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
	movq	-16(%rbp), %rcx	# newstate.fullValue, _9
	movq	-40(%rbp), %rsi	# locks, _10
	leaq	-24(%rbp), %rdx	#, tmp103
	movq	(%rdx), %rax	#, tmp104
	lock cmpxchgq	%rcx, (%rsi)	#, _9,* _10
	movq	%rax, %rcx	# tmp104, tmp105
	sete	%al	#, _11
	testb	%al, %al	# _11
	jne	.L116	#,
	movq	%rcx, (%rdx)	# tmp105,
.L116:
	xorl	$1, %eax	#, _12
	testb	%al, %al	# _12
	je	.L117	#,
# locks.c:283: 		goto lck_readerLock_retry;
	jmp	.L115	#
.L117:
# locks.c:284: 	rlock->readerSeq = lock.parts.sequence;
	movl	-20(%rbp), %edx	# lock.parts.sequence, _13
	movq	-48(%rbp), %rax	# rlock, tmp106
	movl	%edx, 4(%rax)	# _13, rlock_17(D)->readerSeq
	jmp	.L112	#
.L119:
# locks.c:277: 	if (rlock->keeped) return;
	nop
.L112:
# locks.c:285: 	}
	movq	-8(%rbp), %rax	# D.24283, tmp108
	xorq	%fs:40, %rax	#, tmp108
	je	.L118	#,
	call	__stack_chk_fail@PLT	#
.L118:
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE3711:
	.size	lck_readerLock, .-lck_readerLock
	.globl	lck_readerCheck
	.type	lck_readerCheck, @function
lck_readerCheck:
.LFB3712:
	.cfi_startproc
	pushq	%rbp	#
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp	#,
	.cfi_def_cfa_register 6
	subq	$32, %rsp	#,
	movq	%rdi, -24(%rbp)	# locks, locks
	movq	%rsi, -32(%rbp)	# rlock, rlock
# locks.c:288: 	{
	movq	%fs:40, %rax	#, tmp94
	movq	%rax, -8(%rbp)	# tmp94, D.24284
	xorl	%eax, %eax	# tmp94
# locks.c:290: 	lock.fullValue = __atomic_load_n(&locks->rw_lock.fullValue,__ATOMIC_RELAXED); // This call is for infinite loop breaking, no LOCK is ok
	movq	-24(%rbp), %rax	# locks, _1
	movq	(%rax), %rax	#* _1, _2
	movq	%rax, -16(%rbp)	# _2, lock.fullValue
# locks.c:291: 	return _check_reader_lock(lock,rlock);
	movq	-32(%rbp), %rdx	# rlock, tmp91
	movq	-16(%rbp), %rax	# lock, tmp92
	movq	%rdx, %rsi	# tmp91,
	movq	%rax, %rdi	# tmp92,
	call	_check_reader_lock	#
# locks.c:292: 	}
	movq	-8(%rbp), %rcx	# D.24284, tmp95
	xorq	%fs:40, %rcx	#, tmp95
	je	.L122	#,
	call	__stack_chk_fail@PLT	#
.L122:
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE3712:
	.size	lck_readerCheck, .-lck_readerCheck
	.globl	lck_readerUnlock
	.type	lck_readerUnlock, @function
lck_readerUnlock:
.LFB3713:
	.cfi_startproc
	pushq	%rbp	#
	.cfi_def_cfa_offset 16
	.cfi_offset 6, -16
	movq	%rsp, %rbp	#,
	.cfi_def_cfa_register 6
	subq	$48, %rsp	#,
	movq	%rdi, -40(%rbp)	# locks, locks
	movq	%rsi, -48(%rbp)	# rlock, rlock
# locks.c:295: 	{
	movq	%fs:40, %rax	#, tmp143
	movq	%rax, -8(%rbp)	# tmp143, D.24285
	xorl	%eax, %eax	# tmp143
# locks.c:298: 	lock.fullValue = __atomic_load_n(&locks->rw_lock.fullValue,__ATOMIC_SEQ_CST);
	movq	-40(%rbp), %rax	# locks, _1
	movq	(%rax), %rax	#* _1, _2
	movq	%rax, -24(%rbp)	# _2, lock.fullValue
# locks.c:299: 	if (!_check_reader_lock(lock,rlock))
	movq	-48(%rbp), %rdx	# rlock, tmp117
	movq	-24(%rbp), %rax	# lock, tmp118
	movq	%rdx, %rsi	# tmp117,
	movq	%rax, %rdi	# tmp118,
	call	_check_reader_lock	#
	testl	%eax, %eax	# _3
	jne	.L124	#,
# locks.c:300: 		return rlock->keeped = 0,0;
	movq	-48(%rbp), %rax	# rlock, tmp119
	movw	$0, 2(%rax)	#, rlock_42(D)->keeped
	movl	$0, %eax	#, _33
	jmp	.L133	#
.L124:
# locks.c:301: 	newstate = lock;
	movq	-24(%rbp), %rax	# lock, tmp120
	movq	%rax, -16(%rbp)	# tmp120, newstate
# locks.c:302: 	if (rlock->keep)
	movq	-48(%rbp), %rax	# rlock, tmp121
	movzwl	(%rax), %eax	# rlock_42(D)->keep, _4
	testw	%ax, %ax	# _4
	je	.L126	#,
# locks.c:304: 		rlock->keeped = 1;
	movq	-48(%rbp), %rax	# rlock, tmp122
	movw	$1, 2(%rax)	#, rlock_42(D)->keeped
# locks.c:305: 		if (rlock->readerSeq == lock.parts.sequence)
	movq	-48(%rbp), %rax	# rlock, tmp123
	movl	4(%rax), %edx	# rlock_42(D)->readerSeq, _5
	movl	-20(%rbp), %eax	# lock.parts.sequence, _6
	cmpl	%eax, %edx	# _6, _5
	jne	.L127	#,
# locks.c:306: 			return 1;
	movl	$1, %eax	#, _33
	jmp	.L133	#
.L127:
# locks.c:307: 		newstate.parts.counters.counter[lock.parts.sequence & 1]++;
	movl	-20(%rbp), %eax	# lock.parts.sequence, _7
	andl	$1, %eax	#, _7
	movl	%eax, %edx	# _7, _8
	movl	%edx, %eax	# _8, tmp124
	movzwl	-16(%rbp,%rax,2), %eax	# newstate.parts.counters.counter, _9
	addl	$1, %eax	#, _11
	movl	%edx, %edx	# _8, tmp125
	movw	%ax, -16(%rbp,%rdx,2)	# _11, newstate.parts.counters.counter
.L126:
# locks.c:309: 	newstate.parts.counters.counter[rlock->readerSeq & 1]--;
	movq	-48(%rbp), %rax	# rlock, tmp126
	movl	4(%rax), %eax	# rlock_42(D)->readerSeq, _12
	andl	$1, %eax	#, _12
	movl	%eax, %edx	# _12, _13
	movl	%edx, %eax	# _13, tmp127
	movzwl	-16(%rbp,%rax,2), %eax	# newstate.parts.counters.counter, _14
	subl	$1, %eax	#, _16
	movl	%edx, %edx	# _13, tmp128
	movw	%ax, -16(%rbp,%rdx,2)	# _16, newstate.parts.counters.counter
# locks.c:310: 	while (!__atomic_compare_exchange_n(&locks->rw_lock.fullValue, &lock.fullValue, newstate.fullValue, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
	jmp	.L128	#
.L132:
# locks.c:312: 		if (!_check_reader_lock(lock,rlock))
	movq	-48(%rbp), %rdx	# rlock, tmp129
	movq	-24(%rbp), %rax	# lock, tmp130
	movq	%rdx, %rsi	# tmp129,
	movq	%rax, %rdi	# tmp130,
	call	_check_reader_lock	#
	testl	%eax, %eax	# _17
	jne	.L129	#,
# locks.c:313: 			return rlock->keeped = 0,0;
	movq	-48(%rbp), %rax	# rlock, tmp131
	movw	$0, 2(%rax)	#, rlock_42(D)->keeped
	movl	$0, %eax	#, _33
	jmp	.L133	#
.L129:
# locks.c:314: 		newstate = lock;
	movq	-24(%rbp), %rax	# lock, tmp132
	movq	%rax, -16(%rbp)	# tmp132, newstate
# locks.c:315: 		if (rlock->keep)
	movq	-48(%rbp), %rax	# rlock, tmp133
	movzwl	(%rax), %eax	# rlock_42(D)->keep, _18
	testw	%ax, %ax	# _18
	je	.L130	#,
# locks.c:316: 			newstate.parts.counters.counter[lock.parts.sequence & 1]++;
	movl	-20(%rbp), %eax	# lock.parts.sequence, _19
	andl	$1, %eax	#, _19
	movl	%eax, %edx	# _19, _20
	movl	%edx, %eax	# _20, tmp134
	movzwl	-16(%rbp,%rax,2), %eax	# newstate.parts.counters.counter, _21
	addl	$1, %eax	#, _23
	movl	%edx, %edx	# _20, tmp135
	movw	%ax, -16(%rbp,%rdx,2)	# _23, newstate.parts.counters.counter
.L130:
# locks.c:317: 		newstate.parts.counters.counter[rlock->readerSeq & 1]--;
	movq	-48(%rbp), %rax	# rlock, tmp136
	movl	4(%rax), %eax	# rlock_42(D)->readerSeq, _24
	andl	$1, %eax	#, _24
	movl	%eax, %edx	# _24, _25
	movl	%edx, %eax	# _25, tmp137
	movzwl	-16(%rbp,%rax,2), %eax	# newstate.parts.counters.counter, _26
	subl	$1, %eax	#, _28
	movl	%edx, %edx	# _25, tmp138
	movw	%ax, -16(%rbp,%rdx,2)	# _28, newstate.parts.counters.counter
.L128:
# locks.c:310: 	while (!__atomic_compare_exchange_n(&locks->rw_lock.fullValue, &lock.fullValue, newstate.fullValue, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST))
	movq	-16(%rbp), %rcx	# newstate.fullValue, _29
	movq	-40(%rbp), %rsi	# locks, _30
	leaq	-24(%rbp), %rdx	#, tmp139
	movq	(%rdx), %rax	#, tmp140
	lock cmpxchgq	%rcx, (%rsi)	#, _29,* _30
	movq	%rax, %rcx	# tmp140, tmp141
	sete	%al	#, _31
	testb	%al, %al	# _31
	jne	.L131	#,
	movq	%rcx, (%rdx)	# tmp141,
.L131:
	xorl	$1, %eax	#, _32
	testb	%al, %al	# _32
	jne	.L132	#,
# locks.c:319: 	return 1;
	movl	$1, %eax	#, _33
.L133:
# locks.c:320: 	}
	movq	-8(%rbp), %rdi	# D.24285, tmp144
	xorq	%fs:40, %rdi	#, tmp144
	je	.L134	#,
# locks.c:320: 	}
	call	__stack_chk_fail@PLT	#
.L134:
	leave
	.cfi_def_cfa 7, 8
	ret
	.cfi_endproc
.LFE3713:
	.size	lck_readerUnlock, .-lck_readerUnlock
	.ident	"GCC: (Ubuntu 7.4.0-1ubuntu1~18.04.1) 7.4.0"
	.section	.note.GNU-stack,"",@progbits
