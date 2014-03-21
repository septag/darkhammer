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


#include "std-math.h"
#include "mem-mgr.h"

union bits_t
{
    float f;
    int si;
    uint ui;
};

#define SHIFT                   13
#define SHIFT_SIGN              16

#define INFN                    (0x7F800000)
#define MAXN                    (0x477FE000)
#define MINN                    (0x38800000)
#define SIGNN                   (0x80000000)

#define INFC                    (INFN >> SHIFT)
#define NANN                    ((INFC + 1) << SHIFT)
#define MAXC                    (MAXN >> SHIFT)
#define MINC                    (MINN >> SHIFT)
#define SIGNC                   (SIGNN >> SHIFT_SIGN)

#define MULN                    (0x52000000)
#define MULC                    (0x33800000)

#define SUBC                    (0x003FF)
#define NORC                    (0x00400)

#define MAXD                    (INFC - MAXC - 1)
#define MIND                    (MINC - SUBC - 1)

uint16 math_ftou16(float n)
{
    union bits_t v, s;
    v.f = n;
    uint sign = v.si & SIGNN;
    v.si ^= sign;
    sign >>= SHIFT_SIGN; /* logical SHIFT */
    s.si = MULN;
    s.si = (int)(s.f * v.f); /* correct subnormals */
    v.si ^= (s.si ^ v.si) & -(MINN > v.si);
    v.si ^= (INFN ^ v.si) & -((INFN > v.si) & (v.si > MAXN));
    v.si ^= (NANN ^ v.si) & -((NANN > v.si) & (v.si > INFN));
    v.ui >>= SHIFT; /* logical SHIFT */
    v.si ^= ((v.si - MAXD) ^ v.si) & -(v.si > MAXC);
    v.si ^= ((v.si - MIND) ^ v.si) & -(v.si > SUBC);
    return v.ui | sign;
}

float math_u16tof(uint16 n)
{
    union bits_t v;
    v.ui = n;
    int sign = v.si & SIGNC;
    v.si ^= sign;
    sign <<= SHIFT_SIGN;
    v.si ^= ((v.si + MIND) ^ v.si) & -(v.si > SUBC);
    v.si ^= ((v.si + MAXD) ^ v.si) & -(v.si > MAXC);
    union bits_t s;
    s.si = MULC;
    s.f *= v.si;
    int mask = -(NORC > v.si);
    v.si <<= SHIFT;
    v.si ^= (s.si ^ v.si) & mask;
    v.si |= sign;
    return v.f;
}

/* exponential decay function */
float math_decay(float x, float real_x, float springiness, float dt)
{
    fl64 d = 1 - exp(log(0.5)*springiness*dt);
    return (float)(x + (real_x - x)*d);
}