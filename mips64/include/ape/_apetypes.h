#ifndef _BITS64
#define _BITS64
#endif

#if !defined(__BYTE_ORDER) && defined(__BIG_ENDIAN)
#define	__BYTE_ORDER	__BIG_ENDIAN
#endif

#if !defined(BYTE_ORDER) && defined(BIG_ENDIAN)
#define	BYTE_ORDER	BIG_ENDIAN
#endif
