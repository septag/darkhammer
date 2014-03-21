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

#ifndef __GFXFONT_H__
#define __GFXFONT_H__

#include "dhcore/types.h"
#include "engine-api.h"
#include "dhcore/hash-table.h"

/* font handle */
typedef reshandle_t fonthandle_t;

/* register font flags */
enum gfx_font_flags
{
    GFX_FONT_NORMAL = 0,
    GFX_FONT_BOLD,
    GFX_FONT_ITALIC,
    GFX_FONT_UNDERLINE
};

/* structures */
struct gfx_font_kerning
{
    uint second_char;
    float amount;
};

struct gfx_font_chardesc
{
    uint char_id;
    float x;
    float y;
    float width;
    float height;
    float xoffset;
    float yoffset;
    float xadvance;
    uint16 kern_cnt;
    uint kern_idx;
};

enum gfx_font_metaflags
{
    GFX_FONTMETA_NONE = 0,
    GFX_FONTMETA_HASRIGHT = (1<<0),
    GFX_FONTMETA_HASLEFT = (1<<1),
    GFX_FONTMETA_HASMID = (1<<2)
};

struct gfx_font_metadesc
{
    uint flags;
    uint char_id;
    uint16 right_id;
    uint16 left_id;
    uint16 middle_id;
};

struct gfx_font
{
    char name[32];
    reshandle_t tex_hdl;
    uint char_cnt;
    uint kern_cnt;
    uint16 line_height;
    uint16 base_value;
    uint fsize;
    uint flags;
    uint char_width;	/* for fixed-width fonts */
    struct gfx_font_chardesc* chars;
    struct gfx_font_kerning* kerns;
    struct gfx_font_metadesc* meta_rules;
    uint meta_cnt;
    struct hashtable_fixed char_table;
};

void gfx_font_zero();
result_t gfx_font_initmgr();
void gfx_font_releasemgr();

ENGINE_API fonthandle_t gfx_font_register(struct allocator* alloc,
                                          const char* fnt_filepath,
                                          const char* lang_filepath,
                                          const char* name,
                                          uint font_size, uint flags);
ENGINE_API fonthandle_t gfx_font_geth(const char* name, uint size, uint flags);
ENGINE_API const struct gfx_font* gfx_font_getf(fonthandle_t fhdl);

const wchar* gfx_font_resolveunicode(const struct gfx_font* f, const wchar* intext, wchar* outtext,
    uint text_len);

#endif /* GFXFONT_H */
