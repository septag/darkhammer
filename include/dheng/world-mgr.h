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

#ifndef __WORLDMGR_H__
#define __WORLDMGR_H__

#include "dhcore/types.h"
#include "dhcore/variant.h"
#include "engine-api.h"

struct Camera;
struct cmp_obj;

/* callback for changing world vars (can be optionally assigned) */
typedef void (*pfn_wld_varchanged)(const struct variant* v, void* param);

/* */
void wld_zero();
result_t wld_initmgr();
void wld_releasemgr();

ENGINE_API uint wld_register_section(const char* name);
ENGINE_API uint wld_register_var(uint section_id, const char* name, enum variant_type type,
    OPTIONAL pfn_wld_varchanged change_fn, OPTIONAL void* param);
ENGINE_API uint wld_find_section(const char* name);
ENGINE_API uint wld_find_var(uint section_id, const char* name);
ENGINE_API const struct variant* wld_get_var(uint section_id, uint var_id);
ENGINE_API void wld_set_var(uint section_id, uint var_id, const struct variant* var);
ENGINE_API void wld_set_cam(Camera* cam);
ENGINE_API Camera* wld_get_cam();

#endif /* __WORLDMGR_H__ */