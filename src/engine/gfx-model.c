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
#include "dhcore/hash.h"
#include "dhcore/file-io.h"
#include "dhcore/stack-alloc.h"
#include "dhcore/task-mgr.h"

#include "gfx-model.h"
#include "h3d-types.h"
#include "mem-ids.h"
#include "gfx-device.h"
#include "res-mgr.h"
#include "gfx.h"
#include "gfx-shader.h"
#include "gfx-cmdqueue.h"

#define HSEED 2343

/*************************************************************************************************
 * forward declarations
 */
int model_loadnode(struct gfx_model_node* node, file_t f, struct allocator* alloc);
int model_loadmesh(struct gfx_model_mesh* mesh, file_t f, struct allocator* alloc);
int model_loadgeo(struct gfx_model_geo* geo, file_t f, struct allocator* alloc,
		struct allocator* tmp_alloc, uint thread_id);
int model_loadmtl(struct gfx_model_mtl* mtl, file_t f, struct allocator* alloc);
int model_loadocc(struct gfx_model_occ* occ, file_t f, struct allocator* alloc);

int model_checkvertid(const uint* vert_ids, uint vert_id_cnt, enum gfx_input_element_id id);
gfx_buffer model_loadvbuffer(file_t f, uint vert_cnt, uint elem_sz, struct allocator* tmp_alloc,
                             uint thread_id);

void model_unloadgeo(struct gfx_model_geo* geo);

uint model_make_rpathflags(const struct gfx_model* model, uint mesh_id, uint submesh_id);
struct gfx_model_posegpu* model_load_gpupose(struct allocator* alloc,
		struct allocator* tmp_alloc, const struct gfx_model_geo* geo, uint rpath_flags);
struct gfx_model_mtlgpu* model_load_gpumtl(struct allocator* main_alloc, struct allocator* alloc,
        struct allocator* tmp_alloc, const struct gfx_model_mtl* mtl, uint rpath_flags);
void model_destroy_gpumtl(struct allocator* alloc, struct gfx_model_mtlgpu* gmtl);

void model_update_uniqueids(struct gfx_model_instance* inst);
void model_update_alphaflags(struct gfx_model_instance* inst);
const struct mat3f* model_loadmat_pq(struct mat3f* rm, const float* pos, const float* quat);
const struct mat3f* model_loadmat(struct mat3f* rm, const float* f);

int gfx_model_create_inputlayout(struct gfx_model_geo* geo);
uint gfx_model_choose_elem_buffidx(enum gfx_input_element_id id, OUT uint* offset);
size_t gfx_model_choose_vbuff_size(uint idx);

/*************************************************************************************************
 * inlines
 */
INLINE int model_map_issrgb(enum gfx_model_maptype type)
{
    switch (type)   {
    case GFX_MODEL_DIFFUSEMAP:
    case GFX_MODEL_EMISSIVEMAP:
    case GFX_MODEL_REFLECTIONMAP:
        return TRUE;
    default:
        return FALSE;
    }
}

/*************************************************************************************************/
struct gfx_model* gfx_model_load(struct allocator* alloc, const char* h3dm_filepath,
    uint thread_id)
{
	struct allocator* tmp_alloc = tsk_get_tmpalloc(thread_id);
	A_SAVE(tmp_alloc);

	struct h3d_header header;
    struct h3d_model h3dmodel;
	struct gfx_model* model = NULL;
    uint renderable_idx = 0;
    struct stack_alloc stack_mem;
    struct allocator stack_alloc;
    result_t r;

    memset(&stack_mem, 0x00, sizeof(stack_mem));

	file_t f = fio_openmem(tmp_alloc, h3dm_filepath, FALSE, MID_GFX);
	if (f == NULL)	{
		err_printf(__FILE__, __LINE__, "load model '%s' failed: could not open file", h3dm_filepath);
		goto err_cleanup;
	}

	/* header */
    fio_read(f, &header, sizeof(header), 1);
	if (header.sign != H3D_SIGN || header.type != H3D_MESH)	{
		err_printf(__FILE__, __LINE__, "load model '%s' failed: invalid file format", h3dm_filepath);
		goto err_cleanup;
	}

    if (header.version != H3D_VERSION && header.version != H3D_VERSION_13)  {
        err_printf(__FILE__, __LINE__, "load model '%s' failed: file version not implemented/obsolete",
            h3dm_filepath);
        goto err_cleanup;
    }

    /* model */
    fio_read(f, &h3dmodel, sizeof(h3dmodel), 1);

    /* calculate size and create stack allocator for proceeding allocations */
    size_t total_sz =
        sizeof(struct gfx_model) +
        h3dmodel.node_cnt*sizeof(struct gfx_model_node) + 16 +
        h3dmodel.node_cnt*sizeof(uint) +
        h3dmodel.geo_cnt*sizeof(struct gfx_model_geo) +
        h3dmodel.mesh_cnt*sizeof(struct gfx_model_mesh) +
        h3dmodel.mtl_cnt*sizeof(struct gfx_model_mtl) +
        h3dmodel.has_occ*sizeof(struct gfx_model_occ) +
        h3dmodel.total_childidxs*sizeof(uint) +
        h3dmodel.total_geo_subsets*sizeof(struct gfx_model_geosubset) +
        h3dmodel.total_joints*sizeof(struct gfx_model_joint) +
        h3dmodel.total_joints*sizeof(struct mat3f) +
        h3dmodel.total_submeshes*sizeof(struct gfx_model_submesh) +
        h3dmodel.total_skeletons*sizeof(struct gfx_model_skeleton) +
        h3dmodel.total_skeletons*32 + /* 2 aligned allocs per skeleton */
        h3dmodel.total_maps*sizeof(struct gfx_model_map) +
        h3dmodel.occ_idx_cnt*sizeof(uint16) +
        h3dmodel.occ_vert_cnt*sizeof(struct vec3f) +
        h3dmodel.has_occ*16; /* 1 aligned alloc for occ */
    r = mem_stack_create(alloc, &stack_mem, total_sz, MID_GFX);
    if (IS_FAIL(r)) {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        goto err_cleanup;
    }
    mem_stack_bindalloc(&stack_mem, &stack_alloc);

    /* */
    model = (struct gfx_model*)A_ALLOC(&stack_alloc, sizeof(struct gfx_model), MID_GFX);
    if (model == NULL)	{
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        goto err_cleanup;
    }
    memset(model, 0x00, sizeof(struct gfx_model));
    model->alloc = alloc;

	/* nodes */
	if (h3dmodel.node_cnt > 0)	{
		model->nodes = (struct gfx_model_node*)A_ALIGNED_ALLOC(&stack_alloc,
            sizeof(struct gfx_model_node)*h3dmodel.node_cnt, MID_GFX);
        ASSERT(model->nodes);
		memset(model->nodes, 0x00, sizeof(struct gfx_model_node)*h3dmodel.node_cnt);

		for (uint i = 0; i < h3dmodel.node_cnt; i++)	{
			struct gfx_model_node* node = &model->nodes[i];
			if (!model_loadnode(node, f, &stack_alloc))
				goto err_cleanup;

            /* NOTE: we set root matrix to identity and keep the old one as "root_mat" */
            if (i == 0) {
                mat3_setm(&model->root_mat, &node->local_mat);
                mat3_set_ident(&node->local_mat);
            }
			model->node_cnt ++;
		}
	}

	/* meshes */
	if (h3dmodel.mesh_cnt > 0)	{
		model->meshes = (struct gfx_model_mesh*)A_ALLOC(&stack_alloc,
            sizeof(struct gfx_model_mesh)*h3dmodel.mesh_cnt, MID_GFX);
		ASSERT(model->meshes);
		memset(model->meshes, 0x00, sizeof(struct gfx_model_mesh)*h3dmodel.mesh_cnt);
		uint idx = 0;

		for (uint i = 0; i < h3dmodel.mesh_cnt; i++)	{
			struct gfx_model_mesh* mesh = &model->meshes[i];
			if (!model_loadmesh(mesh, f, &stack_alloc))
				goto err_cleanup;

			/* assign global indexes */
			for (uint k = 0; k < mesh->submesh_cnt; k++)
				mesh->submeshes[k].offset_idx = idx++;

			model->mesh_cnt ++;
		}
	}

	/* geos */
	if (h3dmodel.geo_cnt > 0)	{
		model->geos = (struct gfx_model_geo*)A_ALLOC(&stack_alloc,
            sizeof(struct gfx_model_geo)*h3dmodel.geo_cnt, MID_GFX);
		ASSERT(model->geos);

		memset(model->geos, 0x00, sizeof(struct gfx_model_geo)*h3dmodel.geo_cnt);
		for (uint i = 0; i < h3dmodel.geo_cnt; i++)	{
			struct gfx_model_geo* geo = &model->geos[i];
			if (!model_loadgeo(geo, f, &stack_alloc, tmp_alloc, thread_id))
				goto err_cleanup;
			model->geo_cnt ++;
		}
	}

	/* materials */
	if (h3dmodel.mtl_cnt > 0)	{
		model->mtls = (struct gfx_model_mtl*)A_ALLOC(&stack_alloc,
            sizeof(struct gfx_model_mtl)*h3dmodel.mtl_cnt, MID_GFX);
		ASSERT(model->mtls);

		memset(model->mtls, 0x00, sizeof(struct gfx_model_mtl)*h3dmodel.mtl_cnt);
		for (uint i = 0; i < h3dmodel.mtl_cnt; i++)	{
			struct gfx_model_mtl* mtl = &model->mtls[i];
			if (!model_loadmtl(mtl, f, &stack_alloc))
                goto err_cleanup;
			model->mtl_cnt ++;
		}
	}

    if (header.version >= H3D_VERSION_11 && h3dmodel.has_occ)   {
        model->occ = (struct gfx_model_occ*)A_ALLOC(&stack_alloc, sizeof(struct gfx_model_occ),
            MID_GFX);
        ASSERT(model->occ);

        memset(model->occ, 0x00, sizeof(struct gfx_model_occ));
        if (!model_loadocc(model->occ, f, &stack_alloc))
            goto err_cleanup;
    }

    /* populate renderable nodes */
    model->renderable_idxs = (uint*)A_ALLOC(&stack_alloc, sizeof(uint)*h3dmodel.node_cnt,
        MID_GFX);
    ASSERT(model->renderable_idxs);

    for (uint i = 0; i < h3dmodel.node_cnt; i++)	{
        struct gfx_model_node* node = &model->nodes[i];
        if (node->mesh_id != INVALID_INDEX)
            model->renderable_idxs[renderable_idx++] = i;
    }
    model->renderable_cnt = renderable_idx;

    /* calculate sum of aabb(s) from renderable nodes */
	aabb_setzero(&model->bb);
    struct mat3f node_mat;  /* transform matrix, relative to model */
	for (uint i = 0; i < renderable_idx; i++)   {
        struct gfx_model_node* node = &model->nodes[model->renderable_idxs[i]];
        mat3_setm(&node_mat, &node->local_mat);
        struct gfx_model_node* pnode = node;
        while (pnode->parent_id != INVALID_INDEX)	{
            pnode = &model->nodes[pnode->parent_id];
            mat3_mul(&node_mat, &node_mat, &pnode->local_mat);
        }
        if (node->parent_id != INVALID_INDEX)
            mat3_mul(&node_mat, &node_mat, &model->root_mat);

        /* transform local box to model-relative bounding box and merge with final */
        struct aabb bb;
        aabb_xform(&bb, &model->nodes[model->renderable_idxs[i]].bb, &node_mat);
		aabb_merge(&model->bb, &model->bb, &bb);
    }
    /* for empty models, we set a minimal bounding-box */
    if (aabb_iszero(&model->bb))    {
        aabb_pushptf(&model->bb, 0.1f, 0.1f, 0.1f);
        aabb_pushptf(&model->bb, -0.1f, -0.1f, -0.1f);
    }

    fio_close(f);
	A_LOAD(tmp_alloc);

    if (thread_id != 0) {
        gfx_delayed_waitforobjects(thread_id);
        gfx_delayed_fillobjects(thread_id);
    }

	return model;

err_cleanup:
	if (f != NULL)
        fio_close(f);
	if (model != NULL)
		gfx_model_unload(model);
    mem_stack_destroy(&stack_mem);
	A_LOAD(tmp_alloc);
	return NULL;
}

int model_loadnode(struct gfx_model_node* node, file_t f, struct allocator* alloc)
{
	struct h3d_node h3dnode;
    fio_read(f, &h3dnode, sizeof(h3dnode), 1);
	strcpy(node->name, h3dnode.name);
    node->name_hash = hash_str(h3dnode.name);
	node->mesh_id = h3dnode.mesh_idx;
	node->child_cnt = h3dnode.child_cnt;
	node->parent_id = h3dnode.parent_idx;

    model_loadmat(&node->local_mat, h3dnode.local_xform);
    aabb_setf(&node->bb, h3dnode.bb_min[0], h3dnode.bb_min[1], h3dnode.bb_min[2],
        h3dnode.bb_max[0], h3dnode.bb_max[1], h3dnode.bb_max[2]);

	if (h3dnode.child_cnt > 0)	{
		node->child_ids = (uint*)A_ALLOC(alloc, sizeof(uint)*h3dnode.child_cnt, MID_GFX);
		if (node->child_ids == NULL)
			return FALSE;
	 fio_read(f, node->child_ids, sizeof(uint), h3dnode.child_cnt);
	}
	return TRUE;
}


int model_loadmesh(struct gfx_model_mesh* mesh, file_t f, struct allocator* alloc)
{
	struct h3d_mesh h3dmesh;
    fio_read(f, &h3dmesh, sizeof(h3dmesh), 1);
	mesh->geo_id = h3dmesh.geo_idx;
	mesh->submesh_cnt = h3dmesh.submesh_cnt;
	if (h3dmesh.submesh_cnt > 0)	{
		mesh->submeshes = (struct gfx_model_submesh*)A_ALLOC(alloc,
            sizeof(struct gfx_model_submesh)*h3dmesh.submesh_cnt, MID_GFX);
		if (mesh->submeshes == NULL)
			return FALSE;
		for (uint i = 0; i < h3dmesh.submesh_cnt; i++)	{
			struct h3d_submesh h3dsubmesh;
		    fio_read(f, &h3dsubmesh, sizeof(h3dsubmesh), 1);
			mesh->submeshes[i].mtl_id = h3dsubmesh.mtl_idx;
			mesh->submeshes[i].subset_id = h3dsubmesh.subset_idx;
		}
	}
	return TRUE;
}


int model_loadgeo(struct gfx_model_geo* geo, file_t f, struct allocator* alloc,
		struct allocator* tmp_alloc, uint thread_id)
{
	struct h3d_geo h3dgeo;
	uint v_cnt = 0;

    fio_read(f, &h3dgeo, sizeof(h3dgeo), 1);
	geo->vert_cnt = h3dgeo.vert_cnt;
	geo->vert_id_cnt = h3dgeo.vert_id_cnt;
	geo->tri_cnt = h3dgeo.tri_cnt;
	geo->subset_cnt = h3dgeo.subset_cnt;
	geo->ib_type = h3dgeo.ib_isui32 ? gfxIndexType::UINT32 : gfxIndexType::UINT16;
	memcpy(geo->vert_ids, h3dgeo.vert_ids, sizeof(h3dgeo.vert_ids));

	/* subsets */
	ASSERT(h3dgeo.subset_cnt > 0);
	geo->subsets = (struct gfx_model_geosubset*)A_ALLOC(alloc,
        sizeof(struct gfx_model_geosubset)*h3dgeo.subset_cnt, MID_GFX);
	ASSERT(geo->subsets != NULL);

	for (uint i = 0; i < h3dgeo.subset_cnt; i++)	{
		struct h3d_geo_subset h3dsubset;
	    fio_read(f, &h3dsubset, sizeof(h3dsubset), 1);
		geo->subsets[i].ib_idx = h3dsubset.ib_idx;
		geo->subsets[i].idx_cnt = h3dsubset.idx_cnt;
	}

	/* indexes */
	ASSERT(h3dgeo.tri_cnt > 0);
	uint ibuffer_sz = (geo->ib_type == gfxIndexType::UINT16) ? sizeof(uint16)*h3dgeo.tri_cnt*3 :
			sizeof(uint)*h3dgeo.tri_cnt*3;
	void* indexes = A_ALLOC(tmp_alloc, ibuffer_sz, MID_GFX);
	if (indexes == NULL)
		goto err_cleanup;

    fio_read(f, indexes, ibuffer_sz, 1);

 	geo->ibuffer = gfx_create_buffer(gfxBufferType::INDEX, gfxMemHint::STATIC, ibuffer_sz, indexes,
        thread_id);
	A_FREE(tmp_alloc, indexes);
	if (geo->ibuffer == NULL)
		goto err_cleanup;

	/* vertices */
	/* base data */
    int has_pos = model_checkvertid(h3dgeo.vert_ids, h3dgeo.vert_id_cnt,
        GFX_INPUTELEMENT_ID_POSITION);
    int has_norm = model_checkvertid(h3dgeo.vert_ids, h3dgeo.vert_id_cnt,
        GFX_INPUTELEMENT_ID_NORMAL);
    int has_coord = model_checkvertid(h3dgeo.vert_ids, h3dgeo.vert_id_cnt,
        GFX_INPUTELEMENT_ID_TEXCOORD0);

	if (has_pos | has_norm | has_coord)	{
		geo->vbuffers[GFX_MODEL_BUFFER_BASE] =
				model_loadvbuffer(f, h3dgeo.vert_cnt, sizeof(struct h3d_vertex_base), tmp_alloc,
                thread_id);
		if (geo->vbuffers[GFX_MODEL_BUFFER_BASE] == NULL)
			goto err_cleanup;
	}
    geo->vert_ids[v_cnt++] = GFX_INPUTELEMENT_ID_POSITION;
	if (has_norm)
		geo->vert_ids[v_cnt++] = GFX_INPUTELEMENT_ID_NORMAL;
	if (has_coord)
		geo->vert_ids[v_cnt++] = GFX_INPUTELEMENT_ID_TEXCOORD0;

    /* skin data */
    int has_bindex = model_checkvertid(h3dgeo.vert_ids, h3dgeo.vert_id_cnt,
        GFX_INPUTELEMENT_ID_BLENDINDEX);
    int has_bweight = model_checkvertid(h3dgeo.vert_ids, h3dgeo.vert_id_cnt,
        GFX_INPUTELEMENT_ID_BLENDWEIGHT);
    if (has_bindex | has_bweight)   {
        geo->vbuffers[GFX_MODEL_BUFFER_SKIN] = model_loadvbuffer(f, h3dgeo.vert_cnt,
            sizeof(struct h3d_vertex_skin), tmp_alloc, thread_id);
        if (geo->vbuffers[GFX_MODEL_BUFFER_BASE] == NULL)
            goto err_cleanup;
    }

    if (has_bindex)
        geo->vert_ids[v_cnt++] = GFX_INPUTELEMENT_ID_BLENDINDEX;
    if (has_bweight)
        geo->vert_ids[v_cnt++] = GFX_INPUTELEMENT_ID_BLENDWEIGHT;

    /* normal-map coord data */
    int has_tangent = model_checkvertid(h3dgeo.vert_ids, h3dgeo.vert_id_cnt,
        GFX_INPUTELEMENT_ID_TANGENT);
    int has_binorm = model_checkvertid(h3dgeo.vert_ids, h3dgeo.vert_id_cnt,
        GFX_INPUTELEMENT_ID_BINORMAL);
    if (has_tangent | has_binorm)   {
        geo->vbuffers[GFX_MODEL_BUFFER_NMAP] = model_loadvbuffer(f, h3dgeo.vert_cnt,
            sizeof(struct h3d_vertex_nmap), tmp_alloc, thread_id);
        if (geo->vbuffers[GFX_MODEL_BUFFER_NMAP] == NULL)
            goto err_cleanup;
    }
    if (has_tangent)
        geo->vert_ids[v_cnt++] = GFX_INPUTELEMENT_ID_TANGENT;
    if (has_binorm)
        geo->vert_ids[v_cnt++] = GFX_INPUTELEMENT_ID_BINORMAL;

    /* Extra data */
    int has_coord1 = model_checkvertid(h3dgeo.vert_ids, h3dgeo.vert_id_cnt,
        GFX_INPUTELEMENT_ID_TEXCOORD1);
    int has_color = model_checkvertid(h3dgeo.vert_ids, h3dgeo.vert_id_cnt,
        GFX_INPUTELEMENT_ID_COLOR);
    if (has_coord1 | has_color) {
        geo->vbuffers[GFX_MODEL_BUFFER_EXTRA] = model_loadvbuffer(f, h3dgeo.vert_cnt,
            sizeof(struct h3d_vertex_extra), tmp_alloc, thread_id);
        if (geo->vbuffers[GFX_MODEL_BUFFER_EXTRA] == NULL)
            goto err_cleanup;
    }
	if (has_coord1)
		geo->vert_ids[v_cnt++] = GFX_INPUTELEMENT_ID_TEXCOORD1;
	if (has_color)
		geo->vert_ids[v_cnt++] = GFX_INPUTELEMENT_ID_COLOR;

	/* skeleton */
	if (h3dgeo.joint_cnt > 0)	{
		geo->skeleton = (struct gfx_model_skeleton*)A_ALLOC(alloc,
            sizeof(struct gfx_model_skeleton), MID_GFX);
		if (geo->skeleton == NULL)
			return FALSE;
		memset(geo->skeleton, 0x00, sizeof(struct gfx_model_skeleton));
		geo->skeleton->joint_cnt = h3dgeo.joint_cnt;
		geo->skeleton->bones_pervertex_max = h3dgeo.bones_pervertex_max;
        model_loadmat(&geo->skeleton->joints_rootmat, h3dgeo.joints_rootmat);

		geo->skeleton->init_pose = (struct mat3f*)A_ALIGNED_ALLOC(alloc,
            sizeof(struct mat3f)*h3dgeo.joint_cnt, MID_GFX);
		geo->skeleton->joints = (struct gfx_model_joint*)A_ALIGNED_ALLOC(alloc,
				sizeof(struct gfx_model_joint)*h3dgeo.joint_cnt, MID_GFX);
		ASSERT(geo->skeleton->init_pose != NULL);
        ASSERT(geo->skeleton->joints != NULL);

		for (uint i = 0; i < h3dgeo.joint_cnt; i++)	{
			struct h3d_joint h3djoint;
			struct gfx_model_joint* joint = &geo->skeleton->joints[i];
		    fio_read(f, &h3djoint, sizeof(h3djoint), 1);

			strcpy(joint->name, h3djoint.name);
            joint->name_hash = hash_str(h3djoint.name);

            model_loadmat(&joint->offset_mat, h3djoint.offset_mat);

			joint->parent_id = h3djoint.parent_idx;
		}

        fio_read(f, geo->skeleton->init_pose, sizeof(struct mat3f), h3dgeo.joint_cnt);
	}

    ASSERT(v_cnt > 0);

    /* input layout */
    uint buff_cnt = 0;
    uint input_cnt = geo->vert_id_cnt;
    struct gfx_input_vbuff_desc vbuffs[GFX_MODEL_BUFFER_CNT];
    struct gfx_input_element_binding inputs[GFX_INPUTELEMENT_ID_CNT];

    for (uint i = 0; i < input_cnt; i++)  {
        inputs[i].id = (enum gfx_input_element_id)geo->vert_ids[i];
        inputs[i].vb_idx = gfx_model_choose_elem_buffidx(inputs[i].id, &inputs[i].elem_offset);
    }

    for (uint i = 0; i < GFX_MODEL_BUFFER_CNT; i++)   {
        if (geo->vbuffers[i] != NULL)   {
            vbuffs[buff_cnt].stride = gfx_model_choose_vbuff_size(i);
            vbuffs[buff_cnt].vbuff = geo->vbuffers[i];
            buff_cnt ++;
        }
    }

    geo->inputlayout = gfx_create_inputlayout(vbuffs, buff_cnt, inputs, input_cnt,
        geo->ibuffer, geo->ib_type, thread_id);
	if (geo->inputlayout == NULL)
		goto err_cleanup;

	return TRUE;

err_cleanup:
	if (geo != NULL)
		model_unloadgeo(geo);
	return FALSE;
}

const struct mat3f* model_loadmat_pq(struct mat3f* rm, const float* pos, const float* quat)
{
    struct vec3f p;
    struct quat4f q;

    vec3_setf(&p, pos[0], pos[1], pos[2]);
    quat_setf(&q, quat[0], quat[1], quat[2], quat[3]);

    return mat3_set_trans_rot(rm, &p, &q);
}

const struct mat3f* model_loadmat(struct mat3f* rm, const float* f)
{
    return mat3_setf(rm,
        f[0], f[1], f[2],
        f[3], f[4], f[5],
        f[6], f[7], f[8],
        f[9], f[10], f[11]);
}


int model_checkvertid(const uint* vert_ids, uint vert_id_cnt, enum gfx_input_element_id id)
{
	for (uint i = 0; i < vert_id_cnt; i++)	{
		if (vert_ids[i] == id)
			return TRUE;
	}
	return FALSE;
}

gfx_buffer model_loadvbuffer(file_t f, uint vert_cnt, uint elem_sz, struct allocator* tmp_alloc,
                             uint thread_id)
{
	uint size = vert_cnt * elem_sz;
    A_SAVE(tmp_alloc);
	void* buf = A_ALLOC(tmp_alloc, size, MID_GFX);
	if (buf == NULL)    {
        A_LOAD(tmp_alloc);
		return NULL;
    }
    fio_read(f, buf, elem_sz, vert_cnt);

	gfx_buffer gbuf = gfx_create_buffer(gfxBufferType::VERTEX, gfxMemHint::STATIC, size, buf, thread_id);

    A_FREE(tmp_alloc, buf);
    A_LOAD(tmp_alloc);
	return gbuf;
}

int model_loadmtl(struct gfx_model_mtl* mtl, file_t f, struct allocator* alloc)
{
	struct h3d_mtl h3dmtl;
    fio_read(f, &h3dmtl, sizeof(h3dmtl), 1);
	color_setf(&mtl->ambient, h3dmtl.ambient[0], h3dmtl.ambient[1], h3dmtl.ambient[2], 1.0f);
	color_setf(&mtl->diffuse, h3dmtl.diffuse[0], h3dmtl.diffuse[1], h3dmtl.diffuse[2], 1.0f);
	color_setf(&mtl->specular, h3dmtl.specular[0], h3dmtl.specular[1], h3dmtl.specular[2], 1.0f);
	color_setf(&mtl->emissive, h3dmtl.emissive[0], h3dmtl.emissive[1], h3dmtl.emissive[2], 1.0f);
	mtl->spec_exp = h3dmtl.spec_exp;
	mtl->spec_intensity = h3dmtl.spec_intensity;
	mtl->opacity = h3dmtl.opacity;
	mtl->map_cnt = h3dmtl.texture_cnt;
	if (h3dmtl.texture_cnt > 0)	{
		mtl->maps = (struct gfx_model_map*)A_ALLOC(alloc,
            sizeof(struct gfx_model_map)*h3dmtl.texture_cnt, MID_GFX);
		if (mtl->maps == NULL)
			return FALSE;
		for (uint i = 0; i < h3dmtl.texture_cnt; i++)	{
			struct h3d_texture h3dtex;
		    fio_read(f, &h3dtex, sizeof(h3dtex), 1);
			/* h3d_texture_type = gfx_model_maptype */
			mtl->maps[i].type = (enum gfx_model_maptype)h3dtex.type;
			strcpy(mtl->maps[i].filepath, h3dtex.filepath);

            /* TODO: this is a workaround for diffuse mapped materials that
             * imports false color values from assimp */
            if (mtl->maps[i].type == GFX_MODEL_DIFFUSEMAP)
                color_setc(&mtl->diffuse, &g_color_white);
		}
	}

	if (mtl->opacity < (1.0f - EPSILON))
		BIT_ADD(mtl->flags, GFX_MODEL_MTLFLAG_TRANSPARENT);

	return TRUE;
}

void gfx_model_unload(struct gfx_model* model)
{
	if (model->geos != NULL)	{
		for (uint i = 0; i < model->geo_cnt; i++)
			model_unloadgeo(&model->geos[i]);
	}

    A_ALIGNED_FREE(model->alloc, model);
}

void model_unloadgeo(struct gfx_model_geo* geo)
{
	for (uint i = 0; i < GFX_MODEL_BUFFER_CNT; i++)	{
		GFX_DESTROY_DEVOBJ(gfx_destroy_buffer, geo->vbuffers[i]);
	}

	GFX_DESTROY_DEVOBJ(gfx_destroy_buffer, geo->ibuffer);
	GFX_DESTROY_DEVOBJ(gfx_destroy_inputlayout, geo->inputlayout);
}

struct gfx_model_instance* gfx_model_createinstance(struct allocator* alloc,
		struct allocator* tmp_alloc, reshandle_t model)
{
    struct gfx_model* m = rs_get_model(model);
    uint unique_cnt = 0;
    uint joint_cnt = 0;
    uint skeleton_cnt = 0;
    for (uint i = 0; i < m->mesh_cnt; i++)
        unique_cnt += m->meshes[i].submesh_cnt;
    for (uint i = 0; i < m->geo_cnt; i++) {
        if (m->geos[i].skeleton != NULL)    {
            joint_cnt += m->geos[i].skeleton->joint_cnt*3;
            skeleton_cnt ++;
        }
    }

    /* create stack memory for proceeding allocs and calculate size */
    struct stack_alloc stack_mem;
    struct allocator stack_alloc;
    size_t total_sz =
        sizeof(struct gfx_model_instance) +
        m->mtl_cnt*sizeof(struct gfx_model_mtlgpu) +
        m->geo_cnt*sizeof(struct gfx_model_posegpu) +
        sizeof(uint)*unique_cnt +
        sizeof(int)*m->renderable_cnt +
        skeleton_cnt*16 +
        joint_cnt*sizeof(struct mat3f)*3;

    if (IS_FAIL(mem_stack_create(alloc, &stack_mem, total_sz, MID_GFX)))
        return NULL;
    mem_stack_bindalloc(&stack_mem, &stack_alloc);

    /* */
	struct gfx_model_instance* inst = (struct gfx_model_instance*)A_ALLOC(&stack_alloc,
        sizeof(struct gfx_model_instance), MID_GFX);
    ASSERT(inst);
	memset(inst, 0x00, sizeof(struct gfx_model_instance));

	inst->alloc = alloc;
	inst->model = model;

	/* gpu materials */
	if (m->mtl_cnt > 0)	{
		inst->mtls = (struct gfx_model_mtlgpu**)A_ALLOC(&stack_alloc,
            sizeof(struct gfx_model_mtlgpu*)*m->mtl_cnt, MID_GFX);
		ASSERT(inst->mtls);
		memset(inst->mtls, 0x00, sizeof(struct gfx_model_mtlgpu*)*m->mtl_cnt);
	}

	if (m->geo_cnt > 0)	{
		inst->poses = (struct gfx_model_posegpu**)A_ALLOC(&stack_alloc,
            sizeof(struct gfx_model_posegpu*)*m->geo_cnt, MID_GFX);
		ASSERT(inst->poses);
		memset(inst->poses, 0x00, sizeof(struct gfx_model_posegpu*)*m->geo_cnt);
        inst->pose_cnt = m->geo_cnt;
	}

	for (uint i = 0; i < m->renderable_cnt; i++)	{
		struct gfx_model_node* n = &m->nodes[m->renderable_idxs[i]];
		struct gfx_model_mesh* mesh = &m->meshes[n->mesh_id];

		for (uint k = 0; k < mesh->submesh_cnt; k++)	{
			uint geo_id = mesh->geo_id;
			uint mtl_id = mesh->submeshes[k].mtl_id;
			uint rpath_flags = model_make_rpathflags(m, n->mesh_id, k);

			/* create material gpu data, if not created before */
			if (inst->mtls[mtl_id] == NULL)	{
				inst->mtls[mtl_id] = model_load_gpumtl(alloc, &stack_alloc, tmp_alloc,
                    &m->mtls[mtl_id], rpath_flags);
				if (inst->mtls[mtl_id] == NULL)	{
					gfx_model_destroyinstance(inst);
					return NULL;
				}
			}

			if (inst->poses[geo_id] == NULL && BIT_CHECK(rpath_flags, GFX_RPATH_SKINNED))	{
				inst->poses[geo_id] = model_load_gpupose(&stack_alloc, tmp_alloc, &m->geos[geo_id],
						rpath_flags);
				if (inst->poses[geo_id] == NULL)	{
					gfx_model_destroyinstance(inst);
					return NULL;
				}
			}
		}
	}

	/* create unique Ids */
	if (unique_cnt > 0)	{
		inst->unique_ids = (uint*)A_ALLOC(&stack_alloc, sizeof(uint)*unique_cnt, MID_GFX);
		if (inst->unique_ids == NULL)	{
			gfx_model_destroyinstance(inst);
			return NULL;
		}
		memset(inst->unique_ids, 0x00, sizeof(uint)*unique_cnt);
	}

    /* create alpha flags */
    inst->alpha_flags = (int*)A_ALLOC(&stack_alloc, sizeof(int)*m->renderable_cnt, MID_GFX);
    memset(inst->alpha_flags, 0x00, sizeof(int)*m->renderable_cnt);

	/* update data of materials */
	gfx_model_updatemtls(inst);

	return inst;
}

void model_update_uniqueids(struct gfx_model_instance* inst)
{
	struct gfx_model* m = rs_get_model(inst->model);

	for (uint i = 0; i < m->renderable_cnt; i++)	{
		struct gfx_model_node* n = &m->nodes[m->renderable_idxs[i]];
		struct gfx_model_mesh* mesh = &m->meshes[n->mesh_id];

		for (uint k = 0; k < mesh->submesh_cnt; k++)	{
			uint geo_id = mesh->geo_id;
			uint mtl_id = mesh->submeshes[k].mtl_id;
			uint idx = mesh->submeshes[k].offset_idx;
			struct hash_incr hash;

			struct gfx_model_mtlgpu* gmtl = inst->mtls[mtl_id];
			struct gfx_model_geo* geo = &m->geos[geo_id];

			/* hash important stuff that is unique to each submesh */
            /* that would be hashing the sum of :
             * 1) textures
             * 2) material props
             * 3) sub-obj index
             * 4) geometry
             */
			hash_murmurincr_begin(&hash, HSEED);
			hash_murmurincr_add(&hash, gmtl->textures, sizeof(reshandle_t)*GFX_MODEL_MAX_MAPS); /* textures */
            if (gmtl->cb != NULL)
			    hash_murmurincr_add(&hash, gmtl->cb->cpu_buffer, gmtl->cb->buffer_size); /* mtl data */
			hash_murmurincr_add(&hash, &k, sizeof(uint)); /* sub-obj index */
			hash_murmurincr_add(&hash, &geo, sizeof(struct gfx_model_geo*)); /* geo */
			inst->unique_ids[idx] = hash_murmurincr_end(&hash);
		}
	}
}

void model_update_alphaflags(struct gfx_model_instance* inst)
{
	struct gfx_model* m = rs_get_model(inst->model);

	for (uint i = 0; i < m->renderable_cnt; i++)	{
		struct gfx_model_node* n = &m->nodes[m->renderable_idxs[i]];
		struct gfx_model_mesh* mesh = &m->meshes[n->mesh_id];
        int has_alpha = FALSE;

		for (uint k = 0; k < mesh->submesh_cnt && !has_alpha; k++)	{
			uint mtl_id = mesh->submeshes[k].mtl_id;

            struct gfx_model_mtl* mtl = &m->mtls[mtl_id];
			struct gfx_model_mtlgpu* gmtl = inst->mtls[mtl_id];

            has_alpha = BIT_CHECK(mtl->flags, GFX_MODEL_MTLFLAG_TRANSPARENT);

            if (gmtl->textures[GFX_MODEL_ALPHAMAP] != INVALID_HANDLE)
                has_alpha |= TRUE;
  		}

        inst->alpha_flags[i] = has_alpha;
	}
}

void gfx_model_destroyinstance(struct gfx_model_instance* inst)
{
	struct allocator* alloc = inst->alloc;
	ASSERT(inst->model != INVALID_HANDLE);
	struct gfx_model* m = rs_get_model(inst->model);

	if (inst->mtls != NULL)	{
		/* release cblocks and textures for materials */
        for (uint i = 0; i < m->mtl_cnt; i++) {
            struct gfx_model_mtlgpu* gmtl = inst->mtls[i];
            if (gmtl != NULL)
            	model_destroy_gpumtl(alloc, gmtl);
        }
	}

	A_ALIGNED_FREE(alloc, inst);
}

uint model_make_rpathflags(const struct gfx_model* model, uint mesh_id, uint submesh_id)
{
	const struct gfx_model_mesh* mesh = &model->meshes[mesh_id];
	const struct gfx_model_submesh* submesh = &mesh->submeshes[submesh_id];

	const struct gfx_model_geo* geo = &model->geos[mesh->geo_id];
	const struct gfx_model_mtl* mtl = &model->mtls[submesh->mtl_id];

	uint flags = GFX_RPATH_RAW;

	/* maps */
	for (uint i = 0, cnt = mtl->map_cnt; i < cnt; i++)	{
		switch (mtl->maps[i].type)	{
		case GFX_MODEL_DIFFUSEMAP:
			BIT_ADD(flags, GFX_RPATH_DIFFUSEMAP);
			break;
		case GFX_MODEL_GLOSSMAP:
			BIT_ADD(flags, GFX_RPATH_GLOSSMAP);
			break;
		case GFX_MODEL_NORMALMAP:
			BIT_ADD(flags, GFX_RPATH_NORMALMAP);
			break;
		case GFX_MODEL_ALPHAMAP:
			BIT_ADD(flags, GFX_RPATH_ALPHAMAP);
			break;
		case GFX_MODEL_EMISSIVEMAP:
			BIT_ADD(flags, GFX_RPATH_EMISSIVEMAP);
			break;
		case GFX_MODEL_REFLECTIONMAP:
			BIT_ADD(flags, GFX_RPATH_REFLECTIONMAP);
			break;
		}
	}

	/* skinning */
	if (geo->skeleton != NULL)
		BIT_ADD(flags, GFX_RPATH_SKINNED);

	return flags;
}

struct gfx_model_mtlgpu* model_load_gpumtl(struct allocator* main_alloc,
        struct allocator* alloc, struct allocator* tmp_alloc, const struct gfx_model_mtl* mtl,
        uint rpath_flags)
{
	struct gfx_model_mtlgpu* gmtl = (struct gfx_model_mtlgpu*)A_ALLOC(alloc,
        sizeof(struct gfx_model_mtlgpu), MID_GFX);
	ASSERT(gmtl);
	memset(gmtl, 0x00, sizeof(struct gfx_model_mtlgpu));

    for (uint i = 0; i < GFX_MODEL_MAX_MAPS; i++)
        gmtl->textures[i] = INVALID_HANDLE;

    /* load textures */
    for (uint i = 0; i < mtl->map_cnt; i++)	{
    	enum gfx_model_maptype type = mtl->maps[i].type;
        int srgb = FALSE;
        if (type == GFX_MODEL_DIFFUSEMAP || type == GFX_MODEL_REFLECTIONMAP ||
            type == GFX_MODEL_EMISSIVEMAP)
        {
            srgb = TRUE;
        }

    	gmtl->textures[type] = rs_load_texture(mtl->maps[i].filepath, 0, srgb, 0);
    	if (gmtl->textures[type] == INVALID_HANDLE)	{
    		model_destroy_gpumtl(alloc, gmtl);
    		return NULL;
    	}
    }

    const struct gfx_rpath* rpath;

    /* render-passes for each material */
    /* PRIMARY pass */
    rpath = gfx_rpath_detect(CMP_OBJTYPE_MODEL, rpath_flags);
    gmtl->passes[GFX_RENDERPASS_PRIMARY].rpath = rpath;
    if (rpath != NULL)	{
        gmtl->passes[GFX_RENDERPASS_PRIMARY].shader_id =
        		rpath->getshader_fn(CMP_OBJTYPE_MODEL, rpath_flags);
        if (gmtl->passes[GFX_RENDERPASS_PRIMARY].shader_id == 0) {
            log_printf(LOG_WARNING, "unsupported shader for render-path '%s' : %s", rpath->name,
                gfx_rpath_getflagstr(rpath_flags));
            model_destroy_gpumtl(alloc, gmtl);
            return NULL;
        }
    }

    /* sun shadow pass */
    rpath = gfx_rpath_detect(CMP_OBJTYPE_MODEL, rpath_flags | GFX_RPATH_CSMSHADOW);
    gmtl->passes[GFX_RENDERPASS_SUNSHADOW].rpath = rpath;
    if (rpath != NULL)	{
		gmtl->passes[GFX_RENDERPASS_SUNSHADOW].shader_id =
				rpath->getshader_fn(CMP_OBJTYPE_MODEL, rpath_flags | GFX_RPATH_CSMSHADOW);
    }

    /* spot shadow pass */
    rpath = gfx_rpath_detect(CMP_OBJTYPE_MODEL, rpath_flags | GFX_RPATH_SPOTSHADOW);
    gmtl->passes[GFX_RENDERPASS_SPOTSHADOW].rpath = rpath;
    if (rpath != NULL)	{
		gmtl->passes[GFX_RENDERPASS_SPOTSHADOW].shader_id =
				rpath->getshader_fn(CMP_OBJTYPE_MODEL, rpath_flags | GFX_RPATH_SPOTSHADOW);
    }

    /* point shadow pass */
    rpath = gfx_rpath_detect(CMP_OBJTYPE_MODEL, rpath_flags | GFX_RPATH_POINTSHADOW);
    gmtl->passes[GFX_RENDERPASS_POINTSHADOW].rpath = rpath;
    if (rpath != NULL)	{
		gmtl->passes[GFX_RENDERPASS_POINTSHADOW].shader_id =
				rpath->getshader_fn(CMP_OBJTYPE_MODEL, rpath_flags | GFX_RPATH_POINTSHADOW);
    }

    /* mirror/reflection pass */
    rpath = gfx_rpath_detect(CMP_OBJTYPE_MODEL, rpath_flags | GFX_RPATH_MIRROR);
    gmtl->passes[GFX_RENDERPASS_MIRROR].rpath = rpath;
    if (rpath != NULL)	{
		gmtl->passes[GFX_RENDERPASS_MIRROR].shader_id =
				rpath->getshader_fn(CMP_OBJTYPE_MODEL, rpath_flags | GFX_RPATH_MIRROR);
    }

    /* transparent pass */
    rpath = gfx_rpath_detect(CMP_OBJTYPE_MODEL, rpath_flags | GFX_RPATH_ALPHABLEND);
    gmtl->passes[GFX_RENDERPASS_TRANSPARENT].rpath = rpath;
    if (rpath != NULL)	{
		gmtl->passes[GFX_RENDERPASS_TRANSPARENT].shader_id =
				rpath->getshader_fn(CMP_OBJTYPE_MODEL, rpath_flags | GFX_RPATH_ALPHABLEND);
    }

    /* cblock for material: use primary shader for it's creation */
    /* note that cblock of mtl can be NULL, because some render-paths like deferred may not use it*/
    gmtl->cb = gfx_shader_create_cblock(main_alloc, tmp_alloc,
    		gfx_shader_get(gmtl->passes[GFX_RENDERPASS_PRIMARY].shader_id), "cb_mtl", NULL);

    /* if could not find 'cb_mtl' in the shader, try creating it in raw mode (w/o gpu_buffer) */
    if (gmtl->cb == NULL)   {
        static const struct gfx_constant_desc constants[] = {
            {"c_mtl_ambientclr", 0, gfxUniformType::FLOAT4, 16, 1, 16, 0},
            {"c_mtl_diffuseclr", 0, gfxUniformType::FLOAT4, 16, 1, 16, 16},
            {"c_mtl_specularclr", 0, gfxUniformType::FLOAT4, 16, 1, 16, 32},
            {"c_mtl_emissiveclr", 0, gfxUniformType::FLOAT4, 16, 1, 16, 48},
            {"c_mtl_props", 0, gfxUniformType::FLOAT4, 16, 1, 16, 64}
        };
        gmtl->cb = gfx_shader_create_cblockraw(main_alloc, "cb_mtl", constants, 5);
    }

    return gmtl;
}

struct gfx_model_posegpu* model_load_gpupose(struct allocator* alloc,
		struct allocator* tmp_alloc, const struct gfx_model_geo* geo, uint rpath_flags)
{
	ASSERT(geo->skeleton != NULL);

	struct gfx_model_posegpu* gpose = (struct gfx_model_posegpu*)A_ALLOC(alloc,
        sizeof(struct gfx_model_posegpu), MID_GFX);
    ASSERT(gpose);
	memset(gpose, 0x00, sizeof(struct gfx_model_posegpu));

    uint joint_cnt = geo->skeleton->joint_cnt;
	gpose->mats = (struct mat3f*)A_ALIGNED_ALLOC(alloc, sizeof(struct mat3f)*joint_cnt*3, MID_GFX);
	ASSERT(gpose->mats);

	gpose->mat_cnt = geo->skeleton->joint_cnt;
    gpose->offset_mats = gpose->mats + geo->skeleton->joint_cnt;
    gpose->skin_mats = gpose->mats + geo->skeleton->joint_cnt*2;
    gpose->skeleton = geo->skeleton;

	memcpy(gpose->mats, geo->skeleton->init_pose, sizeof(struct mat3f)*geo->skeleton->joint_cnt);
    for (uint i = 0; i < joint_cnt; i++)
        mat3_setm(&gpose->offset_mats[i], &geo->skeleton->joints[i].offset_mat);

    return gpose;
}

void model_destroy_gpumtl(struct allocator* alloc, struct gfx_model_mtlgpu* gmtl)
{
	if (gmtl->cb != NULL)
		gfx_shader_destroy_cblock(gmtl->cb);
	for (uint i = 0; i < GFX_MODEL_MAX_MAPS; i++)    {
		if (gmtl->textures[i] != INVALID_HANDLE)
			rs_unload(gmtl->textures[i]);
	}
}

void gfx_model_updatemtls(struct gfx_model_instance* inst)
{
	struct gfx_model* m = rs_get_model(inst->model);

	for (uint i = 0; i < m->mtl_cnt; i++)	{
		struct gfx_model_mtlgpu* gmtl = inst->mtls[i];
		if (gmtl != NULL && gmtl->cb != NULL)	{
			struct gfx_model_mtl* mtl = &m->mtls[i];
			struct gfx_cblock* cb = gmtl->cb;
            struct color clr;

            /* convert colors to linear-space before sending them to the buffer */
			if (gfx_cb_isvalid(cb, SHADER_NAME(c_mtl_ambientclr)))  {
				gfx_cb_set4f(cb, SHADER_NAME(c_mtl_ambientclr),
                    color_tolinear(&clr, &mtl->ambient)->f);
            }
			if (gfx_cb_isvalid(cb, SHADER_NAME(c_mtl_diffuseclr)))  {
				gfx_cb_set4f(cb, SHADER_NAME(c_mtl_diffuseclr),
                    color_tolinear(&clr, &mtl->diffuse)->f);
            }
			if (gfx_cb_isvalid(cb, SHADER_NAME(c_mtl_specularclr))) {
				gfx_cb_set4f(cb, SHADER_NAME(c_mtl_specularclr),
                    color_muls(&clr, color_tolinear(&clr, &mtl->specular), mtl->spec_intensity)->f);
            }
			if (gfx_cb_isvalid(cb, SHADER_NAME(c_mtl_emissiveclr))) {
				gfx_cb_set4f(cb, SHADER_NAME(c_mtl_emissiveclr),
                    color_tolinear(&clr, &mtl->emissive)->f);
            }
			if (gfx_cb_isvalid(cb, SHADER_NAME(c_mtl_props)))   {
                float props[4] = {mtl->opacity, 0.0f, 0.0f, 0.0f};
                gfx_cb_set4f(cb, SHADER_NAME(c_mtl_props), props);
            }

			gmtl->invalidate_cb = TRUE;
		}
	}

	model_update_uniqueids(inst);
    model_update_alphaflags(inst);
}

void gfx_model_setmtl(gfx_cmdqueue cmdqueue, struct gfx_shader* shader,
		struct gfx_model_instance* inst, uint mtl_id)
{
    gfx_sampler sampler = gfx_get_globalsampler();
    gfx_sampler sampler_low = gfx_get_globalsampler_low();

	struct gfx_model_mtlgpu* gmtl = inst->mtls[mtl_id];
#ifndef _RETAIL_
	if (gmtl != NULL)	{
#endif
        if (gmtl->textures[GFX_MODEL_DIFFUSEMAP] != INVALID_HANDLE)		{
            uint name_hash = SHADER_NAME(s_mtl_diffusemap);
            if (gfx_shader_isvalidtex(shader, name_hash))    {
                gfx_shader_bindsamplertexture(cmdqueue, shader, name_hash, sampler,
                    rs_get_texture(gmtl->textures[GFX_MODEL_DIFFUSEMAP]));
            }
        }

        if (gmtl->textures[GFX_MODEL_NORMALMAP] != INVALID_HANDLE)		{
            uint name_hash = SHADER_NAME(s_mtl_normalmap);
            if (gfx_shader_isvalidtex(shader, name_hash))    {
                gfx_shader_bindsamplertexture(cmdqueue, shader, name_hash, sampler,
                    rs_get_texture(gmtl->textures[GFX_MODEL_NORMALMAP]));
            }
        }

        if (gmtl->textures[GFX_MODEL_ALPHAMAP] != INVALID_HANDLE)		{
            uint name_hash = SHADER_NAME(s_mtl_alphamap);
            if (gfx_shader_isvalidtex(shader, name_hash))    {
                gfx_shader_bindsamplertexture(cmdqueue, shader, name_hash, sampler_low,
                    rs_get_texture(gmtl->textures[GFX_MODEL_ALPHAMAP]));
            }
        }

        if (gmtl->textures[GFX_MODEL_GLOSSMAP] != INVALID_HANDLE)		{
            uint name_hash = SHADER_NAME(s_mtl_ambientmap);
            if (gfx_shader_isvalidtex(shader, name_hash))    {
                gfx_shader_bindsamplertexture(cmdqueue, shader, name_hash, sampler_low,
                    rs_get_texture(gmtl->textures[GFX_MODEL_GLOSSMAP]));
            }
        }

        if (gmtl->textures[GFX_MODEL_EMISSIVEMAP] != INVALID_HANDLE)		{
            uint name_hash = SHADER_NAME(s_mtl_emissivemap);
            if (gfx_shader_isvalidtex(shader, name_hash))    {
                gfx_shader_bindsamplertexture(cmdqueue, shader, name_hash, sampler_low,
                    rs_get_texture(gmtl->textures[GFX_MODEL_EMISSIVEMAP]));
            }
        }

        if (gmtl->textures[GFX_MODEL_REFLECTIONMAP] != INVALID_HANDLE)		{
            uint name_hash = SHADER_NAME(s_mtl_reflectionmap);
            if (gfx_shader_isvalidtex(shader, name_hash))    {
                gfx_shader_bindsamplertexture(cmdqueue, shader, name_hash, sampler_low,
                    rs_get_texture(gmtl->textures[GFX_MODEL_REFLECTIONMAP]));
            }
        }

        if (gmtl->cb != NULL && gmtl->invalidate_cb && gmtl->cb->gpu_buffer != NULL)	{
            gfx_shader_updatecblock(cmdqueue, gmtl->cb);
            gmtl->invalidate_cb = FALSE;
        }

#ifndef _RETAIL_
    }
#endif
}

int model_loadocc(struct gfx_model_occ* occ, file_t f, struct allocator* alloc)
{
    struct h3d_occ h3docc;
    fio_read(f, &h3docc, sizeof(struct h3d_occ), 1);

    strcpy(occ->name, h3docc.name);
    occ->tri_cnt = h3docc.tri_cnt;
    occ->vert_cnt = h3docc.vert_cnt;

    occ->indexes = (uint16*)A_ALLOC(alloc, sizeof(uint16)*h3docc.tri_cnt*3, MID_GFX);
    occ->poss = (struct vec3f*)A_ALIGNED_ALLOC(alloc, sizeof(struct vec3f)*h3docc.vert_cnt, MID_GFX);
    if (occ->indexes == NULL || occ->poss == NULL)  {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return FALSE;
    }

    fio_read(f, occ->indexes, sizeof(uint16), h3docc.tri_cnt*3);
    fio_read(f, occ->poss, sizeof(struct vec3f), h3docc.vert_cnt);
    return TRUE;
}

void model_unloadocc(struct gfx_model_occ* occ, struct allocator* alloc)
{
    if (occ->indexes != NULL)
        A_FREE(alloc, occ->indexes);
    if (occ->poss != NULL)
        A_ALIGNED_FREE(alloc, occ->poss);
}

uint gfx_model_findnode(const struct gfx_model* model, const char* name)
{
    uint namehash = hash_str(name);
    for (uint i = 0, cnt = model->node_cnt; i < cnt; i++) {
        if (namehash == model->nodes[i].name_hash)
            return i;
    }
    return INVALID_INDEX;
}


uint gfx_model_findjoint(const struct gfx_model_skeleton* skeleton, const char* name)
{
    uint namehash = hash_str(name);

    for (uint i = 0, cnt = skeleton->joint_cnt; i < cnt; i++) {
        if (namehash == skeleton->joints[i].name_hash)
            return i;
    }
    return INVALID_INDEX;
}

/* calculate model-view mats and multiply them into their offsets to get skin mats */
void gfx_model_update_skin(struct gfx_model_posegpu* pose)
{
    struct mat3f tmp_mat;
    const struct gfx_model_skeleton* sk = pose->skeleton;

    for (uint i = 0, cnt = pose->mat_cnt; i < cnt; i++)   {
        mat3_setm(&tmp_mat, &pose->mats[i]);
        const struct gfx_model_joint* joint = &sk->joints[i];
        while (joint->parent_id != INVALID_INDEX)   {
            mat3_mul(&tmp_mat, &tmp_mat, &pose->mats[joint->parent_id]);
            joint = &sk->joints[joint->parent_id];
        }

        mat3_mul(&pose->skin_mats[i], &pose->offset_mats[i], &tmp_mat);
    }
}

uint gfx_model_choose_elem_buffidx(enum gfx_input_element_id id, OUT uint* offset)
{
    switch (id) {
    case GFX_INPUTELEMENT_ID_POSITION:
        *offset = offsetof(struct h3d_vertex_base, pos);
        return GFX_MODEL_BUFFER_BASE;
    case GFX_INPUTELEMENT_ID_NORMAL:
        *offset = offsetof(struct h3d_vertex_base, norm);
        return GFX_MODEL_BUFFER_BASE;
    case GFX_INPUTELEMENT_ID_TEXCOORD0:
        *offset = offsetof(struct h3d_vertex_base, coord);
        return GFX_MODEL_BUFFER_BASE;
    case GFX_INPUTELEMENT_ID_BLENDINDEX:
        *offset = offsetof(struct h3d_vertex_skin, indices);
        return GFX_MODEL_BUFFER_SKIN;
    case GFX_INPUTELEMENT_ID_BLENDWEIGHT:
        *offset = offsetof(struct h3d_vertex_skin, weights);
        return GFX_MODEL_BUFFER_SKIN;
    case GFX_INPUTELEMENT_ID_TANGENT:
        *offset = offsetof(struct h3d_vertex_nmap, tangent);
        return GFX_MODEL_BUFFER_SKIN;
    case GFX_INPUTELEMENT_ID_BINORMAL:
        *offset = offsetof(struct h3d_vertex_nmap, binorm);
        return GFX_MODEL_BUFFER_SKIN;
    case GFX_INPUTELEMENT_ID_TEXCOORD1:
        *offset = offsetof(struct h3d_vertex_extra, coord2);
        return GFX_MODEL_BUFFER_EXTRA;
    case GFX_INPUTELEMENT_ID_COLOR:
        *offset = offsetof(struct h3d_vertex_extra, color);
        return GFX_MODEL_BUFFER_EXTRA;
    default:
        ASSERT(0);
        return INVALID_INDEX;
    }
}

size_t gfx_model_choose_vbuff_size(uint idx)
{
    switch (idx)    {
    case GFX_MODEL_BUFFER_BASE:
        return sizeof(struct h3d_vertex_base);
    case GFX_MODEL_BUFFER_SKIN:
        return sizeof(struct h3d_vertex_skin);
    case GFX_MODEL_BUFFER_NMAP:
        return sizeof(struct h3d_vertex_nmap);
    case GFX_MODEL_BUFFER_EXTRA:
        return sizeof(struct h3d_vertex_extra);
    default:
        ASSERT(0);
        return 0;
    }
}