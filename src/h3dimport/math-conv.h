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

#ifndef __MATHCONV_H__
#define __MATHCONV_H__

#include "dhcore/types.h"
#include "dhcore/vec-math.h"
#include "assimp/cimport.h"
#include "h3dimport.h"

#ifdef __cplusplus
#define AI_COLOR4D aiColor4t<float>
#define AI_MATRIX4X4 aiMatrix4x4t<float>
#define AI_VECTOR3D aiVector3t<float>
#else
#define AI_COLOR4D struct aiColor4D
#define AI_MATRIX4X4 struct aiMatrix4x4
#define AI_VECTOR3D struct aiVector3D
#endif

INLINE struct quat4f* import_convert_quat(struct quat4f* rq, const struct quat4f* q,
                                          enum coord_type coord)
{
    switch (coord)  {
    case COORD_NONE:
        return quat_setq(rq, q);
    case COORD_RH_ZUP:
        {
            struct vec3f axis;
            quat_getrotaxis(&axis, q);
            swapf(&axis.y, &axis.z);
            return quat_fromaxis(rq, &axis, -quat_getangle(q));
        }
    case COORD_RH_GL:
        return quat_setf(rq, -q->x, -q->y, q->z, q->w);
    default:
        return quat_setq(rq, q);
    }
}

INLINE struct vec3f* import_convert_vec3(struct vec3f* rv, const struct vec3f* v,
                                         enum coord_type coord)
{
    switch (coord)  {
    case COORD_NONE:
        return vec3_setv(rv, v);
    case COORD_RH_ZUP:
        return vec3_setf(rv, v->x, v->z, v->y);
    case COORD_RH_GL:
        return vec3_setf(rv, v->x, v->y, -v->z);
    default:
        return vec3_setv(rv, v);
    }
}


INLINE const struct mat3f* import_convert_mat(struct mat3f* rm, const AI_MATRIX4X4* m,
                                              enum coord_type coord)
{
    switch (coord)  {
    case COORD_NONE:
        return mat3_setf(rm,
            m->a1, m->b1, m->c1,
            m->a2, m->b2, m->c2,
            m->a3, m->b3, m->c3,
            m->a4, m->b4, m->c4);
    case COORD_RH_GL:
        return mat3_setf(rm,
            m->a1, m->b1, -m->c1,
            m->a2, m->b2, -m->c2,
            -m->a3, -m->b3, m->c3,
            m->a4, m->b4, -m->c4);
    case COORD_RH_ZUP:
        {
            struct mat3f zup_mat;
            mat3_setf(&zup_mat,
                1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f,
                0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, 0.0f);
            mat3_setf(rm,
                m->a1, m->b1, -m->c1,
                m->a2, m->b2, -m->c2,
                -m->a3, -m->b3, m->c3,
                m->a4, m->b4, -m->c4);
            return mat3_mul(rm, rm, &zup_mat);
        }

    default:
        return mat3_setf(rm,
            m->a1, m->b1, m->c1,
            m->a2, m->b2, m->c2,
            m->a3, m->b3, m->c3,
            m->a4, m->b4, m->c4);
    }
}


INLINE void import_save_mat(float* r, const struct mat3f* m)
{
    r[0] = m->m11;    r[1] = m->m12;    r[2] = m->m13;
    r[3] = m->m21;    r[4] = m->m22;    r[5] = m->m23;
    r[6] = m->m31;    r[7] = m->m32;    r[8] = m->m33;
    r[9] = m->m41;    r[10] = m->m42;   r[11] = m->m43;
}

INLINE void import_set3f(float* r, const float* f)
{
    r[0] = f[0];
    r[1] = f[1];
    r[2] = f[2];
}

INLINE void import_set3f1(float* r, float f)
{
    r[0] = f;
    r[1] = f;
    r[2] = f;
}

INLINE void import_set4f(float* r, const float* f)
{
    r[0] = f[0];
    r[1] = f[1];
    r[2] = f[2];
    r[3] = f[3];
}

INLINE void import_setclr(float* r, AI_COLOR4D* clr)
{
    r[0] = clr->r;
    r[1] = clr->g;
    r[2] = clr->b;
}

#endif /* __MATHCONV_H__ */