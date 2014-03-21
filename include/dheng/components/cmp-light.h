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


#ifndef CMP_LIGHT_H_
#define CMP_LIGHT_H_

#include "dhcore/types.h"
#include "dhcore/vec-math.h"
#include "dhcore/color.h"
#include "../cmp-types.h"

enum cmp_light_type
{
	CMP_LIGHT_POINT = 2,
	CMP_LIGHT_SPOT = 3
};

struct ALIGN16 cmp_light
{
    /* interface */
	uint type;
	struct color color;
	float intensity;
	float atten_near;
	float atten_far;
	float atten_narrow;
	float atten_wide;
    char lod_scheme_name[32];

	/* internal */
	struct vec4f pos;
	struct vec4f dir;
	struct color color_lin;
    uint scheme_id;
};

ENGINE_API result_t cmp_light_modifytype(struct cmp_obj* obj, struct allocator* alloc,
	    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);
ENGINE_API result_t cmp_light_modifycolor(struct cmp_obj* obj, struct allocator* alloc,
	    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);
ENGINE_API result_t cmp_light_modifyatten(struct cmp_obj* obj, struct allocator* alloc,
	    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);
ENGINE_API result_t cmp_light_modifylod(struct cmp_obj* obj, struct allocator* alloc,
        struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);

/* descriptors */
static const struct cmp_value cmp_light_values[] = {
	{"type", CMP_VALUE_UINT, offsetof(struct cmp_light, type), sizeof(uint), 1,
		cmp_light_modifytype, "list; items=Point,2,Spot,3;"},
	{"color", CMP_VALUE_FLOAT4, offsetof(struct cmp_light, color), sizeof(struct color), 1,
		cmp_light_modifycolor, "colorpicker;"},
	{"intensity", CMP_VALUE_FLOAT, offsetof(struct cmp_light, intensity), sizeof(float), 1,
			NULL, "spinner;min=0;max=10;stride=0.1;"},
	{"atten_near", CMP_VALUE_FLOAT, offsetof(struct cmp_light, atten_near), sizeof(float), 1,
			cmp_light_modifyatten, "spinner;min=0;max=100;stride=0.5;"},
	{"atten_far", CMP_VALUE_FLOAT, offsetof(struct cmp_light, atten_far), sizeof(float), 1,
			cmp_light_modifyatten, "spinner;min=0;max=100;stride=0.5;"},
	{"atten_narrow", CMP_VALUE_FLOAT, offsetof(struct cmp_light, atten_narrow), sizeof(float), 1,
			cmp_light_modifyatten, "angle;spinner;min=0;max=100;stride=1;"},
	{"atten_wide", CMP_VALUE_FLOAT, offsetof(struct cmp_light, atten_wide), sizeof(float), 1,
			cmp_light_modifyatten, "angle;spinner;min=0;max=100;stride=1;"},
    {"lod_scheme", CMP_VALUE_STRING, offsetof(struct cmp_light, lod_scheme_name), 32, 1,
        cmp_light_modifylod, ""}
};
static const cmptype_t cmp_light_type = 0x4e0e;

result_t cmp_light_register(struct allocator* alloc);

/* used by scene-mgr */
ENGINE_API bool_t cmp_light_applylod(cmphandle_t light_hdl, const struct vec3f* campos,
                                     OUT float* intensity);

#endif /* CMP_LIGHT_H_ */
