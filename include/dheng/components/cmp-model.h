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

#ifndef CMP_MODEL_H_
#define CMP_MODEL_H_

#include "dhcore/types.h"
#include "../cmp-types.h"

#define CMP_MESH_XFORM_MAX	8

/* fwd */
struct gfx_model_instance;

enum CMP_MODEL_FLAGS
{
    CMP_MODELFLAG_ISLOD = (1<<0),   /* model is owned by LOD ? */
    CMP_MODELFLAG_NOBOUNDUPDATE = (1<<1)
};

/* */
struct cmp_model
{
    /* interface */
	char filepath[128];
	int exclude_shadows;

	/* internal */
	reshandle_t model_hdl;
	uint xform_cnt;
	cmphandle_t xforms[CMP_MESH_XFORM_MAX];
	uint filepath_hash;
	struct gfx_model_instance* model_inst; /* instance data is created for each model */
    uint flags;   /* enum CMP_MODEL_FLAGS combination */
};

ENGINE_API result_t cmp_model_modify(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);

/* descriptors */
static const struct cmp_value cmp_model_values[] = {
    {"filepath", CMP_VALUE_STRING, offsetof(struct cmp_model, filepath), 128, 1,
    cmp_model_modify, "customdlg; filepicker; filter=*.h3dm;"},
    {"exclude_shadows", CMP_VALUE_BOOL, offsetof(struct cmp_model, exclude_shadows), sizeof(int),
    1,  NULL, ""}
};
static const uint16 cmp_model_type = 0x4e9b;

result_t cmp_model_register(struct allocator* alloc);

/* callback: reloading from res-mgr */
void cmp_model_reload(const char* filepath, reshandle_t hdl, int manual);

void cmp_model_debug(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
    const struct gfx_view_params* params);

ENGINE_API cmphandle_t cmp_model_findnode(cmphandle_t model_hdl, uint name_hash);

#endif /* CMP_MODEL_H_ */
