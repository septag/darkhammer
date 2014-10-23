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
#include "dhcore/task-mgr.h"

#include "gfx.h"
#include "gfx-shader.h"
#include "gfx-cmdqueue.h"
#include "gfx-model.h"
#include "engine.h"
#include "mem-ids.h"
#include "scene-mgr.h"
#include "gfx-device.h"

/*************************************************************************************************
 * types
 */
struct gfx_fwd_renderer
{
	uint shaderid_raw;
	uint shaderid_diffmap;
	struct gfx_cblock* cb_frame;
	struct gfx_cblock* cb_xforms;
    gfx_depthstencilstate ds;
};

/*************************************************************************************************
 * fwd declarations
 */
void gfx_fwd_preparebatchnode(gfx_cmdqueue cmdqueue, struct gfx_batch_node* bnode,
		struct gfx_shader* shader, OUT struct gfx_model_geo** pgeo, OUT uint* psubset_idx);
void gfx_fwd_drawbatchnode(gfx_cmdqueue cmdqueue, struct gfx_batch_node* bnode,
		struct gfx_shader* shader, struct gfx_model_geo* geo, uint subset_idx);

/*************************************************************************************************
 * globals
 */
struct gfx_fwd_renderer* g_fwd = NULL;

/*************************************************************************************************/
uint gfx_fwd_getshader(enum cmp_obj_type obj_type, uint rpath_flags)
{
	if (BIT_CHECK(rpath_flags, GFX_RPATH_DIFFUSEMAP))
		return g_fwd->shaderid_diffmap;
	else
		return g_fwd->shaderid_raw;
}

result_t gfx_fwd_init(uint width, uint height)
{
	g_fwd = (struct gfx_fwd_renderer*)ALLOC(sizeof(struct gfx_fwd_renderer), MID_GFX);
	if (g_fwd == NULL)
		return RET_OUTOFMEMORY;
	memset(g_fwd, 0x00, sizeof(struct gfx_fwd_renderer));

	struct allocator* lsr_alloc = eng_get_lsralloc();
	struct allocator* tmp_alloc = tsk_get_tmpalloc(0);
    char max_instances_str[8];
    str_itos(max_instances_str, GFX_INSTANCES_MAX);

	/* shaders */
	gfx_shader_beginload(lsr_alloc, "shaders/fwd.vs", "shaders/fwd.ps", NULL, 2,
        "shaders/common.inc", "shaders/skin.inc");
	g_fwd->shaderid_raw = gfx_shader_add("fwd-raw", 3, 1,
			GFX_INPUTELEMENT_ID_POSITION, "vsi_pos", 0,
			GFX_INPUTELEMENT_ID_NORMAL, "vsi_norm", 0,
			GFX_INPUTELEMENT_ID_TEXCOORD0, "vsi_coord0", 0,
            "_MAX_INSTANCES_", max_instances_str);

	g_fwd->shaderid_diffmap = gfx_shader_add("fwd-diffmap", 3, 2,
			GFX_INPUTELEMENT_ID_POSITION, "vsi_pos", 0,
			GFX_INPUTELEMENT_ID_NORMAL, "vsi_norm", 0,
			GFX_INPUTELEMENT_ID_TEXCOORD0, "vsi_coord0", 0,
			"_DIFFUSEMAP_", "1",
            "_MAX_INSTANCES_", max_instances_str);
	gfx_shader_endload();

    if (g_fwd->shaderid_raw == 0 || g_fwd->shaderid_diffmap == 0)	{
        err_printf(__FILE__, __LINE__, "fwd-renderer init failed: could not load shader");
        return RET_FAIL;
    }

	/* cblocks */
	struct gfx_shader* shader = gfx_shader_get(g_fwd->shaderid_diffmap);
	g_fwd->cb_frame = gfx_shader_create_cblock(lsr_alloc, tmp_alloc, shader, "cb_frame", NULL);
	g_fwd->cb_xforms = gfx_shader_create_cblock(lsr_alloc, tmp_alloc, shader, "cb_xforms", NULL);
	if (g_fwd->cb_frame == NULL || g_fwd->cb_xforms == NULL)	{
		err_printf(__FILE__, __LINE__, "fwd-renderer init failed: could not crete cblocks");
		return RET_FAIL;
	}

    /* states */
    struct gfx_depthstencil_desc dsdesc;
    memcpy(&dsdesc, gfx_get_defaultdepthstencil(), sizeof(dsdesc));
    dsdesc.depth_enable = TRUE;
    dsdesc.depth_write = TRUE;
    g_fwd->ds = gfx_create_depthstencilstate(&dsdesc);

	return RET_OK;
}

void gfx_fwd_release()
{
	if (g_fwd != NULL)	{
        if (g_fwd->ds != NULL)
            gfx_destroy_depthstencilstate(g_fwd->ds);
		if (g_fwd->cb_frame != NULL)
			gfx_shader_destroy_cblock(g_fwd->cb_frame);
		if (g_fwd->cb_xforms != NULL)
			gfx_shader_destroy_cblock(g_fwd->cb_xforms);
		if (g_fwd->shaderid_raw != 0)
			gfx_shader_unload(g_fwd->shaderid_raw);
		if (g_fwd->shaderid_diffmap != 0)
			gfx_shader_unload(g_fwd->shaderid_diffmap);

		FREE(g_fwd);
	}
}

void gfx_fwd_render(gfx_cmdqueue cmdqueue, gfx_rendertarget rt,
		const struct gfx_view_params* params, struct gfx_batch_item* batch_items, uint batch_cnt,
		void* userdata, OUT struct gfx_rpath_result* result)
{
	if (batch_cnt == 0)
		return;

    struct gfx_cblock* cb_frame = g_fwd->cb_frame;

    gfx_output_setdepthstencilstate(cmdqueue, g_fwd->ds, 0);
    gfx_cb_set4m(cb_frame, SHADER_NAME(c_viewproj), &params->viewproj);
    gfx_shader_updatecblock(cmdqueue, cb_frame);

    for (uint i = 0; i < batch_cnt; i++)	{
    	struct gfx_batch_item* bitem = &batch_items[i];
    	struct gfx_shader* shader = gfx_shader_get(bitem->shader_id);
    	ASSERT(shader);
    	gfx_shader_bind(cmdqueue, shader);

    	for (int k = 0; k < bitem->nodes.item_cnt; k++)	{
    		struct gfx_batch_node* bnode_first = &((struct gfx_batch_node*)bitem->nodes.buffer)[k];
    		struct gfx_model_geo* geo;
    		uint subset_idx;
    		gfx_fwd_preparebatchnode(cmdqueue, bnode_first, shader, &geo, &subset_idx);

    		struct linked_list* node = bnode_first->bll;
    		while (node != NULL)	{
    			struct gfx_batch_node* bnode = (struct gfx_batch_node*)node->data;
    			gfx_fwd_drawbatchnode(cmdqueue, bnode, shader, geo, subset_idx);
    			node = node->next;
    		}
    	}	/* for: each batch-item */
    }
}

void gfx_fwd_preparebatchnode(gfx_cmdqueue cmdqueue, struct gfx_batch_node* bnode,
		struct gfx_shader* shader, OUT struct gfx_model_geo** pgeo, OUT uint* psubset_idx)
{
	struct scn_render_model* rmodel = (struct scn_render_model*)bnode->ritem;
	struct gfx_model* gmodel = rmodel->gmodel;
	struct gfx_model_instance* inst = rmodel->inst;
	struct gfx_model_mesh* mesh = &gmodel->meshes[gmodel->nodes[rmodel->node_idx].mesh_id];
	struct gfx_model_geo* geo = &gmodel->geos[mesh->geo_id];
	uint mtl_id = mesh->submeshes[bnode->sub_idx].mtl_id;
	struct gfx_model_mtlgpu* gmtl = inst->mtls[mtl_id];
	uint subset_idx = mesh->submeshes[bnode->sub_idx].subset_id;

    const struct gfx_cblock* cblocks[] = {g_fwd->cb_frame, g_fwd->cb_xforms, gmtl->cb};

	*pgeo = geo;
	*psubset_idx = subset_idx;

    gfx_input_setlayout(cmdqueue, geo->inputlayout);
    gfx_model_setmtl(cmdqueue, shader, inst, mtl_id);
    gfx_shader_bindcblocks(cmdqueue, shader, cblocks, 3);
    gfx_shader_bindconstants(cmdqueue, shader);
}

void gfx_fwd_drawbatchnode(gfx_cmdqueue cmdqueue, struct gfx_batch_node* bnode,
		struct gfx_shader* shader, struct gfx_model_geo* geo, uint subset_idx)
{
	struct gfx_model_geosubset* subset = &geo->subsets[subset_idx];

    /* set transform matrices */
	gfx_cb_set3mvp(g_fwd->cb_xforms, SHADER_NAME(c_mats), bnode->instance_mats,
			bnode->instance_cnt);
    gfx_shader_updatecblock(cmdqueue, g_fwd->cb_xforms);

	/* draw */
    gfx_draw_indexedinstance(cmdqueue, GFX_PRIMITIVE_TRIANGLELIST, subset->ib_idx, subset->idx_cnt,
    		geo->ib_type, bnode->instance_cnt, GFX_DRAWCALL_FWD);
}

result_t gfx_fwd_resize(uint width, uint height)
{
    return RET_OK;
}
