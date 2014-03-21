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

/**
 * @defgroup err Error handling
 * Ehen calling internal functions, most of them will return 'result_t' type.\n
 * if error occurs within any function within engine, an error-stack will be created and result_t will not return RET_OK\n
 * To check if result_t is an error, use **IS_FAIL(r)** or **IS_OK(r)** macros.\n
 * Example:\n @code
 *  bool_t my_function()  {
 *      result_t r = some_engine_func();
 *      if (IS_FAIL(r))  {
 *           err_print(__FILE__, __LINE__, "Error occured");
 *           return FALSE;
 *      }
 *      return TRUE;
 *  }
 *
 *  void parent_function()   {
 *      if (!my_function()) {
 *          // send error to console and exit
 *          printf("Error: %s\n", err_getstring());
 *          exit(-1);
 *      }
 *  }
 * @endcode
 */

#ifndef __ERR_H__
#define __ERR_H__

#include "types.h"
#include "core-api.h"

#if defined(_ENABLEASSERT_)

#if defined(ASSERT)
#undef ASSERT
#endif

#if defined(_MSVC_)
#define DEBUG_BREAK()   __debugbreak();
#elif defined(_GNUC_)
#define DEBUG_BREAK()   __builtin_trap();
#endif
#define ASSERT(expr)    \
    if (!(expr))    {   err_reportassert(#expr, __FILE__, __LINE__);     DEBUG_BREAK();  }
#else
#define ASSERT(expr)
#endif

/* Assertion */
CORE_API void err_reportassert(const char* expr, const char* source, uint line);

/**
 * Error handling initialization
 * @ingroup err
 */
CORE_API result_t err_init();
/**
 * error handling release
 * @ingroup err
 */
CORE_API void err_release();

/**
 * print formatted and add item to error-stack
 * @param source source file of error occurance
 * @param line line of error occurance
 * @param fmt formatted string
 * @ingroup err
 */
CORE_API void err_printf(const char* source, uint line, const char* fmt, ...);
/**
 * print text and add item to error-stack
 * @param source source file of error occurance
 * @param line line of error occurance
 * @param text error string
 * @ingroup err
 */
CORE_API void err_print(const char* source, uint line, const char* text);

/**
 * print common error code descriptions to the error-stack
 * @param source source file of error occurance
 * @param line line of error occurance
 * @param err_code error code, usually is casted from result_t
 * @ingroup err
 */
CORE_API result_t err_printn(const char* source, uint line, uint err_code);

/**
 * returns last error code, and **does not clear** error-stack
 * @ingroup err
 */
CORE_API uint err_getcode();

/**
 * sends error descriptions and call-stack to logger, **clears** the error-stack after call
 * @param as_warning send error as warning
 * @ingroup err
 */
CORE_API void err_sendtolog(bool_t as_warning);
/**
 * returns error descriptions and call-stack to the caller, **clears** the error-stack after call
 * @return string buffer
 * @see err_sendtolog   @ingroup err
 */
CORE_API const char* err_getstring();
/**
 * clears error-stack without output to anything
 * @see err_sendtolog @see err_getstring @ingroup err
 */
CORE_API void err_clear();
/**
 * checks if we have errors in the error-stack @ingroup err
 */
CORE_API bool_t err_haserrors();

#endif /* __ERR_H__ */
