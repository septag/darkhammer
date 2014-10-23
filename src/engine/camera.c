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

#include "dhcore/core.h"
#include "camera.h"

/*************************************************************************************************/
struct camera* cam_create(const struct vec4f* pos, const struct vec4f* lookat,
                          float fnear, float ffar, float fov)
{
    struct camera* cam = (struct camera*)ALIGNED_ALLOC(sizeof(struct camera), 0);
    if (cam == NULL)    {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return NULL;
    }
    memset(cam, 0x00, sizeof(struct camera));
    cam_init(cam, pos, lookat, fnear, ffar, fov);
    return cam;
}

void cam_destroy(struct camera* cam)
{
    ASSERT(cam);
    ALIGNED_FREE(cam);
}

void cam_init(struct camera* cam, const struct vec4f* pos, const struct vec4f* lookat,
              float fnear, float ffar, float fov)
{
    ASSERT(!vec3_isequal(pos, lookat));
    memset(cam, 0x00, sizeof(struct camera));

    /* zaxis = lookat - pos */
    vec3_norm(&cam->look, vec3_sub(&cam->look, lookat, pos));
    /* xaxis = up (cross) xaxis */
    vec3_norm(&cam->right, vec3_cross(&cam->right, &g_vec3_unity, &cam->look));
    /* yaxis = zaxis (cross) xaxis */
    vec3_cross(&cam->up, &cam->look, &cam->right);

    struct mat3f m;
    mat3_setf(&m,
              cam->right.x, cam->right.y, cam->right.z,
              cam->up.x, cam->up.y, cam->up.z,
              cam->look.x, cam->look.y, cam->look.z,
              0.0f, 0.0f, 0.0f);

    quat_frommat3(&cam->rot, &m);
    vec3_setv(&cam->pos, pos);
    cam->fnear = fnear;
    cam->ffar = ffar;
    cam->fov = fov;

    /* constraints */
    float pitch, yaw, roll;
    mat3_get_roteuler(&pitch, &yaw, &roll, &m);
    cam->pitch_cur = pitch;
    cam->yaw_cur = yaw;

    cam->const_pitch = TRUE;
    cam->pitch_max = PI_HALF - EPSILON - math_torad(5.0f);
    cam->pitch_min = -PI_HALF + EPSILON + math_torad(5.0f);
}

void cam_set_pitchconst(struct camera* cam, int enable, float pitch_min, float pitch_max)
{
    cam->const_pitch = enable;
    cam->pitch_max = pitch_max;
    cam->pitch_min = pitch_min;
}


void cam_calc_frustumcorners(const struct camera* cam, struct vec4f* corners,
                             float* near_override, float* far_override)
{
    const float ffar = far_override != NULL ? *far_override : cam->ffar;
    const float fnear = near_override != NULL ? *near_override : cam->fnear;

    /* axises are columns of view matrix */
    struct vec4f xaxis;     vec3_setv(&xaxis, &cam->right);
    struct vec4f yaxis;     vec3_setv(&yaxis, &cam->up);
    struct vec4f zaxis;     vec3_setv(&zaxis, &cam->look);

    float near_plane_h = tanf(cam->fov * 0.5f) * fnear;
    float near_plane_w = near_plane_h * cam->aspect;

    float far_plane_h = tanf(cam->fov * 0.5f) * ffar;
    float far_plane_w = far_plane_h * cam->aspect;

    /* calculate near/far plane centers
     * P = CamPos + CamDir*Distance */
    struct vec4f near_center;
    struct vec4f far_center;
    vec3_muls(&near_center, &zaxis, fnear);
    vec3_add(&near_center, &near_center, &cam->pos);
    vec3_muls(&far_center, &zaxis, ffar);
    vec3_add(&far_center, &cam->pos, &far_center);

    /* calculate scaled axises */
    struct vec4f xaxis_near_scaled;
    struct vec4f xaxis_far_scaled;
    struct vec4f yaxis_near_scaled;
    struct vec4f yaxis_far_scaled;
    vec3_muls(&xaxis_near_scaled, &xaxis, near_plane_w);
    vec3_muls(&xaxis_far_scaled, &xaxis, far_plane_w);
    vec3_muls(&yaxis_near_scaled, &yaxis, near_plane_h);
    vec3_muls(&yaxis_far_scaled, &yaxis, far_plane_h);

    struct vec4f tmpv;
    /* corners, start from bottom-left corner point and winds clockwise */
    /* near quad */
    vec3_setv(&corners[0], vec3_sub(&tmpv, &near_center,
        vec3_add(&tmpv, &xaxis_near_scaled, &yaxis_near_scaled)));
    vec3_setv(&corners[1], vec3_sub(&tmpv, &near_center,
        vec3_sub(&tmpv, &xaxis_near_scaled, &yaxis_near_scaled)));
    vec3_setv(&corners[2], vec3_add(&tmpv, &near_center,
        vec3_add(&tmpv, &xaxis_near_scaled, &yaxis_near_scaled)));
    vec3_setv(&corners[3], vec3_add(&tmpv, &near_center,
        vec3_sub(&tmpv, &xaxis_near_scaled, &yaxis_near_scaled)));

    /* far quad */
    vec3_setv(&corners[4], vec3_sub(&tmpv, &far_center,
        vec3_add(&tmpv, &xaxis_far_scaled, &yaxis_far_scaled)));
    vec3_setv(&corners[5], vec3_sub(&tmpv, &far_center,
        vec3_sub(&tmpv, &xaxis_far_scaled, &yaxis_far_scaled)));
    vec3_setv(&corners[6], vec3_add(&tmpv, &far_center,
        vec3_add(&tmpv, &xaxis_far_scaled, &yaxis_far_scaled)));
    vec3_setv(&corners[7], vec3_add(&tmpv, &far_center,
        vec3_sub(&tmpv, &xaxis_far_scaled, &yaxis_far_scaled)));
}

void cam_calc_frustumplanes(struct plane planes[6], const struct mat4f* viewproj)
{
    /* Left clipping plane */
    planes[CAM_FRUSTUM_LEFT].nx = viewproj->m14 + viewproj->m11;
    planes[CAM_FRUSTUM_LEFT].ny = viewproj->m24 + viewproj->m21;
    planes[CAM_FRUSTUM_LEFT].nz = viewproj->m34 + viewproj->m31;
    planes[CAM_FRUSTUM_LEFT].d = viewproj->m44 + viewproj->m41;
    /* Right clipping plane */
    planes[CAM_FRUSTUM_RIGHT].nx = viewproj->m14 - viewproj->m11;
    planes[CAM_FRUSTUM_RIGHT].ny = viewproj->m24 - viewproj->m21;
    planes[CAM_FRUSTUM_RIGHT].nz = viewproj->m34 - viewproj->m31;
    planes[CAM_FRUSTUM_RIGHT].d = viewproj->m44 - viewproj->m41;
    /* Top clipping plane */
    planes[CAM_FRUSTUM_TOP].nx = viewproj->m14 - viewproj->m12;
    planes[CAM_FRUSTUM_TOP].ny = viewproj->m24 - viewproj->m22;
    planes[CAM_FRUSTUM_TOP].nz = viewproj->m34 - viewproj->m32;
    planes[CAM_FRUSTUM_TOP].d = viewproj->m44 - viewproj->m42;
    /* Bottom clipping plane */
    planes[CAM_FRUSTUM_BOTTOM].nx = viewproj->m14 + viewproj->m12;
    planes[CAM_FRUSTUM_BOTTOM].ny = viewproj->m24 + viewproj->m22;
    planes[CAM_FRUSTUM_BOTTOM].nz = viewproj->m34 + viewproj->m32;
    planes[CAM_FRUSTUM_BOTTOM].d = viewproj->m44 + viewproj->m42;
    /* Near clipping plane */
    planes[CAM_FRUSTUM_NEAR].nx = viewproj->m13;
    planes[CAM_FRUSTUM_NEAR].ny = viewproj->m23;
    planes[CAM_FRUSTUM_NEAR].nz = viewproj->m33;
    planes[CAM_FRUSTUM_NEAR].d = viewproj->m43;
    /* Far clipping plane */
    planes[CAM_FRUSTUM_FAR].nx = viewproj->m14 - viewproj->m13;
    planes[CAM_FRUSTUM_FAR].ny = viewproj->m24 - viewproj->m23;
    planes[CAM_FRUSTUM_FAR].nz = viewproj->m34 - viewproj->m33;
    planes[CAM_FRUSTUM_FAR].d = viewproj->m44 - viewproj->m43;

    /* normalize planes */
    struct vec4f nd;
    for (uint i = 0; i < 6; i++)        {
        vec3_setf(&nd, planes[i].nx, planes[i].ny, planes[i].nz);
        float inv_mag = 1.0f / sqrtf(vec3_dot(&nd, &nd));
        vec3_muls(&nd, &nd, inv_mag);

        planes[i].nx = nd.x;
        planes[i].ny = nd.y;
        planes[i].nz = nd.z;
        planes[i].d *= inv_mag;
    }
}


void cam_update(struct camera* cam)
{
    /* we already have rotation quaternion
     * get vectors from rotation matrix generated by quaternion */
    struct mat3f m;
    mat3_set_ident(&m);
    mat3_set_rotquat(&m, &cam->rot);

    mat3_get_xaxis(&cam->right, &m);
    mat3_get_yaxis(&cam->up, &m);
    mat3_get_zaxis(&cam->look, &m);

    float pitch, yaw, roll;
    mat3_get_roteuler(&pitch, &yaw, &roll, &m);
    cam->pitch_cur = pitch;
    cam->yaw_cur = yaw;
}

void cam_set_viewsize(struct camera* cam, float width, float height)
{
    cam->aspect = width/height;
}

struct mat4f* cam_get_perspective(struct mat4f* r,
                                  const struct camera* cam)
{
    return cam_calc_perspective(r, cam->fov, cam->aspect, cam->fnear, cam->ffar);
}

struct mat4f* cam_calc_perspective(struct mat4f* r, float fov, float aspect, float fnear, float ffar)
{
    float xscale = 1.0f / tanf(fov*0.5f);
    float yscale = aspect*xscale;
    float zf = ffar;
    float zn = fnear;
    return mat4_setf(r,
        xscale,    0.0f,       0.0f,           0.0f,
        0.0f,      yscale,     0.0f,           0.0f,
        0.0f,      0.0f,       zf/(zf-zn),     1.0f,
        0.0f,      0.0f,       zn*zf/(zn-zf),  0.0f);
}


struct mat3f* cam_get_view(struct mat3f* r, const struct camera* cam)
{
    /* rebuild view matrix, based on camera vectors and position
     * view-matrix is actually inverse of the Camera world matrix */
    return mat3_setf(r,
        cam->right.x, cam->up.x, cam->look.x,
        cam->right.y, cam->up.y, cam->look.y,
        cam->right.z, cam->up.z, cam->look.z,
        -vec3_dot(&cam->right, &cam->pos),
        -vec3_dot(&cam->up, &cam->pos),
        -vec3_dot(&cam->look, &cam->pos));
}

void cam_pitch(struct camera* cam, float pitch)
{
    struct quat4f q1;
    struct quat4f q2;

    cam->pitch_cur += pitch;
    if (cam->const_pitch)
        cam->pitch_cur = clampf(cam->pitch_cur, cam->pitch_min, cam->pitch_max);

    quat_fromaxis(&q1, &g_vec3_unity, cam->yaw_cur);
    quat_fromaxis(&q2, &g_vec3_unitx, cam->pitch_cur);
    quat_mul(&cam->rot, &q2, &q1);
}

void cam_yaw(struct camera* cam, float yaw)
{
    struct quat4f q1;
    struct quat4f q2;

    cam->yaw_cur += yaw;

    quat_fromaxis(&q1, &g_vec3_unity, cam->yaw_cur);
    quat_fromaxis(&q2, &g_vec3_unitx, cam->pitch_cur);
    quat_mul(&cam->rot, &q2, &q1);
}

void cam_roll(struct camera* cam, float roll)
{
    quat_fromaxis(&cam->rot, &g_vec3_unitz, roll);
}

void cam_fwd(struct camera* cam, float dz)
{
    struct vec4f d;
    vec3_add(&cam->pos, &cam->pos, vec3_muls(&d, &cam->look, dz));
}

void cam_strafe(struct camera* cam, float dx)
{
    struct vec4f d;
    vec3_add(&cam->pos, &cam->pos, vec3_muls(&d, &cam->right, dx));
}

/*************************************************************************************************
 * camera_fps
 */

struct camera_fps* cam_fps_create(const struct vec4f* pos, const struct vec4f* lookat,
    float fnear, float ffar, float fov)
{
    struct camera_fps* cam = (struct camera_fps*)ALIGNED_ALLOC(sizeof(struct camera_fps), 0);
    if (cam == NULL)    {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return NULL;
    }
    memset(cam, 0x00, sizeof(struct camera_fps));
    cam_fps_init(cam, pos, lookat, fnear, ffar, fov);
    return cam;
}

void cam_fps_destroy(struct camera_fps* cam)
{
    ASSERT(cam);
    ALIGNED_FREE(cam);
}

void cam_fps_init(struct camera_fps* cfps, const struct vec4f* pos, const struct vec4f* lookat,
                  float fnear, float ffar, float fov)
{
    memset(cfps, 0x00, sizeof(struct camera_fps));

    cam_init(&cfps->c, pos, lookat, fnear, ffar, fov);

    /* initialize default values */
    cfps->smooth = TRUE;
    cfps->mouse_smoothness = 70.0f;
    cfps->move_smoothness = 80.0f;

    cfps->fwd_keys[0] = INPUT_KEY_UP;
    cfps->fwd_keys[1] = INPUT_KEY_W;
    cfps->back_keys[0] = INPUT_KEY_S;
    cfps->back_keys[1] = INPUT_KEY_DOWN;
    cfps->strafe_right_keys[0] = INPUT_KEY_RIGHT;
    cfps->strafe_right_keys[1] = INPUT_KEY_D;
    cfps->strafe_left_keys[0] = INPUT_KEY_LEFT;
    cfps->strafe_left_keys[1] = INPUT_KEY_A;

    vec3_setv(&cfps->spos, pos);

    cfps->mouse_speed = 0.005f;
    cfps->move_speed = 0.5f;
}

void cam_fps_set_mousespeed(struct camera_fps* cfps, float speed)
{
    cfps->mouse_speed = maxf(EPSILON, speed);
}

void cam_fps_set_movespeed(struct camera_fps* cfps, float speed)
{
    cfps->move_speed = maxf(EPSILON, speed);
}

void cam_fps_set_keys_fwd(struct camera_fps* cfps, enum input_key key1, enum input_key key2)
{
    cfps->fwd_keys[0] = key1;
    cfps->fwd_keys[1] = key2;
}

void cam_fps_set_keys_backwd(struct camera_fps* cfps, enum input_key key1, enum input_key key2)
{
    cfps->back_keys[0] = key1;
    cfps->back_keys[1] = key2;
}

void cam_fps_set_keys_strafeleft(struct camera_fps* cfps, enum input_key key1, enum input_key key2)
{
    cfps->strafe_left_keys[0] = key1;
    cfps->strafe_left_keys[1] = key2;
}

void cam_fps_set_keys_straferight(struct camera_fps* cfps, enum input_key key1, enum input_key key2)
{
    cfps->strafe_right_keys[0] = key1;
    cfps->strafe_right_keys[1] = key2;
}

void cam_fps_set_smoothing(struct camera_fps* cfps, int smooth)
{
    cfps->smooth = smooth;
}

void cam_fps_set_smoothing_values(struct camera_fps* cfps, float mouse_smoothing, float move_smoothing)
{
    cfps->mouse_smoothness = mouse_smoothing;
    cfps->move_smoothness = move_smoothing;
}

void cam_fps_update(struct camera_fps* cfps, int dx, int dy, float dt)
{
    float dxf = (float)dx;
    float dyf = (float)dy;

    /* mouse movement */
    input_mouse_smooth(&cfps->smx, &cfps->smy, dxf, dyf, cfps->mouse_smoothness, dt);
    cam_yaw(&cfps->c, cfps->smx*cfps->mouse_speed);
    cam_pitch(&cfps->c, cfps->smy*cfps->mouse_speed);

    /* keyboard movement */
    struct vec3f pos;
    struct vec3f tmpv;
    vec3_setv(&pos, &cfps->c.pos);

    if (input_kb_getkey(cfps->fwd_keys[0], FALSE) || input_kb_getkey(cfps->fwd_keys[1], FALSE))
        vec3_add(&pos, &pos, vec3_muls(&tmpv, &cfps->c.look, cfps->move_speed));
    if (input_kb_getkey(cfps->back_keys[0], FALSE) || input_kb_getkey(cfps->back_keys[1], FALSE))
        vec3_add(&pos, &pos, vec3_muls(&tmpv, &cfps->c.look, -cfps->move_speed));
    if (input_kb_getkey(cfps->strafe_right_keys[0], FALSE) ||
        input_kb_getkey(cfps->strafe_right_keys[1], FALSE))
    {
        vec3_add(&pos, &pos, vec3_muls(&tmpv, &cfps->c.right, cfps->move_speed));
    }
    if (input_kb_getkey(cfps->strafe_left_keys[0], FALSE) ||
        input_kb_getkey(cfps->strafe_left_keys[1], FALSE))
    {
        vec3_add(&pos, &pos, vec3_muls(&tmpv, &cfps->c.right, -cfps->move_speed));
    }
    vec3_setf(&cfps->spos,
        math_decay(cfps->spos.x, pos.x, cfps->move_smoothness, dt),
        math_decay(cfps->spos.y, pos.y, cfps->move_smoothness, dt),
        math_decay(cfps->spos.z, pos.z, cfps->move_smoothness, dt));
    vec3_setv(&cfps->c.pos, &cfps->spos);

    cam_update(&cfps->c);
}