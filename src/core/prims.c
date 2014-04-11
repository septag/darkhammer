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

#include "prims.h"

/* calculate sphere that goes through 4 points */
struct sphere* sphere_circum(struct sphere* rs, const struct vec4f* v0,
		const struct vec4f* v1, const struct vec4f* v2, const struct vec4f* v3)
{
	struct vec4f a;		vec3_sub(&a, v1, v0);
	struct vec4f b;		vec3_sub(&b, v2, v0);
	struct vec4f c;		vec3_sub(&c, v3, v0);
	struct vec4f o;
	struct vec4f tmp;

	struct mat3f m;
	mat3_setf(&m,
			a.x, a.y, a.z,
			b.x, b.y, b.z,
			c.x, c.y, c.z,
			0.0f, 0.0f, 0.0f);

	float denom = 2.0f * mat3_det(&m);
	vec3_muls(&o, vec3_cross(&tmp, &a, &b), vec3_dot(&c, &c));
	vec3_add(&o, &o, vec3_muls(&tmp, vec3_cross(&tmp, &c, &a), vec3_dot(&b, &b)));
	vec3_add(&o, &o, vec3_muls(&tmp, vec3_cross(&tmp, &b, &c), vec3_dot(&a, &a)));
	vec3_muls(&o, &o, 1.0f/denom);

	return sphere_setf(rs, v0->x + o.x, v0->y + o.y, v0->z + o.z, vec3_len(&o) + EPSILON);
}


struct sphere* sphere_xform(struct sphere* rs, const struct sphere* s, const struct mat3f* m)
{
    /* we have to transform radius by scale value of transform matrix
     * to determine scale value, we find the highest scale component of the matrix */
    float xlen = m->m11*m->m11 + m->m12*m->m12 + m->m13*m->m13;
    float ylen = m->m21*m->m21 + m->m22*m->m22 + m->m23*m->m23;
    float zlen = m->m31*m->m31 + m->m32*m->m32 + m->m33*m->m33;
    float isqr_scale = maxf(xlen, ylen);
    isqr_scale = maxf(isqr_scale, zlen);

    /* transform center */
    struct vec4f c;
    vec3_setf(&c, s->x, s->y, s->z);
    vec3_transformsrt(&c, &c, m);
    return sphere_setf(rs, c.x, c.y, c.z, s->r*sqrtf(isqr_scale));
}

struct sphere* sphere_merge(struct sphere* rs, const struct sphere* s1,
                            const struct sphere* s2)
{
    struct vec4f cdiff;
    vec3_setf(&cdiff, s2->x - s1->x, s2->y - s1->y, s2->z - s1->z);
    float rd = s2->r - s1->r;
    float rd_sqr = rd * rd;
    float ld_sqr = cdiff.x*cdiff.x + cdiff.y*cdiff.y + cdiff.z*cdiff.z;
    if (rd_sqr < ld_sqr)    {
        float ld = sqrtf(ld_sqr);
        struct vec4f c;
        vec3_setf(&c, s1->x, s1->y, s1->z);
        float k = (ld + s2->r - s1->r)/(2.0f*ld);
        vec3_add(&c, &c, vec3_muls(&cdiff, &cdiff, k));
        rs->x = c.x;
        rs->y = c.y;
        rs->z = c.z;
        rs->r = (ld + s1->r + s2->r)*0.5f;
        return rs;
    }    else    {
        if (rd >= 0.0f)     return sphere_sets(rs, s2);
        else                return sphere_sets(rs, s1);
    }
}

struct aabb* aabb_merge(struct aabb* rb, const struct aabb* b1, const struct aabb* b2)
{
    struct vec4f minpt;
    struct vec4f maxpt;

    vec3_setf(&minpt, minf(b1->minpt.x, b2->minpt.x), minf(b1->minpt.y, b2->minpt.y),
        minf(b1->minpt.z, b2->minpt.z));
    vec3_setf(&maxpt, maxf(b1->maxpt.x, b2->maxpt.x), maxf(b1->maxpt.y, b2->maxpt.y),
        maxf(b1->maxpt.z, b2->maxpt.z));
    return aabb_setv(rb, &minpt, &maxpt);
}

struct aabb* aabb_xform(struct aabb* rb, const struct aabb* b, const struct mat3f* mat)
{
    struct vec4f minpt;
    struct vec4f maxpt;

    /* start with translation part */
    struct vec4f t;
    mat3_get_transv(&t, mat);
    minpt.x = maxpt.x = t.x;
    minpt.y = maxpt.y = t.y;
    minpt.z = maxpt.z = t.z;
    minpt.w = maxpt.w = 1.0f;

    if (mat->m11 > 0.0f) {
        minpt.x += mat->m11 * b->minpt.x;
        maxpt.x += mat->m11 * b->maxpt.x;
    }    else    {
        minpt.x += mat->m11 * b->maxpt.x;
        maxpt.x += mat->m11 * b->minpt.x;
    }

    if (mat->m12 > 0.0f) {
        minpt.y += mat->m12 * b->minpt.x;
        maxpt.y += mat->m12 * b->maxpt.x;
    }    else     {
        minpt.y += mat->m12 * b->maxpt.x;
        maxpt.y += mat->m12 * b->minpt.x;
    }

    if (mat->m13 > 0.0f)    {
        minpt.z += mat->m13 * b->minpt.x;
        maxpt.z += mat->m13 * b->maxpt.x;
    }    else    {
        minpt.z += mat->m13 * b->maxpt.x;
        maxpt.z += mat->m13 * b->minpt.x;
    }

    if (mat->m21 > 0.0f) {
        minpt.x += mat->m21 * b->minpt.y;
        maxpt.x += mat->m21 * b->maxpt.y;
    }    else     {
        minpt.x += mat->m21 * b->maxpt.y;
        maxpt.x += mat->m21 * b->minpt.y;
    }

    if (mat->m22 > 0.0f) {
        minpt.y += mat->m22 * b->minpt.y;
        maxpt.y += mat->m22 * b->maxpt.y;
    }    else    {
        minpt.y += mat->m22 * b->maxpt.y;
        maxpt.y += mat->m22 * b->minpt.y;
    }

    if (mat->m23 > 0.0f) {
        minpt.z += mat->m23 * b->minpt.y;
        maxpt.z += mat->m23 * b->maxpt.y;
    }    else    {
        minpt.z += mat->m23 * b->maxpt.y;
        maxpt.z += mat->m23 * b->minpt.y;
    }

    if (mat->m31 > 0.0f) {
        minpt.x += mat->m31 * b->minpt.z;
        maxpt.x += mat->m31 * b->maxpt.z;
    }    else     {
        minpt.x += mat->m31 * b->maxpt.z;
        maxpt.x += mat->m31 * b->minpt.z;
    }

    if (mat->m32 > 0.0f) {
        minpt.y += mat->m32 * b->minpt.z;
        maxpt.y += mat->m32 * b->maxpt.z;
    }    else     {
        minpt.y += mat->m32 * b->maxpt.z;
        maxpt.y += mat->m32 * b->minpt.z;
    }

    if (mat->m33 > 0.0f) {
        minpt.z += mat->m33 * b->minpt.z;
        maxpt.z += mat->m33 * b->maxpt.z;
    }    else     {
        minpt.z += mat->m33 * b->maxpt.z;
        maxpt.z += mat->m33 * b->minpt.z;
    }

    return aabb_setv(rb, &minpt, &maxpt);
}


float ray_intersect_plane(const struct ray* r, const struct plane* p)
{
    /* put (pt + t*dir) into plane equation and solve t -> (pt + t*dir)*N + d = 0 */
    float v_dot_n = r->dir.x*p->nx + r->dir.y*p->ny + r->dir.z*p->nz;
    if (math_iszero(v_dot_n))
        return FL32_MAX;    /* ray is parallel to plane */
    float p_dot_n = r->pt.x*p->nx + r->pt.y*p->ny + r->pt.z*p->nz;
    float t = -(p_dot_n + p->d)/v_dot_n;
    return t;
}

int sphere_intersects(const struct sphere* s1, const struct sphere* s2)
{
    struct vec3f d;
    vec3_setf(&d, s2->x - s1->x, s2->y - s1->y, s2->z - s1->z);
    float l = vec3_dot(&d, &d);
    float sr = s2->r + s1->r;
    return (l*l - sr*sr) < EPSILON;
}
