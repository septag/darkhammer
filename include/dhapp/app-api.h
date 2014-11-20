/***********************************************************************************
 * Copyright (c) 2014, Sepehr Taghdisian
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

#include "dhcore/types.h"

#ifndef __APPAPI_H__
#define __APPAPI_H__

#if defined(_APP_EXPORT_)
  #if defined(_MSVC_)
    #define APP_API _EXTERN_EXPORT_ __declspec(dllexport)
    #define APP_CPP_API __declspec(dllexport)
  #elif defined(_GNUC_)
    #define APP_API _EXTERN_EXPORT_ __attribute__((visibility("default)))
    #define APP_CPP_API __attribute__((visibility("default")))
  #endif
#else
  #if defined(_MSVC_)
    #define APP_API _EXTERN_EXPORT_ __declspec(dllimport)
    #define APP_CPP_API __declspec(dllimport)
  #elif defined(_GNUC_)
    #define APP_API _EXTERN_EXPORT_ __attribute__((visibility("default")))
    #define APP_CPP_API __attribute__((visibility("default")))
  #endif
#endif /* defined(_APP_EXPORT_) */

#if defined(SWIG)
  #define APP_API
#endif

#endif /* __APPAPI_H__ */
