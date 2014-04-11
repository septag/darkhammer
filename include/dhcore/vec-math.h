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

 /**
 * @defgroup vmath Vector Math
 */

 /**
 * common vector-math functions and definitions, vec4f, quat4f, mat3f, ...\n
 * there are different type of implementations for each cpu architecture:\n
 * #1: generic fpu implementation\n
 * #2: x86-64 SSE implementation, which can be activated by defining _SIMD_SSE_ preprocessor\n
 * many of vector-math functions returns pointer to return value provided as 'r' in the first -\n
 * paramter, so they can be combined and used in the single statement
 * @ingroup vmath
 */

#ifndef __VECMATH_H__
#define __VECMATH_H__

#include "types.h"
#include "core-api.h"
#include "std-math.h"
#include "mem-mgr.h"
#include "allocator.h"

#if defined(_MSVC_)
#pragma warning(disable: 662)
#endif

/* check for SSE validity */
#if !(defined(_X86_) || defined(_X64_)) && defined(_SIMD_SSE_)
#error "SSE SIMD instructions not available in any platform except x86-64"
#endif

/* SIMD - SSE */
#if defined(_SIMD_SSE_)
#include <xmmintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>
typedef __m128  simd_t;
typedef __m128i simd4i_t;

#define _mm_all_x(v)    _mm_shuffle_ps((v), (v), _MM_SHUFFLE(0, 0, 0, 0))
#define _mm_all_y(v)    _mm_shuffle_ps((v), (v), _MM_SHUFFLE(1, 1, 1, 1))
#define _mm_all_z(v)    _mm_shuffle_ps((v), (v), _MM_SHUFFLE(2, 2, 2, 2))
#define _mm_all_w(v)    _mm_shuffle_ps((v), (v), _MM_SHUFFLE(3, 3, 3, 3))
/* madd (mul+add): r = v1*v2 + v3 */
#define _mm_madd(v1, v2, v3) _mm_add_ps(_mm_mul_ps((v1), (v2)), (v3))
#endif

/**
 * 2-component integer vector, mainly used for screen-space operations
 * @ingroup vmath
 */
struct vec2i
{
	union	{
		struct	{
			int   x;
			int   y;
		};

		int n[2];
	};
};

/**
 * 2-component float vector
 * @ingroup vmath
 */
struct vec2f
{
	union	{
		struct	{
			float    x;
			float    y;
		};

		float f[2];
	};
};


/**
 * 4-component vector\n
 * can be used as 3-component (float3) or 4-component(float4) and also color (rgba) types
 * @ingroup vmath
 */

struct ALIGN16 vec4f
{
    union  {
        struct {
            float x;
            float y;
            float z;
            float w;
        };

        float f[4];
    };
};

struct ALIGN16 vec4i
{
	union	{
		struct {
			int x;
			int y;
			int z;
			int w;
		};

		int n[4];
	};
};

#define vec3f vec4f

/* vec3 globals */
#if defined(_GNUC_) && !defined(__cplusplus)
static const struct vec3f g_vec3_zero = {.x=0.0f, .y=0.0f, .z=0.0f, .w=1.0f};
static const struct vec3f g_vec3_unitx = {.x=1.0f, .y=0.0f, .z=0.0f, .w=1.0f};
static const struct vec3f g_vec3_unity = {.x=0.0f, .y=1.0f, .z=0.0f, .w=1.0f};
static const struct vec3f g_vec3_unitz = {.x=0.0f, .y=0.0f, .z=1.0f, .w=1.0f};
static const struct vec3f g_vec3_unitx_neg = {.x=-1.0f, .y=0.0f, .z=0.0f, .w=1.0f};
static const struct vec3f g_vec3_unity_neg = {.x=0.0f, .y=-1.0f, .z=0.0f, .w=1.0f};
static const struct vec3f g_vec3_unitz_neg = {.x=0.0f, .y=0.0f, .z=-1.0f, .w=1.0f};
#else
static const struct vec3f g_vec3_zero = {0.0f, 0.0f, 0.0f, 1.0f};
static const struct vec3f g_vec3_unitx = {1.0f, 0.0f, 0.0f, 1.0f};
static const struct vec3f g_vec3_unity = {0.0f, 1.0f, 0.0f, 1.0f};
static const struct vec3f g_vec3_unitz = {0.0f, 0.0f, 1.0f, 1.0f};
static const struct vec3f g_vec3_unitx_neg = {-1.0f, 0.0f, 0.0f, 1.0f};
static const struct vec3f g_vec3_unity_neg = {0.0f, -1.0f, 0.0f, 1.0f};
static const struct vec3f g_vec3_unitz_neg = {0.0f, 0.0f, -1.0f, 1.0f};
#endif

/**
 * row-major 4x3 matrix \n
 * row-major representation: m(row)(col)\n
 * @ingroup vmath
 */
struct ALIGN16 mat3f
{
    union   {
        struct {
            float m11, m12, m13, m14;
            float m21, m22, m23, m24;
            float m31, m32, m33, m34;
            float m41, m42, m43, m44;
        };

        struct {
            float row1[4];
            float row2[4];
            float row3[4];
            float row4[4];
        };

        float    f[16];
    };
};

/**
 * row-major 4x4 matrix \n
 * but the representation is still row-major
 * @ingroup vmath
 */
struct ALIGN16 mat4f
{
    union   {
        struct {
            float m11, m12, m13, m14;    /* row #1 */
            float m21, m22, m23, m24;    /* row #2 */
            float m31, m32, m33, m34;    /* row #3 */
            float m41, m42, m43, m44;    /* row #4 */
        };

        struct {
            float row1[4];
            float row2[4];
            float row3[4];
            float row4[4];
        };
        float    f[16];
   };
};

/**
 * simd friendly array of vectors
 * stores array of vectors 16byte aligned and structure of arrays
 * @ingroup vmath
 */
struct ALIGN16 vec4f_simd
{
    struct allocator* alloc;
    uint cnt;
    float* xs;
    float* ys;
    float* zs;
    float* ws;
};

/**
 * simd friendly column-major 4x4 matrix
 * store four values for each matrix
 * @ingroup vmath
 */
struct ALIGN16 mat4f_simd
{
    union   {
        struct {
            float m11[4], m21[4], m31[4], m41[4];    /* col #1 */
            float m12[4], m22[4], m32[4], m42[4];    /* col #2 */
            float m13[4], m23[4], m33[4], m43[4];    /* col #3 */
            float m14[4], m24[4], m34[4], m44[4];    /* col #4 */
        };

        float f[4][4][4];
    };
};


/**
 * quaternion - 4 components
 * @ingroup vmath
 */
struct ALIGN16 quat4f
{
    union   {
        struct {
            float x, y, z, w;
        };
        float f[4];
    };
};

/**
 * transform - includes everything that represents object transform (rotation/position)
 * @ingroup vmath
 */
struct ALIGN16 xform3d
{
	struct vec4f p;
	struct quat4f q;
};


/* vec2i functions
 **
 * @ingroup vmath
 */
INLINE struct vec2i* vec2i_seti(struct vec2i* v, int x, int y)
{
    v->x = x;
    v->y = y;
    return v;
}

/**
 * @ingroup vmath
 */
INLINE struct vec2i* vec2i_setv(struct vec2i* r, const struct vec2i* v)
{
    r->x = v->x;
    r->y = v->y;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct vec2i* vec2i_setvp(struct vec2i* r, const int* f)
{
    r->x = f[0];
    r->y = f[1];
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct vec2i* vec2i_setzero(struct vec2i* r)
{
    r->x = 0;
    r->y = 0;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct vec2i* vec2i_add(struct vec2i* v, const struct vec2i* v1, const struct vec2i* v2)
{
    v->x = v1->x + v2->x;
    v->y = v1->y + v2->y;
    return v;
}

/**
 * @ingroup vmath
 */
INLINE struct vec2i* vec2i_sub(struct vec2i* v, const struct vec2i* v1, const struct vec2i* v2)
{
    v->x = v1->x - v2->x;
    v->y = v1->y - v2->y;
    return v;
}

/**
 * @ingroup vmath
 */
INLINE int vec2i_dot(const struct vec2i* v1, const struct vec2i* v2)
{
    return v1->x*v2->x + v1->y*v2->y;
}

/**
 * @ingroup vmath
 */
INLINE struct vec2i* vec2i_muls(struct vec2i* v, const struct vec2i* v1, int k)
{
    v->x = v1->x * k;
    v->y = v1->y * k;
    return v;
}

/**
 * @ingroup vmath
 */
INLINE int vec2i_isequal(const struct vec2i* v1, const struct vec2i* v2)
{
    return ((v1->x == v2->x) && (v1->y == v2->y));
}

/**
 * @ingroup vmath
 */
INLINE struct vec2f* vec2f_setf(struct vec2f* v, float x, float y)
{
    v->x = x;
    v->y = y;
    return v;
}

/**
 * @ingroup vmath
 */
INLINE struct vec2f* vec2f_setv(struct vec2f* r, const struct vec2f* v)
{
    r->x = v->x;
    r->y = v->y;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct vec2f* vec2f_setvp(struct vec2f* r, const float* f)
{
    r->x = f[0];
    r->y = f[1];
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct vec2f* vec2f_setzero(struct vec2f* r)
{
    r->x = 0.0f;
    r->y = 0.0f;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct vec2f* vec2f_add(struct vec2f* v, const struct vec2f* v1, const struct vec2f* v2)
{
    v->x = v1->x + v2->x;
    v->y = v1->y + v2->y;
    return v;
}

/**
 * @ingroup vmath
 */
INLINE struct vec2f* vec2f_sub(struct vec2f* v, const struct vec2f* v1, const struct vec2f* v2)
{
    v->x = v1->x - v2->x;
    v->y = v1->y - v2->y;
    return v;
}

/**
 * @ingroup vmath
 */
INLINE struct vec2f* vec2f_muls(struct vec2f* v, const struct vec2f* v1, float k)
{
    v->x = v1->x * k;
    v->y = v1->y * k;
    return v;
}

/**
 * @ingroup vmath
 */
INLINE float vec2f_len(const struct vec2f* v)
{
    return sqrt((v->x*v->x) + (v->y*v->y));
}

/**
 * @ingroup vmath
 */
INLINE int vec2f_isequal(const struct vec2f* v1, const struct vec2f* v2)
{
    return (math_isequal(v1->x, v2->x) && math_isequal(v1->y, v2->y));
}

/* vec3 functions
 **
 * @ingroup vmath
 */
INLINE struct vec4f* vec3_setf(struct vec4f* r, float x, float y, float z)
{
    r->x = x;
    r->y = y;
    r->z = z;
    r->w = 1.0f;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct vec4f* vec3_setv(struct vec4f* r, const struct vec4f* v)
{
    r->x = v->x;
    r->y = v->y;
    r->z = v->z;
    r->w = 1.0f;
    return r;
}

INLINE struct vec4f* vec3_setvp(struct vec3f* r, const float* fv)
{
    r->x = fv[0];
    r->y = fv[1];
    r->z = fv[2];
    r->w = 1.0f;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct vec4f* vec3_setzero(struct vec4f* r)
{
    r->x = 0.0f;
    r->y = 0.0f;
    r->z = 0.0f;
    r->w = 1.0f;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct vec4f* vec3_setunitx(struct vec4f* r)
{
    r->x = 1.0f;
    r->y = 0.0f;
    r->z = 0.0f;
    r->w = 1.0f;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct vec4f* vec4_setunity(struct vec4f* r)
{
    r->x = 0.0f;
    r->y = 1.0f;
    r->z = 0.0f;
    r->w = 1.0f;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct vec4f* vec4_setunitz(struct vec4f* r)
{
    r->x = 0.0f;
    r->y = 0.0f;
    r->z = 1.0f;
    r->w = 1.0f;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct vec4f* vec3_add(struct vec4f* r, const struct vec4f* v1, const struct vec4f* v2)
{
    r->x = v1->x + v2->x;
    r->y = v1->y + v2->y;
    r->z = v1->z + v2->z;
    r->w = 1.0f;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct vec4f* vec3_sub(struct vec4f* r, const struct vec4f* v1, const struct vec4f* v2)
{
    r->x = v1->x - v2->x;
    r->y = v1->y - v2->y;
    r->z = v1->z - v2->z;
    r->w = 1.0f;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct vec4f* vec3_muls(struct vec4f* r, const struct vec4f* v1, float k)
{
    r->x = v1->x * k;
    r->y = v1->y * k;
    r->z = v1->z * k;
    r->w = 1.0f;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE float vec3_dot(const struct vec4f* v1, const struct vec4f* v2)
{
    return (v1->x*v2->x + v1->y*v2->y + v1->z*v2->z);
}

/**
 * @ingroup vmath
 */
INLINE struct vec4f* vec3_cross(struct vec4f* r, const struct vec4f* v1, const struct vec4f* v2)
{
    return vec3_setf(r,
                     v1->y*v2->z - v1->z*v2->y,
                     v1->z*v2->x - v1->x*v2->z,
                     v1->x*v2->y - v1->y*v2->x);
}

/**
 * @ingroup vmath
 */
INLINE int vec3_isequal(const struct vec4f* v1, const struct vec4f* v2)
{
    return (math_isequal(v1->x, v2->x) && math_isequal(v1->y, v2->y) && math_isequal(v1->z, v2->z));
}

/**
 * @ingroup vmath
 */
INLINE float vec3_len(const struct vec4f* v)
{
    return sqrt(vec3_dot(v, v));
}

/**
 * @ingroup vmath
 */
INLINE struct vec4f* vec3_norm(struct vec4f* r, const struct vec4f* v)
{
    return vec3_muls(r, v, 1.0f/vec3_len(v));
}

/**
 * @ingroup vmath
 */
CORE_API float vec3_angle(const struct vec4f* v1, const struct vec4f* v2);

/**
 * lerps (linear-interpolation) between two vectors in space
 * @param t interpolator value between 0~1
 * @ingroup vmath
 */
CORE_API struct vec4f* vec3_lerp(struct vec4f* r, const struct vec4f* v1,
                                 const struct vec4f* v2, float t);

/**
 * Cubic interpolation between two vectors \n
 * Cubic interpolation needs 4 pointrs, two line segments that we should interpolate (v1, v2) ...\n
 * A point before the segment (v0) and a point after segment (v3)
 * @param t interpolator value which is between [0, 1]
 * @ingroup vmath
 */
CORE_API struct vec4f* vec3_cubic(struct vec4f* r, const struct vec4f* v0, const struct vec4f* v1,
    const struct vec4f* v2, const struct vec4f* v3, float t);

/**
 * transform vector3(x,y,z) by SRT(scale/rotation/translate) matrix, which is normally mat3f
 * @ingroup vmath
 */
CORE_API struct vec4f* vec3_transformsrt(struct vec4f* r, const struct vec4f* v,
                                         const struct mat3f* m);

/**
 * transform vector3(x,y,z) by SRT(scale/rotation/translate) matrix4x4
 * @ingroup vmath
 */
CORE_API struct vec4f* vec3_transformsrt_m4(struct vec4f* r, const struct vec4f* v,
    const struct mat4f* m);

/**
 * transform vector3(x,y,z) by SRT(scale/rotation) portion of the matrix, which is normally mat3f
 * @ingroup vmath
 */
CORE_API struct vec4f* vec3_transformsr(struct vec4f* r, const struct vec4f* v,
                                         const struct mat3f* m);

/* vec4 functions
 **
 * @ingroup vmath
 */
INLINE struct vec4f* vec4_setf(struct vec4f* r, float x, float y, float z, float w)
{
    r->x = x;
    r->y = y;
    r->z = z;
    r->w = w;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct vec4f* vec4_setv(struct vec4f* r, const struct vec4f* v)
{
    r->x = v->x;
    r->y = v->y;
    r->z = v->z;
    r->w = v->w;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct vec4f* vec4_setvp(struct vec3f* r, const float* fv)
{
    r->x = fv[0];
    r->y = fv[1];
    r->z = fv[2];
    r->w = fv[3];
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct vec4f* vec4_setzero(struct vec4f* r)
{
    r->x = 0.0f;
    r->y = 0.0f;
    r->z = 0.0f;
    r->w = 0.0f;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct vec4f* vec4_add(struct vec4f* r, const struct vec4f* v1, const struct vec4f* v2)
{
    r->x = v1->x + v2->x;
    r->y = v1->y + v2->y;
    r->z = v1->z + v2->z;
    r->w = v1->w + v2->w;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct vec4f* vec4_sub(struct vec4f* r, const struct vec4f* v1, const struct vec4f* v2)
{
    r->x = v1->x - v2->x;
    r->y = v1->y - v2->y;
    r->z = v1->z - v2->z;
    r->w = v1->w - v2->w;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct vec4f* vec4_muls(struct vec4f* r, const struct vec4f* v1, float k)
{
    r->x = v1->x * k;
    r->y = v1->y * k;
    r->z = v1->z * k;
    r->w = v1->w * k;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE int vec4_isequal(const struct vec4f* v1, const struct vec4f* v2)
{
    return (math_isequal(v1->x, v2->x) &&
            math_isequal(v1->y, v2->y) &&
            math_isequal(v1->z, v2->z) &&
            math_isequal(v1->w, v2->w));
}

/**
 * transform vector3(x,y,z,w) by 4x4 matrix, which is normally mat4f
 * @ingroup vmath
 */
CORE_API struct vec4f* vec4_transform(struct vec4f* r, const struct vec4f* v,
                                      const struct mat4f* m);

/**
 * @ingroup vmath
 */
INLINE struct quat4f* quat_setf(struct quat4f* r, float x, float y, float z, float w)
{
    r->x = x;
    r->y = y;
    r->z = z;
    r->w = w;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct quat4f* quat_setq(struct quat4f* r, const struct quat4f* q)
{
    r->x = q->x;
    r->y = q->y;
    r->z = q->z;
    r->w = q->w;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct quat4f* quat_setidentity(struct quat4f* r)
{
    r->x = 0.0f;
    r->y = 0.0f;
    r->z = 0.0f;
    r->w = 1.0f;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE int quat_isqual(const struct quat4f* q1, const struct quat4f* q2)
{
    return (math_isequal(q1->x, q2->x) &&
            math_isequal(q1->y, q2->y) &&
            math_isequal(q1->z, q2->z) &&
            math_isequal(q1->w, q2->w));
}

/**
 * multiply qutaernions, which is basically combining two quaternion rotations together
 * @ingroup vmath
 */
INLINE struct quat4f* quat_mul(struct quat4f* r, const struct quat4f* q1, const struct quat4f* q2)
{
    return quat_setf(r,
        q1->w*q2->x + q1->x*q2->w + q1->z*q2->y - q1->y*q2->z,
        q1->w*q2->y + q1->y*q2->w + q1->x*q2->z - q1->z*q2->x,
        q1->w*q2->z + q1->z*q2->w + q1->y*q2->x - q1->x*q2->y,
        q1->w*q2->w - q1->x*q2->x - q1->y*q2->y - q1->z*q2->z);
}

/**
 * inverse quaternion (conjucate), which inverses the rotation effect of the quaternion
 * @ingroup vmath
 */
INLINE struct quat4f* quat_inv(struct quat4f* r, const struct quat4f* q)
{
    r->w = q->w;
    r->x = -r->x;
    r->y = -r->y;
    r->z = -r->z;
    return r;
}

/**
 * slerp (spherical-linear interpolation) quaternion
 * @param t: interpolator between 0~1
 * @ingroup vmath
 */
CORE_API struct quat4f* quat_slerp(struct quat4f* r, const struct quat4f* q1,
                                   const struct quat4f* q2, float t);
/**
 * get the angle of the quaternion rotation
 * @ingroup vmath
 */
CORE_API float quat_getangle(const struct quat4f* q);
/**
 * get rotation axis of quaternion
 * @ingroup vmath
 */
CORE_API struct vec4f* quat_getrotaxis(struct vec4f* axis, const struct quat4f* q);
/**
 * get quaternion representation in euler values form
 * @ingroup vmath
 */
CORE_API void quat_geteuler(float* pitch, float* yaw, float* roll, const struct quat4f* q);
/**
 * set rotations for quaternion, by axis/angle, euler or from the transform matrix
 * @ingroup vmath
 */
CORE_API struct quat4f* quat_fromaxis(struct quat4f* r, const struct vec4f* axis, float angle);
/**
 * @ingroup vmath
 */
CORE_API struct quat4f* quat_fromeuler(struct quat4f* r, float pitch, float yaw, float roll);
/**
 * @ingroup vmath
 */
CORE_API struct quat4f* quat_frommat3(struct quat4f* r, const struct mat3f* mat);

/* mat3f functions
 **
 * @ingroup vmath
 */
CORE_API struct mat3f* mat3_setf(struct mat3f* r,
                                 float m11, float m12, float m13,
                                 float m21, float m22, float m23,
                                 float m31, float m32, float m33,
                                 float m41, float m42, float m43);
/**
 * @ingroup vmath
 */
CORE_API struct mat3f* mat3_setm(struct mat3f* r, const struct mat3f* m);
/**
 * @ingroup vmath
 */
CORE_API struct mat3f* mat3_setidentity(struct mat3f* r);
/**
 * @ingroup vmath
 */
CORE_API struct mat3f* mat3_muls(struct mat3f* r, struct mat3f* m, float k);
/**
 * @ingroup vmath
 */
CORE_API struct mat3f* mat3_add(struct mat3f* r, const struct mat3f* m1, const struct mat3f* m2);
/**
 * @ingroup vmath
 */
CORE_API struct mat3f* mat3_sub(struct mat3f* r, const struct mat3f* m1, const struct mat3f* m2);
/**
 * @ingroup vmath
 */
CORE_API struct mat3f* mat3_mul(struct mat3f* r, const struct mat3f* m1, const struct mat3f* m2);
/**
 * @ingroup vmath
 */
CORE_API struct mat3f* mat3_set_trans(struct mat3f* r, const struct vec4f* v);
/**
 * @ingroup vmath
 */
CORE_API struct mat3f* mat3_set_transf(struct mat3f* r, float x, float y, float z);
/**
 * @ingroup vmath
 */
CORE_API struct mat3f* mat3_set_scalef(struct mat3f* r, float x, float y, float z);
/**
 * @ingroup vmath
 */
CORE_API struct mat3f* mat3_set_scale(struct mat3f* r, const struct vec4f* s);
/**
 * @ingroup vmath
 */
CORE_API struct mat3f* mat3_set_rotaxis(struct mat3f* r, const struct vec4f* axis, float angle);
/**
 * @ingroup vmath
 */
CORE_API struct mat3f* mat3_set_roteuler(struct mat3f* r, float pitch, float yaw, float roll);
/**
 * @ingroup vmath
 */
CORE_API struct mat3f* mat3_set_rotquat(struct mat3f* r, const struct quat4f* q);
/**
 * @ingroup vmath
 */
CORE_API struct mat3f* mat3_set_trans_rot(struct mat3f* r, const struct vec3f* t,
                                          const struct quat4f* q);
/**
 * @ingroup vmath
 */
CORE_API struct mat3f* mat3_set_proj(struct mat3f* r, const struct vec4f* proj_plane_norm);
/**
 * @ingroup vmath
 */
CORE_API struct mat3f* mat3_set_refl(struct mat3f* r, const struct vec4f* refl_plane_norm);
/**
 * @ingroup vmath
 */
CORE_API struct mat4f* mat3_mul4(struct mat4f* r, const struct mat3f* m1, const struct mat4f* m2);
/**
 * @ingroup vmath
 */
CORE_API struct mat3f* mat3_inv(struct mat3f* r, const struct mat3f* m);
/**
 * @ingroup vmath
 */
CORE_API float mat3_det(const struct mat3f* m);
/**
 * @ingroup vmath
 */
CORE_API struct mat3f* mat3_transpose(struct mat3f* r, const struct mat3f* m);
/**
 * @ingroup vmath
 */
CORE_API struct mat3f* mat3_transpose_self(struct mat3f* r);
/**
 * @ingroup vmath
 */
CORE_API void mat3_get_roteuler(float* pitch, float* yaw, float* roll, const struct mat3f* m);
/**
 * @ingroup vmath
 */
CORE_API struct quat4f* mat3_get_rotquat(struct quat4f* q, const struct mat3f* m);

/**
 * @ingroup vmath
 */
INLINE struct vec4f* mat3_get_trans(struct vec4f* r, const struct mat3f* m)
{
    return vec3_setf(r, m->m41, m->m42, m->m43);
}

/**
 * @ingroup vmath
 */
INLINE void mat3_get_transf(float* x, float* y, float* z, const struct mat3f* m)
{
    *x = m->m41;
    *y = m->m42;
    *z = m->m43;
}

/**
 * @ingroup vmath
 */
INLINE struct vec4f* mat3_get_transv(struct vec4f* r, const struct mat3f* m)
{
    r->x = m->m41;
    r->y = m->m42;
    r->z = m->m43;
    r->w = 1.0f;
    return r;
}

/**
 * @ingroup vmath
 */
INLINE struct vec4f* mat3_get_xaxis(struct vec4f* r, const struct mat3f* m)
{
    return vec3_setf(r, m->m11, m->m12, m->m13);
}

/**
 * @ingroup vmath
 */
INLINE struct vec4f* mat3_get_yaxis(struct vec4f* r, const struct mat3f* m)
{
   return vec3_setf(r, m->m21, m->m22, m->m23);
}

/**
 * @ingroup vmath
 */
INLINE struct vec4f* mat3_get_zaxis(struct vec4f* r, const struct mat3f* m)
{
    return vec3_setf(r, m->m31, m->m32, m->m33);
}

/**
 * @ingroup vmath
 */
CORE_API struct mat3f* mat3_invrt(struct mat3f* r, const struct mat3f* m);

/**
 * @ingroup vmath
 */
CORE_API struct mat4f* mat4_setf(struct mat4f* r,
                                 float m11, float m12, float m13, float m14,
                                 float m21, float m22, float m23, float m24,
                                 float m31, float m32, float m33, float m34,
                                 float m41, float m42, float m43, float m44);
/**
 * @ingroup vmath
 */
CORE_API struct mat4f* mat4_setm(struct mat4f* r, const struct mat4f* m);
/**
 * @ingroup vmath
 */
CORE_API struct mat4f* mat4_setidentity(struct mat4f* r);
/**
 * @ingroup vmath
 */
CORE_API struct mat4f* mat4_muls(struct mat4f* r, struct mat4f* m, float k);
/**
 * @ingroup vmath
 */
CORE_API struct mat4f* mat4_add(struct mat4f* r, const struct mat4f* m1, const struct mat4f* m2);
/**
 * @ingroup vmath
 */
CORE_API struct mat4f* mat4_sub(struct mat4f* r, const struct mat4f* m1, const struct mat4f* m2);
/**
 * @ingroup vmath
 */
CORE_API struct mat4f* mat4_mul(struct mat4f* r, const struct mat4f* m1, const struct mat4f* m2);
/**
 * @ingroup vmath
 */
CORE_API struct mat4f* mat4_inv(struct mat4f* r, const struct mat4f* m);
/**
 * @ingroup vmath
 */
CORE_API float mat4_det(const struct mat4f* m);
/**
 * @ingroup vmath
 */
CORE_API struct mat4f* mat4_transpose(struct mat4f* r, const struct mat4f* m);
/**
 * @ingroup vmath
 */
CORE_API struct mat4f* mat4_transpose_self(struct mat4f* r);

/**
 * create simd vec4 by a given array count
 * @see vec4simd_destroy
 * @ingroup vmath
 */
CORE_API result_t vec4simd_create(struct vec4f_simd* v, struct allocator* alloc, uint cnt);

/**
 * destroy vec4-simd
 * @see vec4simd_create
 * @ingroup vmath
 */
CORE_API void vec4simd_destroy(struct vec4f_simd* v);


/**
 * @ingroup vmath
 */
CORE_API struct mat4f_simd* mat4simd_setm(struct mat4f_simd* r, const struct mat4f* m);

/**
 * @ingroup vmath
 */
INLINE struct xform3d* xform3d_setpq(struct xform3d* xform,
		const struct vec4f* p, const struct quat4f* q)
{
	vec3_setv(&xform->p, p);
	quat_setq(&xform->q, q);
	return xform;
}

/**
 * @ingroup vmath
 */
INLINE struct xform3d* xform3d_setf(struct xform3d* xform, float x, float y, float z,
		float pitch, float yaw, float roll)
{
	vec3_setf(&xform->p, x, y, z);
	quat_fromeuler(&xform->q, pitch, yaw, roll);
	return xform;
}

/**
 * @ingroup vmath
 */
INLINE struct xform3d* xform3d_setf_raw(struct xform3d* xform, float x, float y, float z,
    float rx, float ry, float rz, float rw)
{
    vec3_setf(&xform->p, x, y, z);
    quat_setf(&xform->q, rx, ry, rz, rw);
    return xform;
}

/**
 * @ingroup vmath
 */
INLINE struct xform3d* xform3d_frommat3(struct xform3d* xform, const struct mat3f* m)
{
	struct quat4f q;
	struct vec4f p;
	return xform3d_setpq(xform, mat3_get_trans(&p, m), quat_frommat3(&q, m));
}

/**
 * @ingroup vmath
 */
INLINE struct xform3d* xform3d_set(struct xform3d* r, const struct xform3d* xform)
{
	memcpy(r, xform, sizeof(struct xform3d));
	return r;
}

/**
 * @ingroup vmath
 */
INLINE struct mat3f* xform3d_getmat(struct mat3f* mat, const struct xform3d* xform)
{
    return mat3_set_trans_rot(mat, &xform->p, &xform->q);
}

/**
 * @ingroup vmath
 */
INLINE struct xform3d* xform3d_setidentity(struct xform3d* xform)
{
	vec3_setzero(&xform->p);
	quat_setidentity(&xform->q);
	return xform;
}

INLINE struct vec4i* vec4i_seti(struct vec4i* r, int x, int y, int z, int w)
{
    r->x = x;
    r->y = y;
    r->z = z;
    r->w = w;
    return r;
}

INLINE struct vec4i* vec4i_seta(struct vec4i* r, int a)
{
    r->x = a;
    r->y = a;
    r->z = a;
    r->w = a;
    return r;
}

INLINE struct vec4i* vec4i_add(struct vec4i* r, const struct vec4i* a, const struct vec4i* b)
{
    r->x = a->x + b->x;
    r->y = a->y + b->y;
    r->z = a->z + b->z;
    r->w = a->w + b->w;
    return r;
}

INLINE struct vec4i* vec4i_mul(struct vec4i* r, const struct vec4i* a, const struct vec4i* b)
{
    r->x = a->x * b->x;
    r->y = a->y * b->y;
    r->z = a->z * b->z;
    r->w = a->w * b->w;
    return r;
}

INLINE struct vec4i* vec4i_or(struct vec4i* r, const struct vec4i* a, const struct vec4i* b)
{
    r->x = a->x | b->x;
    r->y = a->y | b->y;
    r->z = a->z | b->z;
    r->w = a->w | b->w;
    return r;
}

#endif /* __VECMATH_H__ */
