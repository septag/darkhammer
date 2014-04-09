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
 * Low-level camera structures and functions\n
 * Currently we have two camera types: @b camera and @b camera_fps\n
 * @camera_fps is a bit higher level than @camera and automatically processes input and smoothing.
 */

#ifndef __CAMERA_H__
#define __CAMERA_H__

#include "dhcore/types.h"
#include "dhcore/vec-math.h"
#include "dhcore/prims.h"
#include "engine-api.h"
#include "dhapp/input.h"

#define CAM_FRUSTUM_LEFT    0
#define CAM_FRUSTUM_RIGHT   1
#define CAM_FRUSTUM_TOP		2
#define CAM_FRUSTUM_BOTTOM	3
#define CAM_FRUSTUM_NEAR    4
#define CAM_FRUSTUM_FAR     5

/**
 * Low-level camera structure, contains all the data that is needed to control basic low-level camera
 * @ingroup cam
 */
struct ALIGN16 camera
{
    struct quat4f rot;  /* camera rotation */
    struct vec4f look; /* camera look vector (normalized) */
    struct vec4f right; /* camera right vector (normalized) */
    struct vec4f up; /* camera up vector (normalized) */
    struct vec4f pos; /* camera position */

    float fnear;     /**< near view distance */
    float ffar;      /**< far view distance */
    float fov;       /**< horizontal fov (field-of-view) angle (in radians) */
    float aspect;    /**< viewport aspect ration (width/height) */

    float pitch_cur;
    float yaw_cur;

    bool_t const_pitch; /* constraint pitch */
    float pitch_max;
    float pitch_min;
};

/**
 * FPS camera, includes common camera and automatically controls input and smoothing.\n
 * @ingroup cam
 */
struct ALIGN16 camera_fps
{
    struct camera c;    /**< Embedded common camera, you can use this to set active camera to scene or other low-level camera related stuff */

    enum input_key fwd_keys[2];
    enum input_key back_keys[2];
    enum input_key strafe_left_keys[2];
    enum input_key strafe_right_keys[2];

    /* speeds */
    float mouse_speed;
    float move_speed;

    /* smoothing */
    bool_t smooth;
    float mouse_smoothness;
    float move_smoothness;

    float smx;
    float smy;
    struct vec3f spos;
};

void cam_calc_frustumcorners(const struct camera* cam, struct vec4f* corners,
                             float* near_override, float* far_override);
void cam_calc_frustumplanes(struct plane planes[6], const struct mat4f* viewproj);
struct mat4f* cam_calc_perspective(struct mat4f* r, float fov, float aspect, float fnear, float ffar);

/**
 * Create Camera, and allocate it's memory from heap. If you have already allocated memory for
 * the camera, use *cam_init* function instead
 * @see cam_init
 * @ingroup cam
 */
ENGINE_API struct camera* cam_create(const struct vec4f* pos, const struct vec4f* lookat,
                                     float fnear, float ffar, float fov);

/**
 * Destroy created camera
 * @see cam_create
 * @ingroup cam
 */
ENGINE_API void cam_destroy(struct camera* cam);

/**
 * Initialize camera structure
 * @param cam resulting camera that needs to be initialized
 * @param pos camera position
 * @param lookat camera target position (looking at where?)
 * @param fnear near viewing distance
 * @param ffar far viewing distance
 * @param fov horizontal field-of-view angle (in radians)
 * @ingroup cam
 */
ENGINE_API void cam_init(struct camera* cam, const struct vec4f* pos, const struct vec4f* lookat,
    float fnear, float ffar, float fov);

/**
 * Enables/Sets constraints for camera pitch (rotate around x-axis).
 * @param enable Enables/Disables pitch constraints
 * @param if enable=TRUE, pitch_min defines the minimum allowed pitch when looking down (in radians)
 * @param if enable=TRUE, pitch_max defines the maximum allowed pitch when looking down (in radians)
 * @ingroup cam
 */
ENGINE_API void cam_set_pitchconst(struct camera* cam, bool_t enable,
    float pitch_min, float pitch_max);

/**
 * Sets viewport size for the camera, must be called __before__ `cam_get_prespective` or resizing
 * the view
 * @param width width of the viewport (render-view) in pixels
 * @param height height of the viewport (render-view) in pixels
 * @see cam_get_perspective @ingroup cam
 */
ENGINE_API void cam_set_viewsize(struct camera* cam, float width, float height);

/**
 * Updates camera matices and axises. should be called on each frame update or camera modification
 * @ingroup cam
 */
ENGINE_API void cam_update(struct camera* cam);

/**
 * Receives perspective camera matrix for additional processing. aspect ratio should be calculated
 * using cam_set_viewsize function
 * @param r resulting perspective matrix
 * @return resulting perspective matrix
 * @see cam_set_viewsize @ingroup cam
 */
ENGINE_API struct mat4f* cam_get_perspective(struct mat4f* r, const struct camera* cam);

/**
 * Receives view matrix for additional processing, cam_update must be called before this function
 * in order to properly calculate view matrix
 * @param r resulting view matrix, which is also returned by the function
 * @see cam_update
 * @ingroup cam
 */
ENGINE_API struct mat3f* cam_get_view(struct mat3f* r, const struct camera* cam);

/**
 * Adds rotation along x-axis angle to the camera, in radians
 * @ingroup cam
 */
ENGINE_API void cam_pitch(struct camera* cam, float pitch);
/**
 * Adds rotation along y-axis angle to the camera, in radians
 * @ingroup cam
 */
ENGINE_API void cam_yaw(struct camera* cam, float yaw);
/**
 * Adds rotation along z-axis angle to the camera, in radians
 * @ingroup cam
 */
ENGINE_API void cam_roll(struct camera* cam, float roll);
/**
 * Moves camera forward-backward (along camera's look vector) relative to previous position
 * @ingroup cam
 */
ENGINE_API void cam_fwd(struct camera* cam, float dz);
/**
 * Moves camera right-left (along camera's right vector) relative to previous position
 * @ingroup cam
 */
ENGINE_API void cam_strafe(struct camera* cam, float dx);

/*************************************************************************************************/
/**
 * Create FPS Camera, and allocate it's memory from heap. If you have already allocated memory for
 * the camera, use *cam_fps_init* function instead
 * @see cam_fps_init
 * @ingroup cam
 */
ENGINE_API struct camera_fps* cam_fps_create(const struct vec4f* pos, const struct vec4f* lookat,
                                             float fnear, float ffar, float fov);

/**
 * Destroy created FPS camera
 * @see cam_fps_create
 * @ingroup cam
 */
ENGINE_API void cam_fps_destroy(struct camera_fps* cam);

/**
 * Intializes fps camera: fps camera is a low-level wrapper over common camera that also
 * automatically handles inputs and smoothing, the initialization is very much like initializing a
 * common camera.
 * @param cfps Fps Camera structure that you want to initialize
 * @param pos Position of the camera in global space
 * @param lookat Target position of the camera in global space
 * @param fnear Near view distance
 * @param ffar Far view distance, this parameter greatly effects the view distance and performance
 * @param fov Horizontal field-of-view angle (in radians)
 * @see cam_init
 * @ingroup cam
 */
ENGINE_API void cam_fps_init(struct camera_fps* cfps, const struct vec4f* pos,
                             const struct vec4f* lookat, float fnear, float ffar, float fov);

/**
 * Sets mouse speed that affects the rotation of the FPS camera
 * @param cfps Fps camera
 * @param speed By default, this value is 0.005, and it cannot be less than zero
 * @ingroup cam
 */
ENGINE_API void cam_fps_set_mousespeed(struct camera_fps* cfps, float speed);

/**
 * Sets the movement speed of the FPS camera
 * @param cfps Fps camera
 * @param speed By default, this value is 0.5, and it cannot be less than zero
 * @ingroup cam
 */
ENGINE_API void cam_fps_set_movespeed(struct camera_fps* cfps, float speed);

/**
 * Sets keys that moves the FPS camera forward, you can set 2 keys for each movement direction\n
 * By default 'W' and 'Up arrow' keys are set for this movement
 * @param cfps Fps camera
 * @param key1 First key for movement check
 * @param key2 Second key for movement check
 * @see input_key
 * @ingroup cam
 */
ENGINE_API void cam_fps_set_keys_fwd(struct camera_fps* cfps, enum input_key key1,
                                     enum input_key key2);

/**
 * Sets keys that moves the FPS camera backward, you can set 2 keys for each movement direction\n
 * By default 'S' and 'Down arrow' keys are set for this movement
 * @param cfps Fps camera
 * @param key1 First key for movement check
 * @param key2 Second key for movement check
 * @see input_key
 * @ingroup cam
 */
ENGINE_API void cam_fps_set_keys_backwd(struct camera_fps* cfps, enum input_key key1,
                                        enum input_key key2);

/**
 * Sets keys that moves the FPS camera forward, you can set 2 keys for each movement direction\n
 * By default 'A' and 'Left arrow' keys are set for this movement
 * @param cfps Fps camera
 * @param key1 First key for movement check
 * @param key2 Second key for movement check
 * @see input_key
 * @ingroup cam
 */
ENGINE_API void cam_fps_set_keys_strafeleft(struct camera_fps* cfps, enum input_key key1,
                                            enum input_key key2);

/**
 * Sets keys that moves the FPS camera forward, you can set 2 keys for each movement direction\n
 * By default 'D' and 'Right arrow' keys are set for this movement
 * @param cfps Fps camera
 * @param key1 First key for movement check
 * @param key2 Second key for movement check
 * @see input_key
 * @ingroup cam
 */
ENGINE_API void cam_fps_set_keys_straferight(struct camera_fps* cfps, enum input_key key1,
                                            enum input_key key2);

/**
 * Enable/Disable FPS camera smoothing. It makes rotation and movement less jerky.
 * @param cfps Fps camera
 * @param smooth By default smoothing is TRUE, to disable it, set this parameter to FALSE
 * @ingroup cam
 */
ENGINE_API void cam_fps_set_smoothing(struct camera_fps* cfps, bool_t smooth);

/**
 * Set smoothing values for FPS camera, smoothing values are usually between 50~100, but you
 * can experiment with different values for more obscene results
 * @param mouse_smoothing Mouse smoothing, by default this value is 70
 * @param move_smoothing Movement smoothing, by default this value is 80
 * @ingroup cam
 */
ENGINE_API void cam_fps_set_smoothing_values(struct camera_fps* cfps, float mouse_smoothing,
                                             float move_smoothing);

/**
 * Updates FPS camera and it's internal camera structure. Must be called before any rendering.
 * @param cfps Fps camera
 * @param dx Delta of the mouse position (X) with the previous frame (in pixels)
 * @param dy Delta of the mouse position (Y) with the previous frame (in pixels)
 * @param dt Delta time between current frame and the previous frame (in seconds)
 * @ingroup cam
 */
ENGINE_API void cam_fps_update(struct camera_fps* cfps, int dx, int dy, float dt);

#endif /* CAMERA_H */
