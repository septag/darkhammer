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


#ifndef CMP_CAMERA_H_
#define CMP_CAMERA_H_

#include "dhcore/types.h"
#include "dhcore/vec-math.h"
#include "../cmp-types.h"
#include "../camera.h"

enum cmp_camera_type
{
	CMP_CAMERA_FREE = 0,
	CMP_CAMERA_ROLL_CONSTRAINED = 1,
	CMP_CAMERA_ROLLPITCH_CONSTRAINED = 2
};

struct ALIGN16 cmp_camera
{
	uint type;
	float fov;
	float fnear;
	float ffar;
	float pitch_max;
	float pitch_min;
	char bind_path[32];
	int active;

	/* internal */
	Camera c;
	uint path_id;
	struct mat3f view;
	struct mat4f proj;
};

ENGINE_API result_t cmp_camera_modifytype(struct cmp_obj* obj, struct allocator* alloc,
	    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);
ENGINE_API result_t cmp_camera_modifyproj(struct cmp_obj* obj, struct allocator* alloc,
	    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);
ENGINE_API result_t cmp_camera_modifyconstraint(struct cmp_obj* obj, struct allocator* alloc,
	    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);
ENGINE_API result_t cmp_camera_modifybindpath(struct cmp_obj* obj, struct allocator* alloc,
	    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);
ENGINE_API result_t cmp_camera_modifyactive(struct cmp_obj* obj, struct allocator* alloc,
	    struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);

/* descriptors */
static const struct cmp_value cmp_camera_values[] = {
		{"type", CMP_VALUE_UINT, offsetof(struct cmp_camera, type), sizeof(uint), 1,
			cmp_camera_modifytype, "list; items=Free,0,ConstaintRoll,1,ConstaintPitchRoll,2;"},
		{"fov", CMP_VALUE_FLOAT, offsetof(struct cmp_camera, type), sizeof(float), 1,
			cmp_camera_modifyproj, "angle;spinner;min=1;max=90;"},
		{"near_distance", CMP_VALUE_FLOAT, offsetof(struct cmp_camera, fnear), sizeof(float), 1,
			cmp_camera_modifyproj, "spinner;min=0.1;max=100;stride=0.1;"},
		{"far_distance", CMP_VALUE_FLOAT, offsetof(struct cmp_camera, ffar), sizeof(float), 1,
			cmp_camera_modifyproj, "spinner;min=100;max=3000;stride=10;"},
		{"max_pitch", CMP_VALUE_FLOAT, offsetof(struct cmp_camera, pitch_max), sizeof(float), 1,
			cmp_camera_modifyconstraint, "angle;spinner;min=-90;max=90;stride=0.1;"},
		{"min_pitch", CMP_VALUE_FLOAT, offsetof(struct cmp_camera, pitch_min), sizeof(float), 1,
			cmp_camera_modifyconstraint, "angle;spinner;min=-90;max=90;stride=0.1;"},
		{"bind_path", CMP_VALUE_STRING, offsetof(struct cmp_camera, bind_path), 32, 1,
			cmp_camera_modifybindpath, "customdlg; filepicker; filter=*.campath;"},
		{"active", CMP_VALUE_BOOL, offsetof(struct cmp_camera, active), sizeof(int), 1,
			cmp_camera_modifyactive, 0}
};
static const cmptype_t cmp_camera_type = 0x8b72;

result_t cmp_camera_register(struct allocator* alloc);


#endif /* CMP_CAMERA_H_ */
