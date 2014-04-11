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


#ifndef __LOG_H__
#define __LOG_H__

#include "types.h"
#include "core-api.h"

/**
 * @defgroup log Logger
 * Performs engine logging to console, file, debugger or any custom implementation
 */
 
 /**
 * @ingroup log
 */
enum log_type
{
    LOG_TEXT = 0,
    LOG_ERROR = 1,
    LOG_WARNING = 2,
    LOG_INFO = 3,
    LOG_LOAD = 4
};

/**
 * @ingroup log
 */
struct log_stats
{
    long volatile msgs_cnt;
    long volatile errors_cnt;
    long volatile warnings_cnt;
};

/**
 * custom log function callback \n
 * @see log_outputfunc  @ingroup log
 */
typedef void (*pfn_log_handler)(enum log_type /*type*/, const char* /*text*/, void* /*param*/);

/* set output options of the logger 
 **
 * set log output to console
 * @ingroup log
 */
CORE_API result_t log_outputconsole(int enable);
/**
 * set log output to text file
 * @ingroup log
 */
CORE_API result_t log_outputfile(int enable, const char* log_filepath);
/**
 * set log output to debugger
 * @ingroup log
 */
CORE_API result_t log_outputdebugger(int enable);
/**
 * set log output to custom function
 * @ingroup log
 */
CORE_API result_t log_outputfunc(int enable, pfn_log_handler log_fn, void* param);

/* check output options of logger 
 **
 * checks if log output is console
 * @ingroup log
 */
CORE_API int log_isconsole();
/**
 * checks if log output is text file
 * @ingroup log
 */
CORE_API int log_isfile();
/**
 * checks if log output is debugger
 * @ingroup log
 */
CORE_API int log_isdebugger();

/**
 * checks if log output is custom function
 * @ingroup log
 */
CORE_API int log_isoutputfunc();

/**
 * print text to the logger
 * @param type type of log message (@see log_type)
 * @ingroup log
 */
CORE_API void log_print(enum log_type type, const char* text);
/**
 * print formatted text to the logger
 * @param type type of log message (@see log_type)
 * @ingroup log
 */
CORE_API void log_printf(enum log_type type, const char* fmt, ...);

/**
 * get log statistics, count number of errors/warnings/... 
 * @see log_stats   @ingroup log
 */
CORE_API void log_getstats(struct log_stats* stats);

/* init/release log 
 **
 * @ingroup log
 */
CORE_API result_t log_init();
/**
 * @ingroup log
 */
CORE_API void log_release();

#endif /*__LOG_H__*/
