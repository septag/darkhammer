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

/**
 * Core swig interface file for lua bindings
 * Note that I change styles from C style to C++ style for more convenient lua bindings
 */

#if defined(SWIG)
%module core

%{
#include "dhcore/core.h"
#include "luabind/script-lua-common.h"
#include "luacore.i"
%}
%import "../../../include/dhcore/types.h"
%import "../../../include/dhcore/core-api.h"
%import "../../../include/dhcore/log.h"
#else
#include "dhcore/core.h"
#include "dhcore/vec-math.h"
#include "dhcore/color.h"
#include "luabind/script-lua-common.h"
#endif

/* plain C functions */
void printcon(const char* text);
float randFloat(float min = 0.0f, float max = 1.0f);
int randInt(int min = 0, int max = 100);

/* wrapper classes */
#ifdef __cplusplus

class BaseAlloc
{
public:
	BaseAlloc() {}

#if !defined(SWIG)
    void* operator new(size_t sz)    {	return sct_alloc(sz);    }
    void* operator new[](size_t sz)    {	return sct_alloc(sz);    }
    void operator delete(void* p)    {	sct_free(p);    }
    void operator delete[](void* p)    {	sct_free(p);    }
#endif
};

/* Vector2D */
class Vector2D : public BaseAlloc
{
#ifndef SWIG
public:
    struct vec2i v_;
#endif

public:
    Vector2D();
    Vector2D(int x, int y);
    Vector2D(const Vector2D& v);

    void set(int x, int y);

    const char* __str();
    Vector2D& __eq__(const Vector2D& v);
    Vector2D __add__(const Vector2D& v1);
    Vector2D __sub__(const Vector2D& v1);
    Vector2D __mul__(int k);

    void __setitem__(int x, int value);
    int __getitem__(int x);
};

/* Vector */
class Vector : public BaseAlloc
{
#ifndef SWIG
public:
    struct vec3f v_;
#endif

public:
    Vector();
    Vector(float x, float y, float z);
    Vector(const Vector& v);

    const char* __str__();
    Vector& __eq__(const Vector& v);
    Vector __add__(const Vector& v1);
    Vector __sub__(const Vector& v1);
    float __mul__(const Vector& v1);
    Vector __mul__(float k);
    Vector __div__(float k);
    Vector cross(const Vector& v);
    void set(float x, float y, float z);

    float x() const;
    float y() const;
    float z() const;

    void lerp(const Vector& v1, const Vector& v2, float t);
    void cubic(const Vector& v0, const Vector& v1, const Vector& v2, const Vector& v3, float t);
    void normalize();

    void __setitem__(int x, float value);
    float __getitem__(int x);
};

/* color */
class Color : public BaseAlloc
{
#ifndef SWIG
public:
    struct color c_;
#endif

public:
    Color();
    Color(float r, float g, float b, float a = 1.0f);
    Color(const Color& c);

    void set(float r, float g, float b, float a = 1.0f);
    void setInt(uint r, uint g, uint b, uint a = 255);

    const char* __str__();

    Color& __eq__(const Color& c);
    Color __add__(const Color& c1);
    Color __mul__(const Color& c1);
    Color __mul__(float k);

    float r() const;
    float g() const;
    float b() const;
    float a() const;

    void lerp(const Color& c1, const Color& c2, float t);

    Color toGamma();
    Color toLinear();
    uint toUint();

    void __setitem__(int x, float value);
    float __getitem__(int x);
};

/* quat4f */
class Quat : public BaseAlloc
{
#ifndef SWIG
public:
	struct quat4f q_;
#endif

public:
    Quat();
    Quat(float x, float y, float z, float w);
    Quat(const Quat& q);

    const char* __str__();

    Quat& __eq__(const Quat& q);

    void setEuler(float rx, float ry, float rz);
    Vector getEuler();
    Quat& inverse();

    void slerp(const Quat& q1, const Quat& q2, float t);

    void __setitem__(int x, float value);
    float __getitem__(int x);
};

#endif
