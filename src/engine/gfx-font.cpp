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

#include "gfx-font.h"
#include "dhcore/core.h"
#include "dhcore/array.h"
#include "dhcore/file-io.h"
#include "dhcore/task-mgr.h"

#include "mem-ids.h"
#include "res-mgr.h"
#include "gfx-texture.h"
#include "engine.h"
#include "dhcore/json.h"
#include <wchar.h>

struct font_entity
{
    char name[32];
    uint size;
    uint flags;
    struct gfx_font* f;
    struct allocator* alloc;
};

struct font_mgr
{
    struct array fonts; /* items: font_entity */
};

/* fnt file format */
#define FNT_SIGN     "BMF"

#pragma pack(push, 1)
struct _GCCPACKED_ fnt_info
{
    int16 font_size;
    int8 bit_field;
    uint8 charset;
    uint16 stretch_h;
    uint8 aa;
    uint8 padding_up;
    uint8 padding_right;
    uint8 padding_dwn;
    uint8 padding_left;
    uint8 spacing_horz;
    uint8 spacing_vert;
    uint8 outline;
    /* name (str) */
};

struct _GCCPACKED_ fnt_common
{
    uint16 line_height;
    uint16 base;
    uint16 scale_w;
    uint16 scale_h;
    uint16 pages;
    int8 bit_field;
    uint8 alpha_channel;
    uint8 red_channel;
    uint8 green_channel;
    uint8 blue_channel;
};

struct _GCCPACKED_ fnt_page
{
    char path[DH_PATH_MAX];
};

struct _GCCPACKED_ fnt_char
{
    uint id;
    int16 x;
    int16 y;
    int16 width;
    int16 height;
    int16 xoffset;
    int16 yoffset;
    int16 xadvance;
    uint8 page;
    uint8 channel;
};

struct _GCCPACKED_ fnt_kernpair
{
    uint first;
    uint second;
    int16 amount;
};

struct _GCCPACKED_ fnt_block
{
    uint8 id;
    uint size;
};

#pragma pack(pop)

/* globals */
static struct font_mgr g_fontmgr;

/* fwd */
result_t load_font(struct allocator* alloc, struct gfx_font* font, file_t f);
void destroy_font(struct allocator* alloc, struct gfx_font* font);
result_t load_lang(struct allocator* alloc, struct gfx_font* font, file_t f);
void unload_font(struct allocator* alloc, struct gfx_font* font);

/* inlines */
INLINE uint find_metadata(const struct gfx_font* font, uint16 ch_id)
{
    for (uint i = 0, count = font->meta_cnt; i < count; i++)        {
        if (font->meta_rules[i].char_id == ch_id)
        	return i;
    }
    return INVALID_INDEX;
}

/*  */
void gfx_font_zero()
{
    memset(&g_fontmgr, 0x00, sizeof(struct font_mgr));
}

result_t gfx_font_initmgr()
{
    gfx_font_zero();
    return arr_create(mem_heap(), &g_fontmgr.fonts, sizeof(struct font_entity), 10, 10, MID_GFX);
}

void gfx_font_releasemgr()
{
    /* destroy all registered fonts */
    struct font_entity* fe = (struct font_entity*)g_fontmgr.fonts.buffer;
    for (int i = 0; i < g_fontmgr.fonts.item_cnt; i++)   {
        unload_font(fe[i].alloc, fe[i].f);
        A_FREE(fe[i].alloc, fe[i].f);
    }

    arr_destroy(&g_fontmgr.fonts);
    gfx_font_zero();
}

fonthandle_t gfx_font_register(struct allocator* alloc,
                               const char* fnt_filepath,
                               const char* lang_filepath,
                               const char* name,
                               uint font_size, uint flags)
{
    /* first search in existing fonts and see if have similiar font */
    fonthandle_t cur_font = gfx_font_geth(name, font_size, flags);
    if (cur_font != INVALID_HANDLE)
        return cur_font;

    struct allocator* tmp_alloc = tsk_get_tmpalloc(0);
    A_SAVE(tmp_alloc);

    file_t f = fio_openmem(tmp_alloc, fnt_filepath, FALSE, MID_GFX);
    if (f == NULL)   {
        err_printf(__FILE__, __LINE__, "register font '%s' failed: file does not exist",
            fnt_filepath);
        A_LOAD(tmp_alloc);
        return INVALID_HANDLE;
    }

    /* load the font */
    struct gfx_font* font = (struct gfx_font*)A_ALLOC(alloc, sizeof(struct gfx_font), MID_GFX);
    ASSERT(font);

    result_t r = load_font(alloc, font, f);
    fio_close(f);

    if (IS_FAIL(r))     {
        err_printf(__FILE__, __LINE__, "register font '%s' failed: invalid fnt file",
            fnt_filepath);
        A_FREE(alloc, font);
        A_LOAD(tmp_alloc);
        return INVALID_HANDLE;
    }

    /* load language file */
    if (lang_filepath != NULL)  {
        f = fio_openmem(tmp_alloc, lang_filepath, FALSE, MID_GFX);
        if (f == NULL) {
            err_printf(__FILE__, __LINE__, "register font '%s' failed: lang file '%s' does not exist",
                fnt_filepath, lang_filepath);
            unload_font(alloc, font);
            A_FREE(alloc, font);
            A_LOAD(tmp_alloc);
            return INVALID_HANDLE;
        }

        r = load_lang(alloc, font, f);
        fio_close(f);
        if (IS_FAIL(r)) {
            err_printf(__FILE__, __LINE__, "register font '%s' failed: invalid lang file",
                fnt_filepath);
            unload_font(alloc, font);
            A_FREE(alloc, font);
            A_LOAD(tmp_alloc);
            return INVALID_HANDLE;
        }
    }

    /* add to fonts database */
    struct font_entity* fe = (struct font_entity*)arr_add(&g_fontmgr.fonts);
    fe->alloc = alloc;
    fe->f = font;
    fe->flags = flags;
    strcpy(fe->name, name);
    fe->size = font_size;

    log_printf(LOG_LOAD, "(font) \"%s\" - name: %s, size: %d", fnt_filepath, name, font_size);
    A_LOAD(tmp_alloc);

    return (g_fontmgr.fonts.item_cnt - 1);
}

fonthandle_t gfx_font_geth(const char* name, uint size, uint flags)
{
    struct font_entity* fonts = (struct font_entity*)g_fontmgr.fonts.buffer;
    for (int i = 0; i < g_fontmgr.fonts.item_cnt; i++)   {
        if (str_isequal(fonts[i].name, name) &&
            fonts[i].flags == flags &&
            fonts[i].size == size)
        {
            return i;
        }
    }
    return INVALID_HANDLE;
}

const struct gfx_font* gfx_font_getf(fonthandle_t fhdl)
{
    ASSERT(fhdl < g_fontmgr.fonts.item_cnt);
    struct font_entity* fonts = (struct font_entity*)g_fontmgr.fonts.buffer;
    return fonts[fhdl].f;
}

result_t load_font(struct allocator* alloc, struct gfx_font* font, file_t f)
{
    memset(font, 0x00, sizeof(struct gfx_font));
    font->tex_hdl = INVALID_HANDLE;

    /* sign */
    char sign[4];
    fio_read(f, sign, 3, 1);
    sign[3] = 0;
    if (!str_isequal(FNT_SIGN, sign))    {
        return RET_FAIL;
    }

    /* file version */
    uint8 file_ver;
    fio_read(f, &file_ver, sizeof(file_ver), 1);
    if (file_ver != 3)
        return RET_FAIL;

    /* info */
    struct fnt_block block;
    struct fnt_info info;
    char font_name[256];
    fio_read(f, &block, sizeof(block), 1);
    fio_read(f, &info, sizeof(info), 1);
    fio_read(f, font_name, block.size - sizeof(info), 1);
    font->fsize = info.font_size;
    strcpy(font->name, font_name);

    /* common */
    struct fnt_common common;
    fio_read(f, &block, sizeof(block), 1);
    fio_read(f, &common, sizeof(common), 1);

    if (common.pages != 1)
        return RET_FAIL;

    font->line_height = common.line_height;
    font->base_value = common.base;

    /* pages */
    struct fnt_page page;
    char texpath[DH_PATH_MAX];

    fio_read(f, &block, sizeof(block), 1);
    fio_read(f, page.path, block.size, 1);
    path_getdir(texpath, fio_getpath(f));
    path_join(texpath, texpath, page.path, NULL);
    path_tounix(texpath, texpath);
    font->tex_hdl = rs_load_texture(texpath, 0, FALSE, 0);
    if (font->tex_hdl == INVALID_HANDLE)
        return RET_FAIL;

    /* chars */
    struct fnt_char ch;
    fio_read(f, &block, sizeof(block), 1);
    uint char_cnt = block.size / sizeof(ch);
    font->char_cnt = char_cnt;

    /* hash table */
    if (IS_FAIL(hashtable_fixed_create(alloc, &font->char_table, char_cnt, MID_GFX)))
        return RET_FAIL;

    font->chars = (struct gfx_font_chardesc*)A_ALLOC(alloc,
        sizeof(struct gfx_font_chardesc)*char_cnt, MID_GFX);
    ASSERT(font->chars);
    memset(font->chars, 0x00, sizeof(struct gfx_font_chardesc)*char_cnt);

    uint cw_max = 0;
    for (uint i = 0; i < char_cnt; i++)   {
        fio_read(f, &ch, sizeof(ch), 1);
        font->chars[i].char_id = ch.id;
        font->chars[i].width = (float)ch.width;
        font->chars[i].height = (float)ch.height;
        font->chars[i].x = (float)ch.x;
        font->chars[i].y = (float)ch.y;
        font->chars[i].xadvance = (float)ch.xadvance;
        font->chars[i].xoffset = (float)ch.xoffset;
        font->chars[i].yoffset = (float)ch.yoffset;

        if (cw_max < (uint)ch.xadvance)
        	cw_max = ch.width;

        hashtable_fixed_add(&font->char_table, ch.id, i);
    }

    if (char_cnt > 0)
    	font->char_width = (uint)font->chars[0].xadvance;

    /* kerning */
    struct fnt_kernpair kern;
    size_t last_r = fio_read(f, &block, sizeof(block), 1);
    uint kern_cnt = block.size / sizeof(kern);
    if (kern_cnt > 0 && last_r > 0)   {
        font->kerns = (struct gfx_font_kerning*)A_ALLOC(alloc,
            sizeof(struct gfx_font_kerning)*kern_cnt, MID_GFX);
        ASSERT(font->kerns);
        memset(font->kerns, 0x00, sizeof(struct gfx_font_kerning)*kern_cnt);

        for (uint i = 0; i < kern_cnt; i++)   {
            fio_read(f, &kern, sizeof(kern), 1);

            /* find char id and set it's kerning */
            uint id = kern.first;
            for (uint k = 0; k < char_cnt; k++)   {
                if (id == font->chars[k].char_id)   {
                    if (font->chars[k].kern_cnt == 0)
                        font->chars[k].kern_idx = i;
                    font->chars[k].kern_cnt ++;
                    break;
                }
            }

            font->kerns[i].second_char = kern.second;
            font->kerns[i].amount = (float)kern.amount;
        }
    }

    return RET_OK;
}

void unload_font(struct allocator* alloc, struct gfx_font* font)
{
    ASSERT(font);

    hashtable_fixed_destroy(&font->char_table);

    if (font->tex_hdl != INVALID_HANDLE)
        rs_unload(font->tex_hdl);

    if (font->chars != NULL)
        A_FREE(alloc, font->chars);

    if (font->kerns != NULL)
        A_FREE(alloc, font->kerns);

    if (font->meta_rules != NULL)
        A_FREE(alloc, font->meta_rules);

    memset(font, 0x00, sizeof(struct gfx_font));
}

result_t load_lang(struct allocator* alloc, struct gfx_font* font, file_t f)
{
    json_t j = json_parsefilef(f, tsk_get_tmpalloc(0));
    if (j == NULL)
        return RET_FAIL;

    json_t meta_rules = json_getitem(j, "meta-rules");
    if (meta_rules == NULL)    {
        json_destroy(j);
        return RET_FAIL;
    }

    uint meta_cnt = json_getarr_count(meta_rules);
    if (meta_cnt > 0)   {
        font->meta_cnt = meta_cnt;
        font->meta_rules = (struct gfx_font_metadesc*)A_ALLOC(alloc,
            sizeof(struct gfx_font_metadesc)*meta_cnt, MID_GFX);
        ASSERT(font->meta_rules);

        memset(font->meta_rules, 0x00, sizeof(struct gfx_font_metadesc)*meta_cnt);
        for (uint i = 0; i < meta_cnt; i++)   {
            json_t m = json_getarr_item(meta_rules, i);
            uint flags = 0;

            font->meta_rules[i].char_id = json_geti_child(m, "code", INVALID_INDEX);

            json_t left = json_getitem(m, "left");
            if (left != NULL)  {
                font->meta_rules[i].left_id = json_geti(left);
                BIT_ADD(flags, GFX_FONTMETA_HASLEFT);
            }

            json_t right = json_getitem(m, "right");
            if (right != NULL) {
                font->meta_rules[i].right_id = json_geti(right);
                BIT_ADD(flags, GFX_FONTMETA_HASRIGHT);
            }

            json_t mid = json_getitem(m, "middle");
            if (mid != NULL)   {
                font->meta_rules[i].middle_id = json_geti(mid);
                BIT_ADD(flags, GFX_FONTMETA_HASMID);
            }

            font->meta_rules[i].flags = flags;
        }
    }

    json_destroy(j);
    return RET_OK;
}

const wchar* gfx_font_resolveunicode(const struct gfx_font* f, const wchar* intext, wchar* outtext,
                                     uint text_len)
{
    wchar output[512];

    for (uint i = 0; i < text_len; i++)    {
        uint16 ch = intext[i];
        uint16 left = (i > 0) ? intext[i-1] : 0;
        uint16 right = intext[i+1];

        output[i] = ch;
        uint ch_meta = find_metadata(f, ch);

        if (ch_meta != INVALID_INDEX)    {
            uint8 ch_flags = f->meta_rules[ch_meta].flags;
            int ch_left = FALSE;
            int ch_right = FALSE;

            /* check if we have 'left' glyph for current character
             * then look for the 'right-side', if right-side character and
             * see if we can put 'left-side' glyph instead of this one */
            if (BIT_CHECK(ch_flags, GFX_FONTMETA_HASLEFT))    {
                uint right_idx = find_metadata(f, right);
                if (right_idx != INVALID_INDEX &&
                    (BIT_CHECK(f->meta_rules[right_idx].flags, GFX_FONTMETA_HASRIGHT) ||
                    BIT_CHECK(f->meta_rules[right_idx].flags, GFX_FONTMETA_HASMID)))
                {
                    output[i] = f->meta_rules[ch_meta].left_id;
                    ch_left = TRUE;
                }
            }

            if (BIT_CHECK(ch_flags, GFX_FONTMETA_HASRIGHT))    {
                uint left_idx = find_metadata(f, left);
                if (left_idx != INVALID_INDEX &&
                    (BIT_CHECK(f->meta_rules[left_idx].flags, GFX_FONTMETA_HASLEFT) ||
                    BIT_CHECK(f->meta_rules[left_idx].flags, GFX_FONTMETA_HASMID)))
                {
                    output[i] = f->meta_rules[ch_meta].right_id;
                    ch_right = TRUE;
                }
            }

            if (BIT_CHECK(ch_flags, GFX_FONTMETA_HASMID) && ch_left && ch_right)        {
                output[i] = f->meta_rules[ch_meta].middle_id;
            }
        }
    }

    output[text_len] = 0;
    wcscpy(outtext, output);
    return outtext;
}

