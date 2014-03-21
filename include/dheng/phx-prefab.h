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

#ifndef __PHXPREFAB_H__
#define __PHXPREFAB_H__

#include "dhcore/types.h"
#include "dhcore/vec-math.h"

#include "phx-types.h"

/* opaque type */
struct phx_prefab_data;
typedef struct phx_prefab_data* phx_prefab;

phx_prefab phx_prefab_load(const char* h3dp_filepath, struct allocator* alloc, uint thread_id);
void phx_prefab_unload(phx_prefab prefab);

phx_obj phx_createinstance(phx_prefab prefab, struct xform3d* init_pose);

#endif /* __PHXPREFAB_H__ */
