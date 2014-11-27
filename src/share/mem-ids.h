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

#ifndef __MEMIDS_H__
#define __MEMIDS_H__

#define MID_BASE    0
#define MID_GFX     1   /* graphics (device, renderer, shaders, etc) */
#define MID_RES     2   /* resource manager */
#define MID_GUI		3   /* gui stuff */
#define MID_CMP     4   /* component system */
#define MID_SCN		5   /* scene manager */
#define MID_NET		6   /* networking */
#define MID_PRF		7   /* performnce monitoring and profiler */
#define MID_SCT     8   /* script engines and scripts */
#define MID_DATA    9   /* game/engine loaded data */
#define MID_ANIM    10  /* animation data */
#define MID_PHX     11  /* physics */
#define MID_LSR     12  /* Load-stay-resident (init time only allocations) */

#endif /* __MEMIDS_H__ */
