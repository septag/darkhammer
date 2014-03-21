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


/* precompiled header for win32 */
#ifndef __WIN_H__
#define __WIN_H__

#if defined(_WIN_)

#ifndef WINVER
#define WINVER 0x0600
#endif

/* minimum VISTA */
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif

#if !defined(VC_EXTRALEAN)
#define VC_EXTRALEAN
#endif

#include <windows.h>
#include <windowsx.h>
#endif


#endif /* __WIN_H__ */
