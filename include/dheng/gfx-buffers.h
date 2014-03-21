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

#ifndef __GFXBUFFERS_H__
#define __GFXBUFFERS_H__

#include "gfx-types.h"
#include "dhcore/array.h"
#include "dhcore/pool-alloc.h"

/**
 *  Ringbuffer and related functions
 *  Ringbuffer is a normal buffer but divided into segments
 *  And every map command can wrap around the buffer
 */
struct gfx_ringbuffer
{
	gfx_buffer buff;
	uint seg_cnt;
	uint size;
	uint seg_size;
	uint offset;
};

void gfx_ringbuffer_init(struct gfx_ringbuffer* rbuff, gfx_buffer buff, uint seg_cnt);
void* gfx_ringbuffer_map(gfx_cmdqueue cmdqueue, struct gfx_ringbuffer* rbuff,
		OUT uint* offset, OUT uint* size);
void gfx_ringbuffer_unmap(gfx_cmdqueue cmdqueue, struct gfx_ringbuffer* rbuff,
		uint written_bytes);
void gfx_ringbuffer_reset(struct gfx_ringbuffer* rbuff);


/**
 * continous and related functions
 * continous buffer is mapped sequentially
 */
struct gfx_contbuffer
{
	gfx_buffer buff;
	uint offset;
    bool_t reset;   /* if we manually change offset from zero, we have to know to DISCARD it */
};

void gfx_contbuffer_init(struct gfx_contbuffer* cbuff, gfx_buffer buff);
void gfx_contbuffer_setoffset(struct gfx_contbuffer* cbuff, uint offset);
void* gfx_contbuffer_map(gfx_cmdqueue cmdqueue, struct gfx_contbuffer* cbuff,
		uint size, OUT uint* offset);
void gfx_contbuffer_unmap(gfx_cmdqueue cmdqueue, struct gfx_contbuffer* cbuff);

/**
 *
 */
struct gfx_sharedbuffer
{
    gfx_buffer gpu_buff;
    uint size;
    uint offset;
    uint alignment;
};

typedef uint64 sharedbuffer_pos_t;

#define GFX_SHAREDBUFFER_OFFSET(pos) (uint)(((pos)>>32) & 0xffffffff)
#define GFX_SHAREDBUFFER_SIZE(pos) (uint)((pos)&0xffffffff)

struct gfx_sharedbuffer* gfx_sharedbuffer_create(uint size);
void gfx_sharedbuffer_destroy(struct gfx_sharedbuffer* ubuff);
sharedbuffer_pos_t gfx_sharedbuffer_write(struct gfx_sharedbuffer* ubuff, gfx_cmdqueue cmdqueue,
                                          const void* data, uint sz);
void gfx_sharedbuffer_reset(struct gfx_sharedbuffer* ubuff);

#endif /* __GFXBUFFERS_H__ */