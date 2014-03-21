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

#include "dhcore/core.h"
#include "gfx-buffers.h"
#include "gfx-cmdqueue.h"
#include "gfx-device.h"
#include "mem-ids.h"

/*************************************************************************************************
 * ring buffer helper
 */
void gfx_ringbuffer_init(struct gfx_ringbuffer* rbuff, gfx_buffer buff, uint seg_cnt)
{
	ASSERT(buff);
	ASSERT(seg_cnt > 1);

	rbuff->buff = buff;
	rbuff->seg_cnt = seg_cnt;
	rbuff->size = buff->desc.buff.size;
	rbuff->seg_size = rbuff->size / seg_cnt;
	rbuff->offset = 0;
}

void* gfx_ringbuffer_map(gfx_cmdqueue cmdqueue, struct gfx_ringbuffer* rbuff,
		OUT uint* offset, OUT uint* size)
{
	enum gfx_map_mode mode;
	bool_t sync = TRUE;
	if (rbuff->offset == 0)	{
		mode = GFX_MAP_WRITE_DISCARD;
	}	else	{
		mode = GFX_MAP_WRITE_DISCARDRANGE;
#if defined(_GL_)
        sync = FALSE;
#endif
	}

	*size = minun(rbuff->seg_size, rbuff->size - rbuff->offset);
	*offset = rbuff->offset;

	return gfx_buffer_map(cmdqueue, rbuff->buff, *offset, *size, mode, sync);
}

void gfx_ringbuffer_unmap(gfx_cmdqueue cmdqueue, struct gfx_ringbuffer* rbuff,
		uint written_bytes)
{
	gfx_buffer_unmap(cmdqueue, rbuff->buff);

	rbuff->offset += written_bytes;
	rbuff->offset %= rbuff->size;
}

void gfx_ringbuffer_reset(struct gfx_ringbuffer* rbuff)
{
    rbuff->offset = 0;
}


/*************************************************************************************************
 * continous buffer helper
 */
void gfx_contbuffer_init(struct gfx_contbuffer* cbuff, gfx_buffer buff)
{
	cbuff->buff = buff;
	cbuff->offset = 0;
    cbuff->reset = FALSE;
}

void gfx_contbuffer_setoffset(struct gfx_contbuffer* cbuff, uint offset)
{
    if (cbuff->offset == 0)
        cbuff->reset = TRUE;
    cbuff->offset = offset;
}

void* gfx_contbuffer_map(gfx_cmdqueue cmdqueue, struct gfx_contbuffer* cbuff,
		uint size, OUT uint* offset)
{
	uint total_sz = cbuff->buff->desc.buff.size;
	if (total_sz < size)
		return NULL;

	if ((size + cbuff->offset) > total_sz)
		cbuff->offset = 0;

#if defined(_GL_)
    bool_t sync = (cbuff->offset == 0);
#else
    bool_t sync = TRUE;
#endif

	void* p = gfx_buffer_map(cmdqueue, cbuff->buff,
			cbuff->offset, size,
			(cbuff->offset != 0 && !cbuff->reset) ? GFX_MAP_WRITE_DISCARDRANGE : GFX_MAP_WRITE_DISCARD,
            sync);

	*offset = cbuff->offset;
	cbuff->offset += size;
    cbuff->reset = FALSE;
	return p;
}

void gfx_contbuffer_unmap(gfx_cmdqueue cmdqueue, struct gfx_contbuffer* cbuff)
{
	gfx_buffer_unmap(cmdqueue, cbuff->buff);
}

/*************************************************************************************************/
struct gfx_sharedbuffer* gfx_sharedbuffer_create(uint size)
{
    gfx_buffer buff = gfx_create_buffer(GFX_BUFFER_CONSTANT, GFX_MEMHINT_DYNAMIC, size, NULL, 0);
    if (buff == NULL)   {
        err_print(__FILE__, __LINE__, "gfx: creating uniform buffer failed");
        return NULL;
    }

    struct gfx_sharedbuffer* ubuff = (struct gfx_sharedbuffer*)
        ALLOC(sizeof(struct gfx_sharedbuffer), MID_GFX);
    if (ubuff == NULL)  {
        err_print(__FILE__, __LINE__, "gfx: creating uniform buffer failed");
        return NULL;
    }

    memset(ubuff, 0x00, sizeof(struct gfx_sharedbuffer));
    ubuff->gpu_buff = buff;
    ubuff->size = size;
    ubuff->alignment = buff->desc.buff.alignment;

    return ubuff;
}

void gfx_sharedbuffer_destroy(struct gfx_sharedbuffer* ubuff)
{
    if (ubuff->gpu_buff != NULL)
        gfx_destroy_buffer(ubuff->gpu_buff);

    FREE(ubuff);
}

sharedbuffer_pos_t gfx_sharedbuffer_write(struct gfx_sharedbuffer* ubuff, gfx_cmdqueue cmdqueue,
                                          const void* data, uint sz)
{
    ASSERT(ubuff->offset % ubuff->alignment == 0);
    ASSERT(ubuff->offset + sz < ubuff->size);

    /* map gpu buffer and write data to it
     * if offset == 0: orphan buffer
     * else: write to buffer in unsync mode
     */
    uint offset = ubuff->offset;
    uint map_mode;
    bool_t sync;

    if (offset > 0) {
        map_mode = GFX_MAP_WRITE_DISCARDRANGE;
        sync = FALSE;
    }   else    {
        map_mode = GFX_MAP_WRITE_DISCARD;
        sync = TRUE;
    }

    void* mapped = gfx_buffer_map(cmdqueue, ubuff->gpu_buff, offset, sz, map_mode, sync);
    ASSERT(mapped);
    memcpy(mapped, data, sz);
    gfx_buffer_unmap(cmdqueue, ubuff->gpu_buff);

    sharedbuffer_pos_t pos = (sharedbuffer_pos_t)((((uint64)offset) << 32) | ((uint64)sz));

    /* progress offset respecting alignment */
    offset += sz;
    uint misalign = offset & (ubuff->alignment - 1);
    uint8 adjust = ubuff->alignment - (uint8)misalign;
    offset += adjust;
    ubuff->offset = offset;

    return pos;
}

void gfx_sharedbuffer_reset(struct gfx_sharedbuffer* ubuff)
{
    ubuff->offset = 0;
}
