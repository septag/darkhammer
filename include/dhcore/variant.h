/***********************************************************************************
 * Copyright (c) 2013, Sepehr Taghdisian
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

#ifndef __VARIANT_H__
#define __VARIANT_H__

#include "types.h"
#include "core-api.h"

#if defined(_MSVC_)
#pragma warning(disable: 662)
#endif

enum variant_type
{
    VAR_TYPE_BOOL = 1,
    VAR_TYPE_INT = 2,
    VAR_TYPE_UINT = 3,
    VAR_TYPE_FLOAT = 4,
    VAR_TYPE_FLOAT2 = 5,
    VAR_TYPE_FLOAT3 = 6,
    VAR_TYPE_FLOAT4 = 7,
    VAR_TYPE_INT2 = 8,
    VAR_TYPE_INT3 = 9,
    VAR_TYPE_INT4 = 10,
    VAR_TYPE_STRING = 11,
    VAR_TYPE_NULL = 0
};

struct variant
{
    enum variant_type type;

    union   {
        int b;
        int i;
        uint ui;
        float f;
        float fv[4];
        int iv[4];
        char str[16];
    };
};

CORE_API struct variant* var_setv(struct variant* rv, const struct variant* v);
CORE_API struct variant* var_setb(struct variant* v, int b);
CORE_API struct variant* var_seti(struct variant* v, int i);
CORE_API struct variant* var_setui(struct variant* v, uint ui);
CORE_API struct variant* var_setf(struct variant* v, float f);
CORE_API struct variant* var_set2fv(struct variant* v, const float* fv);
CORE_API struct variant* var_set3fv(struct variant* v, const float* fv);
CORE_API struct variant* var_set4fv(struct variant* v, const float* fv);
CORE_API struct variant* var_set2iv(struct variant* v, const int* iv);
CORE_API struct variant* var_set3iv(struct variant* v, const int* iv);
CORE_API struct variant* var_set4iv(struct variant* v, const int* iv);
CORE_API struct variant* var_set2f(struct variant* v, float x, float y);
CORE_API struct variant* var_set3f(struct variant* v, float x, float y, float z);
CORE_API struct variant* var_set4f(struct variant* v, float x, float y, float z, float w);
CORE_API struct variant* var_set2i(struct variant* v, int x, int y);
CORE_API struct variant* var_set3i(struct variant* v, int x, int y, int z);
CORE_API struct variant* var_set4i(struct variant* v, int x, int y, int z, int w);
CORE_API struct variant* var_sets(struct variant* v, const char* str);
CORE_API const char* var_gets(const struct variant* v);

CORE_API int var_geti(const struct variant* v);
CORE_API uint var_getui(const struct variant* v);
CORE_API float var_getf(const struct variant* v);
CORE_API int var_getb(const struct variant* v);
CORE_API const float* var_getfv(const struct variant* v);
CORE_API const int* var_getiv(const struct variant* v);
CORE_API const char* var_gets(const struct variant* v);

#endif /* __VARIANT_H__ */

