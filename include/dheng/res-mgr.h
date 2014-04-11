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
 * @defgroup res Resource Manager
 * Resource manager handles loading/unloading of resources\n
 * All resources that are returned by resource manager are in form of handles\n
 * The actual resource objects should be fetched by rs_get_XXXX functions\n
 * Example:\n
 * @code
 * reshandle_t mdl_hdl = rs_load_model("test/myuglyface.h3dm", 0);
 * if (mdl_hdl != INVALID_HANDLE)	{
 * 		struct gfx_model* mdl = rs_get_model(mdl_hdl);
 * 		...
 * }
 * rs_unload(mdl_hdl);
 * @endcode
 */

#ifndef __RESMGR_H__
#define __RESMGR_H__

#include "dhcore/types.h"
#include "engine-api.h"

/* fwd */
struct gfx_obj_data;
typedef struct gfx_obj_data* gfx_texture;
struct gfx_model;
struct allocator;
struct anim_reel_data;
typedef struct anim_reel_data* anim_reel;
typedef void* sct_s;
struct phx_prefab_data;
typedef struct phx_prefab_data* phx_prefab;
struct anim_ctrl_data;
typedef struct anim_ctrl_data* anim_ctrl;

/* init flags */
enum rs_init_flags
{
    RS_FLAG_PREPARE_BGLOAD = (1<<0),
    RS_FLAG_HOTLOADING = (1<<2), /* hot-loading: monitors resources for change and reloads */
    RS_FLAG_BGLOADING = (1<<3)	/* background-loading: loads resources in background */
};

/**
 * Flags that can be used in rs_load_XXXX functions
 * @ingroup res
 */
enum rs_load_flags
{
    RS_LOAD_REFRESH = (1<<0) /**< Reloads resource if it already exists,
    							  otherwise just loads the resource */
};

void rs_zero();
/* flags: combination of rs_init_flags */
result_t rs_initmgr(uint flags, OPTIONAL uint load_thread_cnt);
result_t rs_init_resources();
void rs_release_resources();
void rs_releasemgr();

void rs_reportleaks();
int rs_isinit();
void rs_update();

/* unloads pointer only, resource data in res-mgr remains intact */
void rs_unloadptr(reshandle_t hdl);

/* API */
/**
 * Loads texture and returns a valid texture resource handle if successful
 * @param tex_filepath path to texture file, must be DDS file (with .dds extension)
 * @param first_mipidx first mip level to start loading, 0 indicates that we want to load from
 * 					   highest texture level, higher values creates a lower texture resolution
 * @param srgb sets texture as SRGB texture (will be linearized in shader)
 * @param flags loading options @see rs_load_flags
 * @return handle to loaded resource, =INVALID_HANDLE if error occurred.
 * 		   Type of the texture stored inside resource handle is @a gfx_texture @a
 * @ingroup res
 */
ENGINE_API reshandle_t rs_load_texture(const char* tex_filepath, uint first_mipidx,
		int srgb, uint flags);
/**
 * Loads model and returns a valid model resource handle if successful
 * @param model_filepath path to model file, must be h3dm file (with .h3dm extension)
 * @param flags loading options @see rs_load_flags
 * @return handle to loaded resource, =INVALID_HANDLE if error occurred.
 * 		   Type of the model stored inside resource handle is @a gfx_model @a
 * @ingroup res
 */
ENGINE_API reshandle_t rs_load_model(const char* model_filepath, uint flags);

/**
 * Loads animation reel (h3da) and returns a valid animation resource handle if successful
 * @param reel_filepath path to animation file, must be h3da file (with .h3da extension)
 * @param flags loading options @see rs_load_flags
 * @return handle to loaded resource, =INVALID_HANDLE if error occurred.
 * 		   Type of the animation stored inside resource handle is @a anim_clip @a
 * @ingroup res
 */
ENGINE_API reshandle_t rs_load_animreel(const char* reel_filepath, uint flags);

/**
 * Loads animation controller and returns a valid resource handle if successful
 * @param ctrl_filepath path to animation controller file, must be a valid controller file
 * @param flags loading options @see rs_load_flags
 * @return handle to loaded resource, =INVALID_HANDLE if error occurred.
 * 		   Type of the animation stored inside resource handle is @a anim_clip @a
 * @ingroup res
 */
ENGINE_API reshandle_t rs_load_animctrl(const char* ctrl_filepath, uint flags);

/**
 * Loads script and returns a valid resource handle if successful
 * @param lua_filepath path to script file, must be lua file (with .lua extension)
 * @param flags loading options @see rs_load_flags
 * @return handle to loaded resource, =INVALID_HANDLE if error occurred.
 * 		   Type of the script stored inside resource handle is @a sct_t @a
 * @ingroup res
 */
ENGINE_API reshandle_t rs_load_script(const char* lua_filepath, uint flags);

/**
 * Loads physics prefab and returns a valid resource handle if successful
 * @param phx_filepath path to physics prefab file (with .h3dp extension)
 * @param flags loading options @see rs_load_flags
 * @return handle to loaded resource, =INVALID_HANDLE if error occurred.
 * 		   Type of the object stored inside resource handle is @a phx_prefab @a
 * @ingroup res
 */
ENGINE_API reshandle_t rs_load_phxprefab(const char* phx_filepath, uint flags);

/**
 * Unloads resource handle by decreasing reference count
 * @param hdl Valid resource handle that is previously loaded by resource manager
 * @ingroup res
 */
ENGINE_API void rs_unload(reshandle_t hdl);

/**
 * Fetch actual texture object binded to resource handle\n
 * Unlike rs_load_XXXX functions, @a get @a functions do not add reference count of resource if
 * it's already loaded
 * @ingroup res
 */
ENGINE_API gfx_texture rs_get_texture(reshandle_t tex_hdl);

/**
 * Fetch actual model object binded to resource handle\n
 * Unlike rs_load_XXXX functions, @a get @a functions do not add reference count of resource if
 * it's already loaded
 * @ingroup res
 */
ENGINE_API struct gfx_model* rs_get_model(reshandle_t mdl_hdl);

/**
 * Fetch actual animation object binded to resource handle\n
 * Unlike rs_load_XXXX functions, @a get @a functions do not add reference count of resource if
 * it's already loaded
 * @ingroup res
 */
ENGINE_API anim_reel rs_get_animreel(reshandle_t anim_hdl);

/**
 * Fetch actual animation controller binded \n
 * Unlike rs_load_XXXX functions, @a get @a functions do not add reference count of resource if
 * it's already loaded
 * @ingroup res
 */
ENGINE_API anim_ctrl rs_get_animctrl(reshandle_t ctrl_hdl);

/**
 * Fetch actual animation object binded to resource handle\n
 * Unlike rs_load_XXXX functions, @a get @a functions do not add reference count of resource if
 * it's already loaded
 * @ingroup res
 */
ENGINE_API sct_s rs_get_script(reshandle_t script_hdl);

/**
 * Fetch actual physics prefab object binded to resource handle\n
 * Unlike rs_load_XXXX functions, @a get @a functions do not add reference count of resource if
 * it's already loaded
 * @ingroup res
 */
ENGINE_API phx_prefab rs_get_phxprefab(reshandle_t prefab_hdl);

/**
 * Returns filepath of the loaded resource handle
 * @ingroup res
 */
ENGINE_API const char* rs_get_filepath(reshandle_t hdl);

/* change data allocator for loading resources */
ENGINE_API void rs_set_dataalloc(struct allocator* alloc);

ENGINE_API void rs_add_flags(uint flags);
ENGINE_API void rs_remove_flags(uint flags);
ENGINE_API uint rs_get_flags();

#endif /* RESMGR_H */
