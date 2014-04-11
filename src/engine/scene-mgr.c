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
#include "dhcore/linked-list.h"
#include "dhcore/array.h"
#include "dhcore/pool-alloc.h"
#include "dhcore/vec-math.h"
#include "dhcore/variant.h"
#include "dhcore/hash-table.h"
#include "dhcore/stack-alloc.h"
#include "dhcore/freelist-alloc.h"
#include "dhcore/stack.h"

#include "scene-mgr.h"
#include "mem-ids.h"
#include "gfx-model.h"
#include "res-mgr.h"
#include "cmp-mgr.h"
#include "engine.h"
#include "prf-mgr.h"
#include "gfx-occ.h"
#include "camera.h"
#include "gfx.h"
#include "gfx-canvas.h"
#include "console.h"
#include "lod-scheme.h"
#include "debug-hud.h"
#include "phx-device.h"
#include "phx.h"
#include "world-mgr.h"

#include "components/cmp-model.h"
#include "components/cmp-bounds.h"
#include "components/cmp-xform.h"
#include "components/cmp-light.h"
#include "components/cmp-lodmodel.h"
#include "components/cmp-camera.h"

#define SCN_OBJ_BLOCKSIZE	200
#define SCN_OCC_NEAR_THRESHOLD 10.0f /* N meters that we always draw occluders */
#define SCN_GRID_BLOCKSIZE 200
#define SCN_GRID_CELLSIZE 50.0f /* N units of cell dimension size */

#define SIGNBIT(d) ((d).i & 0x80000000)

/*************************************************************************************************
 * types
 */
enum scn_mem_mgr
{
    SCN_MEM_HEAP = 0,
    SCN_MEM_FREELIST,
    SCN_MEM_STACK
};

struct scn_grid_item
{
    struct linked_list cell_node; /* linked-list node for list of items in each cell */
    struct linked_list bounds_node; /* linked-list node for list of cells in bounds component */
    struct cmp_obj* obj;
    uint cell_id;
};

struct scn_grid
{
    uint cell_cnt;
    uint col_cnt;
    uint row_cnt;
    float cell_size;
    struct vec4f* cells;    /*count: 2*cell_cnt, grows if required by cell_cnt */
    struct pool_alloc item_pool;    /* item: scn_grid_item */
    struct linked_list** items;   /* pointer to cell items, count: cell_cnt */
    int* vis_cells;  /* count: cell_cnt */
};

struct scn_data
{
	char name[32];
    struct array objs;  /* item: cmp_obj* */
    struct array spatial_updates;   /* item: cmphandle_t (bounds) */
    struct scn_grid grid;
    struct vec3f minpt;
    struct vec3f maxpt;
    uint phx_sceneid; /* physics scene-id */
};

struct scn_mgr
{
    uint active_scene_id;
	struct array scenes;	/* item: scn_data* */
	struct pool_alloc obj_pool;	/* item: cmp_obj */
    struct camera* active_cam;
    struct array vis_objs;  /* item: cmp_obj* */
    int debug_grid;
    struct array global_objs;   /* item: cmp_obj* */
    struct stack* free_scenes;   /* item: index to scenes array, free scene indexes array */
};

/*************************************************************************************************
 * fwd declarations
 */
struct scn_data* scene_create(const char* name);
void scene_destroy(struct scn_data* s);
void scene_destroy_objcmps(struct cmp_obj* obj);

void scene_gather_models_csm(struct scn_data* s, struct array* objs);

/* object addition + lod */
uint scene_add_model(struct cmp_obj* obj, uint bounds_idx, uint item_idx, struct array* mats,
    struct array* models, const struct gfx_view_params* params, OUT uint* obj_idx);
uint scene_add_light(struct cmp_obj* obj, uint bounds_idx, uint item_idx, struct array* mats,
    struct array* lights, const struct gfx_view_params* params, OUT uint* obj_idx);
uint scene_add_model_shadow(struct cmp_obj* obj, uint bounds_idx, uint item_idx,
    struct array* mats, struct array* models, const struct gfx_view_params* params,
        OUT uint* obj_idx);

struct scn_render_model* scene_create_rendermodels(struct allocator* alloc, struct array* models,
    struct mat3f* mats, struct sphere* bounds, uint item_offset, OUT uint* pcnt);

/* culling */
void scene_cullspheres(int* vis, const struct plane frust[6], const struct sphere* bounds,
		uint startidx, uint endidx);
void scene_cullspheres_nosimd(int* vis, const struct plane frust[6], const struct sphere* bounds,
		uint startidx, uint endidx);
uint scene_cullgrid_sphere(const struct scn_grid* grid, OUT struct cmp_obj** objs,
    const struct sphere* sphere);
void scene_cull_aabbs_sweep(int* vis, const struct aabb* frust_aabb, const struct vec3f* dir,
    const struct aabb* aabbs, uint startidx, uint endidx);
void scene_draw_occluders(struct allocator* alloc, struct cmp_obj** objs, uint obj_cnt,
    const int* vis, const struct gfx_view_params* params);
int scene_test_occlusion(const int* vis, INOUT struct cmp_obj** objs, uint* bound_idxs,
    uint obj_cnt, const struct gfx_view_params* params);
uint scene_cullgrid(const struct scn_grid* grid, INOUT struct cmp_obj** objs, uint start_idx,
    uint end_idx, const struct plane frust[6]);

/* space partitioning (grid) */
result_t scene_grid_init(struct scn_grid* grid, float cell_size, const struct vec3f* world_min,
    const struct vec3f* world_max);
void scene_grid_release(struct scn_grid* grid);
result_t scene_grid_resize(uint scene_id, const struct vec3f* world_min,
    const struct vec3f* world_max, float cell_size);
result_t scene_grid_createcells(struct scn_grid* grid, float cell_size, const struct vec3f* minpt,
    const struct vec3f* maxpt);
void scene_grid_destroycells(struct scn_grid* grid);
void scene_grid_clear(struct scn_grid* grid);
void scene_grid_push(struct scn_grid* grid, struct allocator* alloc, const cmphandle_t* obj_bounds,
    uint start_idx, uint end_idx);
void scene_grid_pull(struct scn_grid* grid, const cmphandle_t* obj_bounds, uint start_idx,
    uint end_idx);
void scene_grid_pushsingle(struct scn_grid* grid, cmphandle_t bounds_hdl);
void scene_grid_pullsingle(struct scn_grid* grid, cmphandle_t bounds_hdl);
void scene_grid_addtocell(struct scn_grid* grid, uint cell_id, cmphandle_t bounds_hdl);
void scene_grid_removefromcell(struct scn_grid* grid, uint cell_id, struct scn_grid_item* item);
void scene_calc_frustum_projxz(struct plane frust_proj[4], const struct plane frust_planes[6]);
struct rect2di* scene_conv_coord(struct rect2di* rc, float x_min, float y_min,
    float x_max, float y_max, const struct rect2df* src_coord, const struct rect2df* res_coord);
struct vec2i* scene_conv_coordpt(struct vec2i* pt, float x, float y,
    const struct rect2df* src_coord, const struct rect2df* res_coord);
struct color* scene_get_densitycolor(struct color* c, const struct linked_list* cell,
    const struct scn_grid* grid);

void scene_grid_debug(struct scn_grid* grid, const struct camera* cam);
result_t scene_console_debuggrid(uint argc, const char** argv, void* param);
result_t scene_console_setcellsize(uint argc, const char** argv, void* param);
result_t scene_console_campos(uint argc, const char** argv, void* param);
int scene_debug_cam(gfx_cmdqueue cmqueue, int x, int y, int line_stride, void* param);

result_t scene_create_components(struct cmp_obj* obj);
cmphandle_t scene_add_component(struct cmp_obj* obj, cmptype_t type);

void* scene_alloc(size_t size, const char* source, uint line, uint id, void* param);
void scene_free(void* ptr, void* param);
void* scene_aligned_alloc(size_t size, uint8 alignement, const char* source, uint line, uint id,
                          void* param);
void scene_aligned_free(void* ptr, void* param);
void scene_bindalloc(struct scn_data* s);

/*************************************************************************************************
 * globals
 */
struct scn_mgr g_scn_mgr;

/*************************************************************************************************
 * inlines
 */
INLINE struct scn_data* scene_get(uint scene_id)
{
	ASSERT(scene_id != 0 && scene_id <= g_scn_mgr.scenes.item_cnt);
	struct scn_data* s = ((struct scn_data**)g_scn_mgr.scenes.buffer)[scene_id-1];
	return s;
}


INLINE int scene_test_occ_addobj(INOUT struct cmp_obj** objs, uint* bound_idxs, uint idx,
    int vis_idx)
{
    swapui(&bound_idxs[vis_idx], &bound_idxs[idx]);
    objs[vis_idx] = objs[idx];
    return vis_idx + 1;
}

INLINE struct array* scene_getobjarr(uint scene_id)
{
    ASSERT(scene_id != 0);
    if (scene_id != SCENE_GLOBAL)
        return &scene_get(scene_id)->objs;
    else
        return &g_scn_mgr.global_objs;
}

/*************************************************************************************************/
void scn_zero()
{
	memset(&g_scn_mgr, 0x00, sizeof(g_scn_mgr));
}

result_t scn_initmgr()
{
	result_t r;

    log_print(LOG_TEXT, "init scene-mgr ...");

	r = arr_create(mem_heap(), &g_scn_mgr.scenes, sizeof(struct scn_data*), 5, 5, MID_SCN);
	if (IS_FAIL(r))
		return RET_OUTOFMEMORY;

	r = mem_pool_create(mem_heap(), &g_scn_mgr.obj_pool, sizeof(struct cmp_obj), SCN_OBJ_BLOCKSIZE,
			MID_SCN);
	if (IS_FAIL(r))
		return RET_OUTOFMEMORY;

    r = arr_create(mem_heap(), &g_scn_mgr.vis_objs, sizeof(struct cmp_obj*), 300, 300, MID_SCN);
    if (IS_FAIL(r))
        return RET_OUTOFMEMORY;

    /* */
    r = arr_create(mem_heap(), &g_scn_mgr.global_objs, sizeof(struct cmp_obj*), 20, 40, MID_SCN);
    if (IS_FAIL(r))
        return RET_OUTOFMEMORY;

    /* console */
    if (BIT_CHECK(eng_get_params()->flags, ENG_FLAG_DEV))   {
        con_register_cmd("showgrid", scene_console_debuggrid, NULL, "showgrid [1*/0]");
        con_register_cmd("setcellsize", scene_console_setcellsize, NULL, "setgridsize N");
    }
    con_register_cmd("showcam", scene_console_campos, NULL, "showcam [1*/0]");

	return RET_OK;
}

void scn_releasemgr()
{
    scn_clear(SCENE_GLOBAL);

	for (uint i = 0, cnt = g_scn_mgr.scenes.item_cnt; i < cnt; i++)	{
		struct scn_data* s = ((struct scn_data**)g_scn_mgr.scenes.buffer)[i];
        if (s != NULL)  {
            log_printf(LOG_WARNING, "releasing un-released scene '%s' ...", s->name);
		    scene_destroy(s);
        }
	}
	arr_destroy(&g_scn_mgr.scenes);

    uint obj_leaks = mem_pool_getleaks(&g_scn_mgr.obj_pool);
	if (obj_leaks > 0)	{
		log_print(LOG_WARNING, "un-released objects found in scene-mgr, "
				"attempting to force release.");
	}

	mem_pool_destroy(&g_scn_mgr.obj_pool);
    arr_destroy(&g_scn_mgr.vis_objs);
    arr_destroy(&g_scn_mgr.global_objs);

    struct stack* stack_item;
    while ((stack_item = stack_pop(&g_scn_mgr.free_scenes)) != NULL)    {
        FREE(stack_item);
    }

    log_print(LOG_TEXT, "scene-mgr released.");
}

uint scn_create_scene(const char* name)
{
    /* search in previously created scenes */
    for (uint i = 0, cnt = g_scn_mgr.scenes.item_cnt; i < cnt; i++)   {
        struct scn_data* s = ((struct scn_data**)g_scn_mgr.scenes.buffer)[i];
        if (str_isequal(s->name, name))
            return i + 1;
    }

    /* create a new one */
    struct stack* free_item;
	struct scn_data* s = scene_create(name);
    uint id = 0;

	if (s != NULL)	{
        if ((free_item = stack_pop(&g_scn_mgr.free_scenes)) != NULL)  {
            uint idx = (uint)(uptr_t)free_item->data;
            FREE(free_item);
            ((struct scn_data**)g_scn_mgr.scenes.buffer)[idx] = s;
            return idx + 1;
        }   else    {
            struct scn_data** ps = (struct scn_data**)arr_add(&g_scn_mgr.scenes);
            ASSERT(ps);
            *ps = s;
            id = g_scn_mgr.scenes.item_cnt;
        }
	}

    return id;
}

void scn_destroy_scene(uint scene_id)
{
	struct scn_data* s = scene_get(scene_id);
	ASSERT(s);
	scene_destroy(s);

    /* push scene's index to free stack */
    uptr_t idx = scene_id - 1;
    struct stack* free_item = (struct stack*)ALLOC(sizeof(struct stack), MID_SCN);
    stack_push(&g_scn_mgr.free_scenes, free_item, (void*)idx);
	((struct scn_data**)g_scn_mgr.scenes.buffer)[idx] = NULL;
}

uint scn_findscene(const char* name)
{
    for (uint i = 0, cnt = g_scn_mgr.scenes.item_cnt; i < cnt; i++)   {
        struct scn_data* s = ((struct scn_data**)g_scn_mgr.scenes.buffer)[i];
        if (str_isequal(s->name, name))
            return i + 1;
    }
    return 0;
}

struct scn_data* scene_create(const char* name)
{
	struct scn_data* s = (struct scn_data*)ALLOC(sizeof(struct scn_data), MID_SCN);
	if (s == NULL)
		return NULL;
	memset(s, 0x00, sizeof(struct scn_data));
	strcpy(s->name, name);

    if (IS_FAIL(arr_create(mem_heap(), &s->spatial_updates, sizeof(cmphandle_t), SCN_OBJ_BLOCKSIZE,
        SCN_OBJ_BLOCKSIZE*2, MID_SCN)))
    {
        FREE(s);
        return NULL;
    }

    /* object bank */
    if (IS_FAIL(arr_create(mem_heap(), &s->objs, sizeof(struct cmp_obj*), SCN_OBJ_BLOCKSIZE,
        SCN_OBJ_BLOCKSIZE*2, MID_SCN)))
    {
        scene_destroy(s);
        return NULL;
    }

    /* default scene boundary size */
    vec3_setf(&s->minpt, -250.0f, -10.0f, -250.0f);
    vec3_setf(&s->maxpt, 250.0f, 100.0f, 250.0f);

    /* create grid */
    if (IS_FAIL(scene_grid_init(&s->grid, SCN_GRID_CELLSIZE, &s->minpt, &s->maxpt)))    {
        scene_destroy(s);
        return NULL;
    }

    /* create physics scene */
    if (!BIT_CHECK(eng_get_params()->flags, ENG_FLAG_DISABLEPHX))   {
        /* default gravity */
        struct vec3f gravity;
        uint sec_phys = wld_find_section("physics");
        vec3_setvp(&gravity, wld_get_var(sec_phys, wld_find_var(sec_phys, "gravity-vector"))->fs);
        s->phx_sceneid = phx_create_scene(&gravity, NULL);
        if (s->phx_sceneid == 0)    {
            scene_destroy(s);
            return NULL;
        }
    }

	return s;
}

void scene_destroy(struct scn_data* s)
{
    /* remove all objects */
    struct cmp_obj** objs = (struct cmp_obj**)s->objs.buffer;
    for (uint i = 0, cnt = s->objs.item_cnt; i < cnt; i++)  {
        /* partial object delete */
        scene_destroy_objcmps(objs[i]);
        mem_pool_free(&g_scn_mgr.obj_pool, objs[i]);
    }

    /* physics */
    if (s->phx_sceneid != 0)
        phx_destroy_scene(s->phx_sceneid);

    /* */
    scene_grid_release(&s->grid);
    arr_destroy(&s->spatial_updates);
    arr_destroy(&s->objs);

	FREE(s);
}

struct scn_render_query* scn_create_query(uint scene_id, struct allocator* alloc,
		const struct plane frust_planes[6], const struct gfx_view_params* params, uint flags)
{
    PRF_OPENSAMPLE("visible query");

	struct scn_render_query* rq = (struct scn_render_query*)A_ALLOC(alloc,
        sizeof(struct scn_render_query), MID_SCN);
	if (rq == NULL)
		return NULL;
	memset(rq, 0x00, sizeof(struct scn_render_query));
	rq->alloc = alloc;

    /* get temp-allocator and gather all objects from linked_list */
    result_t r;
    struct array tmp_models; /* item: scn_render_model */
    struct array tmp_lights; /* item: scn_render_light */
    struct array tmp_mats;  /* item: mat3f */
    int* vis = NULL; /* visible flags for each object */
    uint obj_idx = 0;     /* object index (sent to query) */
    uint item_idx = 0;    /* render-item index */
    struct cmp_obj** vis_objs = NULL;  /* non-culled (visible) object list, shrinks in the process*/
    uint vis_cnt;

    struct scn_data* s = scene_get(scene_id);
    if (s->objs.item_cnt == 0)
        return rq;

    /* update spatial partitioning (pull and push objects into the grid) */
    if (!arr_isempty(&s->spatial_updates))  {
        scene_grid_pull(&s->grid, (const cmphandle_t*)s->spatial_updates.buffer, 0,
            s->spatial_updates.item_cnt);
        scene_grid_push(&s->grid, alloc, (const cmphandle_t*)s->spatial_updates.buffer, 0,
            s->spatial_updates.item_cnt);
        arr_clear(&s->spatial_updates);
    }

    /* create an array buffer, holding all scene objects */
    memset(&tmp_models, 0x00, sizeof(tmp_models));
    memset(&tmp_mats, 0x00, sizeof(tmp_mats));

    /* reset visible object cache */
#if 0
    for (uint i = 0, cnt = g_scn_mgr.vis_objs.item_cnt; i < cnt; i++) {
        struct cmp_obj* obj = ((struct cmp_obj**)g_scn_mgr.vis_objs.buffer)[i];
        BIT_REMOVE(obj->flags, CMP_OBJFLAG_VISIBLE);
    }
    arr_clear(&g_scn_mgr.vis_objs);
#endif

    /* gather objects and cull against the spatial structure */
    vis_cnt = s->objs.item_cnt + g_scn_mgr.global_objs.item_cnt;
    vis_objs = (struct cmp_obj**)A_ALLOC(alloc, sizeof(struct cmp_obj*)*vis_cnt, MID_SCN);
    vis_cnt = scene_cullgrid(&s->grid, vis_objs, 0, vis_cnt, frust_planes);

    /* push global objs to visibles */
    struct cmp_obj** global_objs = (struct cmp_obj**)g_scn_mgr.global_objs.buffer;
    for (uint i = 0, cnt = g_scn_mgr.global_objs.item_cnt; i < cnt; i++)
        vis_objs[vis_cnt++] = global_objs[i];

    /* create and get objects bounds */
    rq->bounds = (struct sphere*)A_ALIGNED_ALLOC(alloc, sizeof(struct sphere)*vis_cnt, MID_SCN);
    uint* bidxs = (uint*)A_ALLOC(alloc, sizeof(uint)*vis_cnt, MID_SCN);
    if (rq->bounds == NULL || bidxs == NULL)
        goto err_cleanup;

    for (uint i = 0; i < vis_cnt; i++) {
        struct cmp_obj* obj = vis_objs[i];
        struct cmp_bounds* b = (struct cmp_bounds*)cmp_getinstancedata(obj->bounds_cmp);
        sphere_sets(&rq->bounds[i], &b->ws_s);
        bidxs[i] = i;

        /* reset CMP_OBJFLAG_SPATIALVISIBLE flags */
        BIT_REMOVE(obj->flags, CMP_OBJFLAG_SPATIALVISIBLE);
    }

    /* cull against frustum */
    vis = (int*)A_ALLOC(alloc, sizeof(int)*vis_cnt, MID_SCN);
    if (vis == NULL)
        goto err_cleanup;
    memset(vis, 0x00, sizeof(int)*vis_cnt);
    scene_cullspheres(vis, frust_planes, rq->bounds, 0, vis_cnt);

    /* create output buffers (mats, models, lights, etc. - in form of array) */
    r = arr_create(alloc, &tmp_models, sizeof(struct scn_render_model),
        vis_cnt + (vis_cnt/2), vis_cnt, MID_SCN);
    r |= arr_create(alloc, &tmp_lights, sizeof(struct scn_render_light), 60, 100, MID_SCN);
    if (IS_FAIL(r))
        goto err_cleanup;
    r = arr_create(alloc, &tmp_mats, sizeof(struct mat3f), vis_cnt + (vis_cnt/2), vis_cnt, MID_SCN);
    if (IS_FAIL(r))
        goto err_cleanup;

    /* draw occluder meshes */
    scene_draw_occluders(alloc, vis_objs, vis_cnt, vis, params);
    /* draw potential occludee shapes and test it with occluders */
    vis_cnt = scene_test_occlusion(vis, vis_objs, bidxs, vis_cnt, params);

    for (uint i = 0; i < vis_cnt; i++) {
        struct cmp_obj* obj = vis_objs[i];

#if 0
        /* set visible flag for object and also add to visible object cache */
        BIT_ADD(obj->flags, CMP_OBJFLAG_VISIBLE);
        struct cmp_obj** pvisobj = arr_add(&g_scn_mgr.vis_objs);
        *pvisobj = obj;
#endif

        /* add to proper render-object group */
        switch (obj->type)  {
        case CMP_OBJTYPE_MODEL:
            item_idx += scene_add_model(obj, bidxs[i], item_idx, &tmp_mats, &tmp_models,
                params, &obj_idx);
            break;

        case CMP_OBJTYPE_LIGHT:
            item_idx += scene_add_light(obj, bidxs[i], item_idx, &tmp_mats, &tmp_lights,
                params, &obj_idx);
            break;

        default:
            break;
        }
    }

    /* set remaining query data and cleanup */
    rq->obj_cnt = obj_idx;

    rq->mat_cnt = tmp_mats.item_cnt;
    rq->mats = (struct mat3f*)tmp_mats.buffer;

    rq->models = (struct scn_render_model*)tmp_models.buffer;
    rq->model_cnt = tmp_models.item_cnt;

    rq->lights = (struct scn_render_light*)tmp_lights.buffer;
    rq->light_cnt = tmp_lights.item_cnt;

    A_FREE(alloc, vis);
    A_FREE(alloc, vis_objs);
    A_FREE(alloc, bidxs);

    PRF_CLOSESAMPLE(); /* visible query */

    /* debug grid */
    if (g_scn_mgr.debug_grid)
        scene_grid_debug(&s->grid, params->cam);

    return rq;

err_cleanup:
    arr_destroy(&tmp_mats);
    arr_destroy(&tmp_models);
    if (vis_objs != NULL)
         A_FREE(alloc, vis_objs);
    if (rq != NULL)
        scn_destroy_query(rq);
    if (vis != NULL)
        A_FREE(alloc, vis);

	return NULL;
}

struct scn_render_query* scn_create_query_csm(uint scene_id, struct allocator* alloc,
    const struct aabb* frust_bounds, const struct vec3f* dir_norm,
    const struct gfx_view_params* params)
{
    PRF_OPENSAMPLE("csm query");

    struct scn_render_query* rq = (struct scn_render_query*)A_ALLOC(alloc,
        sizeof(struct scn_render_query), MID_SCN);
    if (rq == NULL)
        return NULL;
    memset(rq, 0x00, sizeof(struct scn_render_query));
    rq->alloc = alloc;

    result_t r;
    struct array tmp_objs; /* item: cmp_obj* */
    struct array tmp_models; /* item: scn_render_model */
    struct array tmp_mats;  /* item: mat3f */
    struct scn_data* s = scene_get(scene_id);
    struct cmp_obj** spatial_culled_objs;
    uint spatial_culled_cnt;
    struct aabb* bounds = NULL;
    int* culls = NULL;
    uint item_idx = 0;
    uint obj_idx = 0;

    /* create an array buffer, holding all scene objects */
    r = arr_create(alloc, &tmp_objs, sizeof(struct cmp_obj*), 100, 500, MID_SCN);
    if (IS_FAIL(r))
        goto err_cleanup;

    /* move through objects and gather objects with model types, used for culling (temp buffer) */
    scene_gather_models_csm(s, &tmp_objs);

    /* spatial cull */
    spatial_culled_cnt = tmp_objs.item_cnt;
    spatial_culled_objs = (struct cmp_obj**)tmp_objs.buffer;
    if (spatial_culled_cnt == 0)    {
        arr_destroy(&tmp_objs);
        return rq;
    }

    bounds = (struct aabb*)A_ALIGNED_ALLOC(alloc, sizeof(struct aabb)*spatial_culled_cnt, MID_SCN);
    if (bounds == NULL)
        goto err_cleanup;

    for (uint i = 0; i < spatial_culled_cnt; i++) {
        struct cmp_obj* obj = spatial_culled_objs[i];
        ASSERT(obj->bounds_cmp != INVALID_HANDLE);
        struct cmp_bounds* b = (struct cmp_bounds*)cmp_getinstancedata(obj->bounds_cmp);
        aabb_setb(&bounds[i], &b->ws_aabb);
    }

    /* create models temp array and cull info array */
    culls = (int*)A_ALLOC(alloc, sizeof(int)*spatial_culled_cnt, MID_SCN);
    if (culls == NULL)
        goto err_cleanup;
    memset(culls, 0x00, sizeof(int)*spatial_culled_cnt);

    r = arr_create(alloc, &tmp_models, sizeof(struct scn_render_model),
        spatial_culled_cnt + (spatial_culled_cnt/2), spatial_culled_cnt, MID_SCN);
    if (IS_FAIL(r))
        goto err_cleanup;
    r = arr_create(alloc, &tmp_mats, sizeof(struct mat3f),
        spatial_culled_cnt + (spatial_culled_cnt/2), spatial_culled_cnt, MID_SCN);
    if (IS_FAIL(r))
        goto err_cleanup;

    /* sweep cull test */
    scene_cull_aabbs_sweep(culls, frust_bounds, dir_norm, bounds, 0, spatial_culled_cnt);

    /* gather */
    for (uint i = 0; i < spatial_culled_cnt; i++) {
        if (culls[i]) {
            struct cmp_obj* obj = spatial_culled_objs[i];
            item_idx += scene_add_model_shadow(obj, i, item_idx, &tmp_mats, &tmp_models, params,
                &obj_idx);
        }  /* endif: not culled */
    }

    /* fill data */
    rq->obj_cnt = obj_idx;
    rq->mat_cnt = tmp_mats.item_cnt;
    rq->mats = (struct mat3f*)tmp_mats.buffer;
    rq->model_cnt = tmp_models.item_cnt;
    rq->models = (struct scn_render_model*)tmp_models.buffer;

    /* */
    A_FREE(alloc, culls);
    A_FREE(alloc, bounds);
    arr_destroy(&tmp_objs);

    PRF_CLOSESAMPLE();
    return rq;

err_cleanup:
    if (culls != NULL)
        A_FREE(alloc, culls);
    if (bounds != NULL)
        A_ALIGNED_FREE(alloc, bounds);
    arr_destroy(&tmp_models);
    arr_destroy(&tmp_objs);
    arr_destroy(&tmp_mats);
    if (rq != NULL)
        scn_destroy_query(rq);
    return NULL;
}

struct scn_render_query* scn_create_query_sphere(uint scene_id, struct allocator* alloc,
    const struct sphere* sphere, const struct gfx_view_params* params)
{
    result_t r;
    struct scn_data* s = scene_get(scene_id);
    uint item_idx = 0;
    uint obj_idx = 0;

    /* create query */
    struct scn_render_query* rq = (struct scn_render_query*)A_ALLOC(alloc,
        sizeof(struct scn_render_query), MID_SCN);
    if (rq == NULL)
        return NULL;
    memset(rq, 0x00, sizeof(struct scn_render_query));
    rq->alloc = alloc;

    /* allocate temp object array */
    uint vis_cnt = s->objs.item_cnt;
    struct cmp_obj** objs = (struct cmp_obj**)A_ALLOC(alloc, sizeof(struct cmp_obj*)*vis_cnt,
        MID_SCN);

    /* cull with grid
     * grid is expected to visibility culled before (scn_create_query), in the current frame */
    uint grid_obj_cnt  = scene_cullgrid_sphere(&s->grid, objs, sphere);

    /* create output buffers (mats, models, lights, etc. - in form of array) */
    struct array tmp_models;
    struct array tmp_mats;
    r = arr_create(alloc, &tmp_models, sizeof(struct scn_render_model),
        grid_obj_cnt + (grid_obj_cnt/2), grid_obj_cnt, MID_SCN);
    if (IS_FAIL(r))
        goto err_cleanup;

    /* intersect with bounds of objects */
    for (uint i = 0; i < grid_obj_cnt; i++)   {
        struct cmp_obj* obj = objs[i];

        /* filter out all objects except models */
        if (obj->type != CMP_OBJTYPE_MODEL)
            continue;

        ASSERT(obj->bounds_cmp != INVALID_HANDLE);
        struct cmp_bounds* b = (struct cmp_bounds*)cmp_getinstancedata(obj->bounds_cmp);

        if (sphere_intersects(sphere, &b->ws_s))    {
            item_idx += scene_add_model_shadow(obj, i, item_idx, &tmp_mats, &tmp_models, params,
                &obj_idx);
        }
    }

    /* fill data */
    rq->obj_cnt = obj_idx;
    rq->mat_cnt = tmp_mats.item_cnt;
    rq->mats = (struct mat3f*)tmp_mats.buffer;
    rq->model_cnt = tmp_models.item_cnt;
    rq->models = (struct scn_render_model*)tmp_models.buffer;

    /* cleanup */
    A_FREE(alloc, objs);

    return rq;

err_cleanup:
    if (objs != NULL)
        A_ALIGNED_FREE(alloc, objs);
    arr_destroy(&tmp_models);
    arr_destroy(&tmp_mats);
    if (rq != NULL)
        scn_destroy_query(rq);
    return NULL;
}


void scn_destroy_query(struct scn_render_query* query)
{
	struct allocator* alloc = query->alloc;

	if (query->mats != NULL)
		A_ALIGNED_FREE(alloc, query->mats);

	if (query->bounds != NULL)
		A_ALIGNED_FREE(alloc, query->bounds);

	if (query->models != NULL)
		A_ALIGNED_FREE(alloc, query->models);

	memset(query, 0x00, sizeof(struct scn_render_query));
	A_FREE(alloc, query);
}

struct cmp_obj* scn_create_obj(uint scene_id, const char* name, enum cmp_obj_type type)
{
	ASSERT(scene_id != 0);

    if (scene_id > g_scn_mgr.scenes.item_cnt || scene_get(scene_id) == NULL)   {
        err_printf(__FILE__, __LINE__, "creating object '%s' failed, scene does not exist", name);
        return NULL;
    }

	struct cmp_obj* obj = (struct cmp_obj*)mem_pool_alloc(&g_scn_mgr.obj_pool);
	if (obj == NULL)
		return NULL;

	cmp_zeroobj(obj);
	str_safecpy(obj->name, sizeof(obj->name), name);
	obj->scene_id = scene_id;
	obj->type = type;

    /* create essential components for the specific object type */
    scene_create_components(obj);

	/* add to scene object bank (or to global list if specified) */
    struct array* objarr = scene_getobjarr(scene_id);
    struct cmp_obj** pobj = (struct cmp_obj**)arr_add(objarr);
    if (pobj == NULL)
        return NULL;
    *pobj = obj;
    obj->id = objarr->item_cnt;

	return obj;
}

void scn_clear(uint scene_id)
{
    struct array* objarr = scene_getobjarr(scene_id);
    struct cmp_obj** objs = (struct cmp_obj**)objarr->buffer;
    for (uint i = 0, cnt = objarr->item_cnt; i < cnt; i++)  {
        /* partial object delete */
        scene_destroy_objcmps(objs[i]);
        mem_pool_free(&g_scn_mgr.obj_pool, objs[i]);
    }
    arr_clear(objarr);

    if (scene_id != SCENE_GLOBAL)   {
        struct scn_data* s = scene_get(scene_id);
        if (s->phx_sceneid != 0)
            phx_scene_flush(s->phx_sceneid);
    }
}

void scn_destroy_obj(struct cmp_obj* obj)
{
    scene_destroy_objcmps(obj);

	/* remove from scene object bank (swap with last one) */
    struct array* objarr = scene_getobjarr(obj->scene_id);
    ASSERT(obj->id > 0 && obj->id <= objarr->item_cnt);
    uint idx = obj->id - 1;
    struct cmp_obj** objs = (struct cmp_obj**)objarr->buffer;
    if (idx < (objarr->item_cnt-1))    {
        struct cmp_obj* last_obj = objs[objarr->item_cnt - 1];
        swapui(&obj->id, &last_obj->id);
        swapptr((void**)&objs[obj->id-1], (void**)&objs[last_obj->id-1]);
    }

    ASSERT(objarr->item_cnt > 0);
    objarr->item_cnt --;

    /* reset object and free it from the pool */
	cmp_zeroobj(obj);
	mem_pool_free(&g_scn_mgr.obj_pool, obj);
}

/* destroy components owned by the object */
void scene_destroy_objcmps(struct cmp_obj* obj)
{
    struct linked_list* cmp_node = obj->chain;
    while (cmp_node != NULL)    {
        struct cmp_chain_node* chnode = (struct cmp_chain_node*)cmp_node->data;
        cmp_node = cmp_node->next;

        ASSERT(chnode->hdl != INVALID_HANDLE);
        cmp_destroy_instance(chnode->hdl);
    }
}


void scene_gather_models_csm(struct scn_data* s, struct array* objs)
{
    struct cmp_obj** scene_objs = (struct cmp_obj**)s->objs.buffer;
    for (uint i = 0, cnt = s->objs.item_cnt; i < cnt; i++)  {
        struct cmp_obj* obj = scene_objs[i];
        if (obj->model_cmp != INVALID_HANDLE) {
            struct cmp_model* m = (struct cmp_model*)cmp_getinstancedata(obj->model_cmp);
            if (!m->exclude_shadows)    {
                struct cmp_obj** pobj = (struct cmp_obj**)arr_add(objs);
                ASSERT(pobj);
                *pobj = obj;
            }
        }
    }

    /* globals */
    struct cmp_obj** global_objs = (struct cmp_obj**)g_scn_mgr.global_objs.buffer;
    for (uint i = 0, cnt = g_scn_mgr.global_objs.item_cnt; i < cnt; i++)  {
        struct cmp_obj* obj = global_objs[i];
        if (obj->model_cmp != INVALID_HANDLE)   {
            struct cmp_model* m = (struct cmp_model*)cmp_getinstancedata(obj->model_cmp);
            if (!m->exclude_shadows)    {
                struct cmp_obj** pobj = (struct cmp_obj**)arr_add(objs);
                ASSERT(pobj);
                *pobj = obj;
            }
        }
    }
}

/* unlike models, for each light we have exactly one light-object for scene-manager */
uint scene_add_light(struct cmp_obj* obj, uint bounds_idx, uint item_idx, struct array* mats,
    struct array* lights, const struct gfx_view_params* params, OUT uint* obj_idx)
{
    float intensity;
    cmphandle_t light_hdl = cmp_findinstance(obj->chain, cmp_light_type);
    int vis = cmp_light_applylod(light_hdl, &params->cam_pos, &intensity);
    if (!vis)
        return 0;

    struct scn_render_light* rlight = (struct scn_render_light*)arr_add(lights);
    struct mat3f* rmat = (struct mat3f*)arr_add(mats);
    if (rlight == NULL || rmat == NULL)
        return 0;

    rlight->light_hdl = light_hdl;
    rlight->mat_idx = item_idx;
    rlight->bounds_idx = bounds_idx;
    rlight->intensity_mul = intensity;

    return 1;
}

/**
 * for each model, we have couple of renderable nodes
 * returns the number of items that are added to 'mats' and 'models' */
uint scene_add_model(struct cmp_obj* obj, uint bounds_idx, uint item_idx, struct array* mats,
    struct array* models, const struct gfx_view_params* params, OUT uint* obj_idx)
{
    int vis = TRUE;
    struct cmp_model* m = (struct cmp_model*)cmp_getinstancedata(obj->model_cmp);

    /* apply LOD if model is owned by LOD component */
    if (BIT_CHECK(m->flags, CMP_MODELFLAG_ISLOD))   {
        vis = cmp_lodmodel_applylod(cmp_findinstance(obj->chain, cmp_lodmodel_type),
            &params->cam_pos);
        if (!vis)
            return 0;
        /* refetch model, because it may be changed by LOD */
        m = (struct cmp_model*)cmp_getinstancedata(obj->model_cmp);
    }

#ifndef _RETAIL_
    if (m->model_hdl == INVALID_HANDLE)
        return 0;
#endif
    struct gfx_model* gmodel = rs_get_model(m->model_hdl);
    if (gmodel == NULL)
        return 0;

    for (uint i = 0, cnt = gmodel->renderable_cnt; i < cnt; i++)  {
        struct scn_render_model* rmodel = (struct scn_render_model*)arr_add(models);
        struct mat3f* rmat = (struct mat3f*)arr_add(mats);
        if (rmodel == NULL || rmat == NULL)
            return 0;

        uint node_idx = gmodel->renderable_idxs[i];

        /* render-model */
        rmodel->model_hdl = obj->model_cmp;
        rmodel->sun_shadows = !m->exclude_shadows;
        rmodel->gmodel = gmodel;
        rmodel->inst = m->model_inst;
        rmodel->mat_idx = item_idx + i;
        rmodel->bounds_idx = bounds_idx;
        rmodel->node_idx = node_idx;

        uint geo_id = gmodel->meshes[gmodel->nodes[node_idx].mesh_id].geo_id;
        rmodel->pose = m->model_inst->poses[geo_id];

        /* world-space transform matrix */
        struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(m->xforms[node_idx]);
        mat3_setm(rmat, &xf->ws_mat);
    }

    (*obj_idx) ++;

    return gmodel->renderable_cnt;
}

uint scene_add_model_shadow(struct cmp_obj* obj, uint bounds_idx, uint item_idx,
    struct array* mats, struct array* models, const struct gfx_view_params* params,
    OUT uint* obj_idx)
{
    int vis = TRUE;
    struct cmp_model* m = (struct cmp_model*)cmp_getinstancedata(obj->model_shadow_cmp);

    /* apply LOD if model is owned by LOD component */
    if (BIT_CHECK(m->flags, CMP_MODELFLAG_ISLOD))   {
        vis = cmp_lodmodel_applylod_shadow(cmp_findinstance(obj->chain, cmp_lodmodel_type),
            &params->cam_pos);
        if (!vis)
            return 0;
        /* refetch model, because it may be changed by LOD */
        m = (struct cmp_model*)cmp_getinstancedata(obj->model_shadow_cmp);
    }

#if !defined(_RETAIL_)
    if (m->model_hdl == INVALID_HANDLE)
        return 0;
#endif

    struct gfx_model* gmodel = rs_get_model(m->model_hdl);
    if (gmodel == NULL)
        return 0;

    for (uint i = 0, cnt = gmodel->renderable_cnt; i < cnt; i++)  {
        struct scn_render_model* rmodel = (struct scn_render_model*)arr_add(models);
        struct mat3f* rmat = (struct mat3f*)arr_add(mats);
        if (rmodel == NULL || rmat == NULL)
            return 0;

        uint node_idx = gmodel->renderable_idxs[i];

        /* render-model */
        rmodel->model_hdl = obj->model_shadow_cmp;
        rmodel->sun_shadows = !m->exclude_shadows;
        rmodel->gmodel = gmodel;
        rmodel->inst = m->model_inst;
        rmodel->mat_idx = item_idx + i;
        rmodel->bounds_idx = bounds_idx;
        rmodel->node_idx = node_idx;

        uint geo_id = gmodel->meshes[gmodel->nodes[node_idx].mesh_id].geo_id;
        rmodel->pose = m->model_inst->poses[geo_id];

        /* world-space transform matrix */
        struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(m->xforms[node_idx]);
        mat3_setm(rmat, &xf->ws_mat);
    }

    (*obj_idx) ++;

    return gmodel->renderable_cnt;
}

uint scn_findobj(uint scene_id, const char* name)
{
    struct array* objarr = scene_getobjarr(scene_id);
    struct cmp_obj** objs = (struct cmp_obj**)objarr->buffer;

    for (uint i = 0, cnt = objarr->item_cnt; i < cnt; i++)  {
        if (str_isequal(objs[i]->name, name))
            return objs[i]->id;
    }

    return 0;
}

struct cmp_obj* scn_getobj(uint scene_id, uint obj_id)
{
    ASSERT(obj_id != 0);

    struct array* objarr = scene_getobjarr(scene_id);
    struct cmp_obj** objs = (struct cmp_obj**)objarr->buffer;
    return objs[obj_id-1];
}

void scene_cull_aabbs_sweep(int* vis, const struct aabb* frust_aabb, const struct vec3f* dir,
    const struct aabb* aabbs, uint startidx, uint endidx)
{
    struct vec3f fmin;
    struct vec3f fmax;
    struct vec3f d;
    struct vec3f tmp;

    vec3_setv(&fmin, &frust_aabb->minpt);
    vec3_setv(&fmax, &frust_aabb->maxpt);
    vec3_setv(&d, dir);

    for (uint i = startidx; i < endidx; i++) {
        /* min/max of each object */
        struct vec3f omin;
        struct vec3f omax;
        struct vec3f fcenter;
        struct vec3f fhalf;
        struct vec3f ocenter;
        struct vec3f ohalf;

        vec3_muls(&fcenter, vec3_add(&tmp, &fmin, &fmax), 0.5f);
        vec3_muls(&fhalf, vec3_sub(&tmp, &fmax, &fmin), 0.5f);
        float fcenter_proj = vec3_dot(&fcenter, &d);

        /* project frustum AABB half-size */
        float fh_proj = fhalf.x*fabs(d.x) + fhalf.y*fabs(d.y) + fhalf.z*fabs(d.z);
        float fp_min = fcenter_proj - fh_proj;
        float fp_max = fcenter_proj + fh_proj;

        /* project object AABB center point */
        vec3_setv(&omin, &aabbs[i].minpt);
        vec3_setv(&omax, &aabbs[i].maxpt);
        vec3_muls(&ocenter, vec3_add(&tmp, &omin, &omax), 0.5f);
        vec3_muls(&ohalf, vec3_sub(&tmp, &omax, &omin), 0.5f);
        float ocenter_proj = vec3_dot(&ocenter, &d);

        /* project object AABB half-size */
        float oh_proj = ohalf.x*fabs(d.x) + ohalf.y*fabs(d.y) + ohalf.z*fabs(d.z);
        float op_min = ocenter_proj - oh_proj;
        float op_max = ocenter_proj + oh_proj;

        /* sweep intersection along dir */
        float dist_min = fp_min - op_max;
        float dist_max = fp_max - op_min;
        if (dist_min > dist_max)
            swapf(&dist_min, &dist_max);

        if (dist_max < 0.0f)
            continue;

        /* test x-axis */
        if (math_iszero(d.x))    {
            if (fmin.x > omax.x || omin.x > fmax.x)
                continue;
        }   else    {
            float dist_min_new = (fmin.x - omax.x)/d.x;
            float dist_max_new = (fmax.x - omin.x)/d.x;
            if (dist_min_new > dist_max_new)
                swapf(&dist_min_new, &dist_max_new);
            if (dist_min > dist_max_new || dist_min_new > dist_max)
                continue;
            dist_min = maxf(dist_min, dist_min_new);
            dist_max = maxf(dist_max, dist_max_new);
        }

        /* test y-axis */
        if (math_iszero(d.y))    {
            if (fmin.y > omax.y || omin.y > fmax.y)
                continue;
        }   else    {
            float dist_min_new = (fmin.y - omax.y)/d.y;
            float dist_max_new = (fmax.y - omin.y)/d.y;
            if (dist_min_new > dist_max_new)
                swapf(&dist_min_new, &dist_max_new);
            if (dist_min > dist_max_new || dist_min_new > dist_max)
                continue;
            dist_min = maxf(dist_min, dist_min_new);
            dist_max = maxf(dist_max, dist_max_new);
        }

        /* test z-axis */
        if (math_iszero(d.z))    {
            if (fmin.z > omax.z || omin.z > fmax.z)
                continue;
        }   else    {
            float dist_min_new = (fmin.z - omax.z)/d.z;
            float dist_max_new = (fmax.z - omin.z)/d.z;
            if (dist_min_new > dist_max_new)
                swapf(&dist_min_new, &dist_max_new);
            if (dist_min > dist_max_new || dist_min_new > dist_max)
                continue;
        }

        /* not culled */
        vis[i] = TRUE;
    }

}

#if defined(_SIMD_SSE_)
void scene_cullspheres(int* vis, const struct plane frust[6], const struct sphere* bounds,
		uint startidx, uint endidx)
{
    PRF_OPENSAMPLE("frustum cull");

    struct vec4f planes_simd[8];

    /* construct SIMD friendly frust planes */
    vec4_setf(&planes_simd[0], frust[0].nx, frust[1].nx, frust[2].nx, frust[3].nx);
    vec4_setf(&planes_simd[1], frust[0].ny, frust[1].ny, frust[2].ny, frust[3].ny);
    vec4_setf(&planes_simd[2], frust[0].nz, frust[1].nz, frust[2].nz, frust[3].nz);
    vec4_setf(&planes_simd[3], frust[0].d, frust[1].d, frust[2].d, frust[3].d);
    vec4_setf(&planes_simd[4], frust[4].nx, frust[5].nx, frust[4].nx, frust[5].nx);
    vec4_setf(&planes_simd[5], frust[4].ny, frust[5].ny, frust[4].ny, frust[5].ny);
    vec4_setf(&planes_simd[6], frust[4].nz, frust[5].nz, frust[4].nz, frust[5].nz);
    vec4_setf(&planes_simd[7], frust[4].d, frust[5].d, frust[4].d, frust[5].d);

    /* intersect: process one sphere in each loop and set 'culls' boolean */
    simd_t _neg = _mm_set1_ps(-1.0f);
    for (uint i = startidx; i < endidx; i++)  {
        simd_t _v;
        simd_t _r;
        uint mask;
        simd_t _s = _mm_load_ps(bounds[i].f);

        simd_t _xxxx = _mm_all_x(_s);
        simd_t _yyyy = _mm_all_y(_s);
        simd_t _zzzz = _mm_all_z(_s);
        simd_t _rrrr = _mm_mul_ps(_mm_all_w(_s), _neg);	/* negate Rs: _rrrr = -_rrrr */

        /* 4 dot products + D */
        _v = _mm_mul_ps(_xxxx, _mm_load_ps(planes_simd[0].f) );
        _v = _mm_madd(_yyyy, _mm_load_ps(planes_simd[1].f), _v);
        _v = _mm_madd(_zzzz, _mm_load_ps(planes_simd[2].f), _v);
        _v = _mm_add_ps(_v, _mm_load_ps(planes_simd[3].f));

        /* if sphere is outside of any plane tested, one of _r values will be 0xffffffff */
        _r = _mm_cmplt_ps(_v, _rrrr);

        /* final frust planes dot products + D */
        _v = _mm_mul_ps(_xxxx, _mm_load_ps(planes_simd[4].f));
        _v = _mm_madd(_yyyy, _mm_load_ps(planes_simd[5].f), _v);
        _v = _mm_madd(_zzzz, _mm_load_ps(planes_simd[6].f), _v);
        _v = _mm_add_ps(_v, _mm_load_ps(planes_simd[7].f));

        /* if sphere is outside of any plane tested, one of _r values will be 0xffffffff */
        _r = _mm_or_ps(_r, _mm_cmplt_ps(_v, _rrrr));

        /* combine results
         * shuffle and OR until to the lower-byte element (x)
         * convert and extract the final value
         * repeat z,w value from _r and OR it with previous _r */
        _r = _mm_or_ps(_r, _mm_movehl_ps(_r, _r));
        _r = _mm_or_ps(_r, _mm_all_y(_r));

        _mm_store_ss((float*)&mask, _r);
        vis[i] |= (~mask) & 0x1;
    }

    PRF_CLOSESAMPLE(); /* frustum cull */
}
#else
#error "not implemented"
#endif

void scene_cullspheres_nosimd(int* vis, const struct plane frust[6], const struct sphere* bounds,
		uint startidx, uint endidx)
{
    PRF_OPENSAMPLE("frustum cull");

    struct vec3f pn;
    struct vec3f center;

    for (uint k = startidx; k < endidx; k++)	{
        const struct sphere* sphere = &bounds[k];
        vec3_setf(&center, sphere->x, sphere->y, sphere->z);

        for (uint i = 0; i < 6; i++)		{
            const struct plane* p = &frust[i];
            vec3_setf(&pn, p->nx, p->ny, p->nz);

            /* calculate distance from sphere center point to frustum's plane */
            float d = vec3_dot(&pn, &center) + p->d;

            /* if d is negative and less than sphere's radius
             * it means that sphere is completely outside of frustum's plane,
             * so it's outside of frustum too */
            if (d < -(sphere->r + EPSILON))		{
                goto skip_sphere;
            }
        } /* foreach frustum plane */

        vis[k] |= 0x1;
skip_sphere:
        ;
    } /* foreach sphere */

    PRF_CLOSESAMPLE(); /* frustum cull */
}

/* draw occluders only within occ_far units */
void scene_draw_occluders(struct allocator* alloc, struct cmp_obj** objs, uint obj_cnt,
    const int* vis, const struct gfx_view_params* params)
{
    struct vec3f campos;

    PRF_OPENSAMPLE("occ-draw");

    float ffar = gfx_occ_getfar();
    vec3_setv(&campos, &params->cam_pos);

    /* recreate cam matrix to change camera range */
    struct mat4f proj;
    struct mat4f viewproj;
    cam_calc_perspective(&proj, params->cam->fov, 1.0f, params->cam->fnear, params->cam->ffar);
    mat3_mul4(&viewproj, &params->view, &proj);

    /* ready the zbuffer */
    gfx_occ_clear();
    gfx_occ_setmatrices(&viewproj);

    /* draw objects with occluders */
    for (uint i = 0; i < obj_cnt; i++)    {
        if (vis[i] && objs[i]->type == CMP_OBJTYPE_MODEL) {
            struct cmp_model* m = (struct cmp_model*)cmp_getinstancedata(objs[i]->model_cmp);

#if !defined(_RETAIL_)
            if (m->model_hdl == INVALID_HANDLE)
                continue;
#endif
            struct gfx_model* gm = rs_get_model(m->model_hdl);
            if (gm != NULL && gm->occ != NULL)    {
                struct cmp_xform* xf = (struct cmp_xform*)cmp_getinstancedata(objs[i]->xform_cmp);
                struct cmp_bounds* bb = (struct cmp_bounds*)cmp_getinstancedata(objs[i]->bounds_cmp);

                struct vec3f d;
                vec3_setf(&d, bb->ws_s.x, bb->ws_s.y, bb->ws_s.z);
                vec3_sub(&d, &d, &campos);
                float l = ffar + bb->ws_s.r;
                if (vec3_dot(&d, &d) < l*l)
                    gfx_occ_drawoccluder(alloc, gm->occ, &xf->ws_mat);
            }
        }   /* foreach unculled object */
    }

    PRF_CLOSESAMPLE(); /* occ-draw */
}


/**
 * @param objs (in/out) inputs objects, outputs shrinked (likely) array of visible objects
 * @return visible object count
 */
int scene_test_occlusion(const int* vis, INOUT struct cmp_obj** objs, uint* bound_idxs,
    uint obj_cnt, const struct gfx_view_params* params)
{
    PRF_OPENSAMPLE("occ-test");

    struct vec3f xaxis;
    struct vec3f yaxis;
    struct vec3f campos;
    int cnt = 0;

    /* calculate inverse-view matrix from view */
    struct mat3f view_inv;
    mat3_setf(&view_inv,
        params->view.m11, params->view.m21, params->view.m31,
        params->view.m12, params->view.m22, params->view.m32,
        params->view.m13, params->view.m23, params->view.m33,
        params->cam_pos.x, params->cam_pos.y, params->cam_pos.z);

    mat3_get_xaxis(&xaxis, &view_inv);
    mat3_get_yaxis(&yaxis, &view_inv);
    mat3_get_trans(&campos, &view_inv);

    /* draw object quads */
    for (uint i = 0; i < obj_cnt; i++)    {
        if (vis[i] && objs[i]->bounds_cmp != INVALID_HANDLE) {
            struct cmp_bounds* b = (struct cmp_bounds*)cmp_getinstancedata(objs[i]->bounds_cmp);
            struct sphere s;
            struct vec4f d;

            sphere_sets(&s, &b->ws_s);
            vec3_setf(&d, campos.x - s.x, campos.y - s.y, campos.z - s.z);
            float dot_d = vec3_dot(&d, &d);

            /* near objects: add to visible objects */
            float l = SCN_OCC_NEAR_THRESHOLD + s.r;
            if (dot_d < l*l) {
                cnt = scene_test_occ_addobj(objs, bound_idxs, i, cnt);
                continue;
            }

            if (gfx_occ_testbounds(&s, &xaxis, &yaxis, &campos))
                cnt = scene_test_occ_addobj(objs, bound_idxs, i, cnt);
        }   /* foreach unculled object */
    }

    PRF_CLOSESAMPLE();
    return cnt;
}

result_t scene_grid_init(struct scn_grid* grid, float cell_size, const struct vec3f* world_min,
    const struct vec3f* world_max)
{
    result_t r;
    r = mem_pool_create(mem_heap(), &grid->item_pool, sizeof(struct scn_grid_item),
        SCN_GRID_BLOCKSIZE, MID_SCN);
    if (IS_FAIL(r))
        return RET_OUTOFMEMORY;

    if (IS_FAIL(scene_grid_createcells(grid, cell_size, world_min, world_max)))
        return RET_FAIL;

    return RET_OK;
}

void scene_grid_release(struct scn_grid* grid)
{
    scene_grid_destroycells(grid);
    mem_pool_destroy(&grid->item_pool);
    memset(grid, 0x00, sizeof(struct scn_grid));
}

result_t scene_grid_resize(uint scene_id, const struct vec3f* world_min,
    const struct vec3f* world_max, float cell_size)
{
    struct scn_data* s = scene_get(scene_id);
    struct scn_grid* grid = &s->grid;

    scene_grid_destroycells(grid);
    result_t r = scene_grid_createcells(grid, cell_size, world_min, world_max);
    if (IS_OK(r))   {
        /* re-push objects into grid */
        cmphandle_t* bounds = (cmphandle_t*)ALLOC(sizeof(cmphandle_t)*s->objs.item_cnt, MID_SCN);
        uint bcnt = 0;
        struct cmp_obj** objs = (struct cmp_obj**)s->objs.buffer;
        for (uint i = 0, cnt = s->objs.item_cnt; i < cnt; i++)  {
            if (objs[i]->bounds_cmp != INVALID_HANDLE)
                bounds[bcnt++] = objs[i]->bounds_cmp;
        }

        scene_grid_push(&s->grid, mem_heap(), bounds, 0, bcnt);
        FREE(bounds);
    }

    return r;
}

result_t scene_grid_createcells(struct scn_grid* grid, float cell_size, const struct vec3f* minpt,
    const struct vec3f* maxpt)
{
    float world_width = maxpt->x - minpt->x;
    float world_depth = maxpt->z - minpt->z;

    int col_cnt = (int)floorf(world_width/cell_size);
    int row_cnt = (int)floorf(world_depth/cell_size);
    float last_w = fmodf(world_width, cell_size);
    float last_d = fmodf(world_depth, cell_size);
    if (last_w > EPSILON)
        col_cnt ++;
    if (last_d > EPSILON)
        row_cnt ++;
    int cell_cnt = col_cnt * row_cnt;

    /* create cell data (count = 2*cell_cnt) */
    struct vec4f* cells = (struct vec4f*)ALIGNED_ALLOC(sizeof(struct vec4f)*cell_cnt*2, MID_SCN);
    if (cells == NULL)
        return RET_OUTOFMEMORY;

    /* start from minimum point and move to maximum point */
    struct vec3f pt;
    vec3_setf(&pt, minpt->x, 0.0f, minpt->z);
    for (int i = 0; i < row_cnt; i++)    {
        float d = (i != row_cnt-1 || last_d < EPSILON) ? cell_size : last_d;
        for (int k = 0; k < col_cnt; k++)    {
            int idx = k + i*col_cnt;
            float w = (k != col_cnt-1 || last_w < EPSILON) ? cell_size : last_w;

            struct vec4f* r1 = &cells[idx*2];
            struct vec4f* r2 = &cells[idx*2 + 1];

            vec4_setf(r1, pt.x, pt.z, pt.x, pt.z); /* minpt */
            vec4_setf(r2, pt.x + w, pt.z + d, pt.x + w, pt.z + d); /* maxpt */

            pt.x += cell_size;
        }

        pt.x = minpt->x;
        pt.z += cell_size;
    }

    /* cell items */
    grid->items = (struct linked_list**)ALLOC(sizeof(struct linked_list*)*cell_cnt, MID_SCN);
    if (grid->items == NULL)    {
        ALIGNED_FREE(cells);
        return RET_OUTOFMEMORY;
    }
    memset(grid->items, 0x00, sizeof(struct linked_list*)*cell_cnt);

    /* visible cells (for debugging) */
    grid->vis_cells = (int*)ALLOC(sizeof(int)*cell_cnt, MID_SCN);
    if (grid->vis_cells == NULL)    {
        ALIGNED_FREE(cells);
        FREE(grid->items);
        return RET_OUTOFMEMORY;
    }
    memset(grid->vis_cells, 0x00, sizeof(int)*cell_cnt);

    /* */
    grid->cells = cells;
    grid->cell_cnt = cell_cnt;
    grid->row_cnt = row_cnt;
    grid->col_cnt = col_cnt;
    grid->cell_size = cell_size;

    return RET_OK;
}


void scene_grid_destroycells(struct scn_grid* grid)
{
    scene_grid_clear(grid);

    if (grid->cells != NULL)    {
        ALIGNED_FREE(grid->cells);
        grid->cells = NULL;
    }

    if (grid->items != NULL)    {
        FREE(grid->items);
        grid->items = NULL;
    }

    if (grid->vis_cells != NULL)    {
        FREE(grid->vis_cells);
        grid->vis_cells = NULL;
    }

    grid->cell_cnt = 0;
    grid->row_cnt = 0;
    grid->col_cnt = 0;
}

void scene_grid_push(struct scn_grid* grid, struct allocator* alloc, const cmphandle_t* obj_bounds,
    uint start_idx, uint end_idx)
{
    /* data :
     * two vec4f hold two rectangles on x-z plane
     * 1) x1, y1, x2, y2 (min)
     * 2) x1, y1, x2, y2 (max)
     */
    uint init_cnt = end_idx - start_idx;
    uint cnt = init_cnt;
    if (cnt % 2 != 0)
        cnt ++;

    /* make 2d bounding rectangles on x-z plane for each object */
    struct vec4f* data = (struct vec4f*)A_ALIGNED_ALLOC(alloc, sizeof(struct vec4f)*cnt, MID_SCN);
    for (uint i = start_idx; i < end_idx; i+=2)    {
        struct vec4f minpt1;
        struct vec4f maxpt1;
        struct vec4f minpt2;
        struct vec4f maxpt2;
        struct vec3f p;
        struct vec3f tmp;

        /* first bound */
        struct cmp_bounds* b1 = (struct cmp_bounds*)cmp_getinstancedata(obj_bounds[i]);
        vec3_setf(&p, b1->ws_s.x, b1->ws_s.y, b1->ws_s.z);
        float r1 = b1->ws_s.r;
        vec3_setf(&tmp, r1, 0.0f, r1);
        vec3_sub(&minpt1, &p, &tmp);
        vec3_add(&maxpt1, &p, &tmp);

        /* second bound (wrap from start if we reach the end, data doesn't matter) */
        struct cmp_bounds* b2 = (struct cmp_bounds*)cmp_getinstancedata(obj_bounds[(i + 1)%end_idx]);
        vec3_setf(&p, b2->ws_s.x, b2->ws_s.y, b2->ws_s.z);
        float r2 = b2->ws_s.r;
        vec3_setf(&tmp, r2, 0.0f, r2);
        vec3_sub(&minpt2, &p, &tmp);
        vec3_add(&maxpt2, &p, &tmp);

        vec4_setf(&data[i-start_idx], minpt1.x, minpt1.z, minpt2.x, minpt2.z);
        vec4_setf(&data[i-start_idx+1], maxpt1.x, maxpt1.z, maxpt2.x, maxpt2.z);
    }

    /* cull flags - culls[i] != 0 --> object not in cell (it's culled) */
    int* culls = (int*)A_ALIGNED_ALLOC(alloc, sizeof(int)*cnt, MID_SCN);
    ASSERT(culls);

    struct vec4f* cells = grid->cells;
    for (uint c = 0, cell_cnt = grid->cell_cnt; c < cell_cnt; c++)    {
        simd_t cmin = _mm_load_ps(cells[2*c].f);
        simd_t cmax = _mm_load_ps(cells[2*c + 1].f);

        /* process two objects at once
         * see scene_grid_pushsingle for non-simd implementation */
        for (uint i = 0; i < cnt; i+=2)    {
            simd_t vmin = _mm_load_ps(data[i].f);
            simd_t vmax = _mm_load_ps(data[i+1].f);
            simd_t r1 = _mm_cmpgt_ps(cmin, vmax); /* cell-min > obj-max ? */
            simd_t r2 = _mm_cmplt_ps(cmax, vmin); /* cell-max < obj-min ? */
            simd_t r = _mm_or_ps(r1, r2);
            int mask = _mm_movemask_ps(r);

            culls[i] = mask & 0x3;
            culls[i+1] = mask & 0xC;
        }

        /* save results */
        for (uint i = start_idx; i < end_idx; i++)    {
            if (culls[i-start_idx] == 0)
                scene_grid_addtocell(grid, c, obj_bounds[i]);
        }
    }

    A_ALIGNED_FREE(alloc, data);
    A_ALIGNED_FREE(alloc, culls);
}

void scene_grid_pull(struct scn_grid* grid, const cmphandle_t* obj_bounds, uint start_idx,
    uint end_idx)
{
    for (uint i = start_idx; i < end_idx; i++)    {
        struct cmp_bounds* b = (struct cmp_bounds*)cmp_getinstancedata(obj_bounds[i]);
        struct linked_list* node = b->cell_list;
        while (node != NULL)    {
            struct scn_grid_item* item = (struct scn_grid_item*)node->data;
            struct linked_list* next = node->next;
            scene_grid_removefromcell(grid, item->cell_id, item);
            node = next;
        }
        b->cell_list = NULL;
    }
}

void scn_push_spatial(uint scene_id, cmphandle_t bounds_hdl)
{
    if (scene_id == SCENE_GLOBAL)
        return;

    struct scn_data* s = scene_get(scene_id);
    scene_grid_pushsingle(&s->grid, bounds_hdl);
}

void scn_pull_spatial(uint scene_id, cmphandle_t bounds_hdl)
{
    if (scene_id == SCENE_GLOBAL)
        return;

    struct scn_data* s = scene_get(scene_id);
    scene_grid_pullsingle(&s->grid, bounds_hdl);
}

void scene_grid_pushsingle(struct scn_grid* grid, cmphandle_t bounds_hdl)
{
    struct vec4f minpt;
    struct vec4f maxpt;
    struct vec3f p;
    struct vec3f tmp;
    const struct vec4f* cells = grid->cells;

    /* first bound */
    struct cmp_bounds* b = (struct cmp_bounds*)cmp_getinstancedata(bounds_hdl);
    vec3_setf(&p, b->ws_s.x, b->ws_s.y, b->ws_s.z);
    float r = b->ws_s.r;
    vec3_setf(&tmp, r, 0.0f, r);
    vec3_sub(&minpt, &p, &tmp);
    vec3_add(&maxpt, &p, &tmp);

    for (uint i = 0, cnt = grid->cell_cnt; i < cnt; i++)  {
        const struct vec4f* cmin = &cells[2*i]; /* = x_min, z_min, x_min, z_min */
        const struct vec4f* cmax = &cells[2*i + 1]; /* = x_max, z_max, x_max, z_max */

        int x_out = (minpt.x > cmax->x) | (maxpt.x < cmin->x);
        int z_out = (minpt.z > cmax->y) | (maxpt.z < cmin->y);

        if (x_out | z_out)
            continue;

        scene_grid_addtocell(grid, i, bounds_hdl);
    }
}

void scene_grid_pullsingle(struct scn_grid* grid, cmphandle_t bounds_hdl)
{
    struct cmp_bounds* b = (struct cmp_bounds*)cmp_getinstancedata(bounds_hdl);
    struct linked_list* node = b->cell_list;
    while (node != NULL)    {
        struct scn_grid_item* item = (struct scn_grid_item*)node->data;
        struct linked_list* next = node->next;
        scene_grid_removefromcell(grid, item->cell_id, item);
        node = next;
    }
    b->cell_list = NULL;
}

void scene_grid_clear(struct scn_grid* grid)
{
    for (uint i = 0; i < grid->cell_cnt; i++) {
        struct linked_list* node = grid->items[i];
        while (node != NULL)    {
            struct scn_grid_item* item = (struct scn_grid_item*)node->data;
            struct cmp_bounds* b = (struct cmp_bounds*)cmp_getinstancedata(item->obj->bounds_cmp);
            b->cell_list = NULL;
            node = node->next;
        }
        grid->items[i] = NULL;
    }

    mem_pool_clear(&grid->item_pool);
}

void scene_grid_addtocell(struct scn_grid* grid, uint cell_id, cmphandle_t bounds_hdl)
{
    struct scn_grid_item* item = (struct scn_grid_item*)mem_pool_alloc(&grid->item_pool);
    ASSERT(item != NULL);
    memset(item, 0x00, sizeof(struct scn_grid_item));

    struct cmp_bounds* b = (struct cmp_bounds*)cmp_getinstancedata(bounds_hdl);
    item->obj = cmp_getinstancehost(bounds_hdl);
    item->cell_id = cell_id;
    ASSERT(item->obj);

    list_add(&grid->items[cell_id], &item->cell_node, item); /* add to cell linked-list */
    list_add(&b->cell_list, &item->bounds_node, item);    /* add to bounds (obj) linked-list */
}

void scene_grid_removefromcell(struct scn_grid* grid, uint cell_id, struct scn_grid_item* item)
{
    struct cmp_bounds* b = (struct cmp_bounds*)cmp_getinstancedata(item->obj->bounds_cmp);
    list_remove(&grid->items[cell_id], &item->cell_node);
    list_remove(&b->cell_list, &item->bounds_node);
    mem_pool_free(&grid->item_pool, item);
}

void scn_update_spatial(uint scene_id, cmphandle_t bounds_hdl)
{
    if (scene_id == SCENE_GLOBAL)
        return;

    struct scn_data* s = scene_get(scene_id);
    cmphandle_t* pb_hdl = (cmphandle_t*)arr_add(&s->spatial_updates);
    ASSERT(pb_hdl);
    *pb_hdl = bounds_hdl;
}

/**
 * @param objs (in/out) inputs objects that needs to be tested, outputs shrinked visible array
 * @return number of unculled objects
 */
uint scene_cullgrid(const struct scn_grid* grid, OUT struct cmp_obj** objs, uint start_idx,
    uint end_idx, const struct plane frust[6])
{
    union f_t
    {
        float f;
        int i;
    };

    struct plane frust2d[4];
    scene_calc_frustum_projxz(frust2d, frust);

    uint c = 0;
    const struct vec4f* cells = grid->cells;
    for (uint i = 0, cnt = grid->cell_cnt; i < cnt; i++)  {
        const struct vec4f* vmin = &cells[2*i]; /* = x_min, z_min, x_min, z_min */
        const struct vec4f* vmax = &cells[2*i + 1]; /* = x_max, z_max, x_max, z_max */
        union f_t d1, d2, d3, d4;
        const struct linked_list* node;

        grid->vis_cells[i] = FALSE;

        /* test against four side planes of each cell */
        for (uint k = 0; k < 4; k++)  {
            d1.f = frust2d[k].nx*vmin->x + frust2d[k].nz*vmin->y + frust2d[k].d;
            d2.f = frust2d[k].nx*vmin->x + frust2d[k].nz*vmax->y + frust2d[k].d;
            d3.f = frust2d[k].nx*vmax->x + frust2d[k].nz*vmin->y + frust2d[k].d;
            d4.f = frust2d[k].nx*vmax->x + frust2d[k].nz*vmax->y + frust2d[k].d;

            /* if all points of the cell is outside of frustum, then it is definitely outside */
            if (SIGNBIT(d1) & SIGNBIT(d2) & SIGNBIT(d3) & SIGNBIT(d4))
                goto skip_cell;
        }

        grid->vis_cells[i] = TRUE;

        /* intersect/inside: add to unculled objects, set spatial flag for object */
        node = grid->items[i];
        while (node != NULL)    {
            struct scn_grid_item* item = (struct scn_grid_item*)node->data;
            if (!BIT_CHECK(item->obj->flags, CMP_OBJFLAG_SPATIALVISIBLE))   {
                objs[c++] = item->obj;
                BIT_ADD(item->obj->flags, CMP_OBJFLAG_SPATIALVISIBLE);
            }
            node = node->next;
        }

skip_cell:
        ;
    }

    return c;
}

/* cull with visible cells only */
uint scene_cullgrid_sphere(const struct scn_grid* grid, OUT struct cmp_obj** objs,
    const struct sphere* sphere)
{
    /* project spheres into XZ plane */
    simd_t smin = _mm_set_ps(sphere->z - sphere->r, sphere->x - sphere->r,
        sphere->z - sphere->r, sphere->x - sphere->r);
    simd_t smax = _mm_set_ps(sphere->z + sphere->r, sphere->x + sphere->r,
        sphere->z + sphere->r, sphere->x + sphere->r);

    /* check with cells and extract unculled cell objects */
    uint obj_cnt = 0;
    struct vec4f* cells = grid->cells;
    for (uint c = 0, cell_cnt = grid->cell_cnt; c < cell_cnt; c++)    {
        if (!grid->vis_cells[c])
            continue;

        simd_t cmin = _mm_load_ps(cells[2*c].f);
        simd_t cmax = _mm_load_ps(cells[2*c + 1].f);

        simd_t r1 = _mm_cmpgt_ps(cmin, smax); /* cell-min > obj-max ? */
        simd_t r2 = _mm_cmplt_ps(cmax, smin); /* cell-max < obj-min ? */
        simd_t r = _mm_or_ps(r1, r2);
        int mask = _mm_movemask_ps(r);

        if ((mask & 0x3) == 0)  {
            struct linked_list* node = grid->items[c];
            while (node != NULL)    {
                struct scn_grid_item* item = (struct scn_grid_item*)node->data;

                /* search in existing objects for duplicates */
                for (uint i = 0; i < obj_cnt; i++)    {
                    if (item->obj == objs[i])
                        goto skip_obj;
                }
                objs[obj_cnt++] = item->obj;
skip_obj:
                node = node->next;
            }
        }
    }

    return obj_cnt;
}

void scene_calc_frustum_projxz(struct plane frust_proj[4], const struct plane frust_planes[6])
{
    const struct plane* p;

    p = &frust_planes[CAM_FRUSTUM_RIGHT];
    plane_setf(&frust_proj[0], p->nx, 0.0f, p->nz, p->d);

    p = &frust_planes[CAM_FRUSTUM_LEFT];
    plane_setf(&frust_proj[1], p->nx, 0.0f, p->nz, p->d);

    p = &frust_planes[CAM_FRUSTUM_NEAR];
    plane_setf(&frust_proj[2], p->nx, 0.0f, p->nz, p->d);

    p = &frust_planes[CAM_FRUSTUM_FAR];
    plane_setf(&frust_proj[3], p->nx, 0.0f, p->nz, p->d);
}

result_t scene_console_debuggrid(uint argc, const char** argv, void* param)
{
    int show = TRUE;
    if (argc == 1)
        show = str_tobool(argv[0]);
    else if (argc > 1)
        return RET_INVALIDARG;

    g_scn_mgr.debug_grid = show;
    return RET_OK;
}

void scene_grid_debug(struct scn_grid* grid, const struct camera* cam)
{
    uint screen_width = gfx_get_params()->width;
    uint screen_height = gfx_get_params()->height;

    float aspect = (float)grid->col_cnt / (float)grid->row_cnt;
    uint width = maxui(screen_width/3, 128);
    uint height = (uint)ceilf((float)width/aspect);

    struct rect2di rc;
    rect2di_seti(&rc, screen_width-width-5, screen_height-height-5, width, height);
    gfx_canvas_setalpha(0.7f);
    gfx_canvas_setfillcolor_solid(&g_color_grey);
    gfx_canvas_rect2d(&rc, 0, 0);

    /* draw grid */
    struct rect2di grc;
    struct rect2df src_coord, res_coord;
    uint cell_cnt = grid->cell_cnt;
    struct color tmp_color;
    color_setf(&tmp_color, 0.7f, 0.7f, 0.7f, 1.0f);

    rect2di_shrink(&grc, &rc, 3);
    rect2df_setf(&src_coord, grid->cells[0].x, grid->cells[0].y,
        grid->cells[2*cell_cnt-1].x - grid->cells[0].x,
        grid->cells[2*cell_cnt-1].y - grid->cells[0].y);
    rect2df_setf(&res_coord, (float)grc.x, (float)grc.y, (float)grc.w, (float)grc.h);
    gfx_canvas_setlinecolor(&tmp_color);
    uint cnt_x = grid->col_cnt;
    uint cnt_y = grid->row_cnt;
    struct rect2di rc1;
    for (uint y = 0; y < cnt_y; y++)    {
        uint idx = y * cnt_x;
        struct vec4f* minpt = &grid->cells[idx*2];
        struct vec4f* maxpt = &grid->cells[(idx + cnt_x - 1)*2 + 1];
        scene_conv_coord(&rc1, minpt->x, minpt->y, maxpt->x, maxpt->y, &src_coord, &res_coord);
        if (y == 0)
            gfx_canvas_line2d(rc1.x, rc1.y + rc1.h, rc1.x + rc1.w, rc1.y + rc1.h, 1);
        gfx_canvas_line2d(rc1.x, rc1.y, rc1.x + rc1.w, rc1.y, 1);
    }

    for (uint x = 0; x < cnt_x; x++)  {
        struct vec4f* minpt = &grid->cells[x*2];
        struct vec4f* maxpt = &grid->cells[(x + (cnt_y-1)*cnt_x)*2 + 1];
        scene_conv_coord(&rc1, minpt->x, minpt->y, maxpt->x, maxpt->y, &src_coord, &res_coord);
        gfx_canvas_line2d(rc1.x, rc1.y, rc1.x, rc1.y + rc1.h, 1);
    }
    gfx_canvas_line2d(rc1.x + rc1.w, rc1.y, rc1.x + rc1.w, rc1.y + rc1.h, 1);

    /* fill grid cells with object density color */
    gfx_canvas_setlinecolor(&g_color_yellow);
    for (uint i = 0; i < cell_cnt; i++)   {
        struct color c;
        struct vec4f* minpt = &grid->cells[i*2];
        struct vec4f* maxpt = &grid->cells[i*2 + 1];
        scene_conv_coord(&rc1, minpt->x, minpt->y, maxpt->x, maxpt->y, &src_coord, &res_coord);

        rect2di_shrink(&rc1, &rc1, 1);
        scene_get_densitycolor(&c, grid->items[i], grid);
        if (math_iszero(c.a))
            continue;
        gfx_canvas_setfillcolor_solid(&c);
        gfx_canvas_rect2d(&rc1, 0, 0);

        if (grid->vis_cells[i]) {
            rect2di_grow(&rc1, &rc1, 1);
            gfx_canvas_rect2d(&rc1, 1, GFX_RECT2D_HOLLOW);
        }
    }

    /* draw camera (position/direction) */
    struct vec2i arrow_pos;
    struct vec2i arrow_target;

    scene_conv_coordpt(&arrow_pos, cam->pos.x, cam->pos.z, &src_coord, &res_coord);
    /* early exit ? */
    if (!rect2di_testpt(&rc, &arrow_pos))
        return;

    gfx_canvas_setfillcolor_solid(&g_color_white);
    gfx_canvas_rect2d(rect2di_seti(&rc1, arrow_pos.x - 4, arrow_pos.y - 4, 8, 8), 0, 0);
    scene_conv_coordpt(&arrow_target, cam->pos.x + cam->look.x*grid->cell_size*0.6f,
        cam->pos.z + cam->look.z*grid->cell_size*0.6f, &src_coord, &res_coord);
    gfx_canvas_arrow2d(&arrow_pos, &arrow_target, FALSE, 1, 3.0f);

    gfx_canvas_setalpha(1.0f);
}

struct rect2di* scene_conv_coord(struct rect2di* rc, float x_min, float y_min,
    float x_max, float y_max, const struct rect2df* src_coord, const struct rect2df* res_coord)
{
    /* res = ( src - src_min ) / ( src_max - src_min ) * ( res_max - res_min ) + res_min */
    float target_xmin = ((x_min - src_coord->x)/src_coord->w)*res_coord->w + res_coord->x;
    float target_xmax = ((x_max - src_coord->x)/src_coord->w)*res_coord->w + res_coord->x;
    float target_ymin = (1.0f - (y_min - src_coord->y)/src_coord->h)*res_coord->h + res_coord->y;
    float target_ymax = (1.0f - (y_max - src_coord->y)/src_coord->h)*res_coord->h + res_coord->y;
    /* note: because target coord system is Y upside down, ymin and ymax must be swaped */
    return rect2di_seti(rc, (int)target_xmin, (int)target_ymax,
        (int)ceilf(target_xmax - target_xmin), (int)ceilf(target_ymin - target_ymax));
}

struct vec2i* scene_conv_coordpt(struct vec2i* pt, float x, float y,
    const struct rect2df* src_coord, const struct rect2df* res_coord)
{
    /* res = ( src - src_min ) / ( src_max - src_min ) * ( res_max - res_min ) + res_min */
    float target_x = ((x - src_coord->x)/src_coord->w)*res_coord->w + res_coord->x;
    float target_y = (1.0f - (y - src_coord->y)/src_coord->h)*res_coord->h + res_coord->y;
    return vec2i_seti(pt, (int)target_x, (int)target_y);
}

struct color* scene_get_densitycolor(struct color* c, const struct linked_list* cell,
    const struct scn_grid* grid)
{
#ifdef _GNUC_
    static const struct color cs[] = {
        {.r=0.0f, .g=0.0f, .b=1.0f, .a=1.0f},
        {.r=0.0f, .g=1.0f, .b=0.0f, .a=1.0f},
        {.r=1.0f, .g=0.0f, .b=0.0f, .a=1.0f}
    };
#else
    static const struct color cs[] = {
        {0.0f, 0.0f, 1.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 1.0f}
    };
#endif


    /* count items in the linked-list */
    float cnt = 0;
    const struct linked_list* node = cell;
    while (node != NULL)    {
        cnt++;
        node = node->next;
    }

    float cmax = grid->cell_size*3.0f;

    /* resolve density color */
    int d = mini((int)(cnt*765.0f/cmax), 765);
    return color_muls(c, &cs[d/255], ((float)(d % 256))/255.0f);
}

void scn_setsize(uint scene_id, const struct vec3f* minpt, const struct vec3f* maxpt)
{
    ASSERT(minpt->x < maxpt->x);
    ASSERT(minpt->y < maxpt->y);
    ASSERT(minpt->z < maxpt->z);

    if (minpt->x >= maxpt->x || minpt->y >= maxpt->y || minpt->z >= maxpt->z)   {
        log_print(LOG_WARNING, "invalid scene bounding values");
        return;
    }

    struct scn_data* s = scene_get(scene_id);
    vec3_setv(&s->minpt, minpt);
    vec3_setv(&s->maxpt, maxpt);
    scene_grid_resize(scene_id, minpt, maxpt, s->grid.cell_size);
}

void scn_getsize(uint scene_id, OUT struct vec3f* minpt, OUT struct vec3f* maxpt)
{
    struct scn_data* s = scene_get(scene_id);
    vec3_setv(minpt, &s->minpt);
    vec3_setv(maxpt, &s->maxpt);
}

void scn_setactive(uint scene_id)
{
    g_scn_mgr.active_scene_id = scene_id;
    if (scene_id != 0)  {
        struct scn_data* s = scene_get(scene_id);
        phx_setactive(s->phx_sceneid);
    }   else    {
        phx_setactive(0);
    }
}

uint scn_getactive()
{
    return g_scn_mgr.active_scene_id;
}

void scn_setcellsize(uint scene_id, float cell_size)
{
    struct scn_data* s = scene_get(scene_id);
    scene_grid_resize(scene_id, &s->minpt, &s->maxpt, clampf(cell_size, 10.0f, 1000.0f));
}

float scn_getcellsize(uint scene_id)
{
    struct scn_data* s = scene_get(scene_id);
    return s->grid.cell_size;
}

result_t scene_console_setcellsize(uint argc, const char** argv, void* param)
{
    if (argc != 1)
        return RET_INVALIDARG;

    scn_setcellsize(g_scn_mgr.active_scene_id, str_tofl32(argv[0]));
    return RET_OK;
}

result_t scene_console_campos(uint argc, const char** argv, void* param)
{
    int show = TRUE;
    if (argc == 1)
        show = str_tobool(argv[0]);
    else if (argc > 1)
        return RET_INVALIDARG;

    if (show)
    	hud_add_label("lbl-debugcam", scene_debug_cam, NULL);
    else
    	hud_remove_label("lbl-debugcam");
    return RET_OK;
}

int scene_debug_cam(gfx_cmdqueue cmqueue, int x, int y, int line_stride, void* param)
{
    char text[128];
    struct camera* cam = wld_get_cam();
    if (cam != NULL)    {
        sprintf(text, "camera:pos(%.2f, %.2f, %.2f)", cam->pos.x, cam->pos.y, cam->pos.z);
        gfx_canvas_text2dpt(text, x, y, 0);
        return (y + line_stride);
    }
    return y;
}

uint scn_getphxscene(uint scene_id)
{
    ASSERT(scene_id != 0);
    return scene_get(scene_id)->phx_sceneid;
}

result_t scene_create_components(struct cmp_obj* obj)
{
    /* always add transform/bounds components */
    scene_add_component(obj, cmp_xform_type);
    scene_add_component(obj, cmp_bounds_type);

    switch (obj->type)  {
    case CMP_OBJTYPE_MODEL:
        scene_add_component(obj, cmp_model_type);
        break;
    case CMP_OBJTYPE_LIGHT:
        scene_add_component(obj, cmp_light_type);
        break;
    case CMP_OBJTYPE_CAMERA:
        scene_add_component(obj, cmp_camera_type);
        break;
    default:
        break;
    }

    return RET_OK;
}

cmphandle_t scene_add_component(struct cmp_obj* obj, cmptype_t type)
{
    cmp_t c = cmp_findtype(type);
    if (c == NULL)
        return INVALID_HANDLE;

    cmphandle_t hdl = cmp_findinstance(obj->chain, cmp_gettype(c));
    if (hdl != INVALID_HANDLE)
        return hdl;

    hdl = cmp_create_instance(c, obj, 0, INVALID_HANDLE, 0);
    if (hdl == INVALID_HANDLE)
        return INVALID_HANDLE;

    return hdl;
}

