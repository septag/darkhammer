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

#ifndef __CMPMODELLOD_H__
#define __CMPMODELLOD_H__

#include "dhcore/types.h"
#include "../cmp-types.h"

#define CMP_LOD_MODELS_MAX 3

struct cmp_lodmodel
{
    char filepath_hi[128];
    char filepath_md[128];
    char filepath_lo[128];
    char scheme_name[32];
    int exclude_shadows;

    /* internal */
    cmphandle_t models[CMP_LOD_MODELS_MAX]; /* 0:highest detail, N-1:lowest detail */
    uint lod_idxs[CMP_LOD_MODELS_MAX];    /* indexes to 'models' for each lod-level */
    uint scheme_id;
};

/* functions */
ENGINE_API result_t cmp_lodmodel_modify_lo(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);
ENGINE_API result_t cmp_lodmodel_modify_md(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);
ENGINE_API result_t cmp_lodmodel_modify_hi(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);
ENGINE_API result_t cmp_lodmodel_modify_scheme(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);
ENGINE_API result_t cmp_lodmodel_modify_shadows(struct cmp_obj* obj, struct allocator* alloc,
    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);

/* descriptors */
static const struct cmp_value cmp_lodmodel_values[] = {
    {"model_high", CMP_VALUE_STRING, offsetof(struct cmp_lodmodel, filepath_hi), 128, 1,
    cmp_lodmodel_modify_hi, "customdlg; filepicker; filter=*.h3dm;"},
    {"model_medium", CMP_VALUE_STRING, offsetof(struct cmp_lodmodel, filepath_md), 128, 1,
    cmp_lodmodel_modify_md, "customdlg; filepicker; filter=*.h3dm;"},
    {"model_low", CMP_VALUE_STRING, offsetof(struct cmp_lodmodel, filepath_lo), 128, 1,
    cmp_lodmodel_modify_lo, "customdlg; filepicker; filter=*.h3dm;"},
    {"scheme", CMP_VALUE_STRING, offsetof(struct cmp_lodmodel, scheme_name), 32, 1,
    cmp_lodmodel_modify_scheme, ""},
    {"exclude_shadows", CMP_VALUE_BOOL, offsetof(struct cmp_lodmodel, exclude_shadows),
    sizeof(int), 1,  cmp_lodmodel_modify_shadows, ""}
};
static const uint cmp_lodmodel_type = 0x2ac8;

/* register */
result_t cmp_lodmodel_register(struct allocator* alloc);

/* used by scene-mgr */
ENGINE_API int cmp_lodmodel_applylod(cmphandle_t lodmdl_hdl, const struct vec3f* campos);
ENGINE_API int cmp_lodmodel_applylod_shadow(cmphandle_t lodmdl_hdl, const struct vec3f* campos);

#endif /* __CMPMODELLOD_H__ */
