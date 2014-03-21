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
 * @defgroup gfx Graphics
 * Includes all graphics/renderer related functions and data structures
 */

#ifndef GFX_H_
#define GFX_H_

#include "dhcore/types.h"
#include "dhcore/json.h"
#include "dhcore/array.h"
#include "dhcore/hash-table.h"
#include "dhcore/vec-math.h"
#include "dhcore/linked-list.h"

#include "engine-api.h"
#include "gfx-types.h"
#include "init-params.h"
#include "cmp-types.h"

/* fwd */
struct gfx_model_mtl;
struct gfx_model_geo;
struct gfx_model_posegpu;
struct scn_render_query;
struct scn_render_light;
struct gfx_shader;

/* global defines */
#define GFX_INSTANCES_MAX 32
#define GFX_DEFAULT_RENDER_OBJ_CNT 2000
#define GFX_SKIN_BONES_MAX 64

/* each batch is mainly identified by it's unique_id
 * 'unique_id' represents all the stuff that a sub-object needs for a draw (hashed)
 * to draw batch_node:
 * 	- cast ritem to proper scn_render_XXX structure (see parent gfx_batch_item to identify type)
 * 	- use sub_idx to access the sub-obj (depending on the object)
 * 	- set material constants and textures for each batch_node
 * 	- recurse the 'bll' (batch linked_list) :
 * 		- draw in instanced mode with transform matrices (instance_mats) and count (instance_cnt)
 */
struct gfx_batch_node
{
    uint unique_id; /* keep the unique id for instancing */
	uint sub_idx; /* =INVALID_INDEX if the whole mesh is needed to draw in one call */
	void* ritem;	/* pointer to scn_render_XXX (see scene-mgr.h), must cast based on obj_type */
	uint instance_cnt;
	const struct mat3f* instance_mats[GFX_INSTANCES_MAX];
    const struct gfx_model_posegpu* poses[GFX_INSTANCES_MAX];
	struct linked_list lnode; /* items with same unique-ids will be linked together */
    struct linked_list* bll; /* linked-list connecting batch-nodes: only the first one uses this */
    uint64 meta_data;
};

/* items presents a full batch, which contains a linked_list to render items (batch nodes) */
struct gfx_batch_item
{
	enum cmp_obj_type objtype;
	uint shader_id;
    struct hashtable_chained uid_table;  /* key: unique_id, value: index to nodes */
    struct array nodes; /* item: gfx_batch_node */
};

/* possible output result from render-paths :
 * if render-path should output any of these components, then they should not be NULL
*/
struct gfx_rpath_result
{
    gfx_rendertarget rt;    /* result render-target, can contain color/normals/depth/etc... */
};

struct gfx_renderpass_lightdata
{
    uint cnt;
    struct scn_render_light* lights;
    struct sphere* bounds;  /* global bounds referenced by lights (see scn_render_query) */
};

/* callbacks - must be implemented by render-path */
typedef uint (*pfn_gfx_rpath_getshader)(enum cmp_obj_type obj_type, uint rpath_flags);
typedef result_t (*pfn_gfx_rpath_init)(uint width, uint height);
typedef void (*pfn_gfx_rpath_release)();
typedef void (*pfn_gfx_rpath_render)(gfx_cmdqueue cmdqueue, gfx_rendertarget rt,
		const struct gfx_view_params* params, struct gfx_batch_item* batch_items, uint batch_cnt,
		void* userdata, OUT struct gfx_rpath_result* result);
typedef result_t (*pfn_gfx_rpath_resize)(uint width, uint height);

/* render-path: callback functions to render a subset of render data and choose shaders */
struct gfx_rpath
{
	const char* name;
	pfn_gfx_rpath_getshader getshader_fn;
	pfn_gfx_rpath_init init_fn;
	pfn_gfx_rpath_release release_fn;
	pfn_gfx_rpath_render render_fn;
    pfn_gfx_rpath_resize resize_fn;
};

/**
 * Additional debug render callback\n
 * Note: debug render implementation automatically comes between canvas3d_begin/canvas3d_end\n
 * Do the user shouldn't be worried about those calls, he can just call gfx_canvas_XXX functions within the body of the callback
 * @param cmdqueue current command queue
 * @param render params @see gfx_view_params
 * @ingroup gfx
 */
typedef void (*pfn_debug_render)(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params);


/* */
/*************************************************************************************************
 * internal
 */
/* init/release functions */
void gfx_zero();
void gfx_parseparams(struct gfx_params* params, json_t j);
result_t gfx_init(const struct gfx_params* params);
void gfx_release();

/* render/display */
void gfx_render();
_EXTERN_ gfx_cmdqueue gfx_get_cmdqueue(OPTIONAL uint id);

/* render-path */
const struct gfx_rpath* gfx_rpath_detect(enum cmp_obj_type obj_type, uint rpath_flags);
const char* gfx_rpath_getflagstr(uint rpath_flags);

/* used internally by gfx-device(es) in order to set current render-target size parameters */
void gfx_set_rtvsize(uint width, uint height);

/* misc */
gfx_sampler gfx_get_globalsampler();
gfx_sampler gfx_get_globalsampler_low();
void gfx_draw_fullscreenquad();
const struct gfx_params* gfx_get_params();
void gfx_set_previewrenderflag();



void gfx_resize(uint width, uint height);

/*************************************************************************************************
 * API
 */

/* default callback implementations */
void gfx_render_grid(gfx_cmdqueue cmdqueue, const struct gfx_view_params* params);

ENGINE_API void gfx_set_gridcallback(bool_t enable);

/**
 * Sets debug render function callback, for additional custom debug rendering by the application\n
 * debug render happens within canvas3d executation, so only gfx_canvas_XXXX calls are valid in debug callback function body
 * @param fn Callback function for debug rendering override @see pfn_debug_render
 * @ingroup gfx
 */
ENGINE_API void gfx_set_debug_renderfunc(pfn_debug_render fn);


#endif /* GFX_H_ */
