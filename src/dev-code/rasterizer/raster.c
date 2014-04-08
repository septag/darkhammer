#include "raster.h"
#include "dhcore/core.h"

/* */
struct raster_state
{
    void* buff;
    int buff_width;
    int buff_height;
    float vp_x;
    float vp_y;
    float vp_width;
    float vp_height;
};

struct raster_state g_rs;

/* inlines */
INLINE int min3(int n1, int n2, int n3)
{
    return mini(n1, mini(n2, n3));
}

INLINE int max3(int n1, int n2, int n3)
{
    return maxi(n1, maxi(n2, n3));
}

INLINE int orient2d(const struct vec2i* a, const struct vec2i* b, const struct vec2i* c)
{
    return (b->x - a->x)*(c->y - a->y) - (b->y - a->y)*(c->x - a->x);
}

INLINE void set_pixel2d(uint* buff, uint c, const struct vec2i* p, int buff_w, int buff_h)
{
    buff[p->x + (buff_h - p->y)*buff_w] = c;
}

INLINE void set_pixel2di(uint* buff, uint c, int x, int y, int buff_w, int buff_h)
{
    buff[x + (buff_h - y)*buff_w] = c;
}

INLINE void set_pixelz(uint* buff, int w0, int w1, int w2, int buff_w, int buff_h,
    int x, int y)
{
    uint z = w0 + w1 + w2;
    buff[x + (buff_h - y)*buff_w] = z;
}

/* */
void rs_zero()
{
    memset(&g_rs, 0x00, sizeof(g_rs));
}

void rs_setviewport(int x, int y, int width, int height)
{
    g_rs.vp_x = (float)x;
    g_rs.vp_y = (float)y;
    g_rs.vp_width = (float)width;
    g_rs.vp_height = (float)height;
}

void rs_setrendertarget(void* buffer, int width, int height)
{
    ASSERT(buffer);

    g_rs.buff = buffer;
    g_rs.buff_width = maxi(width, 1);
    g_rs.buff_height = maxi(height, 1);

    ASSERT(width % 4 == 0);
    ASSERT(height % 4 == 0);
}

void rs_drawtri2d_1(const struct vec2i* v0, const struct vec2i* v1, const struct vec2i* v2,
    uint c)
{
    uint* buff = g_rs.buff;
    int w = g_rs.buff_width;
    int h = g_rs.buff_height;

    /* compute bounding box */
    struct vec2i minpt;
    struct vec2i maxpt;

    minpt.x = min3(v0->x, v1->x, v2->x);
    minpt.y = min3(v0->y, v1->y, v2->y);
    maxpt.x = max3(v0->x, v1->x, v2->x);
    maxpt.y = max3(v0->y, v1->y, v2->y);

    /* clip (clamp box) */
    minpt.x = maxi(minpt.x, 0);
    minpt.y = maxi(minpt.y, 0);
    maxpt.x = mini(maxpt.x, w-1);
    maxpt.y = mini(maxpt.y, h-1);

    struct vec2i p;
    for (p.y = minpt.y; p.y < maxpt.y; p.y++)   {
        for (p.x = minpt.x; p.x < maxpt.x; p.x++)   {
            /* determine half-spaces (barycentric) */
            int w0 = orient2d(v1, v2, &p);
            int w1 = orient2d(v2, v0, &p);
            int w2 = orient2d(v0, v1, &p);

            /* if p is inside all 3 edeges - draw */
            if (w0 >= 0 && w1 >= 0 && w2 >= 0)
                set_pixel2d(buff, c, &p, w, h);
        }
    }
}

void rs_drawtri2d_2(const struct vec2i* v0, const struct vec2i* v1, const struct vec2i* v2,
    uint c)
{
    uint* buff = g_rs.buff;
    int w = g_rs.buff_width;
    int h = g_rs.buff_height;

    /* compute bounding box */
    struct vec2i minpt;
    struct vec2i maxpt;

    minpt.x = min3(v0->x, v1->x, v2->x);
    minpt.y = min3(v0->y, v1->y, v2->y);
    maxpt.x = max3(v0->x, v1->x, v2->x);
    maxpt.y = max3(v0->y, v1->y, v2->y);

    /* clip (clamp box) */
    minpt.x = maxi(minpt.x, 0);
    minpt.y = maxi(minpt.y, 0);
    maxpt.x = mini(maxpt.x, w-1);
    maxpt.y = mini(maxpt.y, h-1);

    /* triangle setup */
    int A01 = v0->y - v1->y;
    int B01 = v1->x - v0->x;
    int A12 = v1->y - v2->y;
    int B12 = v2->x - v1->x;
    int A20 = v2->y - v0->y;
    int B20 = v0->x - v2->x;

    /* start point barycentric coords */
    int w0_row = orient2d(v1, v2, &minpt);
    int w1_row = orient2d(v2, v0, &minpt);
    int w2_row = orient2d(v0, v1, &minpt);

    struct vec2i p;
    for (p.y = minpt.y; p.y < maxpt.y; p.y++)  {
        int w0 = w0_row;
        int w1 = w1_row;
        int w2 = w2_row;

        for (p.x = minpt.x; p.x < maxpt.x; p.x++)  {
            if ((w0 | w1 | w2) >= 0)    {
                set_pixel2d(buff, c, &p, w, h);
            }
            w0 += A12;
            w1 += A20;
            w2 += A01;
        }

        w0_row += B12;
        w1_row += B20;
        w2_row += B01;
    }
}

/* doing 4 pixels at once */
#define STEPX_SIZE 4
#define STEPY_SIZE 1

struct edge
{
    struct vec4i stepx;
    struct vec4i stepy;
};

struct vec4i edge_init(struct edge* e, const struct vec2i* v0, const struct vec2i* v1,
    const struct vec2i* o)
{
    /* edge setup */
    int A = v0->y - v1->y;
    int B = v1->x - v0->x;
    int C = v0->x*v1->y - v0->y*v1->x;

    vec4i_seta(&e->stepx, STEPX_SIZE*A);
    vec4i_seta(&e->stepy, STEPY_SIZE*B);

    struct vec4i x;
    struct vec4i y;
    struct vec4i tmp;
    struct vec4i tmp2;
    struct vec4i r;

    vec4i_add(&x, vec4i_seta(&x, o->x), vec4i_seti(&tmp, 0, 1, 2, 3));
    vec4i_seta(&y, o->y);

    vec4i_add(&r, vec4i_mul(&tmp, vec4i_seta(&tmp, A), &x),
        vec4i_mul(&tmp2, vec4i_seta(&tmp2, B), &y));
    vec4i_add(&r, &r, vec4i_seta(&tmp, C));
    return r;
}

void rs_drawtri2d_3(const struct vec2i* v0, const struct vec2i* v1, const struct vec2i* v2,
    uint c)
{
    uint* buff = g_rs.buff;
    int w = g_rs.buff_width;
    int h = g_rs.buff_height;

    /* compute bounding box */
    struct vec2i minpt;
    struct vec2i maxpt;

    minpt.x = min3(v0->x, v1->x, v2->x);
    minpt.y = min3(v0->y, v1->y, v2->y);
    maxpt.x = max3(v0->x, v1->x, v2->x);
    maxpt.y = max3(v0->y, v1->y, v2->y);

    /* align the box x-values to 4 pixels */
     minpt.x = minpt.x - (minpt.x & 3);
     int align = 4 - (maxpt.x & 3);
     maxpt.x = (maxpt.x + align - 1) & ~(align - 1);

    /* clip (clamp box) */
    minpt.x = maxi(minpt.x, 0);
    minpt.y = maxi(minpt.y, 0);
    maxpt.x = mini(maxpt.x, w-1);
    maxpt.y = mini(maxpt.y, h-1);

    struct vec2i p;
    struct edge e01, e12, e20;
    vec2i_setv(&p, &minpt);
    struct vec4i w0_row = edge_init(&e12, v1, v2, &p);
    struct vec4i w1_row = edge_init(&e20, v2, v0, &p);
    struct vec4i w2_row = edge_init(&e01, v0, v1, &p);

    for (p.y = minpt.y; p.y < maxpt.y; p.y += STEPY_SIZE)   {
        struct vec4i w0 = w0_row;
        struct vec4i w1 = w1_row;
        struct vec4i w2 = w2_row;
        for (p.x = minpt.x; p.x < maxpt.x; p.x += STEPX_SIZE,
                vec4i_add(&w0, &w0, &e12.stepx),
                vec4i_add(&w1, &w1, &e20.stepx),
                vec4i_add(&w2, &w2, &e01.stepx))
        {
            struct vec4i mask;
            vec4i_or(&mask, &w0, vec4i_or(&mask, &w1, &w2));
            if (mask.x < 0 && mask.y < 0 && mask.z < 0 && mask.w < 0)
                continue;

            if (mask.x >= 0)
                set_pixel2di(buff, c, p.x, p.y, w, h);
            if (mask.y >= 0)
                set_pixel2di(buff, c, p.x+1, p.y, w, h);
            if (mask.z >= 0)
                set_pixel2di(buff, c, p.x+2, p.y, w, h);
            if (mask.w >= 0)
                set_pixel2di(buff, c, p.x+3, p.y, w, h);
        }

        vec4i_add(&w0_row, &w0_row, &e12.stepy);
        vec4i_add(&w1_row, &w1_row, &e20.stepy);
        vec4i_add(&w2_row, &w2_row, &e01.stepy);
    }
}

/* reference: http://www.opengl.org/wiki/Vertex_Transformation */
void rs_transform_verts(struct vec3f* rs, const struct vec3f* vs, uint vert_cnt,
    const struct mat3f* world, const struct mat4f* viewproj)
{
    float vpx = g_rs.vp_x;
    float vpy = g_rs.vp_y;
    float wf = g_rs.vp_width;
    float hf = g_rs.vp_height;

    struct mat4f wvp;
    struct vec4f t;
    mat3_mul4(&wvp, world, viewproj);

    for (uint i = 0; i < vert_cnt; i++)   {
        /* to proj-space */
        simd_t row1 = _mm_load_ps(wvp.row1);
        simd_t row2 = _mm_load_ps(wvp.row2);
        simd_t row3 = _mm_load_ps(wvp.row3);
        simd_t row4 = _mm_load_ps(wvp.row4);
        simd_t v = _mm_load_ps(vs[i].f);
        simd_t r = _mm_mul_ps(_mm_all_x(v), row1);
        r = _mm_madd(_mm_all_y(v), row2, r);
        r = _mm_madd(_mm_all_z(v), row3, r);
        r = _mm_madd(_mm_all_w(v), row4, r);

        /* normalize */
        simd_t nr = _mm_div_ps(r, _mm_all_w(r));
        _mm_store_ps(t.f, nr);

        /* viewport transform */
        vec3_setf(&rs[i],
            (t.x*0.5f + 0.5f)*wf + vpx,
            (t.y*0.5f + 0.5f)*hf + vpy,
            t.z);
    }
}

INLINE float rs_calc_area(const struct vec3f* a, const struct vec3f* b, const struct vec3f* c)
{
    return (b->x - a->x)*(c->y - a->y) - (b->y - a->y)*(c->x - a->x);
}

/**
 * @param e_stepx returns stepx for specified edge
 * @param e_stepy returns stepy for specified edge
 * @return barycentric for 4 pixels */
simd4i_t edge_init_simd(struct vec4i* e_stepx, struct vec4i* e_stepy,
    const struct vec2i* v0, const struct vec2i* v1, const struct vec2i* origin)
{
    struct vec4i x;
    struct vec4i y;
    struct vec4i tmp;
    struct vec4i r;

    /* edge setup */
    int A = v0->y - v1->y;
    int B = v1->x - v0->x;
    int C = v0->x*v1->y - v0->y*v1->x;

    vec4i_seta(e_stepx, STEPX_SIZE*A);
    vec4i_seta(e_stepy, STEPY_SIZE*B);

    vec4i_add(&x, vec4i_seta(&x, origin->x), vec4i_seti(&tmp, 0, 1, 2, 3));
    vec4i_seta(&y, origin->y);

    vec4i_mul(&r, vec4i_seta(&tmp, A), &x);
    vec4i_add(&r, &r, vec4i_mul(&tmp, vec4i_seta(&tmp, B), &y));
    vec4i_add(&r, &r, vec4i_seta(&tmp, C));

    return _mm_load_si128((simd4i_t*)r.n);
}

void rs_drawtri(const struct vec3f* v0, const struct vec3f* v1, const struct vec3f* v2)
{
    float* buff = g_rs.buff;
    int w = g_rs.buff_width;
    int h = g_rs.buff_height;

    /* extract the stuff we need from the triangle */
    struct vec2i vs[3];
    simd_t z0 = _mm_set1_ps(v0->z);
    simd_t z1 = _mm_set1_ps(v1->z);
    simd_t z2 = _mm_set1_ps(v2->z);

    vec2i_seti(&vs[0], (int)v0->x, (int)v0->y);
    vec2i_seti(&vs[1], (int)v1->x, (int)v1->y);
    vec2i_seti(&vs[2], (int)v2->x, (int)v2->y);

    float area = rs_calc_area(v0, v1, v2);

    /* cull degenerate and back-facing triangles (counter-clockwise is front) */
    if (area <= EPSILON)
        return;

    /* compute bounding box */
    /* TODO: we can defer this operation and do in SIMD style and save the result in a buffer */
    struct vec2i minpt;
    struct vec2i maxpt;
    minpt.x = min3(vs[0].x, vs[1].x, vs[2].x);
    minpt.y = min3(vs[0].y, vs[1].y, vs[2].y);
    maxpt.x = max3(vs[0].x, vs[1].x, vs[2].x);
    maxpt.y = max3(vs[0].y, vs[1].y, vs[2].y);

    /* align the box x-values to 4 pixels */
    minpt.x = minpt.x - (minpt.x & 3);
    int align = 4 - (maxpt.x & 3);
    maxpt.x = (maxpt.x + align - 1) & ~(align - 1);

    /* clip (clamp box) */
    minpt.x = maxi(minpt.x, 0);
    minpt.y = maxi(minpt.y, 0);
    maxpt.x = mini(maxpt.x, w-1);
    maxpt.y = mini(maxpt.y, h-1);

    /* construct edge values */
    struct vec2i p;
    struct vec4i e12[2];
    struct vec4i e20[2];
    struct vec4i e01[2];

    vec2i_setv(&p, &minpt);
    simd4i_t w0_row = edge_init_simd(&e12[0], &e12[1], &vs[1], &vs[2], &p);
    simd4i_t w1_row = edge_init_simd(&e20[0], &e20[1], &vs[2], &vs[0], &p);
    simd4i_t w2_row = edge_init_simd(&e01[0], &e01[1], &vs[0], &vs[1], &p);

    /* generate optimized z values for interpolation */
    simd_t a = _mm_set1_ps(area);
    z1 = _mm_div_ps(_mm_sub_ps(z1, z0), a);
    z2 = _mm_div_ps(_mm_sub_ps(z2, z0), a);

    /* rasterize: process 4 pixels in each iteration */
    int idx = minpt.x + (h - minpt.y - 1)*w;
    simd4i_t zero = _mm_set1_epi32(0);
    for (p.y = minpt.y; p.y < maxpt.y; p.y+=STEPY_SIZE, idx-=w)   {
        simd4i_t w0 = w0_row;
        simd4i_t w1 = w1_row;
        simd4i_t w2 = w2_row;
        int x_idx = idx;

        for (p.x = minpt.x; p.x < maxpt.x; p.x+=STEPX_SIZE,
                x_idx+=STEPX_SIZE,
                w0 = _mm_add_epi32(w0, _mm_load_si128((simd4i_t*)e12[0].n)),
                w1 = _mm_add_epi32(w1, _mm_load_si128((simd4i_t*)e20[0].n)),
                w2 = _mm_add_epi32(w2, _mm_load_si128((simd4i_t*)e01[0].n)))
        {
            /* check inside the triangle (OR and compare results) */
            simd4i_t mask = _mm_cmplt_epi32(zero, _mm_or_si128(_mm_or_si128(w0, w1), w2));
            if (_mm_test_all_zeros(mask, mask))
                continue;

            /* interpolate depth */
            simd_t depth = z0;
            depth = _mm_add_ps(depth, _mm_mul_ps(_mm_cvtepi32_ps(w1), z1));
            depth = _mm_add_ps(depth, _mm_mul_ps(_mm_cvtepi32_ps(w2), z2));

            /* write to buffer (with the help of masks
             * if (mask[lane] == 0 AND prev_depth > depth) then pixel will not be written */
            simd_t prev_depth = _mm_load_ps(&buff[x_idx]);
            simd_t depth_mask = _mm_cmplt_ps(depth, prev_depth);
            simd4i_t final_mask = _mm_and_si128(mask, _mm_castps_si128(depth_mask));
            depth = _mm_blendv_ps(prev_depth, depth, _mm_castsi128_ps(final_mask));
            _mm_store_ps(&buff[x_idx], depth);
        }

        w0_row = _mm_add_epi32(w0_row, _mm_load_si128((simd4i_t*)e12[1].n));
        w1_row = _mm_add_epi32(w1_row, _mm_load_si128((simd4i_t*)e20[1].n));
        w2_row = _mm_add_epi32(w2_row, _mm_load_si128((simd4i_t*)e01[1].n));
    }
}

/* it essentially draws the triangle like rs_drawtri, but does not write to zbuffer
 * instead counts the pixels that should be written
 */
float rs_testtri(const struct vec3f* v0, const struct vec3f* v1, const struct vec3f* v2)
{
    float* buff = g_rs.buff;
    int w = g_rs.buff_width;
    int h = g_rs.buff_height;

    /* extract the stuff we need from the triangle */
    struct vec2i vs[3];
    simd_t z0 = _mm_set1_ps(v0->z);
    simd_t z1 = _mm_set1_ps(v1->z);
    simd_t z2 = _mm_set1_ps(v2->z);

    vec2i_seti(&vs[0], (int)v0->x, (int)v0->y);
    vec2i_seti(&vs[1], (int)v1->x, (int)v1->y);
    vec2i_seti(&vs[2], (int)v2->x, (int)v2->y);

    float area = rs_calc_area(v0, v1, v2);

    /* cull degenerate and back-facing triangles (counter-clockwise is front) */
    if (area <= EPSILON)
        return 0.0f;

    /* compute bounding box */
    /* TODO: we can defer this operation and do in SIMD style and save the result in a buffer */
    struct vec2i minpt;
    struct vec2i maxpt;
    minpt.x = min3(vs[0].x, vs[1].x, vs[2].x);
    minpt.y = min3(vs[0].y, vs[1].y, vs[2].y);
    maxpt.x = max3(vs[0].x, vs[1].x, vs[2].x);
    maxpt.y = max3(vs[0].y, vs[1].y, vs[2].y);

    /* align the box x-values to 4 pixels */
    minpt.x = minpt.x - (minpt.x & 3);
    int align = 4 - (maxpt.x & 3);
    maxpt.x = (maxpt.x + align - 1) & ~(align - 1);

    /* clip (clamp box) */
    minpt.x = maxi(minpt.x, 0);
    minpt.y = maxi(minpt.y, 0);
    maxpt.x = mini(maxpt.x, w-1);
    maxpt.y = mini(maxpt.y, h-1);

    /* construct edge values */
    struct vec2i p;
    struct vec4i e12[2];
    struct vec4i e20[2];
    struct vec4i e01[2];

    vec2i_setv(&p, &minpt);
    simd4i_t w0_row = edge_init_simd(&e12[0], &e12[1], &vs[1], &vs[2], &p);
    simd4i_t w1_row = edge_init_simd(&e20[0], &e20[1], &vs[2], &vs[0], &p);
    simd4i_t w2_row = edge_init_simd(&e01[0], &e01[1], &vs[0], &vs[1], &p);

    /* generate optimized z values for interpolation */
    simd_t a = _mm_set1_ps(area);
    z1 = _mm_div_ps(_mm_sub_ps(z1, z0), a);
    z2 = _mm_div_ps(_mm_sub_ps(z2, z0), a);

    /* rasterize: process 4 pixels in each iteration */
    int idx = minpt.x + (h - minpt.y - 1)*w;
    float cnt;
    simd4i_t zero = _mm_set1_epi32(0);
    simd_t onef = _mm_set1_ps(1.0f);
    simd_t zerof = _mm_set1_ps(0.0f);
    simd_t cnt4 = _mm_set1_ps(0.0f);
    for (p.y = minpt.y; p.y < maxpt.y; p.y+=STEPY_SIZE, idx-=w)   {
        simd4i_t w0 = w0_row;
        simd4i_t w1 = w1_row;
        simd4i_t w2 = w2_row;
        int x_idx = idx;

        for (p.x = minpt.x; p.x < maxpt.x; p.x+=STEPX_SIZE,
                x_idx+=STEPX_SIZE,
                w0 = _mm_add_epi32(w0, _mm_load_si128((simd4i_t*)e12[0].n)),
                w1 = _mm_add_epi32(w1, _mm_load_si128((simd4i_t*)e20[0].n)),
                w2 = _mm_add_epi32(w2, _mm_load_si128((simd4i_t*)e01[0].n)))
        {
            /* check inside the triangle (OR and compare results) */
            simd4i_t mask = _mm_cmplt_epi32(zero, _mm_or_si128(_mm_or_si128(w0, w1), w2));
            if (_mm_test_all_zeros(mask, mask))
                continue;

            /* interpolate depth */
            simd_t depth = z0;
            depth = _mm_add_ps(depth, _mm_mul_ps(_mm_cvtepi32_ps(w1), z1));
            depth = _mm_add_ps(depth, _mm_mul_ps(_mm_cvtepi32_ps(w2), z2));

            /* write to buffer (with the help of masks
             * if (mask[lane] == 0 AND prev_depth > depth) then pixel will not be written */
            simd_t prev_depth = _mm_load_ps(&buff[x_idx]);
            simd_t depth_mask = _mm_cmplt_ps(depth, prev_depth);
            simd4i_t final_mask = _mm_and_si128(mask, _mm_castps_si128(depth_mask));
            simd_t test = _mm_blendv_ps(zerof, onef, _mm_castsi128_ps(final_mask));
            test = _mm_hadd_ps(test, test);
            test = _mm_hadd_ps(test, test);
            cnt4 = _mm_add_ss(cnt4, test);
        }

        w0_row = _mm_add_epi32(w0_row, _mm_load_si128((simd4i_t*)e12[1].n));
        w1_row = _mm_add_epi32(w1_row, _mm_load_si128((simd4i_t*)e20[1].n));
        w2_row = _mm_add_epi32(w2_row, _mm_load_si128((simd4i_t*)e01[1].n));
    }

    _mm_store_ss(&cnt, cnt4);
    return cnt;
}
