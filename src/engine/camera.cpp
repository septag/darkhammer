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

#include "dheng/camera.h"

using namespace dh;

// Camera
Camera::Camera(const dh::Vec3 &pos, const dh::Vec3 &lookat, float fnear, float ffar, float fov)
{
    _pitch_max = PI_HALF - EPSILON - math_torad(5.0f);
    _pitch_min = -PI_HALF + EPSILON + math_torad(5.0f);

    set(pos, lookat, fnear, ffar, fov);
}

void Camera::set(const dh::Vec3 &pos, const dh::Vec3 &lookat, float fnear, float ffar, float fov)
{
    ASSERT(pos != lookat);

    // Make (and normalize) Camera vectors
    _look = Vec3::normalize(lookat - pos);
    _right = Vec3::normalize(Vec3::cross(Vec3::UnitY, _look));
    _up = Vec3::cross(_look, _right);
    Mat3 m(_right.x(), _right.y(), _right.z(),
           _up.x(), _up.y(), _up.z(),
           _look.x(), _look.y(), _look.z(),
           0.0f, 0.0f, 0.0f);

    // Assign values
    _quat = m.rotation_quat();
    _pos = pos;
    _far = tmax<float>(ffar, fnear + EPSILON);
    _near = fnear;
    _fov = fov;

    Vec3 euler = m.rotation_euler();
    _pitch = euler.x();
    _yaw = euler.y();
}

void Camera::set_view(const dh::Vec3 &pos, const dh::Vec3 &lookat)
{
    set(pos, lookat, _near, _far, _fov);
}

void Camera::set_pos(const Vec3 &pos)
{
    _pos = pos;
}

void Camera::set_proj(float fnear, float ffar, float fov)
{
    _far = tmax<float>(ffar, fnear + EPSILON);
    _near = fnear;
    _fov = fov;
}

void Camera::frustum_corners(float aspect_ratio, OUT Vec3 corners[8],
                                float near_override, float far_override) const
{
    const float ffar = far_override != -1.0f ? far_override : _far;
    const float fnear = near_override != -1.0f ? near_override : _near;

    Vec3 xaxis(_right);
    Vec3 yaxis(_up);
    Vec3 zaxis(_look);

    float near_plane_h = tan(_fov * 0.5f) * fnear;
    float near_plane_w = near_plane_h * aspect_ratio;

    float far_plane_h = tan(_fov * 0.5f) * ffar;
    float far_plane_w = far_plane_h * aspect_ratio;

    // Far/Near planes
    Vec3 center_near = zaxis*fnear + _pos;
    Vec3 center_far = zaxis*ffar + _pos;

    // Scaled axises
    Vec3 xaxis_near_scaled = xaxis*near_plane_w;
    Vec3 xaxis_far_scaled = xaxis*far_plane_w;
    Vec3 yaxis_near_scaled = yaxis*near_plane_h;
    Vec3 yaxis_far_scaled = yaxis*far_plane_h;

    // Near quad
    corners[0] = center_near - (xaxis_near_scaled + yaxis_near_scaled);
    corners[1] = center_near - (xaxis_near_scaled - yaxis_near_scaled);
    corners[2] = center_near + (xaxis_near_scaled + yaxis_near_scaled);
    corners[3] = center_near + (xaxis_near_scaled - yaxis_near_scaled);

    // Far quad
    corners[4] = center_far - (xaxis_far_scaled + yaxis_far_scaled);
    corners[5] = center_far - (xaxis_far_scaled - yaxis_far_scaled);
    corners[6] = center_far + (xaxis_far_scaled + yaxis_far_scaled);
    corners[7] = center_far + (xaxis_far_scaled - yaxis_far_scaled);
}

void Camera::frustum_planes(const Mat4 &viewproj, OUT Plane planes[6]) const
{
    const mat4f *vp = viewproj;
    planes[(int)Camera::FrustumPlane::LEFT].set(
                vp->m14 + vp->m11,
                vp->m24 + vp->m21,
                vp->m34 + vp->m31,
                vp->m44 + vp->m41);
    planes[(int)Camera::FrustumPlane::RIGHT].set(
                vp->m14 - vp->m11,
                vp->m24 - vp->m21,
                vp->m34 - vp->m31,
                vp->m44 - vp->m41);
    planes[(int)Camera::FrustumPlane::TOP].set(
                vp->m14 - vp->m12,
                vp->m24 - vp->m22,
                vp->m34 - vp->m32,
                vp->m44 - vp->m42);
    planes[(int)Camera::FrustumPlane::BOTTOM].set(
                vp->m14 + vp->m12,
                vp->m24 + vp->m22,
                vp->m34 + vp->m32,
                vp->m44 + vp->m42);
    planes[(int)Camera::FrustumPlane::PNEAR].set(vp->m13, vp->m23, vp->m33, vp->m43);
    planes[(int)Camera::FrustumPlane::PFAR].set(vp->m14-vp->m13, vp->m24-vp->m23, vp->m34-vp->m33,
                                               vp->m44-vp->m43);

    // Normalize planes
    for (uint i = 0; i < 6; i++)
        planes[i].set_normalize();
}

Mat4 Camera::perspective(float aspect_ratio) const
{
    float xscale = 1.0f / tanf(_fov*0.5f);
    float yscale = aspect_ratio*xscale;
    float zf = _far;
    float zn = _near;
    return Mat4(
        xscale,    0.0f,       0.0f,           0.0f,
        0.0f,      yscale,     0.0f,           0.0f,
        0.0f,      0.0f,       zf/(zf-zn),     1.0f,
        0.0f,      0.0f,       zn*zf/(zn-zf),  0.0f);
}

dh::Mat3 Camera::view() const
{
    return Mat3(
        _right.x(), _up.x(), _look.x(),
        _right.y(), _up.y(), _look.y(),
        _right.z(), _up.z(), _look.z(),
        -Vec3::dot(_right, _pos),
        -Vec3::dot(_up, _pos),
        -Vec3::dot(_look, _pos));
}

void Camera::update()
{
    Mat3 m;
    m.set_rotation_quat(_quat);
    _right = m.row(0);
    _up = m.row(1);
    _look = m.row(2);

    Vec3 euler = m.rotation_euler();
    _pitch = euler.x();
    _yaw = euler.y();
}

void Camera::rotate_pitch(float pitch)
{
    _pitch += tclamp<float>(pitch, _pitch_min, _pitch_max);

    Quat q1, q2;
    q1.from_axis(Vec3::UnitY, _yaw);
    q2.from_axis(Vec3::UnitX, _pitch);
    _quat = q2 * q1;
}

void Camera::rotate_yaw(float yaw)
{
    _yaw += yaw;

    Quat q1, q2;
    q1.from_axis(Vec3::UnitY, _yaw);
    q2.from_axis(Vec3::UnitX, _pitch);
    _quat = q2 * q1;
}

void Camera::rotate_roll(float roll)
{
    Quat q;
    q.from_axis(Vec3::UnitZ, roll);
    _quat *= q;
}

void Camera::move_forward(float df)
{
    _pos += _look * df;
}

void Camera::move_strafe(float ds)
{
    _pos += _right * ds;
}

void Camera::set_pitch_range(float min_pitch, float max_pitch)
{
    _pitch_min = min_pitch;
    _pitch_max = tmax<float>(min_pitch + EPSILON, max_pitch);
}

// C-API
Camera* cam_create(const struct vec4f *pos, const struct vec4f *lookat,
                   float fnear, float ffar, float fov)
{
    Camera *cam = mem_new_aligned<Camera>();
    if (cam == nullptr)
        return nullptr;
    cam->set(Vec3(*pos), Vec3(*lookat), fnear, ffar, fov);
    return cam;
}

void cam_destroy(Camera *cam)
{
    ASSERT(cam);
    mem_delete_aligned<Camera>(cam);
}

void cam_init(Camera *cam, const vec4f *pos, const vec4f *lookat,
              float fnear, float ffar, float fov)
{
    cam->set(Vec3(*pos), Vec3(*lookat), fnear, ffar, fov);
}

void cam_set_pitchconst(Camera *cam, float pitch_min, float pitch_max)
{
    cam->set_pitch_range(pitch_min, pitch_max);
}

void cam_update(Camera* cam)
{
    cam->update();
}

mat4f* cam_get_perspective(const Camera *cam, mat4f *r, float aspect)
{
    return mat4_setm(r, cam->perspective(aspect));
}

mat3f* cam_get_view(const Camera *cam, struct mat3f *r)
{
    return mat3_setm(r, cam->view());
}

void cam_pitch(Camera* cam, float pitch)
{
    cam->rotate_pitch(pitch);
}

void cam_yaw(Camera* cam, float yaw)
{
    cam->rotate_yaw(yaw);
}

void cam_roll(Camera* cam, float roll)
{
    cam->rotate_roll(roll);
}

void cam_fwd(Camera* cam, float dz)
{
    cam->move_forward(dz);
}

void cam_strafe(Camera* cam, float dx)
{
    cam->move_strafe(dx);
}

/**************************************************************************************************/
// CameraFPS
void CameraFPS::set_rotate_speed(float speed)
{
    _rotate_speed = maxf(EPSILON, speed);
}

void CameraFPS::set_move_speed(float speed)
{
    _move_speed = maxf(EPSILON, speed);
}

void CameraFPS::set_keys_forward(inKey key1, inKey key2)
{
    _fwd_keys[0] = key1;
    _fwd_keys[1] = key2;
}

void CameraFPS::set_keys_backward(inKey key1, inKey key2)
{
    _back_keys[0] = key1;
    _back_keys[1] = key2;
}

void CameraFPS::set_keys_strafeleft(inKey key1, inKey key2)
{
    _strafe_left_keys[0] = key1;
    _strafe_left_keys[1] = key2;
}

void CameraFPS::set_keys_straferight(inKey key1, inKey key2)
{
    _strafe_right_keys[0] = key1;
    _strafe_right_keys[1] = key2;
}

void CameraFPS::set_smoothing(bool enable, float mouse_smoothness, float move_smoothness)
{
    _smooth = enable;
    _mouse_smoothness = mouse_smoothness;
    _move_smoothness = move_smoothness;
}

void CameraFPS::update_input(float rotate_dx, float rotate_dy, float dt)
{
    // Rotation (mouse)
    input_mouse_smooth(&_smx, &_smy, rotate_dx, rotate_dy, _mouse_smoothness, dt);
    rotate_yaw(_smx*_rotate_speed);
    rotate_pitch(_smy*_rotate_speed);

    // Keyboard
    Vec3 pos = _pos;
    if (input_kb_getkey(_fwd_keys[0]) || input_kb_getkey(_fwd_keys[1]))
        pos += _look*_move_speed;
    if (input_kb_getkey(_back_keys[0]) || input_kb_getkey(_back_keys[1]))
        pos -= _look*_move_speed;
    if (input_kb_getkey(_strafe_right_keys[0]) || input_kb_getkey(_strafe_right_keys[1]))
        pos += _right*_move_speed;
    if (input_kb_getkey(_strafe_left_keys[0]) || input_kb_getkey(_strafe_left_keys[1]))
        pos -= _right*_move_speed;

    _spos.set(
        math_decay(_spos.x(), pos.x(), _move_smoothness, dt),
        math_decay(_spos.y(), pos.y(), _move_smoothness, dt),
        math_decay(_spos.z(), pos.z(), _move_smoothness, dt));
    _pos = _spos;

    update();
}

// C-API
CameraFPS* cam_fps_create(const vec4f *pos, const vec4f *lookat, float fnear, float ffar, float fov)
{
    CameraFPS *cam = mem_new_aligned<CameraFPS>();
    if (!cam)
        return nullptr;
    cam->set(Vec3(*pos), Vec3(*lookat), fnear, ffar, fov);

    return cam;
}

void cam_fps_destroy(CameraFPS* cam)
{
    ASSERT(cam);
    mem_delete_aligned<CameraFPS>(cam);
}

void cam_fps_init(CameraFPS* cfps, const vec4f *pos, const vec4f *lookat, float fnear, float ffar, 
                  float fov)
{
    cfps->set(Vec3(*pos), Vec3(*lookat), fnear, ffar, fov);
}

void cam_fps_set_mousespeed(CameraFPS* cfps, float speed)
{
    cfps->set_rotate_speed(speed);
}

void cam_fps_set_movespeed(CameraFPS* cfps, float speed)
{
    cfps->set_move_speed(speed);
}

void cam_fps_set_keys_fwd(CameraFPS* cfps, inKey key1, inKey key2)
{
    cfps->set_keys_forward(key1, key2);
}

void cam_fps_set_keys_backwd(CameraFPS* cfps, inKey key1, inKey key2)
{
    cfps->set_keys_backward(key1, key2);
}

void cam_fps_set_keys_strafeleft(CameraFPS* cfps, inKey key1, inKey key2)
{
    cfps->set_keys_strafeleft(key1, key2);
}

void cam_fps_set_keys_straferight(CameraFPS* cfps, inKey key1, inKey key2)
{
    cfps->set_keys_straferight(key1, key2);
}

void cam_fps_set_smoothing(CameraFPS* cfps, bool smooth, float mouse_smoothing, float move_smoothing)
{
    cfps->set_smoothing(smooth, mouse_smoothing, move_smoothing);
}

void cam_fps_updateinput(CameraFPS* cfps, float dx, float dy, float dt)
{
    cfps->update_input(dx, dy, dt);
}
