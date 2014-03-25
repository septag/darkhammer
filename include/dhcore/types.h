/***********************************************************************************
 * Copyright (c) 2012, Sepehr Taghdisian
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 ***********************************************************************************/


#ifndef __TYPES_H__
#define __TYPES_H__

#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "error-codes.h"

/* Compiler macros
 * _MSVC_: Microsoft visual-c compiler
 * _GNUC_: GNUC compiler
 */
#if _MSC_VER
#define _MSVC_
#endif

#if __GNUC__
#define _GNUC_
#endif

#if defined(_DEBUG) && !defined(_DEBUG_)
#define _DEBUG_
#endif

#if defined(WIN32) && !defined(_WIN_)
#define _WIN_
#endif

#if defined(_WINDLL) && !defined(_WIN_)
#define _WIN_
#endif

#if defined(SWIG) && defined(SWIGWIN) && !defined(_WIN_)
#define _WIN_
#endif

#if (defined(__APPLE__) || defined(DARWIN)) && !defined(_OSX_)
#define _OSX_
#endif

#if defined(linux) && !defined(_LINUX_)
#define _LINUX_
#endif

#if (!defined(_OSX_) && !defined(_WIN_) && !defined(_LINUX_)) && !defined(SWIG)
#error "Platform is not defined, use macros _WIN_, _LINUX_ or _OSX_"
#endif

/* check for unsupported compilers */
#if !defined(_MSVC_) && !defined(_GNUC_) && !defined(SWIG)
#error "Compile Error: Unsupported compiler."
#endif

/* Platform macros
 * _x86_: 32bit intel architecture
 * _x64_: 64bit AMD architecture
 */
#if (defined(_M_IX86) || defined(__i386__)) && !defined(_X86_)
#define _X86_
#endif

#if (defined(_M_X64) || defined(__X86_64__) || defined(__LP64__) || defined(_LP64)) && \
    !defined(_X64_)
#define _X64_
#endif

#if defined(__arm__)
#define _ARM_
    #if defined(__ARM_ARCH) && __ARM_ARCH == 6
    #define _ARM6_
    #else
    #error "Unsupported ARM architecture"
    #endif
#endif

#if (!defined(_X86_) && !defined(_X64_) && !defined(_ARM_)) && !defined(SWIG)
#error "CPU architecture is unknown"
#endif

/* assume that linux, osx, bsd have posix libraries */
#if (defined(_LINUX_) || defined(_OSX_)) && !defined(_POSIXLIB_)
#define _POSIXLIB_
#endif

#if (defined(_DEBUG) || defined(__Debug__)) && !defined(_DEBUG_)
#define _DEBUG_
#endif


/* On the default msvc compiler, I use /TP flag which compiles all files as CPP
 * But under ICC we use the common C99 feature (compile in C)
 * So this macro is defined for msvc compiler only, so we can ignore extern "C" in headers
 * On my compiler, only MSVC x64 compiler rasises extern "C" symbol errors (not the x86 one).
 */
#if defined(_MSVC_) && !defined(__INTEL_COMPILER)
#define _CPP_IS_FORCED_
#endif

#if defined(__cplusplus)
    #if !defined(_CPP_IS_FORCED_)
        #define _EXTERN_ extern "C"
        #define _EXTERN_BEGIN_ extern "C" {
        #define _EXTERN_END_ }
    #else
        #define _EXTERN_
        #define _EXTERN_BEGIN_
        #define _EXTERN_END_
    #endif
    #define _EXTERN_EXPORT_ extern "C" 
#else
#define _EXTERN_
#define _EXTERN_BEGIN_
#define _EXTERN_END_
#define _EXTERN_EXPORT_
#endif

/* common data-type defs */
typedef int int32;
typedef long long int int64;
typedef short int16;
typedef char int8;
typedef unsigned int uint;
typedef unsigned int uint32;
typedef unsigned short uint16;
typedef unsigned char uint8;
typedef unsigned long long int uint64;
typedef float fl32;
typedef double fl64;
typedef wchar_t wchar;
typedef int result_t;
typedef uint64 reshandle_t;

/* pointer type (64/32 platforms are different) */
#if defined(_X64_)
typedef uint64 uptr_t;
#else
typedef uint uptr_t;
#endif

/* bool_t value for non-cplusplus apps */
typedef uint bool_t;

/* TRUE/FALSE definitions */
#ifndef TRUE
#define TRUE    1
#endif
#ifndef FALSE
#define FALSE   0
#endif

/* NULL */
#ifndef NULL
#define NULL    0x0
#endif

/* maximum/minimum definitions */
#if !defined(_WIN_)
#if defined(UINT64_MAX)
#undef UINT64_MAX
#endif
#if defined(UINT64_MAX)
#undef UINT64_MAX
#endif
#if defined(UINT64_MAX)
#undef UINT64_MAX
#endif
#if defined(UINT32_MAX)
#undef UINT32_MAX
#endif
#if defined(UINT16_MAX)
#undef UINT16_MAX
#endif
#if defined(UINT8_MAX)
#undef UINT8_MAX
#endif
#if defined(INT64_MAX)
#undef INT64_MAX
#endif
#if defined(INT64_MAX)
#undef INT64_MAX
#endif
#if defined(INT64_MAX)
#undef INT64_MAX
#endif
#if defined(INT32_MAX)
#undef INT32_MAX
#endif
#if defined(INT16_MAX)
#undef INT16_MAX
#endif
#if defined(INT8_MAX)
#undef INT8_MAX
#endif
#if defined(INT64_MIN)
#undef INT64_MIN
#endif
#if defined(INT64_MIN)
#undef INT64_MIN
#endif
#if defined(INT64_MIN)
#undef INT64_MIN
#endif
#if defined(INT32_MIN)
#undef INT32_MIN
#endif
#if defined(INT16_MIN)
#undef INT16_MIN
#endif
#if defined(INT8_MIN)
#undef INT8_MIN
#endif

#define UINT64_MAX              (0xffffffffffffffff)
#define UINT32_MAX              (0xffffffff)
#define UINT16_MAX              (0xffff)
#define UINT8_MAX               (0xff)
#define INT64_MAX               (9223372036854775807)
#define INT64_MIN               (-9223372036854775807-1)
#define INT32_MAX               (2147483647)
#define INT32_MIN               (-2147483647-1)
#define INT16_MAX               (32767)
#define INT16_MIN               (-32768)
#define INT8_MAX                127
#define INT8_MIN                -127
#else
#define __STDC_LIMIT_MACROS
#include <stdint.h>
#endif

#define FL64_MAX                (1.7976931348623158e+308)
#define FL64_MIN                (2.2250738585072014e-308)
#define FL32_MAX                (3.402823466e+38f)
#define FL32_MIN                (1.175494351e-38f)


#if defined(_GNUC_)
#define _GCCPACKED_ __attribute__((packed))
#else
#define _GCCPACKED_
#endif

/* Version info structure, mostly used in file formats */
#pragma pack(push, 1)
struct _GCCPACKED_ version_info
{
    int major;
    int minor;
};
#pragma pack(pop)

#define VERSION_MAKE(vinfo, mJ, mN)   (vinfo).major = (mJ);     (vinfo).minor = (mN);
#define VERSION_CHECK(vinfo, mJ, mN)  (((vinfo).major == (mJ))&&((vinfo).minor == (mN)))

/* useful macros */
#define INVALID_INDEX       UINT32_MAX
#define INVALID_HANDLE      UINT64_MAX

/* inlining */
#if defined(_MSVC_)
#define INLINE          inline
#define FORCE_INLINE    INLINE
#endif

#if defined(_GNUC_)
#define INLINE          static inline
#define FORCE_INLINE    INLINE
#endif

/* bitwise operators */
#define BIT_CHECK(v, b)     (((v)&(b)) != 0)
#define BIT_ADD(v, b)       ((v) |= (b))
#define BIT_REMOVE(v, b)    ((v) &= ~(b))

/* out/in/INOUT for readability */
#if defined(OUT)
#undef OUT
#endif

#if defined(OPTIONAL)
#undef OPTIONAL
#endif

#if defined(INOUT)
#undef INOUT
#endif

#define OUT
#define INOUT
#define OPTIONAL

#if defined(_GNUC_)
#define DEF_ALLOC inline
#else
#define DEF_ALLOC INLINE
#endif

/* maximum path string length */
#define DH_PATH_MAX  255

#endif /* __TYPES_H__ */
