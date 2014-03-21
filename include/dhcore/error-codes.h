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

#ifndef __ERRORCODES_H__
#define __ERRORCODES_H__


 /* error codes */
#define RET_ABORT           2
#define RET_OK              1
#define RET_FAIL            0
#define RET_OUTOFMEMORY     -1
#define RET_WARNING         -2
#define RET_INVALIDARG      -3
#define RET_FILE_ERROR      -4
#define RET_NOT_IMPL        -5
#define RET_NOT_SUPPORTED   -6
#define RET_INVALIDCALL		-7

/* macros to check for error codes */
#define IS_FAIL(r)      ((r) <= 0)
#define IS_OK(r)        ((r) > 0)

#endif /* __ERRORCODES_H__ */
