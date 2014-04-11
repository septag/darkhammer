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

#ifndef __PRIMS_H__
#define __PRIMS_H__

#include "types.h"
#include "vec-math.h"
#include "std-math.h"

struct rect2di
{
    int x;
    int y;
    int w;
    int h;
};

struct rect2df
{
	float x;
	float y;
	float w;
	float h;
};

/* sphere */
struct ALIGN16 sphere
{
    union   {
        struct {
            float x;
            float y;
            float z;
            float r;
        };

        float f[4];
    };
};

struct ALIGN16 aabb
{
    struct vec4f minpt;
    struct vec4f maxpt;
};

/* eq: N(dot)P + d = 0 */
struct ALIGN16 plane
{
    union   {
        struct {
            float nx;
            float ny;
            float nz;
            float d;
        };

        float f[4];
    };
};

struct ALIGN16 frustum
{
    union   {
        struct plane planes[6];
        struct vec3f points[8];
    };
};

/* eq: p = ray.pt + ray.dir*t */
struct ALIGN16 ray
{
    struct vec4f pt;
    struct vec4f dir;
};

/* inlines */
INLINE struct plane* plane_setv(struct plane* p, const struct vec3f* n, float d)
{
    p->nx = n->x;
    p->ny = n->y;
    p->nz = n->z;
    p->d = d;
    return p;
}

INLINE struct plane* plane_setf(struct plane* p, float nx, float ny, float nz, float d)
{
    p->nx = nx;
    p->ny = ny;
    p->nz = nz;
    p->d = d;
    return p;
}

INLINE struct ray* ray_setv(struct ray* r, const struct vec3f* pt, const struct vec3f* dir)
{
    vec3_setv(&r->pt, pt);
    vec3_setv(&r->dir, dir);
    return r;
}

INLINE struct rect2di* rect2di_setr(struct rect2di* r, const struct rect2di* rc)
{
    r->x = rc->x;
    r->y = rc->y;
    r->w = rc->w;
    r->h = rc->h;
    return r;
}

INLINE struct rect2di* rect2di_seti(struct rect2di* rc, int x, int y, int w, int h)
{
    rc->x = x;
    rc->y = y;
    rc->w = w;
    rc->h = h;
    return rc;
}

INLINE int rect2di_isequal(const struct rect2di rc1, const struct rect2di rc2)
{
	return (rc1.x == rc2.x && rc1.y == rc2.y && rc1.w == rc2.w && rc1.h == rc2.h);
}

INLINE struct rect2di* rect2di_shrink(struct rect2di* rr, const struct rect2di* r, int shrink)
{
    return rect2di_seti(rr, r->x + shrink, r->y + shrink, r->w - 2*shrink, r->h - 2*shrink);
}

INLINE struct rect2di* rect2di_grow(struct rect2di* rr, const struct rect2di* r, int grow)
{
    return rect2di_seti(rr, r->x - grow, r->y - grow, r->w + 2*grow, r->h + 2*grow);
}

INLINE int rect2di_testpt(const struct rect2di* rc, const struct vec2i* pt)
{
    return (pt->x > rc->x)&(pt->x < (rc->x + rc->w))&(pt->y > rc->y)&(pt->y < (rc->y + rc->h));
}

INLINE struct rect2df* rect2df_setf(struct rect2df* rc, float x, float y, float w, float h)
{
    rc->x = x;
    rc->y = y;
    rc->w = w;
    rc->h = h;
    return rc;
}

INLINE int rect2df_isequal(const struct rect2df rc1, const struct rect2df rc2)
{
	return (rc1.x == rc2.x && rc1.y == rc2.y && rc1.w == rc2.w && rc1.h == rc2.h);
}


INLINE struct sphere* sphere_setzero(struct sphere* s)
{
    s->x = 0.0f;
    s->y = 0.0f;
    s->z = 0.0f;
    s->r = 0.0f;
    return s;
}

INLINE struct sphere* sphere_setf(struct sphere* s, float x, float y, float z, float r)
{
    s->x = x;
    s->y = y;
    s->z = z;
    s->r = r;
    return s;
}

INLINE struct sphere* sphere_sets(struct sphere* r, const struct sphere* s)
{
    r->x = s->x;
    r->y = s->y;
    r->z = s->z;
    r->r = s->r;
    return r;
}

INLINE struct sphere* sphere_from_aabb(struct sphere* rs, const struct aabb* b)
{
    struct vec4f v;
    vec3_muls(&v, vec3_add(&v, &b->minpt, &b->maxpt), 0.5f);
    rs->x = v.x;
    rs->y = v.y;
    rs->z = v.z;
    rs->r = vec3_len(vec3_sub(&v, &b->maxpt, &b->minpt)) * 0.5f;
    return rs;
}

INLINE struct aabb* aabb_setf(struct aabb* r, float min_x, float min_y, float min_z,
    float max_x, float max_y, float max_z)
{
    vec3_setf(&r->minpt, min_x, min_y, min_z);
    vec3_setf(&r->maxpt, max_x, max_y, max_z);
    return r;
}

INLINE struct aabb* aabb_setb(struct aabb* r, const struct aabb* b)
{
    vec4_setv(&r->minpt, &b->minpt);
    vec4_setv(&r->maxpt, &b->maxpt);
    return r;
}

INLINE struct aabb* aabb_setv(struct aabb* r, const struct vec4f* minpt, const struct vec4f* maxpt)
{
    vec4_setv(&r->minpt, minpt);
    vec4_setv(&r->maxpt, maxpt);
    return r;
}

INLINE struct aabb* aabb_setzero(struct aabb* r)
{
    vec3_setf(&r->minpt, FL32_MAX, FL32_MAX, FL32_MAX);
    vec3_setf(&r->maxpt, -FL32_MAX, -FL32_MAX, -FL32_MAX);
    return r;
}

INLINE int aabb_iszero(const struct aabb* b)
{
    return (b->minpt.x == FL32_MAX && b->minpt.y == FL32_MAX && b->minpt.z == FL32_MAX) &&
           (b->maxpt.x == -FL32_MAX && b->maxpt.y == -FL32_MAX && b->maxpt.z == -FL32_MAX);
}

/*
 *            6                                7
 *              ------------------------------
 *             /|                           /|
 *            / |                          / |
 *           /  |                         /  |
 *          /   |                        /   |
 *         /    |                       /    |
 *        /     |                      /     |
 *       /      |                     /      |
 *      /       |                    /       |
 *     /        |                   /        |
 *  2 /         |                3 /         |
 *   /----------------------------/          |
 *   |          |                 |          |
 *   |          |                 |          |      +Y
 *   |        4 |                 |          |
 *   |          |-----------------|----------|      |
 *   |         /                  |         /  5    |
 *   |        /                   |        /        |       +Z
 *   |       /                    |       /         |
 *   |      /                     |      /          |     /
 *   |     /                      |     /           |    /
 *   |    /                       |    /            |   /
 *   |   /                        |   /             |  /
 *   |  /                         |  /              | /
 *   | /                          | /               |/
 *   |/                           |/                ----------------- +X
 *   ------------------------------
 *  0                              1
 */
INLINE struct vec4f* aabb_getpt(struct vec4f* r, const struct aabb* b, uint idx)
{
    return vec3_setf(r, (idx&1) ? b->maxpt.x : b->minpt.x,
                        (idx&2) ? b->maxpt.y : b->minpt.y,
                        (idx&4) ? b->maxpt.z : b->minpt.z);
}

INLINE struct vec4f* aabb_getptarr(struct vec4f* rs, const struct aabb* b)
{
    vec3_setv(&rs[0], &b->minpt);
    vec3_setf(&rs[1], b->maxpt.x, b->minpt.y, b->minpt.z);
    vec3_setf(&rs[2], b->minpt.x, b->maxpt.y, b->minpt.z);
    vec3_setf(&rs[3], b->maxpt.x, b->maxpt.y, b->minpt.z);
    vec3_setf(&rs[4], b->minpt.x, b->minpt.y, b->maxpt.z);
    vec3_setf(&rs[5], b->maxpt.x, b->minpt.y, b->maxpt.z);
    vec3_setf(&rs[6], b->minpt.x, b->maxpt.y, b->maxpt.z);
    vec3_setv(&rs[7], &b->maxpt);
    return rs;
}

INLINE void aabb_pushptv(struct aabb* rb, const struct vec4f* pt)
{
    if (pt->x < rb->minpt.x)        rb->minpt.x = pt->x;
    if (pt->x > rb->maxpt.x)        rb->maxpt.x = pt->x;
    if (pt->y < rb->minpt.y)        rb->minpt.y = pt->y;
    if (pt->y > rb->maxpt.y)        rb->maxpt.y = pt->y;
    if (pt->z < rb->minpt.z)        rb->minpt.z = pt->z;
    if (pt->z > rb->maxpt.z)        rb->maxpt.z = pt->z;
}

INLINE void aabb_pushptf(struct aabb* rb, float x, float y, float z)
{
    if (x < rb->minpt.x)        rb->minpt.x = x;
    if (x > rb->maxpt.x)        rb->maxpt.x = x;
    if (y < rb->minpt.y)        rb->minpt.y = y;
    if (y > rb->maxpt.y)        rb->maxpt.y = y;
    if (z < rb->minpt.z)        rb->minpt.z = z;
    if (z > rb->maxpt.z)        rb->maxpt.z = z;
}

INLINE float aabb_getwidth(const struct aabb* bb)
{
    return bb->maxpt.x - bb->minpt.x;
}

INLINE float aabb_getheight(const struct aabb* bb)
{
    return bb->maxpt.y - bb->minpt.y;
}

INLINE float aabb_getdepth(const struct aabb* bb)
{
    return bb->maxpt.z - bb->minpt.z;
}

INLINE struct aabb* aabb_from_sphere(struct aabb* rb, const struct sphere* s)
{
    vec3_setf(&rb->minpt, FL32_MAX, FL32_MAX, FL32_MAX);
    vec3_setf(&rb->maxpt, -FL32_MAX, -FL32_MAX, -FL32_MAX);
    aabb_pushptf(rb, s->x + s->r, s->y + s->r, s->z + s->r);
    aabb_pushptf(rb, s->x - s->r, s->y - s->r, s->z - s->r);
    return rb;
}

INLINE int sphere_ptinv(const struct sphere* s, const struct vec4f* pt)
{
    struct vec4f d;
    vec3_setf(&d, pt->x - s->x, pt->y - s->y, pt->z - s->z);
    return ((vec3_dot(&d, &d) - s->r*s->r) < EPSILON);
}

INLINE int sphere_ptinf(const struct sphere* s, float x, float y, float z)
{
    struct vec4f d;
    vec3_setf(&d, x - s->x, y - s->y, z - s->z);
    return ((vec3_dot(&d, &d) - s->r*s->r) < EPSILON);
}

/*  */
CORE_API struct sphere* sphere_circum(struct sphere* rs, const struct vec4f* v0,
		const struct vec4f* v1, const struct vec4f* v2, const struct vec4f* v3);
CORE_API struct sphere* sphere_merge(struct sphere* rs, const struct sphere* s1,
		const struct sphere* s2);
CORE_API struct aabb* aabb_merge(struct aabb* rb, const struct aabb* b1, const struct aabb* b2);
CORE_API struct aabb* aabb_xform(struct aabb* rb, const struct aabb* b, const struct mat3f* mat);
CORE_API struct sphere* sphere_xform(struct sphere* rs, const struct sphere* s,
		const struct mat3f* m);
CORE_API int sphere_intersects(const struct sphere* s1, const struct sphere* s2);

/**
 * intersects plane with ray
 * @return a floating point number which we can put in ray equation and find a point where \
 * intersection happens, or FL32_MAX if no intersection
 */
CORE_API float ray_intersect_plane(const struct ray* r, const struct plane* p);

#endif /* PRIMS_H */
