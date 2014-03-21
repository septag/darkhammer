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


#ifndef CMP_BOUNDS_H_
#define CMP_BOUNDS_H_

#include "dhcore/types.h"
#include "dhcore/prims.h"
#include "dhcore/linked-list.h"
#include "../cmp-types.h"

/* */
struct ALIGN16 cmp_bounds
{
	struct sphere s;
	struct sphere ws_s;
	struct aabb ws_aabb;
	struct linked_list* cell_list;  /* item-data: scn_grid_item */
};

ENGINE_API result_t cmp_bounds_modify(struct cmp_obj* obj, struct allocator* alloc,
	    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);

/* descriptors */
static const struct cmp_value cmp_bounds_values[] = {
	{"sphere", CMP_VALUE_FLOAT4, offsetof(struct cmp_bounds, s), sizeof(struct sphere), 1,
		cmp_bounds_modify, ""}
};
static const uint16 cmp_bounds_type = 0x8bbd;

result_t cmp_bounds_register(struct allocator* alloc);

ENGINE_API struct vec4f* cmp_bounds_getfarcorner(struct vec4f* r, cmphandle_t bounds_hdl,
		const struct vec4f* cam_pos);

#endif /* CMP_BOUNDS_H_ */
