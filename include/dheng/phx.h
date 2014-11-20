/***********************************************************************************
 * Copyright (c) 2013, Sepehr Taghdisian
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

#ifndef __PHX_H__
#define __PHX_H__

#include "dhcore/json.h"
#include "phx-types.h"
#include "engine-api.h"

struct appInitParams;
struct variant;
struct appPhysicsParams;

/* */
void phx_zero();

void phx_parseparams(struct appPhysicsParams* params, json_t j);
result_t phx_init(const struct appInitParams* params);
void phx_release();

void phx_update_xforms(int simulated);

/* simulation update: returns FALSE if simulation didn't need any update */
int phx_update_sim(float dt);
void phx_wait();

void phx_setactive(uint scene_id);
uint phx_getactive();

/* creates XZ plane on the origin (for debugging purposes) */
ENGINE_API void phx_create_debugplane(float friction, float restitution);

void phx_setgravity_callback(const struct variant* v, void* param);

#endif /* __PHX_H__ */
