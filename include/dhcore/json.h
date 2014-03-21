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
 * @defgroup JSON JSON
 * using cjson library, this file is basically a wrapper to cjson\n
 * note: library is thread-safe as long as multiple threads do not manipulate same JSON tree
 * for more JSON information visit: http://www.JSON.org/fatfree.html\n
 * libcjson : http://sourceforge.net/projects/cjson/\n
 */

#ifndef __JSON_H__
#define __JSON_H__

#include "types.h"
#include "core-api.h"
#include "file-io.h"
#include "allocator.h"

/**
 * basic JSON type, used in JSON functions, if =NULL then it is either uninitialized or error
 * @ingroup JSON
 */
typedef void*   json_t;

/**
 * Initialize JSON parser with custom (and fast) memory management
 * @ingroup JSON
 */
CORE_API result_t json_init();

/**
 * Release JSON allocators and pre-allocated memory buffers
 * @see json_init
 * @ingroup JSON
 */
CORE_API void json_release();

/**
 * Parse JSON file and create a root object, root object must be destroyed if not needed anymore
 * @param f file handle of JSON file
 * @return JSON object, NULL if error occured
 * @ingroup JSON
 */
CORE_API json_t json_parsefile(file_t f, struct allocator* tmp_alloc);

/**
 * Parse JSON string (buffer)
 * @param str JSON formatted string
 * @return JSON object, NULL if error occured
 * @ingroup JSON
 */
CORE_API json_t json_parsestring(const char* str);

/**
 * Save JSON data to file
 * @param filepath path to the file on the disk
 * @param trim trims output JSON data (no formatting) to be more optimized and compact
 * @ingroup JSON
 */
CORE_API result_t json_savetofile(json_t j, const char* filepath, bool_t trim);

/**
 * Save JSON data to file handle
 * @param f file handle that is ready and opened for writing
 * @param trim trims output JSON data (no formatting) to be more optimized and compact
 * @ingroup JSON
 */
CORE_API result_t json_savetofilef(json_t j, file_t f, bool_t trim);

/**
 * Save JSON data to buffer, returns resulting string. user should call json_deletebuffer after
 * @param alloc allocator that is used to create the output buffer
 * @param outsize output buffer size, including /0 character
 * @param trim trims output JSON data (no formatting) to be more optimized and compact
 * @see json_deletebuffer
 * @ingroup JSON
 */
CORE_API char* json_savetobuffer(json_t j, size_t* outsize, bool_t trim);

/**
 * Deletes previously created JSON buffer @see json_savetobuffer
 */
CORE_API void json_deletebuffer(char* buffer);

/**
 * Destroys JSON object and free memory
 * @param j JSON object that is allocated previously
 * @ingroup JSON
 */
CORE_API void json_destroy(json_t j);

/* set/get functions for JSON items
 **
 * @ingroup JSON
 */
CORE_API void json_seti(json_t j, int n);

/**
 * @ingroup JSON
 */
CORE_API void json_setf(json_t j, float f);

/**
 * @ingroup JSON
 */
CORE_API void json_sets(json_t j, const char* str);

/**
 * @ingroup JSON
 */
CORE_API void json_setb(json_t j, bool_t b);

/**
 * @ingroup JSON
 */
CORE_API int json_geti(json_t j);

/**
 * @ingroup JSON
 */
CORE_API float json_getf(json_t j);

/**
 * @ingroup JSON
 */
CORE_API const char* json_gets(json_t j);

/**
 * @ingroup JSON
 */
CORE_API bool_t json_getb(json_t j);

/**
 * @ingroup JSON
 */
CORE_API int json_geti_child(json_t parent, const char* name, int def_value);

/**
 * @ingroup JSON
 */
CORE_API float json_getf_child(json_t parent, const char* name, float def_value);

/**
 * @ingroup JSON
 */
CORE_API const char* json_gets_child(json_t parent, const char* name, const char* def_value);

/**
 * @ingroup JSON
 */
CORE_API bool_t json_getb_child(json_t parent, const char* name, bool_t def_value);

/**
 * get array size (number of items) from a JSON array item
 * @ingroup JSON
 */
CORE_API int json_getarr_count(json_t j);

/**
 * get array item from a JSON array item
 * @param idx zero-based index to the array
 * @ingroup JSON
 */
CORE_API json_t json_getarr_item(json_t j, int idx);

/**
 * get child item from an JSON object referenced by a name
 * @ingroup JSON
 */
CORE_API json_t json_getitem(json_t j, const char* name);


/* creating JSON items for different types
 **
 * @ingroup JSON
 */
CORE_API json_t json_create_null();

/**
 * @ingroup JSON
 */
CORE_API json_t json_create_obj();

/**
 * @ingroup JSON
 */
CORE_API json_t json_create_bool(bool_t b);

/**
 * @ingroup JSON
 */
CORE_API json_t json_create_num(fl64 n);

/**
 * @ingroup JSON
 */
CORE_API json_t json_create_str(const char* str);

/**
 * @ingroup JSON
 */
CORE_API json_t json_create_arr();

/**
 * @ingroup JSON
 */
CORE_API json_t json_create_arri(int* nums, int count);

/**
 * @ingroup JSON
 */
CORE_API json_t json_create_arrf(float* nums, int count);

/**
 * @ingroup JSON
 */
CORE_API json_t json_create_arrs(const char** strs, int count);

/**
 * add single item to array type
 * @ingroup JSON
 */
CORE_API void json_additem_toarr(json_t arr, json_t item);

/**
 * add item to object type
 * @ingroup JSON
 */
CORE_API void json_additem_toobj(json_t obj, const char* name, json_t item);

#endif /* __JSON_H__ */
