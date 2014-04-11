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


#ifndef __STDMATH_H__
#define __STDMATH_H__

#include <math.h>
#include "types.h"
#include "core-api.h"
#include "numeric.h"

/**
 * @defgroup stdmath Std-math
 */

 /**
 * check if float value is NAN
 * @ingroup stdmath
 */
INLINE int math_isnan(float n)
{
    return (n != n);
}

/**
 * round float value to it's closest value
 * @ingroup stdmath
 */
INLINE float math_round(float n)
{
    return (n > 0.0f) ? floorf(n + 0.5f) : ceilf(n - 0.5f);
}

/**
 * checks equality of two floating-point values with a tolerance
 * @ingroup stdmath
 */
INLINE int math_isequal(float a, float b)
{
    if (fabs(a-b) < EPSILON)
        return TRUE;

    float err = 0.0f;
    if (fabsf(b) > fabsf(a))
        err = fabsf((a-b)/b);
    else
        err = fabsf((a-b)/a);

    return (err < EPSILON);
}

/**
 * checks if floating-point value is zero
 * @ingroup stdmath
 */
INLINE int math_iszero(float n)
{
    return fabs(n) < EPSILON;
}

/**
 * converts radians to degrees
 * @ingroup stdmath
 */
INLINE float math_todeg(float rad)
{
    return 180.0f * rad / PI;
}

/*
 * converts degrees to radians
 * @ingroup stdmath
 */
INLINE float math_torad(float deg)
{
    return deg * PI / 180.0f;
}

/**
 * returns the sign of float value
 * @return 1.0 if 'n' is positive, -1.0 if 'n' is negative and 0 if 'n' is zero
 * @ingroup stdmath
 */
INLINE float math_sign(float n)
{
    if (n > EPSILON)            return 1.0f;
    else if (n < -EPSILON)      return -1.0f;
    else                        return 0.0f;
}

/**
 * Encodes 32bit float value to uint16
 * @ingroup stdmath
 */
CORE_API uint16 math_ftou16(float n);

/**
 * Decodes uint16 value to 32bit floating point value
 * @ingroup stdmath
 */
CORE_API float math_u16tof(uint16 n);

/**
 * Safe ACos
 * @return PI if x is less than -1.0f, 0.0f if x is more than 1.0f, and acos(x) if x is within range
 */
INLINE float math_acosf(float x)
{
    if (x <= -1.0f)    return PI;
    if (x >= 1.0f)     return 0.0f;
    return acosf(x);
}

/**
 * Apply decay function
 * @param
 * @param
 * @return
 */
CORE_API float math_decay(float x, float real_x, float springiness, float dt);

#endif
