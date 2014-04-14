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

#include "variant.h"
#include "str.h"
#include "err.h"

struct variant* var_setv(struct variant* rv, const struct variant* v)
{
    rv->type = v->type;
    rv->fv[0] = v->fv[0];
    rv->fv[1] = v->fv[1];
    rv->fv[2] = v->fv[2];
    rv->fv[3] = v->fv[3];
    return rv;
}

struct variant* var_setb(struct variant* v, int b)
{
    v->type = VAR_TYPE_BOOL;
    v->b = b;
    return v;
}

struct variant* var_seti(struct variant* v, int i)
{
    v->type = VAR_TYPE_INT;
    v->i = i;
    return v;
}

struct variant* var_setf(struct variant* v, float f)
{
    v->type = VAR_TYPE_FLOAT;
    v->f = f;
    return v;
}

struct variant* var_set2fv(struct variant* v, const float* fv)
{
    v->type = VAR_TYPE_FLOAT2;
    v->fv[0] = fv[0];
    v->fv[1] = fv[1];
    return v;
}

struct variant* var_set3fv(struct variant* v, const float* fv)
{
    v->type = VAR_TYPE_FLOAT3;
    v->fv[0] = fv[0];
    v->fv[1] = fv[1];
    v->fv[2] = fv[2];
    return v;
}

struct variant* var_set4fv(struct variant* v, const float* fv)
{
    v->type = VAR_TYPE_FLOAT4;
    v->fv[0] = fv[0];
    v->fv[1] = fv[1];
    v->fv[2] = fv[2];
    v->fv[3] = fv[3];
    return v;
}

struct variant* var_set2iv(struct variant* v, const int* iv)
{
    v->type = VAR_TYPE_INT2;
    v->iv[0] = iv[0];
    v->iv[1] = iv[1];
    return v;
}

struct variant* var_set3iv(struct variant* v, const int* iv)
{
    v->type = VAR_TYPE_INT3;
    v->iv[0] = iv[0];
    v->iv[1] = iv[1];
    v->iv[2] = iv[2];
    return v;
}

struct variant* var_set4iv(struct variant* v, const int* iv)
{
    v->type = VAR_TYPE_INT4;
    v->iv[0] = iv[0];
    v->iv[1] = iv[1];
    v->iv[2] = iv[2];
    v->iv[3] = iv[3];
    return v;
}

struct variant* var_set2f(struct variant* v, float x, float y)
{
    v->type = VAR_TYPE_FLOAT2;
    v->fv[0] = x;
    v->fv[1] = y;
    return v;
}

struct variant* var_set3f(struct variant* v, float x, float y, float z)
{
    v->type = VAR_TYPE_FLOAT3;
    v->fv[0] = x;
    v->fv[1] = y;
    v->fv[2] = z;
    return v;
}

struct variant* var_set4f(struct variant* v, float x, float y, float z, float w)
{
    v->type = VAR_TYPE_FLOAT4;
    v->fv[0] = x;
    v->fv[1] = y;
    v->fv[2] = z;
    v->fv[3] = w;
    return v;
}

struct variant* var_set2i(struct variant* v, int x, int y)
{
    v->type = VAR_TYPE_INT2;
    v->iv[0] = x;
    v->iv[1] = y;
    return v;
}

struct variant* var_set3i(struct variant* v, int x, int y, int z)
{
    v->type = VAR_TYPE_INT3;
    v->iv[0] = x;
    v->iv[1] = y;
    v->iv[2] = z;
    return v;
}

struct variant* var_set4i(struct variant* v, int x, int y, int z, int w)
{
    v->type = VAR_TYPE_INT4;
    v->iv[0] = x;
    v->iv[1] = y;
    v->iv[2] = z;
    v->iv[3] = w;
    return v;
}

struct variant* var_sets(struct variant* v, const char* str)
{
    v->type = VAR_TYPE_STRING;
    str_safecpy(v->str, sizeof(v->str), str);
    return v;
}

struct variant* var_setui(struct variant* v, uint ui)
{
    v->type = VAR_TYPE_UINT;
    v->ui = ui;
    return v;
}

int var_geti(const struct variant* v)
{
    ASSERT(v->type == VAR_TYPE_INT);
    return v->i;
}

uint var_getui(const struct variant* v)
{
    ASSERT(v->type == VAR_TYPE_UINT);
    return v->ui;
}

float var_getf(const struct variant* v)
{
    ASSERT(v->type == VAR_TYPE_FLOAT);
    return v->f;
}

int var_getb(const struct variant* v)
{
    ASSERT(v->type == VAR_TYPE_BOOL);
    return v->b;
}

const float* var_getfv(const struct variant* v)
{
    ASSERT(v->type == VAR_TYPE_FLOAT2 || v->type == VAR_TYPE_FLOAT3 || v->type == VAR_TYPE_FLOAT4);
    return v->fv;
}

const int* var_getiv(const struct variant* v)
{
    ASSERT(v->type == VAR_TYPE_INT2 || v->type == VAR_TYPE_INT3 || v->type == VAR_TYPE_INT4);
    return v->iv;
}

const char* var_gets(const struct variant* v)
{
    ASSERT(v->type == VAR_TYPE_STRING);
    return v->str;
}
