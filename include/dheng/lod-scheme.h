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

#ifndef __LODSCHEME_H__
#define __LODSCHEME_H__

#include "dhcore/types.h"

struct lod_model_scheme
{
    char name[32];
    float high_range;
    float medium_range;
    float low_range;
};

struct lod_light_scheme
{
    char name[32];
    float vis_range;
};

/* api */
void lod_zero();
result_t lod_initmgr();
void lod_releasemgr();

/* model scheme access */
uint lod_findmodelscheme(const char* name);
const struct lod_model_scheme* lod_getmodelscheme(uint id);

/* light scheme access */
uint lod_findlightscheme(const char* name);
const struct lod_light_scheme* lod_getlightscheme(uint id);


#endif /* __LOD-SCHEME_H__ */
