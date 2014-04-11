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

#ifndef __STR_H__
#define __STR_H__

#include "types.h"
#include "core-api.h"

#if defined(_MSVC_)
#define snprintf _snprintf
#endif

/**
 * @defgroup str Strings
 * string utility functions\n
 * most string/path functions get output string as paramter and returns result as a const return value\n
 * all functions can be operated on single string buffer. example: return path_getfilename(path, path);
 * @ingroup str
 */

/**
 * checks equality of two strings - case sensitive
 * @ingroup str
 */
CORE_API int str_isequal(const char* str1, const char* str2);
/**
 * checks equality of two strings - case insensitive
 * @ingroup str
 */
CORE_API int str_isequal_nocase(const char* str1, const char* str2);
/**
 * converts integer 'n' to string ('instr')
 * @ingroup str
 */
CORE_API char* str_itos(char* instr, int n);
/**
 * converts float 'f' to string
 * @ingroup str
 */
CORE_API char* str_ftos(char* instr, float f);
/**
 * converts boolean 'b' to string
 * @ingroup str
 */
CORE_API char* str_btos(char* instr, int b);
/**
 * converts string to integer
 * @ingroup str
 */
CORE_API int str_toint32(const char* str);
/**
 * converts string to float
 * @ingroup str
 */
CORE_API float str_tofl32(const char* str);
/**
 * converts string to bool, ('false', '0', '') defines FALSE and ('true', '1') defines TRUE
 * @ingroup str
 */
CORE_API int str_tobool(const char* str);
/**
 * trims string 'instr' from sequence of characters defined by 'trim_chars'
 * @param instr input string
 * @param trim_chars sequence of characters to be trimmed from input string
 * @param outstr output string
 * @param outstr_size output string buffer size (including null-terminated char)
 * @ingroup str
 */
CORE_API char* str_trim(char* outstr, uint outstr_size, const char* instr, const char* trim_chars);
/**
 * replace characters of 'str' with another character
 * @param str input string
 * @param replace_ch character to be replaced
 * @param with_ch new character
 * @ingroup str
 */
CORE_API char* str_replace(char* str, char replace_ch, char with_ch);
/**
 * convert wide unicode string to multi-byte utf-8
 * @ingroup str
 */
CORE_API char* str_widetomb(char* outstr, const wchar* instr, uint outstr_size);
/**
 * convert multi-byte utf-8 string to wide unicode
 * @ingroup str
 */
CORE_API wchar* str_mbtowide(wchar* outstr, const char* instr, uint outstr_size);
/**
 * checks if string is empty (='')
 * @ingroup str
 */
INLINE int str_isempty(const char* str)
{
    return (str[0] == 0);
}

/**
 * safe copy string (checks size)
 * @ingroup str
 */
CORE_API char* str_safecpy(char* outstr, uint outstr_sz, const char* instr);

/**
 * safe cat string (checks size)
 * @ingroup str
 */
CORE_API char* str_safecat(char* outstr, uint outstr_sz, const char* instr);

/**
 * encode a normal single-byte string into utf-8
 * @ingroup str
 */
CORE_API char* str_utf8_encode(const char* instr, uint instr_len, uint* out_len);

/**
 * decode utf-8 string into single-byte
 * @ingroup str
 */
CORE_API char* str_utf8_decode(const char* instr, uint instr_len, uint* out_len);

/**
 * free allocated utf-8 string using either @see str_utf8_encode or @see str_utf8_decode
 * @ingroup str
 */
CORE_API void str_utf8_free(char* s);

/**
 * convert hex pointer string to actual pointer (ex. 0x014A3BC)
 * @ingroup str
 */
CORE_API void* str_toptr(const char* s);

/**
 * convert windows-style path ("\\t\\win\\path") to unix-style path ("/t/win/path")
 * @ingroup str
 */
CORE_API char* path_tounix(char* outpath, const char* inpath);
/**
 * convert unix-style path ("/t/unix/path") to windows-style path ("\\t\\unix\\path")
 * @ingroup str
 */
CORE_API char* path_towin(char* outpath, const char* inpath);
/**
 * convert path to platform specific format
 * @ingroup str
 */
CORE_API char* path_norm(char* outpath, const char* inpath);
/**
 * extract directory from the path
 * @ingroup str
 */
CORE_API char* path_getdir(char* outpath, const char* inpath);
/**
 * extract filename from the path, without any extensions
 * @ingroup str
 */
CORE_API char* path_getfilename(char* outpath, const char* inpath);
/**
 * extract file extension from the path
 * @ingroup str
 */
CORE_API char* path_getfileext(char* outpath, const char* inpath);
/**
 * extract full filename (with extension) from path
 * @ingroup str
 */
CORE_API char* path_getfullfilename(char* outpath, const char* inpath);
/**
 * go up one directory in path string
 * @ingroup str
 */
CORE_API char* path_goup(char* outpath, const char* inpath);
/**
 * check if file is valid (exists)
 * @ingroup str
 */
CORE_API int path_isfilevalid(const char* inpath);

/**
 * join multiple paths (or filenames) into one \n
 * Last argument should always be NULL to indicate that join arguments are finished
 * @ingroup str
 */
CORE_API char* path_join(char* outpath, const char* join0, const char* join1, ...);

#endif /*__STR_H__*/
