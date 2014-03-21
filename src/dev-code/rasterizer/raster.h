
#ifndef __RASTER_H__
#define __RASTER_H__

#include "dhcore/types.h"
#include "dhcore/vec-math.h"

void rs_zero();
void rs_setviewport(int x, int y, int width, int height);
void rs_setrendertarget(void* buffer, int width, int height);
void rs_drawtri2d_1(const struct vec2i* v1, const struct vec2i* v2, const struct vec2i* v3,
    uint c);
void rs_drawtri2d_2(const struct vec2i* v0, const struct vec2i* v1, const struct vec2i* v2,
    uint c);
void rs_drawtri2d_3(const struct vec2i* v0, const struct vec2i* v1, const struct vec2i* v2,
    uint c);
void rs_transform_verts(struct vec3f* rs, const struct vec3f* vs, uint vert_cnt,
    const struct mat3f* world, const struct mat4f* viewproj);
void rs_drawtri(const struct vec3f* v0, const struct vec3f* v1, const struct vec3f* v2);
float rs_testtri(const struct vec3f* v0, const struct vec3f* v1, const struct vec3f* v2);
#endif
