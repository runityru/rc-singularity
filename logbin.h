/*
 * Copyright (C) “Hostcomm” LLC
 * Copyright (C) Evgeniy Buevich
 * Contact email: singularity@nic.ru
 */

#ifndef _LOGBIN_H
#define _LOGBIN_H

// Возвращает округленный двоичный логарифм A (0-31, номер старшего установленного бита). Для A=0 не определена. 
#define LOG_BIN(A) ( 31 - __builtin_clz(A) )

// version for #if directive
#define LOG_BIN_MACRO(A) ( (A) >= 0x00010000 ? \
	( (A) >= 0x01000000 ? \
		( (A) >= 0x10000000 ? \
			( (A) >= 0x40000000 ? ( (A) >= 0x80000000 ? 31 : 30) : ( (A) >= 0x20000000 ? 29 : 28) ) : \
			( (A) >= 0x04000000 ? ( (A) >= 0x08000000 ? 27 : 26) : ( (A) >= 0x02000000 ? 25 : 24) ) ) : \
		( (A) >= 0x00100000 ? \
			( (A) >= 0x00400000 ? ( (A) >= 0x00800000 ? 23 : 22) : ( (A) >= 0x00200000 ? 21 : 20) ) : \
			( (A) >= 0x00040000 ? ( (A) >= 0x00080000 ? 19 : 18) : ( (A) >= 0x00020000 ? 17 : 16) ) ) ) : \
	( (A) >= 0x00000100 ? \
		( (A) >= 0x00001000 ? \
			( (A) >= 0x00004000 ? ( (A) >= 0x00008000 ? 15 : 14) : ( (A) >= 0x00002000 ? 13 : 12) ) : \
			( (A) >= 0x00000400 ? ( (A) >= 0x00000800 ? 11 : 10) : ( (A) >= 0x00000200 ?  9 :  8) ) ) : \
		( (A) >= 0x00000010 ? \
			( (A) >= 0x00000040 ? ( (A) >= 0x00000080 ?  7 :  6) : ( (A) >= 0x00000020 ?  5 :  4) ) : \
			( (A) >= 0x00000004 ? ( (A) >= 0x00000008 ?  3 :  2) : ( (A) >= 0x00000002 ?  1 :  0) ) ) ) )

#endif