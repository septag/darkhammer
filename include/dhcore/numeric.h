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

#ifndef __NUMERIC_H__
#define __NUMERIC_H__

#include "types.h"
#include "core-api.h"

#if defined(PI)
#undef PI
#endif

#if defined(EPSILON)
#undef EPSILON
#endif

#define EPSILON    0.00001f
#define PI         3.14159265f
#define PI_2X      6.2831853f
#define PI_HALF    1.570796325f

/**
 * @defgroup num Numeric
 */

 /**
 * initializes random seed by system timer
 * @ingroup num
 */
CORE_API void rand_seed();

/**
 * flips a count given a probability value
 * @param prob should be between 0~100, it is the chance of heads for the coin (true) -
 * which normally whould be 50 for 50-50 chance
 * @ingroup num
 */
CORE_API int rand_flipcoin(uint prob);

/**
 * get a random value between two integer values , range = [min, max]
 * @ingroup num
 */
CORE_API int rand_geti(int min, int max);

/**
 * get a random value between two floating-point values, range = [min, max]
 * @ingroup num
 */
CORE_API float rand_getf(float min, float max);

/**
 * powers an integer value (base) to n
 * @ingroup num
 */
INLINE int powi(int base, int n)
{
    if (n == 0)
        return 1;

    int r = base;
    for (int i = 1; i < n; i++)
        r *= base;
    return r;
}

/**
 * clamp input float value to [min_value, max_value]
 * @ingroup num
 */
INLINE float clampf(float value, float min_value, float max_value)
{
    if (value < min_value)      return min_value;
    else if (value > max_value) return max_value;
    else                        return value;
}

/**
 * clamp input integer value to [min_value, max_value]
 * @ingroup num
 */
INLINE int clampi(int value, int min_value, int max_value)
{
    if (value < min_value)      return min_value;
    else if (value > max_value) return max_value;
    else                        return value;
}

/**
 * clamp input unsigned integer value to [min_value, max_value]
 * @ingroup num
 */
INLINE uint clampui(uint value, uint min_value, uint max_value)
{
    if (value < min_value)      return min_value;
    else if (value > max_value) return max_value;
    else                        return value;
}

/**
 * clamp input size_t value to [min_value, max_value]
 * @ingroup num
 */
INLINE size_t clampsz(size_t value, size_t min_value, size_t max_value)
{
    if (value < min_value)      return min_value;
    else if (value > max_value) return max_value;
    else                        return value;
}

/**
 * return minimum of two float values
 * @ingroup num
 */
INLINE float minf(float n1, float n2)
{
    return (n1 < n2) ? n1 : n2;
}

/**
 * return minimum of two integer values
 * @ingroup num
 */
INLINE int mini(int n1, int n2)
{
    return (n1 < n2) ? n1 : n2;
}

/**
 * return minimum of two unsigned integer values
 * @ingroup num
 */
INLINE uint minui(uint n1, uint n2)
{
    return (n1 < n2) ? n1 : n2;
}

/**
 * return maximum of two float values
 * @ingroup num
 */
INLINE float maxf(float n1, float n2)
{
    return (n1 > n2) ? n1 : n2;
}

/**
 * return maximum of two integer values
 * @ingroup num
 */
INLINE int maxi(int n1, int n2)
{
    return (n1 > n2) ? n1 : n2;
}

/**
 * return maximum of two unsigned integer values
 * @ingroup num
 */
INLINE uint maxui(uint n1, uint n2)
{
    return (n1 > n2) ? n1 : n2;
}

/**
 * swap two float values with each other
 * @ingroup num
 */
INLINE void swapf(float* n1, float* n2)
{
    register float tmp = *n1;
    *n1 = *n2;
    *n2 = tmp;
}

/**
 * swap two integer values with each other
 * @ingroup num
 */
INLINE void swapi(int* n1, int* n2)
{
    register int tmp = *n1;
    *n1 = *n2;
    *n2 = tmp;
}

/**
 * swap two unsigned integer values with each other
 * @ingroup num
 */
INLINE void swapui(uint* n1, uint* n2)
{
    register uint tmp = *n1;
    *n1 = *n2;
    *n2 = tmp;
}

/**
 * swap two pointers with each other
 * @ingroup num
 */
INLINE void swapptr(void** pp1, void** pp2)
{
	register void* tmp = *pp1;
	*pp1 = *pp2;
	*pp2 = tmp;
}

#endif /* __NUMERIC_H__ */
