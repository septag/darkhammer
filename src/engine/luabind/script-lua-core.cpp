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

#include <stdio.h>
#include "luacore.i"
#include "script.h"

#include "dhcore/util.h"

/*************************************************************************************************
 * global funcs
 */
void printcon(const char* text)
{
    if (text == NULL)   {
        log_printf(LOG_WARNING, "incoming [null] text for 'printcon'");
        return;
    }

    log_printf(LOG_INFO, "lua: %s", text);
}

float randFloat(float min, float max)
{
    return rand_getf(min, max);
}

int randInt(int min, int max)
{
    return rand_geti(min, max);
}


/*************************************************************************************************
 * vector funcs
 */
Vector::Vector()
{
    vec3_setzero(&v_);
}

Vector::Vector(float x, float y, float z)
{
    vec3_setf(&v_, x, y, z);
}

Vector::Vector(const Vector& v)
{
    vec3_setv(&this->v_, &v.v_);
}

const char* Vector::__str__()
{
    static char text[64];
    sprintf(text, "Vector: %.3f %.3f %.3f", v_.x, v_.y, v_.z);
    return text;
}

Vector& Vector::__eq__(const Vector& v)
{
    vec3_setv(&this->v_, &v.v_);
    return *this;
}

Vector Vector::__add__(const Vector& v1)
{
    Vector r;
    vec3_add(&r.v_, &this->v_, &v1.v_);
    return r;
}

Vector Vector::__sub__(const Vector& v1)
{
    Vector r;
    vec3_sub(&r.v_, &this->v_, &v1.v_);
    return r;
}

float Vector::__mul__(const Vector& v1)
{
    return vec3_dot(&this->v_, &v1.v_);
}

Vector Vector::__mul__(float k)
{
    Vector r;
    vec3_muls(&r.v_, &this->v_, k);
    return r;
}

Vector Vector::__div__(float k)
{
    Vector r;
    vec3_muls(&r.v_, &this->v_, 1.0f/k);
    return r;
}

Vector Vector::cross(const Vector& v)
{
    Vector r;
    vec3_cross(&r.v_, &this->v_, &v.v_);
    return r;
}

float Vector::x() const
{
    return v_.x;
}

float Vector::y() const
{
    return v_.y;
}

float Vector::z() const
{
    return v_.z;
}

void Vector::set(float x, float y, float z)
{
    v_.x = x;    v_.y = y;      v_.z = z;
}

void Vector::lerp(const Vector& v1, const Vector& v2, float t)
{
    vec3_lerp(&this->v_, &v1.v_, &v2.v_, t);
}

void Vector::cubic(const Vector& v0, const Vector& v1, const Vector& v2, const Vector& v3, float t)
{
    vec3_cubic(&this->v_, &v0.v_, &v1.v_, &v2.v_, &v3.v_, t);
}

void Vector::normalize()
{
    vec3_norm(&v_, &v_);
}

void Vector::__setitem__(int x, float value)
{
    if (x < 0 || x > 2) {
        sct_throwerror("Index out of bounds");
        return;
    }
    v_.f[x] = value;
}

float Vector::__getitem__(int x)
{
    if (x < 0 || x > 2) {
        sct_throwerror("Index out of bounds");
        return 0.0f;
    }
    return v_.f[x];
}

/*************************************************************************************************
 * Quat
 */
Quat::Quat()
{
    quat_setidentity(&this->q_);
}

Quat::Quat(float x, float y, float z, float w)
{
    quat_setf(&this->q_, x, y, z, w);
}

Quat::Quat(const Quat& q)
{
    quat_setq(&this->q_, &q.q_);
}

const char* Quat::__str__()
{
    static char text[64];
    sprintf(text, "Quat: %.3f %.3f %.3f %.3f", q_.x, q_.y, q_.z, q_.w);
    return text;
}

Quat& Quat::__eq__(const Quat& q)
{
    quat_setq(&this->q_, &q.q_);
    return *this;
}

void Quat::setEuler(float rx, float ry, float rz)
{
    quat_fromeuler(&this->q_, rx, ry, rz);
}

Vector Quat::getEuler()
{
    float rx;
    float ry;
    float rz;

    quat_geteuler(&rx, &ry, &rz, &this->q_);
    Vector r;
    vec3_setf(&r.v_, rx, ry, rz);
    return r;
}

Quat& Quat::inverse()
{
    quat_inv(&this->q_, &this->q_);
    return *this;
}

void Quat::slerp(const Quat& q1, const Quat& q2, float t)
{
    quat_slerp(&this->q_, &q1.q_, &q2.q_, t);
}

float Quat::__getitem__(int x)
{
    if (x < 0 || x > 3) {
        sct_throwerror("Index out of bounds");
        return 0.0f;
    }

    return q_.f[x];
}

void Quat::__setitem__(int x, float value)
{
    if (x < 0 || x > 3) {
        sct_throwerror("Index out of bounds");
        return;
    }
    q_.f[x] = value;
}

/*************************************************************************************************
 * Color
 */
Color::Color()
{
    color_setc(&c_, &g_color_black);
}

Color::Color(float r, float g, float b, float a)
{
    color_setf(&c_, r, g, b, a);
}

Color::Color(const Color& c)
{
    color_setc(&c_, &c.c_);
}

void Color::set(float r, float g, float b, float a)
{
    color_setf(&c_, r, g, b, a);
}

void Color::setInt(uint r, uint g, uint b, uint a)
{
    color_seti(&c_, (uint8)r, (uint8)g, (uint8)b, (uint8)a);
}

const char* Color::__str__()
{
    static char text[64];
    sprintf(text, "Color: %.3f %.3f %.3f %.3f", c_.r, c_.r, c_.b, c_.a);
    return text;
}

Color& Color::__eq__(const Color& c)
{
    color_setc(&c_, &c.c_);
    return *this;
}

Color Color::__add__(const Color& c1)
{
    Color r;
    color_add(&r.c_, &c_, &c1.c_);
    return r;
}

Color Color::__mul__(const Color& c1)
{
    Color r;
    color_mul(&r.c_, &c_, &c1.c_);
    return r;
}

Color Color::__mul__(float k)
{
    Color r;
    color_muls(&r.c_, &c_, k);
    return r;
}

float Color::r() const
{
    return c_.r;
}

float Color::g() const
{
    return c_.g;
}

float Color::b() const
{
    return c_.b;
}

float Color::a() const
{
    return c_.a;
}

void Color::lerp(const Color& c1, const Color& c2, float t)
{
    color_lerp(&c_, &c1.c_, &c2.c_, t);
}

Color Color::toGamma()
{
    Color r;
    color_togamma(&r.c_, &c_);
    return r;
}

Color Color::toLinear()
{
    Color r;
    color_tolinear(&r.c_, &c_);
    return r;
}

uint Color::toUint()
{
    return color_rgba_uint(&c_);
}

float Color::__getitem__(int x)
{
    if (x < 0 || x > 3) {
        sct_throwerror("Index out of bounds");
        return 0.0f;
    }

    return c_.f[x];
}

void Color::__setitem__(int x, float value)
{
    if (x < 0 || x > 3) {
        sct_throwerror("Index out of bounds");
        return;
    }
    c_.f[x] = value;
}

/*************************************************************************************************
 * Vector2D
 */
Vector2D::Vector2D()
{
    vec2i_setzero(&v_);
}

Vector2D::Vector2D(int x, int y)
{
    vec2i_seti(&v_, x, y);
}

Vector2D::Vector2D(const Vector2D& v)
{
    vec2i_setv(&v_, &v.v_);
}

void Vector2D::set(int x, int y)
{
    vec2i_seti(&v_, x, y);
}

const char* Vector2D::__str()
{
    static char text[64];
    sprintf(text, "Vector2D: %d, %d", v_.x, v_.y);
    return text;
}

Vector2D& Vector2D::__eq__(const Vector2D& v)
{
    vec2i_setv(&v_, &v.v_);
    return *this;
}

Vector2D Vector2D::__add__(const Vector2D& v1)
{
    Vector2D r;
    vec2i_add(&r.v_, &v_, &v1.v_);
    return r;
}

Vector2D Vector2D::__sub__(const Vector2D& v1)
{
    Vector2D r;
    vec2i_sub(&r.v_, &v_, &v1.v_);
    return r;
}

Vector2D Vector2D::__mul__(int k)
{
    Vector2D r;
    vec2i_muls(&r.v_, &v_, k);
    return r;
}

void Vector2D::__setitem__(int x, int value)
{
    if (x < 0 || x > 1) {
        sct_throwerror("Index out of bounds");
        return;
    }
    v_.n[x] = value;
}

int Vector2D::__getitem__(int x)
{
    if (x < 0 || x > 1) {
        sct_throwerror("Index out of bounds");
        return 0;
    }
    return v_.n[x];
}