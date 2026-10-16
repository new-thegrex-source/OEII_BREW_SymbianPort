/* Drop-in replacement for the S60 3rd Open C stdint.h.
 * That header typedefs int8_t from __int8_t, which GCCE 3.4.3 never defines,
 * so every file that includes it fails. ..\inc is the first SYSTEMINCLUDE, so
 * <stdint.h> resolves here and the broken one in epoc32\include\stdapis is
 * never opened. */
#ifndef OE_S60_STDINT_H
#define OE_S60_STDINT_H
#define _STDINT_H_
#define _STDINT_H
#define __STDINT_H

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef signed long long int64_t;
typedef unsigned long long uint64_t;

typedef int8_t int_least8_t;
typedef int16_t int_least16_t;
typedef int32_t int_least32_t;
typedef int64_t int_least64_t;
typedef uint8_t uint_least8_t;
typedef uint16_t uint_least16_t;
typedef uint32_t uint_least32_t;
typedef uint64_t uint_least64_t;

typedef int8_t int_fast8_t;
typedef int32_t int_fast16_t;
typedef int32_t int_fast32_t;
typedef int64_t int_fast64_t;
typedef uint8_t uint_fast8_t;
typedef uint32_t uint_fast16_t;
typedef uint32_t uint_fast32_t;
typedef uint64_t uint_fast64_t;

typedef int32_t intptr_t;
typedef uint32_t uintptr_t;
typedef int64_t intmax_t;
typedef uint64_t uintmax_t;

#endif
