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

#include "vec-math.h"
#include "err.h"

/* vec3 functions */
float vec3_angle(const struct vec4f* v1, const struct vec4f* v2)
{
    return math_acosf(vec3_dot(v1, v2)/(vec3_len(v1)*vec3_len(v2)));
}

struct vec4f* vec3_lerp(struct vec4f* r, const struct vec4f* v1, const struct vec4f* v2, float t)
{
    /* r = v1 + (v2 - v1)*t : t = [0, 1] */
    return vec3_add(r, v1, vec3_muls(r, vec3_sub(r, v2, v1), t));
}

struct vec4f* vec3_cubic(struct vec4f* r, const struct vec4f* v0, const struct vec4f* v1,
    const struct vec4f* v2, const struct vec4f* v3, float t)
{
    /* uses catmull-rom splines :
     * reference: http://paulbourke.net/miscellaneous/interpolation/
     */
#if !defined(_SIMD_SSE_)
    struct vec3f a0, a1, a2, a3, tmp;

    /* a0 = -0.5*v0 + 1.5*v1 - 1.5*v2 + 0.5*v3 */
    vec3_add(&a0, vec3_muls(&a0, v0, -0.5f), vec3_muls(&tmp, v1, 1.5f));
    vec3_add(&a0, &a0, vec3_muls(&tmp, v2, -1.5f));
    vec3_add(&a0, &a0, vec3_muls(&tmp, v3, 0.5f));

    vec3_add(&a1, v0, vec3_muls(&tmp, v1, -2.5f));
    vec3_add(&a1, &a1, vec3_muls(&tmp, v2, 2.0f));
    vec3_add(&a1, &a1, vec3_muls(&tmp, v3, -0.5f));

    vec3_add(&a2, vec3_muls(&a2, v0, -0.5f), vec3_muls(&tmp, v2, -0.5f));

    vec3_setv(&a3, v1);

    /* interpolate */
    float tt = t*t;
    vec3_add(r, vec3_muls(&a0, &a0, t*tt), vec3_muls(&a1, &a1, tt));
    vec3_add(r, vec3_muls(&a2, &a2, t), &a3);
#else
    simd_t mv0 = _mm_load_ps(v0->f);
    simd_t mv1 = _mm_load_ps(v1->f);
    simd_t mv2 = _mm_load_ps(v2->f);
    simd_t mv3 = _mm_load_ps(v3->f);
    simd_t mf0 = _mm_set1_ps(-0.5f);
    simd_t mf1 = _mm_set1_ps(-1.5f);

    /* a0 = -0.5*y0 + 1.5*y1 - 1.5*y2 + 0.5*y3 */
    simd_t a0 = _mm_mul_ps(mv0, mf0);
    a0 = _mm_sub_ps(a0, _mm_mul_ps(mv1, mf1));
    a0 = _mm_add_ps(a0, _mm_mul_ps(mv2, mf1));
    a0 = _mm_sub_ps(a0, _mm_mul_ps(mv3, mf0));

    /* a1 = y0 - 2.5*y1 + 2*y2 - 0.5*y3 */
    simd_t a1 = _mm_add_ps(mv0, _mm_mul_ps(mv1, _mm_set1_ps(-2.5f)));
    a1 = _mm_add_ps(a1, _mm_mul_ps(mv2, _mm_set1_ps(2.0f)));
    a1 = _mm_add_ps(a1, _mm_mul_ps(mv3, mf0));

    /* a2 = -0.5*y0 + 0.5*y2 */
    simd_t a2 = _mm_mul_ps(mv0, mf0);
    a2 = _mm_sub_ps(a2, _mm_mul_ps(mv2, mf0));

    /* a3 = y1 */
    simd_t a3 = mv1;

    /* interpolate */
    float tt = t*t;
    simd_t mr = _mm_add_ps(_mm_mul_ps(a0, _mm_set1_ps(tt*t)), _mm_mul_ps(a1, _mm_set1_ps(tt)));
    mr = _mm_add_ps(mr, _mm_mul_ps(a2, _mm_set1_ps(t)));
    mr = _mm_add_ps(mr, a3);
    _mm_store_ps(r->f, mr);
    r->w = 1.0;
#endif

    return r;
}

struct vec4f* vec3_transformsrt(struct vec4f* r, const struct vec4f* v, const struct mat3f* m)
{
#if defined(_SIMD_SSE_)
    simd_t row1 = _mm_load_ps(m->row1);
    simd_t row2 = _mm_load_ps(m->row2);
    simd_t row3 = _mm_load_ps(m->row3);
    simd_t row4 = _mm_load_ps(m->row4);
    simd_t vs = _mm_load_ps(v->f);
    simd_t rs;

    rs = _mm_mul_ps(_mm_all_x(vs), row1);
    rs = _mm_madd(_mm_all_y(vs), row2, rs);
    rs = _mm_madd(_mm_all_z(vs), row3, rs);
    rs = _mm_add_ps(rs, row4);
    _mm_store_ps(r->f, rs);
    return r;
#else
    return vec3_setf(r,
                     v->x*m->m11 + v->y*m->m21 + v->z*m->m31 + m->m41,
                     v->x*m->m12 + v->y*m->m22 + v->z*m->m32 + m->m42,
                     v->x*m->m13 + v->y*m->m23 + v->z*m->m33 + m->m43);
#endif
}

struct vec4f* vec3_transformsr(struct vec4f* r, const struct vec4f* v, const struct mat3f* m)
{
#if defined(_SIMD_SSE_)
    simd_t row1 = _mm_load_ps(m->row1);
    simd_t row2 = _mm_load_ps(m->row2);
    simd_t row3 = _mm_load_ps(m->row3);
    simd_t vs = _mm_load_ps(v->f);
    simd_t rs;

    rs = _mm_mul_ps(_mm_all_x(vs), row1);
    rs = _mm_madd(_mm_all_y(vs), row2, rs);
    rs = _mm_madd(_mm_all_z(vs), row3, rs);
    _mm_store_ps(r->f, rs);
    return r;
#else
    return vec3_setf(r,
                     v->x*m->m11 + v->y*m->m21 + v->z*m->m31,
                     v->x*m->m12 + v->y*m->m22 + v->z*m->m32,
                     v->x*m->m13 + v->y*m->m23 + v->z*m->m33);
#endif
}

struct vec4f* vec3_transformsrt_m4(struct vec4f* r, const struct vec4f* v, const struct mat4f* m)
{
#if defined(_SIMD_SSE_)
    simd_t row1 = _mm_load_ps(m->row1);
    simd_t row2 = _mm_load_ps(m->row2);
    simd_t row3 = _mm_load_ps(m->row3);
    simd_t row4 = _mm_load_ps(m->row4);
    simd_t vs = _mm_load_ps(v->f);
    simd_t rs;

    rs = _mm_mul_ps(_mm_all_x(vs), row1);
    rs = _mm_madd(_mm_all_y(vs), row2, rs);
    rs = _mm_madd(_mm_all_z(vs), row3, rs);
    rs = _mm_add_ps(rs, row4);
    _mm_store_ps(r->f, rs);
    return r;
#else
    return vec3_setf(r,
        v->x*m->m11 + v->y*m->m21 + v->z*m->m31 + m->m41,
        v->x*m->m12 + v->y*m->m22 + v->z*m->m32 + m->m42,
        v->x*m->m13 + v->y*m->m23 + v->z*m->m33 + m->m43);
#endif
}

/* vec4 functions */
struct vec4f* vec4_transform(struct vec4f* r, const struct vec4f* v, const struct mat4f* m)
{
#if defined(_SIMD_SSE_)
    simd_t row1 = _mm_load_ps(m->row1);
    simd_t row2 = _mm_load_ps(m->row2);
    simd_t row3 = _mm_load_ps(m->row3);
    simd_t row4 = _mm_load_ps(m->row4);
    simd_t vs = _mm_load_ps(v->f);
    simd_t rs;

    rs = _mm_mul_ps(_mm_all_x(vs), row1);
    rs = _mm_madd(_mm_all_y(vs), row2, rs);
    rs = _mm_madd(_mm_all_z(vs), row3, rs);
    rs = _mm_madd(_mm_all_w(vs), row4, rs);
    _mm_store_ps(r->f, rs);
    return r;
#else
    return vec4_setf(r,
                     v->x*m->m11 + v->y*m->m21 + v->z*m->m31 + v->w*m->m41,
                     v->x*m->m12 + v->y*m->m22 + v->z*m->m32 + v->w*m->m42,
                     v->x*m->m13 + v->y*m->m23 + v->z*m->m33 + v->w*m->m43,
                     v->x*m->m14 + v->y*m->m24 + v->z*m->m34 + v->w*m->m44);
#endif
}

/* quat4f functions */
/* reference: http://www.euclideanspace.com/maths/algebra/realNormedAlgebra/quaternions/slerp/ */
struct quat4f* quat_slerp(struct quat4f* r,
                          const struct quat4f* q1, const struct quat4f* q2, float t)
{
    /* t = [0, 1] */
    struct quat4f qa;
    struct quat4f qb;
    quat_setq(&qa, q1);
    quat_setq(&qb, q2);

    float cos_ht = qa.x*qb.x + qa.y*qb.y + qa.z*qb.z + qa.w*qb.w;
    if (cos_ht < 0.0f)  {
        qb.x = -qb.x;   qb.y = -qb.y;   qb.z = -qb.z;   qb.w = -qb.w;
        cos_ht = -cos_ht;
    }

    if (fabs(cos_ht) >= 1.0f)    {
        return quat_setq(r, &qa);
    }

    float ht = acosf(cos_ht);
    float sin_ht = sqrtf(1.0f - cos_ht*cos_ht);
    if (fabsf(sin_ht) < 0.001f)  {
        return quat_setf(r,
                         qa.x*0.5f + qb.x*0.5f,
                         qa.y*0.5f + qb.y*0.5f,
                         qa.z*0.5f + qb.z*0.5f,
                         qa.w*0.5f + qb.w*0.5f);
    }

    float k1 = sinf((1.0f - t)*ht)/sin_ht;
    float k2 = sinf(t*ht)/sin_ht;
    return quat_setf(r,
                     qa.x*k1 + qb.x*k2,
                     qa.y*k1 + qb.y*k2,
                     qa.z*k1 + qb.z*k2,
                     qa.w*k1 + qb.w*k2);

}

float quat_getangle(const struct quat4f* q)
{
    float th = math_acosf(q->w);
    return (th * 2.0f);
}

struct vec4f* quat_getrotaxis(struct vec4f* axis, const struct quat4f* q)
{
    float sin_t2sq = 1.0f - q->w*q->w;
    if (sin_t2sq <= 0.0f)    {
        return vec3_setf(axis, 1.0f, 0.0f, 0.0f);
    }

    float inv_sin_t2 = 1.0f / sqrtf(sin_t2sq);
    return vec3_setf(axis, q->x*inv_sin_t2, q->y*inv_sin_t2, q->z*inv_sin_t2);
}

/* reference: http://forums.create.msdn.com/forums/t/4574.aspx */
void quat_geteuler(float* pitch, float* yaw, float* roll, const struct quat4f* q)
{
    const float epsilon = 0.0009765625f;
    const float threshold = 0.5f - epsilon;
    float xy = q->x*q->y;
    float zw = q->z*q->w;
    float t = xy + zw;

    if ((t < -threshold) || (t > threshold))    {
        float sign = math_sign(t);
        *pitch = sign * PI_HALF;
        *yaw = sign * 2.0f * atan2f(q->x, q->w);
        *roll = 0.0f;
    }    else    {
        float xx = q->x * q->x;
        float xz = q->x * q->z;
        float xw = q->x * q->w;

        float yy = q->y * q->y;
        float yw = q->y * q->w;
        float yz = q->y * q->z;

        float zz = q->z * q->z;

        *yaw = atan2f(2 * yw - 2.0f * xz, 1.0f - 2.0f * yy - 2.0f * zz);
        *pitch = atan2f(2 * xw - 2.0f * yz, 1.0f - 2.0f * xx - 2.0f * zz);
        *roll = asinf(2.0f * t);
    }
}

struct quat4f* quat_fromaxis(struct quat4f* r, const struct vec4f* axis, float angle)
{
    /* calculate the half angle and it's sine */
    float th = angle * 0.5f;
    float sin_th = sinf(th);

    return quat_setf(r, axis->x*sin_th, axis->y*sin_th, axis->z*sin_th, cosf(th));
}

struct quat4f* quat_fromeuler(struct quat4f* r, float pitch, float yaw, float roll)
{
    float sp = sinf(pitch * 0.5f);
    float sy = sinf(yaw * 0.5f);
    float sr = sinf(roll * 0.5f);
    float cp = sqrtf(1.0f - sp*sp);
    float cy = sqrtf(1.0f - sy*sy);
    float cr = sqrtf(1.0f - sr*sr);

    r->x = cy*sp*cr + sy*cp*sr;
    r->y = -cy*sp*sr + sy*cp*cr;
    r->z = -sy*sp*cr + cy*cp*sr;
    r->w = cy*cp*cr + sy*sp*sr;
    return r;
}

struct quat4f* quat_frommat3(struct quat4f* r, const struct mat3f* mat)
{
    float w_sqminus1 = mat->m11 + mat->m22 + mat->m33;
    float x_sqminus1 = mat->m11 - mat->m22 - mat->m33;
    float y_sqminus1 = mat->m22 - mat->m11 - mat->m33;
    float z_sqminus1 = mat->m33 - mat->m11 - mat->m22;

    int biggest = 0;
    float biggest_sqminus1 = w_sqminus1;
    if (x_sqminus1 > biggest_sqminus1)   {
        biggest_sqminus1 = x_sqminus1;
        biggest = 1;
    }
    if (y_sqminus1 > biggest_sqminus1)   {
        biggest_sqminus1 = y_sqminus1;
        biggest = 2;
    }
    if (z_sqminus1 > biggest_sqminus1)   {
        biggest_sqminus1 = z_sqminus1;
        biggest = 3;
    }

    float biggest_val = sqrtf(biggest_sqminus1 + 1.0f) * 0.5f;
    float mult = 0.25f / biggest_val;

    switch (biggest)  {
        case 0:
            r->w = biggest_val;
            r->x = (mat->m23 - mat->m32) * mult;
            r->y = (mat->m31 - mat->m13) * mult;
            r->z = (mat->m12 - mat->m21) * mult;
            break;
        case 1:
            r->x = biggest_val;
            r->w = (mat->m23 - mat->m32) * mult;
            r->y = (mat->m12 + mat->m21) * mult;
            r->z = (mat->m31 + mat->m13) * mult;
            break;
        case 2:
            r->y = biggest_val;
            r->w = (mat->m31 - mat->m13) * mult;
            r->x = (mat->m12 + mat->m21) * mult;
            r->z = (mat->m23 + mat->m32) * mult;
            break;
        case 3:
            r->z = biggest_val;
            r->w = (mat->m12 - mat->m21) * mult;
            r->x = (mat->m31 + mat->m13) * mult;
            r->y = (mat->m23 + mat->m32) * mult;
            break;
    }
    return r;
}

/* mat3f functions */
struct mat3f* mat3_setf(struct mat3f* r,
                        float m11, float m12, float m13,
                        float m21, float m22, float m23,
                        float m31, float m32, float m33,
                        float m41, float m42, float m43)
{
    r->m11 = m11;   r->m21 = m21;   r->m31 = m31;   r->m41 = m41;
    r->m12 = m12;   r->m22 = m22;   r->m32 = m32;   r->m42 = m42;
    r->m13 = m13;   r->m23 = m23;   r->m33 = m33;   r->m43 = m43;
    r->m14 = 0.0f;  r->m24 = 0.0f;  r->m34 = 0.0f;  r->m44 = 1.0f;
    return r;
}

struct mat3f* mat3_setm(struct mat3f* r, const struct mat3f* m)
{
    r->m11 = m->m11;   r->m21 = m->m21;   r->m31 = m->m31;   r->m41 = m->m41;
    r->m12 = m->m12;   r->m22 = m->m22;   r->m32 = m->m32;   r->m42 = m->m42;
    r->m13 = m->m13;   r->m23 = m->m23;   r->m33 = m->m33;   r->m43 = m->m43;
    return r;
}

struct mat3f* mat3_setidentity(struct mat3f* r)
{
    r->m11 = 1.0f;   r->m21 = 0.0f;   r->m31 = 0.0f;   r->m41 = 0.0f;
    r->m12 = 0.0f;   r->m22 = 1.0f;   r->m32 = 0.0f;   r->m42 = 0.0f;
    r->m13 = 0.0f;   r->m23 = 0.0f;   r->m33 = 1.0f;   r->m43 = 0.0f;
    return r;
}

struct mat3f* mat3_muls(struct mat3f* r, struct mat3f* m, float k)
{
#if defined(_SIMD_SSE_)
    simd_t k_ = _mm_set_ps1(k);
    _mm_store_ps(r->row1, _mm_mul_ps(_mm_load_ps(m->row1), k_));
    _mm_store_ps(r->row2, _mm_mul_ps(_mm_load_ps(m->row2), k_));
    _mm_store_ps(r->row3, _mm_mul_ps(_mm_load_ps(m->row3), k_));
    _mm_store_ps(r->row4, _mm_mul_ps(_mm_load_ps(m->row4), k_));
#else
    r->m11 *= k;   r->m21 *= k;   r->m31 *= k;   r->m41 *= k;
    r->m12 *= k;   r->m22 *= k;   r->m32 *= k;   r->m42 *= k;
    r->m13 *= k;   r->m23 *= k;   r->m33 *= k;   r->m43 *= k;
#endif
    return r;
}

struct mat3f* mat3_add(struct mat3f* r, const struct mat3f* m1, const struct mat3f* m2)
{
#if defined(_SIMD_SSE_)
    _mm_store_ps(r->row1, _mm_add_ps(_mm_load_ps(m1->row1), _mm_load_ps(m2->row1)));
    _mm_store_ps(r->row2, _mm_add_ps(_mm_load_ps(m1->row2), _mm_load_ps(m2->row2)));
    _mm_store_ps(r->row3, _mm_add_ps(_mm_load_ps(m1->row3), _mm_load_ps(m2->row3)));
    _mm_store_ps(r->row4, _mm_add_ps(_mm_load_ps(m1->row4), _mm_load_ps(m2->row4)));
    r->m44 = 1.0f;  /* beware of LHS in serial operations */
#else
    r->m11 = m1->m11 + m2->m11;
    r->m21 = m1->m21 + m2->m21;
    r->m31 = m1->m31 + m2->m31;
    r->m41 = m1->m41 + m2->m41;

    r->m12 = m1->m12 + m2->m12;
    r->m22 = m1->m22 + m2->m22;
    r->m32 = m1->m32 + m2->m32;
    r->m42 = m1->m42 + m1->m42;

    r->m13 = m1->m13 + m2->m13;
    r->m23 = m1->m23 + m2->m23;
    r->m33 = m1->m33 + m2->m33;
    r->m43 = m1->m43 + m2->m43;
#endif

    return r;
}

struct mat3f* mat3_sub(struct mat3f* r, const struct mat3f* m1, const struct mat3f* m2)
{
#if defined(_SIMD_SSE_)
    _mm_store_ps(r->row1, _mm_sub_ps(_mm_load_ps(m1->row1), _mm_load_ps(m2->row1)));
    _mm_store_ps(r->row2, _mm_sub_ps(_mm_load_ps(m1->row2), _mm_load_ps(m2->row2)));
    _mm_store_ps(r->row3, _mm_sub_ps(_mm_load_ps(m1->row3), _mm_load_ps(m2->row3)));
    _mm_store_ps(r->row4, _mm_sub_ps(_mm_load_ps(m1->row4), _mm_load_ps(m2->row4)));
    r->m44 = 1.0f;  /* beware of LHS in serial operations */
#else
    r->m11 = m1->m11 - m2->m11;
    r->m21 = m1->m21 - m2->m21;
    r->m31 = m1->m31 - m2->m31;
    r->m41 = m1->m41 - m2->m41;

    r->m12 = m1->m12 - m2->m12;
    r->m22 = m1->m22 - m2->m22;
    r->m32 = m1->m32 - m2->m32;
    r->m42 = m1->m42 - m1->m42;

    r->m13 = m1->m13 - m2->m13;
    r->m23 = m1->m23 - m2->m23;
    r->m33 = m1->m33 - m2->m33;
    r->m43 = m1->m43 - m2->m43;
#endif

    return r;
}

struct mat3f* mat3_mul(struct mat3f* r, const struct mat3f* m1, const struct mat3f* m2)
{
#if defined(_SIMD_SSE_)
    /* transform rows of first matrix (m1) by the second matrix (m2)
     * also see 'vec3_transformsrt'
     */
    simd_t row1 = _mm_load_ps(m2->row1);
    simd_t row2 = _mm_load_ps(m2->row2);
    simd_t row3 = _mm_load_ps(m2->row3);
    simd_t row4 = _mm_load_ps(m2->row4);
    simd_t vs;
    simd_t rs;

    /* m1->row1 (assume w = 0.0f, so last operation is ignored) */
    vs = _mm_load_ps(m1->row1);
    rs = _mm_mul_ps(_mm_all_x(vs), row1);
    rs = _mm_madd(_mm_all_y(vs), row2, rs);
    rs = _mm_madd(_mm_all_z(vs), row3, rs);
    _mm_store_ps(r->row1, rs);
    /* m1->row2 (assume w = 0.0f, so last operation is ignored) */
    vs = _mm_load_ps(m1->row2);
    rs = _mm_mul_ps(_mm_all_x(vs), row1);
    rs = _mm_madd(_mm_all_y(vs), row2, rs);
    rs = _mm_madd(_mm_all_z(vs), row3, rs);
    _mm_store_ps(r->row2, rs);
    /* m1->row3 (assume w = 0.0f, so last operation is ignored) */
    vs = _mm_load_ps(m1->row3);
    rs = _mm_mul_ps(_mm_all_x(vs), row1);
    rs = _mm_madd(_mm_all_y(vs), row2, rs);
    rs = _mm_madd(_mm_all_z(vs), row3, rs);
    _mm_store_ps(r->row3, rs);
    /* m1->row4 (assume w = 1.0f, so we have an add for last op) */
    vs = _mm_load_ps(m1->row4);
    rs = _mm_mul_ps(_mm_all_x(vs), row1);
    rs = _mm_madd(_mm_all_y(vs), row2, rs);
    rs = _mm_madd(_mm_all_z(vs), row3, rs);
    rs = _mm_add_ps(rs, row4);
    _mm_store_ps(r->row4, rs);

    return r;
#else
    return mat3_setf(r,
                    m1->m11*m2->m11 + m1->m12*m2->m21 + m1->m13*m2->m31,
                    m1->m11*m2->m12 + m1->m12*m2->m22 + m1->m13*m2->m32,
                    m1->m11*m2->m13 + m1->m12*m2->m23 + m1->m13*m2->m33,
                    m1->m21*m2->m11 + m1->m22*m2->m21 + m1->m23*m2->m31,
                    m1->m21*m2->m12 + m1->m22*m2->m22 + m1->m23*m2->m32,
                    m1->m21*m2->m13 + m1->m22*m2->m23 + m1->m23*m2->m33,
                    m1->m31*m2->m11 + m1->m32*m2->m21 + m1->m33*m2->m31,
                    m1->m31*m2->m12 + m1->m32*m2->m22 + m1->m33*m2->m32,
                    m1->m31*m2->m13 + m1->m32*m2->m23 + m1->m33*m2->m33,
                    m1->m41*m2->m11 + m1->m42*m2->m21 + m1->m43*m2->m31 + m2->m41,
                    m1->m41*m2->m12 + m1->m42*m2->m22 + m1->m43*m2->m32 + m2->m42,
                    m1->m41*m2->m13 + m1->m42*m2->m23 + m1->m43*m2->m33 + m2->m43);
#endif
}

struct mat4f* mat3_mul4(struct mat4f* r, const struct mat3f* m1, const struct mat4f* m2)
{
#if defined(_SIMD_SSE_)
    /* transform rows of first matrix (m1) by the second matrix (m2)
     * also see 'vec3_transformsrt'
     */
    simd_t row1 = _mm_load_ps(m2->row1);
    simd_t row2 = _mm_load_ps(m2->row2);
    simd_t row3 = _mm_load_ps(m2->row3);
    simd_t row4 = _mm_load_ps(m2->row4);
    simd_t vs;
    simd_t rs;

    /* m1->row1 (assume w = 0.0f, so last operation is ignored) */
    vs = _mm_load_ps(m1->row1);
    rs = _mm_mul_ps(_mm_all_x(vs), row1);
    rs = _mm_madd(_mm_all_y(vs), row2, rs);
    rs = _mm_madd(_mm_all_z(vs), row3, rs);
    _mm_store_ps(r->row1, rs);
    /* m1->row2 (assume w = 0.0f, so last operation is ignored) */
    vs = _mm_load_ps(m1->row2);
    rs = _mm_mul_ps(_mm_all_x(vs), row1);
    rs = _mm_madd(_mm_all_y(vs), row2, rs);
    rs = _mm_madd(_mm_all_z(vs), row3, rs);
    _mm_store_ps(r->row2, rs);
    /* m1->row3 (assume w = 0.0f, so last operation is ignored) */
    vs = _mm_load_ps(m1->row3);
    rs = _mm_mul_ps(_mm_all_x(vs), row1);
    rs = _mm_madd(_mm_all_y(vs), row2, rs);
    rs = _mm_madd(_mm_all_z(vs), row3, rs);
    _mm_store_ps(r->row3, rs);
    /* m1->row4 (assume w = 1.0f, so we have an add for last op) */
    vs = _mm_load_ps(m1->row4);
    rs = _mm_mul_ps(_mm_all_x(vs), row1);
    rs = _mm_madd(_mm_all_y(vs), row2, rs);
    rs = _mm_madd(_mm_all_z(vs), row3, rs);
    rs = _mm_add_ps(rs, row4);
    _mm_store_ps(r->row4, rs);

    return r;
#else
    return mat4_setf(r,
                     m1->m11*m2->m11 + m1->m12*m2->m21 + m1->m13*m2->m31,
                     m1->m11*m2->m12 + m1->m12*m2->m22 + m1->m13*m2->m32,
                     m1->m11*m2->m13 + m1->m12*m2->m23 + m1->m13*m2->m33,
                     m1->m11*m2->m14 + m1->m12*m2->m24 + m1->m13*m2->m34,
                     m1->m21*m2->m11 + m1->m22*m2->m21 + m1->m23*m2->m31,
                     m1->m21*m2->m12 + m1->m22*m2->m22 + m1->m23*m2->m32,
                     m1->m21*m2->m13 + m1->m22*m2->m23 + m1->m23*m2->m33,
                     m1->m21*m2->m14 + m1->m22*m2->m24 + m1->m23*m2->m34,
                     m1->m31*m2->m11 + m1->m32*m2->m21 + m1->m33*m2->m31,
                     m1->m31*m2->m12 + m1->m32*m2->m22 + m1->m33*m2->m32,
                     m1->m31*m2->m13 + m1->m32*m2->m23 + m1->m33*m2->m33,
                     m1->m31*m2->m14 + m1->m32*m2->m24 + m1->m33*m2->m34,
                     m1->m41*m2->m11 + m1->m42*m2->m21 + m1->m43*m2->m31 + m2->m41,
                     m1->m41*m2->m12 + m1->m42*m2->m22 + m1->m43*m2->m32 + m2->m42,
                     m1->m41*m2->m13 + m1->m42*m2->m23 + m1->m43*m2->m33 + m2->m43,
                     m1->m41*m2->m14 + m1->m42*m2->m24 + m1->m43*m2->m34 + m2->m44);
#endif
}

struct mat3f* mat3_set_trans(struct mat3f* r, const struct vec4f* v)
{
    r->m41 = v->x;      r->m42 = v->y;      r->m43 = v->z;
    return r;
}

struct mat3f* mat3_set_transf(struct mat3f* r, float x, float y, float z)
{
    r->m41 = x;         r->m42 = y;         r->m43 = z;
    return r;
}

struct mat3f* mat3_set_rotaxis(struct mat3f* r, const struct vec4f* axis, float angle)
{
    float s = sin(angle);
    float c = cos(angle);

    float a = 1.0f - c;
    float ax = a * axis->x;
    float ay = a * axis->y;
    float az = a * axis->z;

    r->m11 = ax*axis->x + c;
    r->m12 = ax*axis->y + axis->z*s;
    r->m13 = ax*axis->z - axis->y*s;

    r->m21 = ay*axis->x - axis->z*s;
    r->m22 = ay*axis->y + c;
    r->m23 = ay*axis->z + axis->x*s;

    r->m31 = az*axis->x + axis->y*s;
    r->m32 = az*axis->y - axis->x*s;
    r->m33 = az*axis->z + c;

    return r;
}

struct mat3f* mat3_set_roteuler(struct mat3f* r, float pitch, float yaw, float roll)
{
    float sp = sinf(pitch);
    float cp = cosf(pitch);
    float sy = sinf(yaw);
    float cy = cosf(yaw);
    float sr = sinf(roll);
    float cr = cosf(roll);

    r->m11 = cy*cr + sy*sp*sr;
    r->m21 = -cy*sr + sy*sp*cr;
    r->m31 = sy*cp;

    r->m12 = sr*cp;
    r->m22 = cr*cp;
    r->m32 = -sp;

    r->m13 = -sy*cr + cy*sp*sr;
    r->m23 = sr*sy + cy*sp*cr;
    r->m33 = cy*cp;

    return r;
}

struct mat3f* mat3_set_rotquat(struct mat3f* r, const struct quat4f* q)
{
    float x2 = q->x*q->x;
    float y2 = q->y*q->y;
    float z2 = q->z*q->z;

    float xy = q->x*q->y;
    float xz = q->x*q->z;
    float yz = q->y*q->z;
    float wx = q->w*q->x;
    float wy = q->w*q->y;
    float wz = q->w*q->z;

    r->m11 = 1.0f - 2.0f*(y2 + z2);
    r->m12 = 2.0f * (xy + wz);
    r->m13 = 2.0f * (xz - wy);

    r->m21 = 2.0f * (xy - wz);
    r->m22 = 1.0f - 2.0f*(x2 + z2);
    r->m23 = 2.0f * (yz + wx);

    r->m31 = 2.0f * (xz + wy);
    r->m32 = 2.0f * (yz - wx);
    r->m33 = 1.0f - 2.0f*(x2 + y2);

    return r;
}

struct mat3f* mat3_set_trans_rot(struct mat3f* r, const struct vec3f* t, const struct quat4f* q)
{
    float x2 = q->x*q->x;
    float y2 = q->y*q->y;
    float z2 = q->z*q->z;

    float xy = q->x*q->y;
    float xz = q->x*q->z;
    float yz = q->y*q->z;
    float wx = q->w*q->x;
    float wy = q->w*q->y;
    float wz = q->w*q->z;

    r->m11 = 1.0f - 2.0f*(y2 + z2);
    r->m12 = 2.0f * (xy + wz);
    r->m13 = 2.0f * (xz - wy);

    r->m21 = 2.0f * (xy - wz);
    r->m22 = 1.0f - 2.0f*(x2 + z2);
    r->m23 = 2.0f * (yz + wx);

    r->m31 = 2.0f * (xz + wy);
    r->m32 = 2.0f * (yz - wx);
    r->m33 = 1.0f - 2.0f*(x2 + y2);

    r->m41 = t->x;
    r->m42 = t->y;
    r->m43 = t->z;

    r->m14 = 0.0f;
    r->m24 = 0.0f;
    r->m34 = 0.0f;
    r->m44 = 1.0f;

    return r;
}

struct mat3f* mat3_set_proj(struct mat3f* r, const struct vec4f* proj_plane_norm)
{
    r->m11 = 1.0f - proj_plane_norm->x*proj_plane_norm->x;
    r->m22 = 1.0f - proj_plane_norm->y*proj_plane_norm->y;
    r->m33 = 1.0f - proj_plane_norm->z*proj_plane_norm->z;

    r->m12 = r->m21 = -proj_plane_norm->x*proj_plane_norm->y;
    r->m13 = r->m31 = -proj_plane_norm->x*proj_plane_norm->z;
    r->m23 = r->m32 = -proj_plane_norm->y*proj_plane_norm->z;

    return r;
}

struct mat3f* mat3_set_refl(struct mat3f* r, const struct vec4f* refl_plane_norm)
{
    float ax = -2.0f * refl_plane_norm->x;
    float ay = -2.0f * refl_plane_norm->y;
    float az = -2.0f * refl_plane_norm->z;

    r->m11 = 1.0f + ax*refl_plane_norm->x;
    r->m22 = 1.0f + ay*refl_plane_norm->y;
    r->m33 = 1.0f + az*refl_plane_norm->z;

    r->m12 = r->m21 = ax*refl_plane_norm->y;
    r->m13 = r->m31 = ax*refl_plane_norm->z;
    r->m23 = r->m32 = ay*refl_plane_norm->z;

    return r;
}

struct mat3f* mat3_inv(struct mat3f* r, const struct mat3f* m)
{
    float invDet = 1.0f / mat3_det(m);
    float tx = m->m41;
    float ty = m->m42;
    float tz = m->m43;

    mat3_setf(r,
               (m->m22*m->m33 - m->m23*m->m32) * invDet,
               (m->m13*m->m32 - m->m12*m->m33) * invDet,
               (m->m12*m->m23 - m->m13*m->m22) * invDet,
               (m->m23*m->m31 - m->m21*m->m33) * invDet,
               (m->m11*m->m33 - m->m13*m->m31) * invDet,
               (m->m13*m->m21 - m->m11*m->m23) * invDet,
               (m->m21*m->m32 - m->m22*m->m31) * invDet,
               (m->m12*m->m31 - m->m11*m->m32) * invDet,
               (m->m11*m->m22 - m->m12*m->m21) * invDet,
               m->m41, m->m42, m->m43);

    r->m41 = -(tx*r->m11 + ty*r->m21 + tz*r->m31);
    r->m42 = -(tx*r->m12 + ty*r->m22 + tz*r->m32);
    r->m43 = -(tx*r->m13 + ty*r->m23 + tz*r->m33);
    return r;
}

struct mat3f* mat3_invrt(struct mat3f* r, const struct mat3f* m)
{
    /* inverse matrix with rotation/translation parts
     * so Det(Rotation-Part) = 1, */
    return mat3_setf(r,
        m->m11, m->m21, m->m31,
        m->m12, m->m22, m->m32,
        m->m13, m->m23, m->m33,
        -(m->m41*m->m11 + m->m42*m->m12 + m->m43*m->m13),
        -(m->m41*m->m21 + m->m42*m->m22 + m->m43*m->m23),
        -(m->m41*m->m31 + m->m42*m->m32 + m->m43*m->m33));
}

float mat3_det(const struct mat3f* m)
{
    return  (m->m11 * (m->m22*m->m33 - m->m23*m->m32) +
            m->m12 * (m->m23*m->m31 - m->m21*m->m33) +
            m->m13 * (m->m21*m->m32 - m->m22*m->m31));
}

struct mat3f* mat3_transpose(struct mat3f* r, const struct mat3f* m)
{
    r->m11 = m->m11;   r->m21 = m->m12;   r->m31 = m->m13;
    r->m12 = m->m21;   r->m22 = m->m22;   r->m32 = m->m23;
    r->m13 = m->m31;   r->m23 = m->m32;   r->m33 = m->m33;
    r->m41 = m->m41;   r->m42 = m->m42;   r->m43 = m->m43;  /* transform stays the same */
    return r;
}

struct mat3f* mat3_transpose_self(struct mat3f* r)
{
    swapf(&r->m21, &r->m12);
    swapf(&r->m31, &r->m13);
    swapf(&r->m23, &r->m32);
    return r;
}

void mat3_get_roteuler(float* pitch, float* yaw, float* roll, const struct mat3f* m)
{
    *pitch = asin(-m->m32);

    const float threshold = 0.001f;
    float t = cos(*pitch);

    if (t > threshold)  {
        *roll = atan2(m->m12, m->m22);
        *yaw = atan2(m->m31, m->m33);
    }    else    {
        *roll = atan2(-m->m21, m->m11);
        *yaw = 0.0f;
    }
}

struct quat4f* mat3_get_rotquat(struct quat4f* q, const struct mat3f* m)
{
    return quat_frommat3(q, m);
}

struct mat3f* mat3_set_scalef(struct mat3f* r, float x, float y, float z)
{
    r->m11 = x;     r->m22 = y;     r->m33 = z;
    return r;

}

struct mat3f* mat3_set_scale(struct mat3f* r, const struct vec4f* s)
{
    r->m11 = s->x;  r->m22 = s->y;  r->m33 = s->z;
    return r;
}

/* mat4f functions */
struct mat4f* mat4_setf(struct mat4f* r,
                        float m11, float m12, float m13, float m14,
                        float m21, float m22, float m23, float m24,
                        float m31, float m32, float m33, float m34,
                        float m41, float m42, float m43, float m44)
{
    r->m11 = m11;      r->m21 = m21;      r->m31 = m31;      r->m41 = m41;
    r->m12 = m12;      r->m22 = m22;      r->m32 = m32;      r->m42 = m42;
    r->m13 = m13;      r->m23 = m23;      r->m33 = m33;      r->m43 = m43;
    r->m14 = m14;      r->m24 = m24;      r->m34 = m34;      r->m44 = m44;
    return r;
}

struct mat4f* mat4_setm(struct mat4f* r, const struct mat4f* m)
{
    memcpy(r, m, sizeof(struct mat4f));
    return r;
}

struct mat4f* mat4_setidentity(struct mat4f* r)
{
    memset(r, 0x00, sizeof(struct mat4f));
    r->m11 = 1.0f;      r->m22 = 1.0f;      r->m33 = 1.0f;  r->m44 = 1.0f;
    return r;
}

struct mat4f* mat4_muls(struct mat4f* r, struct mat4f* m, float k)
{
    r->m11 *= k;   r->m21 *= k;   r->m31 *= k;   r->m41 *= k;
    r->m12 *= k;   r->m22 *= k;   r->m32 *= k;   r->m42 *= k;
    r->m13 *= k;   r->m23 *= k;   r->m33 *= k;   r->m43 *= k;
    r->m14 *= k;   r->m24 *= k;   r->m34 *= k;   r->m44 *= k;
    return r;
}

struct mat4f* mat4_add(struct mat4f* r, const struct mat4f* m1, const struct mat4f* m2)
{
#if defined(_SIMD_SSE_)
    _mm_store_ps(r->row1, _mm_add_ps(_mm_load_ps(m1->row1), _mm_load_ps(m2->row1)));
    _mm_store_ps(r->row2, _mm_add_ps(_mm_load_ps(m1->row2), _mm_load_ps(m2->row2)));
    _mm_store_ps(r->row3, _mm_add_ps(_mm_load_ps(m1->row3), _mm_load_ps(m2->row3)));
    _mm_store_ps(r->row4, _mm_add_ps(_mm_load_ps(m1->row4), _mm_load_ps(m2->row4)));
#else
    r->m11 = m1->m11 + m2->m11;
    r->m21 = m1->m21 + m2->m21;
    r->m31 = m1->m31 + m2->m31;
    r->m41 = m1->m41 + m2->m41;

    r->m12 = m1->m12 + m2->m12;
    r->m22 = m1->m22 + m2->m22;
    r->m32 = m1->m32 + m2->m32;
    r->m42 = m1->m42 + m1->m42;

    r->m13 = m1->m13 + m2->m13;
    r->m23 = m1->m23 + m2->m23;
    r->m33 = m1->m33 + m2->m33;
    r->m43 = m1->m43 + m2->m43;

    r->m14 = m1->m14 + m2->m14;
    r->m24 = m1->m24 + m2->m24;
    r->m34 = m1->m34 + m2->m34;
    r->m44 = m1->m44 + m2->m44;
#endif
    return r;
}

struct mat4f* mat4_sub(struct mat4f* r, const struct mat4f* m1, const struct mat4f* m2)
{
#if defined(_SIMD_SSE_)
    _mm_store_ps(r->row1, _mm_sub_ps(_mm_load_ps(m1->row1), _mm_load_ps(m2->row1)));
    _mm_store_ps(r->row2, _mm_sub_ps(_mm_load_ps(m1->row2), _mm_load_ps(m2->row2)));
    _mm_store_ps(r->row3, _mm_sub_ps(_mm_load_ps(m1->row3), _mm_load_ps(m2->row3)));
    _mm_store_ps(r->row4, _mm_sub_ps(_mm_load_ps(m1->row4), _mm_load_ps(m2->row4)));
#else
    r->m11 = m1->m11 - m2->m11;
    r->m21 = m1->m21 - m2->m21;
    r->m31 = m1->m31 - m2->m31;
    r->m41 = m1->m41 - m2->m41;

    r->m12 = m1->m12 - m2->m12;
    r->m22 = m1->m22 - m2->m22;
    r->m32 = m1->m32 - m2->m32;
    r->m42 = m1->m42 - m1->m42;

    r->m13 = m1->m13 - m2->m13;
    r->m23 = m1->m23 - m2->m23;
    r->m33 = m1->m33 - m2->m33;
    r->m43 = m1->m43 - m2->m43;

    r->m14 = m1->m14 - m2->m14;
    r->m24 = m1->m24 - m2->m24;
    r->m34 = m1->m34 - m2->m34;
    r->m44 = m1->m44 - m2->m44;
#endif
    return r;
}

struct mat4f* mat4_mul(struct mat4f* r, const struct mat4f* m1, const struct mat4f* m2)
{
#if defined(_SIMD_SSE_)
    /* transform rows of first matrix (m1) by the second matrix (m2)
     * also see 'vec4_transform'
     */
    simd_t row1 = _mm_load_ps(m2->row1);
    simd_t row2 = _mm_load_ps(m2->row2);
    simd_t row3 = _mm_load_ps(m2->row3);
    simd_t row4 = _mm_load_ps(m2->row4);
    simd_t vs;
    simd_t rs;

    vs = _mm_load_ps(m1->row1);
    rs = _mm_mul_ps(_mm_all_x(vs), row1);
    rs = _mm_madd(_mm_all_y(vs), row2, rs);
    rs = _mm_madd(_mm_all_z(vs), row3, rs);
    rs = _mm_madd(_mm_all_w(vs), row4, rs);
    _mm_store_ps(r->row1, rs);

    vs = _mm_load_ps(m1->row2);
    rs = _mm_mul_ps(_mm_all_x(vs), row1);
    rs = _mm_madd(_mm_all_y(vs), row2, rs);
    rs = _mm_madd(_mm_all_z(vs), row3, rs);
    rs = _mm_madd(_mm_all_w(vs), row4, rs);
    _mm_store_ps(r->row2, rs);

    vs = _mm_load_ps(m1->row3);
    rs = _mm_mul_ps(_mm_all_x(vs), row1);
    rs = _mm_madd(_mm_all_y(vs), row2, rs);
    rs = _mm_madd(_mm_all_z(vs), row3, rs);
    rs = _mm_madd(_mm_all_w(vs), row4, rs);
    _mm_store_ps(r->row3, rs);

    vs = _mm_load_ps(m1->row4);
    rs = _mm_mul_ps(_mm_all_x(vs), row1);
    rs = _mm_madd(_mm_all_y(vs), row2, rs);
    rs = _mm_madd(_mm_all_z(vs), row3, rs);
    rs = _mm_madd(_mm_all_w(vs), row4, rs);
    _mm_store_ps(r->row4, rs);

    return r;
#else
    return mat4_setf(r,
                     m1->m11*m2->m11 + m1->m12*m2->m21 + m1->m13*m2->m31 + m1->m14*m2->m41,
                     m1->m11*m2->m12 + m1->m12*m2->m22 + m1->m13*m2->m32 + m1->m14*m2->m42,
                     m1->m11*m2->m13 + m1->m12*m2->m23 + m1->m13*m2->m33 + m1->m14*m2->m43,
                     m1->m11*m2->m14 + m1->m12*m2->m24 + m1->m13*m2->m34 + m1->m14*m2->m44,
                     m1->m21*m2->m11 + m1->m22*m2->m21 + m1->m23*m2->m31 + m1->m24*m2->m41,
                     m1->m21*m2->m12 + m1->m22*m2->m22 + m1->m23*m2->m32 + m1->m24*m2->m42,
                     m1->m21*m2->m13 + m1->m22*m2->m23 + m1->m23*m2->m33 + m1->m24*m2->m43,
                     m1->m21*m2->m14 + m1->m22*m2->m24 + m1->m23*m2->m34 + m1->m24*m2->m44,
                     m1->m31*m2->m11 + m1->m32*m2->m21 + m1->m33*m2->m31 + m1->m34*m2->m41,
                     m1->m31*m2->m12 + m1->m32*m2->m22 + m1->m33*m2->m32 + m1->m34*m2->m42,
                     m1->m31*m2->m13 + m1->m32*m2->m23 + m1->m33*m2->m33 + m1->m34*m2->m43,
                     m1->m31*m2->m14 + m1->m32*m2->m24 + m1->m33*m2->m34 + m1->m34*m2->m44,
                     m1->m41*m2->m11 + m1->m42*m2->m21 + m1->m43*m2->m31 + m1->m44*m2->m41,
                     m1->m41*m2->m12 + m1->m42*m2->m22 + m1->m43*m2->m32 + m1->m44*m2->m42,
                     m1->m41*m2->m13 + m1->m42*m2->m23 + m1->m43*m2->m33 + m1->m44*m2->m43,
                     m1->m41*m2->m14 + m1->m42*m2->m24 + m1->m43*m2->m34 + m1->m44*m2->m44);
#endif

}

struct mat4f* mat4_inv(struct mat4f* r, const struct mat4f* m)
{
    float invdet = 1.0f / mat4_det(m);
    return mat4_setf(r,
                    (m->m22*m->m33*m->m44 + m->m23*m->m34*m->m42 + m->m24*m->m32*m->m43
                    - m->m22*m->m34*m->m43 - m->m23*m->m32*m->m44 - m->m24*m->m33*m->m42) * invdet,
                    (m->m12*m->m34*m->m43 + m->m13*m->m32*m->m44 + m->m14*m->m33*m->m42
                    - m->m12*m->m33*m->m44 - m->m13*m->m34*m->m42 - m->m14*m->m32*m->m43) * invdet,
                    (m->m12*m->m23*m->m44 + m->m13*m->m24*m->m42 + m->m14*m->m22*m->m43
                    - m->m12*m->m24*m->m43 - m->m13*m->m22*m->m44 - m->m14*m->m23*m->m42) * invdet,
                    (m->m12*m->m24*m->m33 + m->m13*m->m22*m->m34 + m->m14*m->m23*m->m32
                    - m->m12*m->m23*m->m34 - m->m13*m->m24*m->m32 - m->m14*m->m22*m->m33) * invdet,
                    (m->m21*m->m34*m->m43 + m->m23*m->m31*m->m44 + m->m24*m->m33*m->m41
                    - m->m21*m->m33*m->m44 - m->m23*m->m34*m->m41 - m->m24*m->m31*m->m43) * invdet,
                    (m->m11*m->m33*m->m44 + m->m13*m->m34*m->m41 + m->m14*m->m31*m->m43
                    - m->m11*m->m34*m->m43 - m->m13*m->m31*m->m44 - m->m14*m->m33*m->m41) * invdet,
                    (m->m11*m->m24*m->m43 + m->m13*m->m21*m->m44 + m->m14*m->m23*m->m41
                    - m->m11*m->m23*m->m44 - m->m13*m->m24*m->m41 - m->m14*m->m21*m->m43) * invdet,
                    (m->m11*m->m23*m->m34 + m->m13*m->m24*m->m31 + m->m14*m->m21*m->m33
                    - m->m11*m->m24*m->m33 - m->m13*m->m21*m->m34 - m->m14*m->m23*m->m31) * invdet,
                    (m->m21*m->m32*m->m44 + m->m22*m->m34*m->m41 + m->m24*m->m31*m->m42
                    - m->m21*m->m34*m->m42 - m->m22*m->m31*m->m44 - m->m24*m->m32*m->m41) * invdet,
                    (m->m11*m->m34*m->m42 + m->m12*m->m31*m->m44 + m->m14*m->m32*m->m41
                    - m->m11*m->m32*m->m44 - m->m12*m->m34*m->m41 - m->m14*m->m31*m->m42) * invdet,
                    (m->m11*m->m22*m->m44 + m->m12*m->m24*m->m41 + m->m14*m->m21*m->m42
                    - m->m11*m->m24*m->m42 - m->m12*m->m21*m->m44 - m->m14*m->m22*m->m41) * invdet,
                    (m->m11*m->m24*m->m32 + m->m12*m->m21*m->m34 + m->m14*m->m22*m->m31
                    - m->m11*m->m22*m->m34 - m->m12*m->m24*m->m31 - m->m14*m->m21*m->m32) * invdet,
                    (m->m21*m->m33*m->m42 + m->m22*m->m31*m->m43 + m->m23*m->m32*m->m41
                    - m->m21*m->m32*m->m43 - m->m22*m->m33*m->m41 - m->m23*m->m31*m->m42) * invdet,
                    (m->m11*m->m32*m->m43 + m->m12*m->m33*m->m41 + m->m13*m->m31*m->m42
                    - m->m11*m->m33*m->m42 - m->m12*m->m31*m->m43 - m->m13*m->m32*m->m41) * invdet,
                    (m->m11*m->m23*m->m42 + m->m12*m->m21*m->m43 + m->m13*m->m22*m->m41
                    - m->m11*m->m22*m->m43 - m->m12*m->m23*m->m41 - m->m13*m->m21*m->m42) * invdet,
                    (m->m11*m->m22*m->m33 + m->m12*m->m23*m->m31 + m->m13*m->m21*m->m32
                    - m->m11*m->m23*m->m32 - m->m12*m->m21*m->m33 - m->m13*m->m22*m->m31) * invdet);
}

float mat4_det(const struct mat4f* m)
{
    return  m->m11 * (m->m22*m->m33*m->m44 + m->m23*m->m34*m->m42 + m->m24*m->m32*m->m43 -
            m->m22*m->m34*m->m43 - m->m23*m->m32*m->m44 - m->m24*m->m33*m->m42)
            + m->m12 * (m->m21*m->m34*m->m43 + m->m23*m->m31*m->m44 + m->m24*m->m33*m->m41 -
            m->m21*m->m33*m->m44 - m->m23*m->m34*m->m41 - m->m24*m->m31*m->m43)
            + m->m13 * (m->m21*m->m32*m->m44 + m->m22*m->m34*m->m41 + m->m24*m->m31*m->m42 -
            m->m21*m->m34*m->m42 - m->m22*m->m31*m->m44 - m->m24*m->m32*m->m41)
            + m->m14 * (m->m21*m->m33*m->m42 + m->m22*m->m31*m->m43 + m->m23*m->m32*m->m41 -
            m->m21*m->m32*m->m43 - m->m22*m->m33*m->m41 - m->m23*m->m31*m->m42);
}


struct mat4f* mat4_transpose(struct mat4f* r, const struct mat4f* m)
{
    return mat4_setf(r,
                     m->m11, m->m21, m->m31, m->m41,
                     m->m12, m->m22, m->m32, m->m42,
                     m->m13, m->m23, m->m33, m->m43,
                     m->m14, m->m24, m->m34, m->m44);
}

struct mat4f* mat4_transpose_self(struct mat4f* r)
{
    swapf(&r->m12, &r->m21);
    swapf(&r->m13, &r->m31);
    swapf(&r->m14, &r->m41);
    swapf(&r->m34, &r->m43);
    swapf(&r->m23, &r->m32);
    swapf(&r->m42, &r->m24);
    return r;
}

struct mat4f_simd* mat4simd_setm(struct mat4f_simd* r, const struct mat4f* m)
{
    r->m11[0] = m->m11;		r->m11[1] = m->m11;		r->m11[2] = m->m11;		r->m11[3] = m->m11;
    r->m21[0] = m->m21;		r->m21[1] = m->m21;		r->m21[2] = m->m21;		r->m21[3] = m->m21;
    r->m31[0] = m->m31;		r->m31[1] = m->m31;		r->m31[2] = m->m31;		r->m31[3] = m->m31;
    r->m41[0] = m->m41;		r->m41[1] = m->m41;		r->m41[2] = m->m41;		r->m41[3] = m->m41;

    r->m12[0] = m->m12;		r->m12[1] = m->m12;		r->m12[2] = m->m12;		r->m12[3] = m->m12;
    r->m22[0] = m->m22;		r->m22[1] = m->m22;		r->m22[2] = m->m22;		r->m22[3] = m->m22;
    r->m32[0] = m->m32;		r->m32[1] = m->m32;		r->m32[2] = m->m32;		r->m32[3] = m->m32;
    r->m42[0] = m->m42;		r->m42[1] = m->m42;		r->m42[2] = m->m42;		r->m42[3] = m->m42;

    r->m13[0] = m->m13;		r->m13[1] = m->m13;		r->m13[2] = m->m13;		r->m13[3] = m->m13;
    r->m23[0] = m->m23;		r->m23[1] = m->m23;		r->m23[2] = m->m23;		r->m23[3] = m->m23;
    r->m33[0] = m->m33;		r->m33[1] = m->m33;		r->m33[2] = m->m33;		r->m33[3] = m->m33;
    r->m43[0] = m->m43;		r->m43[1] = m->m43;		r->m43[2] = m->m43;		r->m43[3] = m->m43;

    r->m14[0] = m->m14;		r->m14[1] = m->m14;		r->m14[2] = m->m14;		r->m14[3] = m->m14;
    r->m24[0] = m->m24;		r->m24[1] = m->m24;		r->m24[2] = m->m24;		r->m24[3] = m->m24;
    r->m34[0] = m->m34;		r->m34[1] = m->m34;		r->m34[2] = m->m34;		r->m34[3] = m->m34;
    r->m44[0] = m->m44;		r->m44[1] = m->m44;		r->m44[2] = m->m44;		r->m44[3] = m->m44;

    return r;
}

result_t vec4simd_create(struct vec4f_simd* v, struct allocator* alloc, uint cnt)
{
    v->alloc = alloc;

    /* align count to be multiple of 4 (simd friendly) */
    if (cnt % 4 != 0)
        v->cnt = cnt + (4 - (cnt%4));
    else
        v->cnt = cnt;

    float* buff = (float*)A_ALIGNED_ALLOC(alloc, sizeof(float)*v->cnt*4, 0);
    if (buff == NULL)
        return RET_OUTOFMEMORY;
    memset(buff, 0x0, sizeof(float)*v->cnt*4);

    v->xs = buff;
    v->ys = buff + v->cnt;
    v->zs = buff + v->cnt*2;
    v->ws = buff + v->cnt*3;

    return RET_OK;
}

void vec4simd_destroy(struct vec4f_simd* v)
{
    if (v->xs != NULL)
        A_ALIGNED_FREE(v->alloc, v->xs);
    memset(v, 0x00, sizeof(struct vec4f_simd));
}
