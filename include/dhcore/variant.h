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
    VARIANT_BOOL,
    VARIANT_INT,
    VARIANT_UINT,
    VARIANT_FLOAT,
    VARIANT_FLOAT2,
    VARIANT_FLOAT3,
    VARIANT_FLOAT4,
    VARIANT_INT2,
    VARIANT_INT3,
    VARIANT_INT4,
    VARIANT_STRING,
    VARIANT_NULL
};

struct variant
{
    enum variant_type type;

    union   {
        bool_t b;
        int i;
        uint ui;
        float f;
        float fs[4];
        int is[4];
        char str[16];
    };
};

CORE_API struct variant* var_setv(struct variant* rv, const struct variant* v);
CORE_API struct variant* var_setb(struct variant* v, bool_t b);
CORE_API struct variant* var_seti(struct variant* v, int i);
CORE_API struct variant* var_setui(struct variant* v, uint ui);
CORE_API struct variant* var_setf(struct variant* v, float f);
CORE_API struct variant* var_set2fv(struct variant* v, const float* fs);
CORE_API struct variant* var_set3fv(struct variant* v, const float* fs);
CORE_API struct variant* var_set4fv(struct variant* v, const float* fs);
CORE_API struct variant* var_set2iv(struct variant* v, const int* is);
CORE_API struct variant* var_set3iv(struct variant* v, const int* is);
CORE_API struct variant* var_set4iv(struct variant* v, const int* is);
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
CORE_API bool_t var_getb(const struct variant* v);
CORE_API const float* var_getfv(const struct variant* v);
CORE_API const int* var_getiv(const struct variant* v);
CORE_API const char* var_gets(const struct variant* v);

#endif /* __VARIANT_H__ */

