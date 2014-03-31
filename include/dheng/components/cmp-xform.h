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

#ifndef CMP_XFORM_H_
#define CMP_XFORM_H_

#include "dhcore/types.h"
#include "dhcore/vec-math.h"
#include "../cmp-types.h"

struct ALIGN16 cmp_xform
{
	struct mat3f mat;		/* local transform */
	struct vec4f vel_lin;	/* linear velocity */
	struct vec4f vel_ang;	/* angular velocity */
	struct mat3f ws_mat;	/* world-space transform */
	cmphandle_t parent_hdl;	/* handle to parent xform component */
};


ENGINE_API result_t cmp_xform_modify(struct cmp_obj* obj, struct allocator* alloc,
	struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);

/* descriptors */
static const struct cmp_value cmp_xform_values[] = {
	{"transform", CMP_VALUE_MATRIX, offsetof(struct cmp_xform, mat), sizeof(struct mat3f),
			1, cmp_xform_modify, "transform;gizmo;"}
};
static const uint16 cmp_xform_type = 0x7887;

result_t cmp_xform_register(struct allocator* alloc);
void cmp_xform_updatedeps(struct cmp_obj* obj, uint flags);

ENGINE_API void cmp_xform_setm(struct cmp_obj* obj, const struct mat3f* mat);
ENGINE_API void cmp_xform_setpos(struct cmp_obj* obj, const struct vec3f* pos);
ENGINE_API void cmp_xform_setposf(struct cmp_obj* obj, float x, float y, float z);
ENGINE_API void cmp_xform_setrot_deg(struct cmp_obj* obj, float rx_deg, float ry_deg, float rz_deg);
ENGINE_API void cmp_xform_setrot(struct cmp_obj* obj, float rx, float ry, float rz);
ENGINE_API void cmp_xform_setrot_quat(struct cmp_obj* obj, const struct quat4f* q);
ENGINE_API struct vec3f* cmp_xform_getpos(struct cmp_obj* obj, OUT struct vec3f* pos);
ENGINE_API struct quat4f* cmp_xform_getrot(struct cmp_obj* obj, OUT struct quat4f* q);

#endif /* CMP_XFORM_H_ */
