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
 * @defgroup cam Camera
 * Low-level Camera structures and functions\n
 * Currently we have two Camera types: @b Camera and @b CameraFPS\n
 * @CameraFPS is a bit higher level than @Camera and automatically processes input and smoothing.
 */

#ifndef __CAMERA_H__
#define __CAMERA_H__

#include "dhcore/types.h"
#include "dhcore/vec-math.h"
#include "dhcore/prims.h"

#include "dhapp/input.h"

#include "engine-api.h"

/**
 * Low-level Camera, contains all the data that is needed to control basic low-level Camera
 * @ingroup cam
 */
class ALIGN16 Camera
{
public:
    enum class FrustumPlane
    {
        LEFT = 0,
        RIGHT = 1,
        TOP = 2,
        BOTTOM = 3,
        PNEAR = 4,
        PFAR = 5
    };

protected:
    dh::Quat _quat = dh::Quat::Ident;
    dh::Vec3 _look = dh::Vec3::UnitZ;
    dh::Vec3 _right = dh::Vec3::UnitX;
    dh::Vec3 _up = dh::Vec3::UnitY;
    dh::Vec3 _pos = dh::Vec3::Zero;

    float _near = 0.0f;
    float _far = 0.0f;
    float _fov = 0.0f;

    float _pitch = 0.0f;
    float _yaw = 0.0f;

    float _pitch_max = PI_HALF - EPSILON - 0.0872664625f;
    float _pitch_min = -PI_HALF + EPSILON + 0.0872664625f;

public:
    Camera() = default;

    Camera(const dh::Vec3 &pos, const dh::Vec3 &lookat, float fnear, float ffar, float fov);

    void set(const dh::Vec3 &pos, const dh::Vec3 &lookat, float fnear, float ffar, float fov);
    void set_view(const dh::Vec3 &pos, const dh::Vec3 &lookat);
    void set_proj(float fnear, float ffar, float fov);
    void set_pos(const dh::Vec3 &pos);

    void frustum_corners(float aspect_ratio, OUT dh::Vec3 corners[8],
                         float near_override = -1.0f, float far_override = -1.0f) const;
    void frustum_planes(const dh::Mat4 &viewproj, OUT dh::Plane planes[6]) const;

    dh::Mat4 perspective(float aspect_ratio) const;
    dh::Mat3 view() const;

    /*! Call Update when rotation is changed */
    void update();

    void rotate_pitch(float pitch);
    void rotate_yaw(float yaw);
    void rotate_roll(float roll);

    void move_forward(float df);
    void move_strafe(float ds);

    void set_pitch_range(float min_pitch, float max_pitch);

    const dh::Vec3& look() const    {   return _look;  }
    const dh::Vec3& up() const      {   return _up;    }
    const dh::Vec3& right() const   {   return _right; }
    const dh::Vec3& pos() const     {   return _pos;   }
    float clip_near() const         {   return _near;  }
    float clip_far() const          {   return _far;   }
    float fov() const               {   return _fov;   }
};

/**
 * Create Camera, and allocate it's memory from heap. If you have already allocated memory for
 * the Camera, use *cam_init* function instead
 * @see cam_init
 * @ingroup cam
 */
ENGINE_API Camera* cam_create(const vec4f *pos, const vec4f *lookat,
                              float fnear, float ffar, float fov);

/**
 * Destroy created Camera
 * @see cam_create
 * @ingroup cam
 */
ENGINE_API void cam_destroy(Camera *cam);

/**
 * Initialize Camera structure
 * @param cam resulting Camera that needs to be initialized
 * @param pos Camera position
 * @param lookat Camera target position (looking at where?)
 * @param fnear near viewing distance
 * @param ffar far viewing distance
 * @param fov horizontal field-of-view angle (in radians)
 * @ingroup cam
 */
ENGINE_API void cam_init(Camera *cam, const vec4f *pos, const vec4f *lookat,
                         float fnear, float ffar, float fov);

/**
 * Enables/Sets constraints for Camera pitch (rotate around x-axis).
 * @param enable Enables/Disables pitch constraints
 * @param if enable=TRUE, pitch_min defines the minimum allowed pitch when looking down (in radians)
 * @param if enable=TRUE, pitch_max defines the maximum allowed pitch when looking down (in radians)
 * @ingroup cam
 */
ENGINE_API void cam_set_pitchconst(Camera *cam, float pitch_min, float pitch_max);

/**
 * Updates Camera matices and axises. should be called on each frame update or Camera modification
 * @ingroup cam
 */
ENGINE_API void cam_update(Camera *cam);

/**
 * Receives perspective Camera matrix for additional processing. aspect ratio should be calculated
 * using cam_set_viewsize function
 * @param r resulting perspective matrix
 * @return resulting perspective matrix
 * @see cam_set_viewsize @ingroup cam
 */
ENGINE_API mat4f* cam_get_perspective(const Camera *cam, mat4f *r, float aspect);

/**
 * Receives view matrix for additional processing, cam_update must be called before this function
 * in order to properly calculate view matrix
 * @param r resulting view matrix, which is also returned by the function
 * @see cam_update
 * @ingroup cam
 */
ENGINE_API mat3f* cam_get_view(const Camera *cam, mat3f *r);

/**
 * Adds rotation along x-axis angle to the Camera, in radians
 * @ingroup cam
 */
ENGINE_API void cam_pitch(Camera *cam, float pitch);

/**
 * Adds rotation along y-axis angle to the Camera, in radians
 * @ingroup cam
 */
ENGINE_API void cam_yaw(Camera *cam, float yaw);

/**
 * Adds rotation along z-axis angle to the Camera, in radians
 * @ingroup cam
 */
ENGINE_API void cam_roll(Camera *cam, float roll);

/**
 * Moves Camera forward-backward (along Camera's look vector) relative to previous position
 * @ingroup cam
 */
ENGINE_API void cam_fwd(Camera *cam, float dz);

/**
 * Moves Camera right-left (along Camera's right vector) relative to previous position
 * @ingroup cam
 */
ENGINE_API void cam_strafe(Camera *cam, float dx);

/*************************************************************************************************/

/**
 * FPS Camera, includes common Camera and automatically controls input and smoothing.\n
 * @ingroup cam
 */
class ALIGN16 CameraFPS : public Camera
{
private:
    inKey _fwd_keys[2];
    inKey _back_keys[2];
    inKey _strafe_left_keys[2];
    inKey _strafe_right_keys[2];

    // Speed
    float _rotate_speed = 0.005f;
    float _move_speed = 0.5f;

    // Smoothing
    bool _smooth = true;
    float _mouse_smoothness = 70.0f;
    float _move_smoothness = 80.0f;

    float _smx = 0.0f;
    float _smy = 0.0f;
    dh::Vec3 _spos = dh::Vec3::Zero;

public:
    CameraFPS()
    {
        _fwd_keys[0] = inKey::UP;
        _fwd_keys[1] = inKey::W;
        _back_keys[0] = inKey::DOWN;
        _back_keys[1] = inKey::S;
        _strafe_left_keys[0] = inKey::LEFT;
        _strafe_left_keys[1] = inKey::A;
        _strafe_right_keys[0] = inKey::RIGHT; 
        _strafe_right_keys[1] = inKey::D;
    }

    void set_rotate_speed(float speed);
    void set_move_speed(float speed);

    void set_keys_forward(inKey key1, inKey key2 = inKey::COUNT);
    void set_keys_backward(inKey key1, inKey key2 = inKey::COUNT);
    void set_keys_strafeleft(inKey key1, inKey key2 = inKey::COUNT);
    void set_keys_straferight(inKey key1, inKey key2 = inKey::COUNT);

    void set_smoothing(bool enable, float mouse_smoothness = 70.0f, float move_smoothness = 80.0f);

    /*!
     * \brief Update FPS camera (for Rotation)
     * \param rotate_dx DeltaX of rotation (around Y-Axis), usually mouse deltaX
     * \param rotate_dy DeltaY of rotation (around X-AXis), usually mouse deltaY
     * \param dt Delta Time
     */
    void update_input(float rotate_dx, float rotate_dy, float dt);
};

/**
 * Create FPS Camera, and allocate it's memory from heap. If you have already allocated memory for
 * the Camera, use *cam_fps_init* function instead
 * @see cam_fps_init
 * @ingroup cam
 */
ENGINE_API CameraFPS* cam_fps_create(const vec4f *pos, const vec4f *lookat,
                                     float fnear, float ffar, float fov);

/**
 * Destroy created FPS Camera
 * @see cam_fps_create
 * @ingroup cam
 */
ENGINE_API void cam_fps_destroy(CameraFPS *cam);

/**
 * Intializes fps Camera: fps Camera is a low-level wrapper over common Camera that also
 * automatically handles inputs and smoothing, the initialization is very much like initializing a
 * common Camera.
 * @param cfps Fps Camera structure that you want to initialize
 * @param pos Position of the Camera in global space
 * @param lookat Target position of the Camera in global space
 * @param fnear Near view distance
 * @param ffar Far view distance, this parameter greatly effects the view distance and performance
 * @param fov Horizontal field-of-view angle (in radians)
 * @see cam_init
 * @ingroup cam
 */
ENGINE_API void cam_fps_init(CameraFPS *cfps, const vec4f *pos, const vec4f *lookat,
                             float fnear, float ffar, float fov);

/**
 * Sets mouse speed that affects the rotation of the FPS Camera
 * @param cfps Fps Camera
 * @param speed By default, this value is 0.005, and it cannot be less than zero
 * @ingroup cam
 */
ENGINE_API void cam_fps_set_mousespeed(CameraFPS *cfps, float speed);

/**
 * Sets the movement speed of the FPS Camera
 * @param cfps Fps Camera
 * @param speed By default, this value is 0.5, and it cannot be less than zero
 * @ingroup cam
 */
ENGINE_API void cam_fps_set_movespeed(CameraFPS *cfps, float speed);

/**
 * Sets keys that moves the FPS Camera forward, you can set 2 keys for each movement direction\n
 * By default 'W' and 'Up arrow' keys are set for this movement
 * @param cfps Fps Camera
 * @param key1 First key for movement check
 * @param key2 Second key for movement check
 * @see input_key
 * @ingroup cam
 */
ENGINE_API void cam_fps_set_keys_fwd(CameraFPS *cfps, inKey key1, inKey key2);

/**
 * Sets keys that moves the FPS Camera backward, you can set 2 keys for each movement direction\n
 * By default 'S' and 'Down arrow' keys are set for this movement
 * @param cfps Fps Camera
 * @param key1 First key for movement check
 * @param key2 Second key for movement check
 * @see input_key
 * @ingroup cam
 */
ENGINE_API void cam_fps_set_keys_backwd(CameraFPS *cfps, inKey key1, inKey key2);

/**
 * Sets keys that moves the FPS Camera forward, you can set 2 keys for each movement direction\n
 * By default 'A' and 'Left arrow' keys are set for this movement
 * @param cfps Fps Camera
 * @param key1 First key for movement check
 * @param key2 Second key for movement check
 * @see input_key
 * @ingroup cam
 */
ENGINE_API void cam_fps_set_keys_strafeleft(CameraFPS *cfps, inKey key1, inKey key2);

/**
 * Sets keys that moves the FPS Camera forward, you can set 2 keys for each movement direction\n
 * By default 'D' and 'Right arrow' keys are set for this movement
 * @param cfps Fps Camera
 * @param key1 First key for movement check
 * @param key2 Second key for movement check
 * @see input_key
 * @ingroup cam
 */
ENGINE_API void cam_fps_set_keys_straferight(CameraFPS *cfps, inKey key1, inKey key2);

/**
 * Enable/Disable FPS Camera smoothing. It makes rotation and movement less jerky.
 * @param cfps Fps Camera
 * @param smooth By default smoothing is TRUE, to disable it, set this parameter to FALSE
 * @ingroup cam
 */
ENGINE_API void cam_fps_set_smoothing(CameraFPS *cfps, bool smooth);

/**
 * Updates FPS Camera and it's internal Camera structure. Must be called before any rendering.
 * @param cfps Fps Camera
 * @param dx Delta of the mouse position (X) with the previous frame (in pixels)
 * @param dy Delta of the mouse position (Y) with the previous frame (in pixels)
 * @param dt Delta time between current frame and the previous frame (in seconds)
 * @ingroup cam
 */
ENGINE_API void cam_fps_updateinput(CameraFPS *cfps, float dx, float dy, float dt);

#endif /* CAMERA_H */
