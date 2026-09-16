/* 32-bit ABI: leave _BITS64 undefined */

#if !defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN)
#define	__BYTE_ORDER	__LITTLE_ENDIAN
#endif

#if !defined(BYTE_ORDER) && defined(LITTLE_ENDIAN)
#define	BYTE_ORDER	LITTLE_ENDIAN
#endif
