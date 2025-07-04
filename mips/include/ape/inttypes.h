#ifndef _SUSV2_SOURCE
#error "inttypes.h is SUSV2"
#endif

#ifndef _INTTYPES_H_
#define _INTTYPES_H_ 1

#include "_apetypes.h"

#define    PRId8   "hhd"
#define    PRId16  "hd"
#define    PRId32  "d"
#define    PRId64  "lld"
#define    PRIdMAX "lld"

#define    PRIi8   "hhi"
#define    PRIi16  "hi"
#define    PRIi32  "i"
#define    PRIi64  "lli"
#define    PRIiMAX "lli"

#define    PRIu8   "hhu"
#define    PRIu16  "hu"
#define    PRIu32  "lu"
#define    PRIu64  "llu"
#define    PRIuMAX "llu"

#define    PRIo8   "hho"
#define    PRIo16  "ho"
#define    PRIo32  "o"
#define    PRIo64  "llo"
#define    PRIoMAX "llo"

#define    PRIx8   "hhx"
#define    PRIx16  "hx"
#define    PRIx32  "x"
#define    PRIx64  "llx"
#define    PRIxMAX "llx"

#define    PRIX8   "hhX"
#define    PRIX16  "hX"
#define    PRIX32  "X"
#define    PRIX64  "llX"
#define    PRIXMAX "llX"

#define    SCNoMAX "llo"
#define    SCNuMAX "llu"
#define    SCNxMAX "llx"

typedef char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef long _intptr_t;
typedef unsigned long _uintptr_t;
typedef _intptr_t intptr_t;
typedef _uintptr_t uintptr_t;
typedef int64_t intmax_t;
typedef uint64_t uintmax_t;

typedef unsigned char u8int;
typedef unsigned short u16int;
#ifndef _U32INT_
#define _U32INT_
typedef unsigned int u32int;
#endif
#ifndef _U64INT_
#define _U64INT_
typedef unsigned long long u64int;
#endif
typedef unsigned char u_int8_t;
typedef unsigned short u_int16_t;
typedef unsigned int u_int32_t;
typedef unsigned long long u_int64_t;

typedef char s8;
typedef short s16;
typedef int s32;
typedef long long s64;
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

#endif
