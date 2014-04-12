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

#include "app.h"

#include "dhcore/core.h"
#include "dhcore/json.h"
#include "dhcore/util.h"

#define PHX_DEFAULT_SUBSTEPS 2

/* fwd */
void gfx_parseparams(struct gfx_params* params, json_t j);
void phx_parseparams(struct phx_params* params, json_t j);
void sct_parseparams(struct sct_params* params, json_t j);

/* */
struct init_params* app_config_load(const char* cfg_jsonfile)
{
    struct init_params* params = (struct init_params*)ALLOC(sizeof(struct init_params), 0);
    ASSERT(params);
    memset(params, 0x00, sizeof(struct init_params));

    char* buffer = util_readtextfile(cfg_jsonfile, mem_heap());
    if (buffer == NULL) {
        err_printf(__FILE__, __LINE__, "loading confing file '%s' failed", cfg_jsonfile);
        FREE(params);
        return NULL;
    }
    json_t root = json_parsestring(buffer);
    FREE(buffer);

    if (root == NULL)  {
        err_printf(__FILE__, __LINE__, "parsing confing file '%s' failed: invalid json",
            cfg_jsonfile);
        FREE(params);
        return NULL;
    }

    /* fill paramters from json data */
    /* general */
    json_t general = json_getitem(root, "general");
    if (general != NULL)   {
        if (json_getb_child(general, "debug", FALSE))
            BIT_ADD(params->flags, ENG_FLAG_DEBUG);
        if (json_getb_child(general, "dev", FALSE))
            BIT_ADD(params->flags, ENG_FLAG_DEV);
        if (json_getb_child(general, "console", FALSE))
            BIT_ADD(params->flags, ENG_FLAG_CONSOLE);
        if (json_getb_child(general, "no-physics", FALSE))
            BIT_ADD(params->flags, ENG_FLAG_DISABLEPHX);
        if (json_getb_child(general, "optimize-mem", FALSE))
            BIT_ADD(params->flags, ENG_FLAG_OPTIMIZEMEMORY);
        if (json_getb_child(general, "no-bgload", FALSE))
            BIT_ADD(params->flags, ENG_FLAG_DISABLEBGLOAD);
        params->console_lines_max = json_geti_child(general, "console-lines", 1000);
    }	else	{
        params->console_lines_max = 1000;
    }

    /* gfx */
    gfx_parseparams(&params->gfx, root);

    /* physics */
    phx_parseparams(&params->phx, root);

    /* script */
    sct_parseparams(&params->sct, root);

    /* developer */
    json_t dev = json_getitem(root, "dev");
    if (dev != NULL)   {
        params->dev.webserver_port = json_geti_child(dev, "webserver-port", 8888);
        params->dev.buffsize_data = json_geti_child(dev, "buffsize-data", 0);
        params->dev.buffsize_tmp = json_geti_child(dev, "buffsize-tmp", 0);
    }
    else    {
        params->dev.webserver_port = 8888;
    }

    /* console commands */
    json_t console = json_getitem(root, "console");
    if (console != NULL)   {
        params->console_cmds_cnt = json_getarr_count(console);
        params->console_cmds = (char*)ALLOC(params->console_cmds_cnt*128, 0);
        ASSERT(params->console_cmds != NULL);
        for (uint i = 0; i < params->console_cmds_cnt; i++) {
            char* data = params->console_cmds + i*128;
            strcpy(data, json_gets(json_getarr_item(console, i)));
        }
    }

    json_destroy(root);

    return params;
}

void app_config_unload(struct init_params* cfg)
{
    if (cfg->console_cmds != NULL)
        FREE(cfg->console_cmds);

    FREE(cfg);
}

struct init_params* app_config_default()
{
    struct init_params* params = (struct init_params*)ALLOC(sizeof(struct init_params), 0);
    ASSERT(params);
    memset(params, 0x00, sizeof(struct init_params));

    params->console_lines_max = 1000;
    params->dev.webserver_port = 8888;

    params->gfx.width = 1280;
    params->gfx.height = 720;
    params->gfx.refresh_rate = 60;
    params->phx.substeps_max = PHX_DEFAULT_SUBSTEPS;

#if defined(_OSX_)
    params->gfx.hwver = GFX_HWVER_GL3_2;
#endif

    return params;
}

void app_config_addconsolecmd(struct init_params* cfg, const char* cmd)
{
    if (cfg->console_cmds != NULL)  {
        uint size = cfg->console_cmds_cnt*128;
        char* tmp = (char*)ALLOC(size + 128, 0);
        ASSERT(tmp);
        memcpy(tmp, cfg->console_cmds, size);
        str_safecpy(tmp + size, 128, cmd);
        FREE(cfg->console_cmds);
        cfg->console_cmds = tmp;
        cfg->console_cmds_cnt ++;
    }   else    {
        cfg->console_cmds = (char*)ALLOC(128, 0);
        ASSERT(cfg->console_cmds);
        str_safecpy(cfg->console_cmds, 128, cmd);
        cfg->console_cmds_cnt ++;
    }
}

void gfx_parseparams(struct gfx_params* params, json_t j)
{
    memset(params, 0x00, sizeof(struct gfx_params));

    /* graphics */
    json_t gfx = json_getitem(j, "gfx");
    if (gfx != NULL)   {
        if (json_getb_child(gfx, "fullscreen", FALSE))
            BIT_ADD(params->flags, GFX_FLAG_FULLSCREEN);

        if (json_getb_child(gfx, "vsync", FALSE))
            BIT_ADD(params->flags, GFX_FLAG_VSYNC);

        if (json_getb_child(gfx, "debug", FALSE))
            BIT_ADD(params->flags, GFX_FLAG_DEBUG);

        if (json_getb_child(gfx, "fxaa", FALSE))
            BIT_ADD(params->flags, GFX_FLAG_FXAA);

        if (json_getb_child(gfx, "rebuild-shaders", FALSE))
            BIT_ADD(params->flags, GFX_FLAG_REBUILDSHADERS);

        int msaa = json_geti_child(gfx, "msaa", 0);
        switch (msaa)   {
        case 2: params->msaa = MSAA_2X; break;
        case 4: params->msaa = MSAA_4X; break;
        case 8: params->msaa = MSAA_8X; break;
        default: params->msaa = MSAA_NONE;  break;
        }

        const char* texq = json_gets_child(gfx, "texture-quality", "highest");
        if (str_isequal_nocase(texq, "high"))
            params->tex_quality = TEXTURE_QUALITY_HIGH;
        else if(str_isequal_nocase(texq, "normal"))
            params->tex_quality = TEXTURE_QUALITY_NORMAL;
        else if(str_isequal_nocase(texq, "low"))
            params->tex_quality = TEXTURE_QUALITY_LOW;
        else
            params->tex_quality = TEXTURE_QUALITY_HIGHEST;

        const char* texf = json_gets_child(gfx, "texture-filter", "trilinear");
        if (str_isequal_nocase(texf, "trilinear"))
            params->tex_filter = TEXTURE_FILTER_TRILINEAR;
        else if(str_isequal_nocase(texf, "bilinear"))
            params->tex_filter = TEXTURE_FILTER_BILINEAR;
        else if(str_isequal_nocase(texf, "aniso2x"))
            params->tex_filter = TEXTURE_FILTER_ANISO2X;
        else if(str_isequal_nocase(texf, "aniso4x"))
            params->tex_filter = TEXTURE_FILTER_ANISO4X;
        else if(str_isequal_nocase(texf, "aniso8x"))
            params->tex_filter = TEXTURE_FILTER_ANISO8X;
        else if(str_isequal_nocase(texf, "aniso16x"))
            params->tex_filter = TEXTURE_FILTER_ANISO16X;
        else
            params->tex_filter = TEXTURE_FILTER_TRILINEAR;

        const char* shq = json_gets_child(gfx, "shading-quality", "high");
        if (str_isequal_nocase(shq, "normal"))
            params->shading_quality = SHADING_QUALITY_NORMAL;
        else if(str_isequal_nocase(shq, "low"))
            params->shading_quality = SHADING_QUALITY_LOW;
        else
            params->shading_quality = SHADING_QUALITY_HIGH;

        const char* ver = json_gets_child(gfx, "hw-version", "");
        if (str_isequal_nocase(ver, "d3d10"))
            params->hwver = GFX_HWVER_D3D10_0;
        else if (str_isequal_nocase(ver, "d3d10.1"))
            params->hwver = GFX_HWVER_D3D10_1;
        else if (str_isequal_nocase(ver, "d3d11"))
            params->hwver = GFX_HWVER_D3D11_0;
        else if (str_isequal_nocase(ver, "d3d11.1"))
            params->hwver = GFX_HWVER_D3D11_1;
        else if (str_isequal_nocase(ver, "gl3.2"))
            params->hwver = GFX_HWVER_GL3_2;
        else if (str_isequal_nocase(ver, "gl3.3"))
            params->hwver = GFX_HWVER_GL3_3;
        else if (str_isequal_nocase(ver, "gl4.0"))
            params->hwver = GFX_HWVER_GL4_0;
        else if (str_isequal_nocase(ver, "gl4.1"))
            params->hwver = GFX_HWVER_GL4_1;
        else if (str_isequal_nocase(ver, "gl4.2"))
            params->hwver = GFX_HWVER_GL4_2;
        else if (str_isequal_nocase(ver, "gl4.3"))
            params->hwver = GFX_HWVER_GL4_3;
        else if (str_isequal_nocase(ver, "gl4.4"))
            params->hwver = GFX_HWVER_GL4_4;
        else
            params->hwver = GFX_HWVER_UNKNOWN;

        params->adapter_id = json_geti_child(gfx, "adapter-id", 0);
        params->width = json_geti_child(gfx, "width", 1280);
        params->height = json_geti_child(gfx, "height", 720);
        params->refresh_rate = json_geti_child(gfx, "refresh-rate", 60);
    }   else    {
        params->width = 1280;
        params->height = 720;
    }
}

void phx_parseparams(struct phx_params* params, json_t j)
{
    memset(params, 0x00, sizeof(struct phx_params));

    /* physics */
    json_t jphx = json_getitem(j, "physics");
    if (jphx != NULL)  {
        if (json_getb_child(jphx, "track-mem", FALSE))
            BIT_ADD(params->flags, PHX_FLAG_TRACKMEM);
        if (json_getb_child(jphx, "profile", FALSE))
            BIT_ADD(params->flags, PHX_FLAG_PROFILE);
        params->mem_sz = json_geti_child(jphx, "mem-size", 0);
        params->substeps_max = json_geti_child(jphx, "substeps-max", PHX_DEFAULT_SUBSTEPS);
        params->scratch_sz = json_geti_child(jphx, "scratch-size", 0);
    }   else    {
        params->flags = 0;
        params->mem_sz = 0;
        params->substeps_max = PHX_DEFAULT_SUBSTEPS;
        params->scratch_sz = 0;
    }
}

void sct_parseparams(struct sct_params* params, json_t j)
{
    memset(params, 0x00, sizeof(struct sct_params));

    /* script */
    json_t jsct = json_getitem(j, "script");
    if (jsct != NULL)  {
        params->mem_sz = json_geti_child(jsct, "mem-size", 0);
    }   else    {
        params->mem_sz = 0;
    }
}
