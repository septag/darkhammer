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


#ifndef UTIL_H_
#define UTIL_H_

/**
 * @defgroup util Utility
 */

#include "types.h"
#include "core-api.h"
#include "allocator.h"

/* terminal ANSI colors for linux console only */
#if defined(_POSIXLIB_)
#define TERM_RESET   "\033[0m"
#define TERM_DIM     "\033[2m"
#define TERM_BLACK   "\033[30m"      /* Black */
#define TERM_GREY	 "\033[90m"		 /* Grey */
#define TERM_RED     "\033[31m"      /* Red */
#define TERM_GREEN   "\033[32m"      /* Green */
#define TERM_YELLOW  "\033[33m"      /* Yellow */
#define TERM_BLUE    "\033[34m"      /* Blue */
#define TERM_MAGENTA "\033[35m"      /* Magenta */
#define TERM_CYAN    "\033[36m"      /* Cyan */
#define TERM_WHITE   "\033[37m"      /* White */
#define TERM_BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define TERM_BOLDGREY 	 "\033[1m\033[90m"		/* Bold Grey */
#define TERM_BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define TERM_BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define TERM_BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define TERM_BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define TERM_BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define TERM_BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define TERM_BOLDWHITE   "\033[1m\033[37m"      /* Bold White */
#define TERM_DIMBLACK   "\033[2;30m"      /* Dim Black */
#define TERM_DIMRED     "\033[2;31m"      /* Dim Red */
#define TERM_DIMGREEN   "\033[2;32m"      /* Dim Green */
#define TERM_DIMYELLOW  "\033[2;33m"      /* Dim Yellow */
#define TERM_DIMBLUE    "\033[2;34m"      /* Dim Blue */
#define TERM_DIMMAGENTA "\033[2;35m"      /* Dim Magenta */
#define TERM_DIMCYAN    "\033[2;36m"      /* Dim Cyan */
#define TERM_DIMWHITE   "\033[2;37m"      /* Dim White */
#elif defined(_WIN_)
#define TERM_RESET ""
#define TERM_DIM ""
#define TERM_BLACK ""
#define TERM_GREY ""
#define TERM_RED ""
#define TERM_GREEN ""
#define TERM_YELLOW ""
#define TERM_BLUE ""
#define TERM_MAGENTA ""
#define TERM_CYAN ""
#define TERM_WHITE ""
#define TERM_BOLDBLACK ""
#define TERM_BOLDGREY ""
#define TERM_BOLDRED ""
#define TERM_BOLDGREEN ""
#define TERM_BOLDYELLOW ""
#define TERM_BOLDBLUE ""
#define TERM_BOLDMAGENTA ""
#define TERM_BOLDCYAN ""
#define TERM_BOLDWHITE ""
#define TERM_DIMBLACK   ""
#define TERM_DIMRED     ""
#define TERM_DIMGREEN   ""
#define TERM_DIMYELLOW  ""
#define TERM_DIMBLUE    ""
#define TERM_DIMMAGENTA ""
#define TERM_DIMCYAN    ""
#define TERM_DIMWHITE   ""
#endif

/**
 * runs system command and returns console output result in null-terminated string\n
 * note that returned string must be freed (FREE) after use
 * @ingroup util
 */
CORE_API char* util_runcmd(const char* cmd);

/**
 * returns current executable directory
 * @param outdir: preallocated output directory string
 * @param outsize: size of characters in 'outstr'
 * @return: outstr
 * @ingroup util
 */
CORE_API char* util_getexedir(char* outdir);

/**
 * stalls the program and gets a character from input
 * @ingroup util
 */
CORE_API char util_getch();

/**
 * returns current user profile directory \n
 * (/home/$user under linux, /My Documents under win)
 * @param outdir: preallocated output directory string
 * @return: outdir
 * @ingroup util
 */
CORE_API char* util_getuserdir(char* outdir);

/**
 * returns temp directory
 * @ingroup util
 */
CORE_API char* util_gettempdir(char* outdir);

/**
 * creates a directory
 * @return TRUE if successful
 * @ingroup util
 */
CORE_API bool_t util_makedir(const char* dir);

/**
 * copies a file from source to destination
 * @return TRUE if successful
 * @ingroup util
 */
CORE_API bool_t util_copyfile(const char* dest, const char* src);

/**
 * deletees a file from disk
 * @return TRUE if success
 * @ingroup util
 */
CORE_API bool_t util_delfile(const char* filepath);

/**
 * moves a file from source to destination path
 * @return TRUE if success
 * @ingroup util
 */
CORE_API bool_t util_movefile(const char* dest, const char* src);

/**
 * checks if specified path is a directory
 * @return TRUE if path is directory
 * @ingroup util
 */
CORE_API bool_t util_pathisdir(const char* path);

/**
 * stalls program and sleeps for N milliseconds
 * @param msecs number of milliseconds to sleep
 * @ingroup util
 */
CORE_API void util_sleep(uint msecs);

/**
 * loads a text file from disk and returns string buffer \n
 * @param filepath path of the text
 * @return null-terminated string containing file text, must be freed after use (use A_FREE)
 * @ingroup util
 */
CORE_API char* util_readtextfile(const char* txt_filepath, struct allocator* alloc);

#endif /* UTIL_H_ */
