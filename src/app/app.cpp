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

#include "dhapp/app.h"

#include "dhcore/core.h"
#include "dhcore/json.h"
#include "dhcore/util.h"

using namespace dh;

static void gfx_parseparams(appGfxParams* params, JNode j)
{
    memset(params, 0x00, sizeof(appGfxParams));

    // Graphics
    JNode gfx = j.child("gfx");
    if (gfx.is_valid())   {
        if (gfx.child_bool("fullscreen"))
            BIT_ADD(params->flags, (uint)appGfxFlags::FULLSCREEN);

        if (gfx.child_bool("vsync"))
            BIT_ADD(params->flags, (uint)appGfxFlags::VSYNC);

        if (gfx.child_bool("debug"))
            BIT_ADD(params->flags, (uint)appGfxFlags::DEBUG);

        if (gfx.child_bool("fxaa"))
            BIT_ADD(params->flags, (uint)appGfxFlags::FXAA);

        if (gfx.child_bool("rebuild-shaders"))
            BIT_ADD(params->flags, (uint)appGfxFlags::REBUILD_SHADERS);

        int msaa = gfx.child_int("msaa");
        switch (msaa)   {
        case 2: params->msaa = appGfxMSAA::X2; break;
        case 4: params->msaa = appGfxMSAA::X4; break;
        case 8: params->msaa = appGfxMSAA::X8; break;
        default: params->msaa = appGfxMSAA::NONE;  break;
        }

        const char* texq = gfx.child_str("texture-quality", "highest");
        if (str_isequal_nocase(texq, "high"))
            params->tex_quality = appGfxTextureQuality::HIGH;
        else if (str_isequal_nocase(texq, "normal"))
            params->tex_quality = appGfxTextureQuality::NORMAL;
        else if (str_isequal_nocase(texq, "low"))
            params->tex_quality = appGfxTextureQuality::LOW;
        else
            params->tex_quality = appGfxTextureQuality::HIGHEST;

        const char* texf = gfx.child_str("texture-filter", "trilinear");
        if (str_isequal_nocase(texf, "trilinear"))
            params->tex_filter = appGfxTextureFilter::TRILINEAR;
        else if (str_isequal_nocase(texf, "bilinear"))
            params->tex_filter = appGfxTextureFilter::BILINEAR;
        else if (str_isequal_nocase(texf, "aniso2x"))
            params->tex_filter = appGfxTextureFilter::ANISO2X;
        else if (str_isequal_nocase(texf, "aniso4x"))
            params->tex_filter = appGfxTextureFilter::ANISO4X;
        else if (str_isequal_nocase(texf, "aniso8x"))
            params->tex_filter = appGfxTextureFilter::ANISO8X;
        else if (str_isequal_nocase(texf, "aniso16x"))
            params->tex_filter = appGfxTextureFilter::ANISO16X;
        else
            params->tex_filter = appGfxTextureFilter::TRILINEAR;

        const char* shq = gfx.child_str("shading-quality", "high");
        if (str_isequal_nocase(shq, "normal"))
            params->shading_quality = appGfxShadingQuality::NORMAL;
        else if (str_isequal_nocase(shq, "low"))
            params->shading_quality = appGfxShadingQuality::LOW;
        else
            params->shading_quality = appGfxShadingQuality::HIGH;

        const char* ver = gfx.child_str("hw-version");
        if (str_isequal_nocase(ver, "d3d10"))
            params->hwver = appGfxDeviceVersion::D3D10_0;
        else if (str_isequal_nocase(ver, "d3d10.1"))
            params->hwver = appGfxDeviceVersion::D3D10_1;
        else if (str_isequal_nocase(ver, "d3d11"))
            params->hwver = appGfxDeviceVersion::D3D11_0;
        else if (str_isequal_nocase(ver, "d3d11.1"))
            params->hwver = appGfxDeviceVersion::D3D11_1;
        else if (str_isequal_nocase(ver, "gl3.2"))
            params->hwver = appGfxDeviceVersion::GL3_2;
        else if (str_isequal_nocase(ver, "gl3.3"))
            params->hwver = appGfxDeviceVersion::GL3_3;
        else if (str_isequal_nocase(ver, "gl4.0"))
            params->hwver = appGfxDeviceVersion::GL4_0;
        else if (str_isequal_nocase(ver, "gl4.1"))
            params->hwver = appGfxDeviceVersion::GL4_1;
        else if (str_isequal_nocase(ver, "gl4.2"))
            params->hwver = appGfxDeviceVersion::GL4_2;
        else if (str_isequal_nocase(ver, "gl4.3"))
            params->hwver = appGfxDeviceVersion::GL4_3;
        else if (str_isequal_nocase(ver, "gl4.4"))
            params->hwver = appGfxDeviceVersion::GL4_4;
        else
            params->hwver = appGfxDeviceVersion::UNKNOWN;

        params->adapter_id = gfx.child_int("adapter-id");
        params->width = gfx.child_int("width", 1280);
        params->height = gfx.child_int("height", 720);
        params->refresh_rate = gfx.child_int("refresh-rate", 60);
    }
    else    {
        params->width = 1280;
        params->height = 720;
    }
}

static void phx_parseparams(struct appPhysicsParams* params, JNode j)
{
    memset(params, 0x00, sizeof(struct appPhysicsParams));

    /* physics */
    JNode jphx = j.child("physics");
    if (jphx.is_valid())  {
        if (jphx.child_bool("track-mem"))
            BIT_ADD(params->flags, (uint)appPhysicsFlags::TRACKMEM);
        if (jphx.child_bool("profile"))
            BIT_ADD(params->flags, (uint)appPhysicsFlags::PROFILE);
        params->mem_sz = jphx.child_int("mem-size");
        params->substeps_max = jphx.child_int("substeps-max", 2);
        params->scratch_sz = jphx.child_int("scratch-size");
    }
    else    {
        params->flags = 0;
        params->mem_sz = 0;
        params->substeps_max = 2;
        params->scratch_sz = 0;
    }
}

static void sct_parseparams(struct appScriptParams* params, JNode j)
{
    memset(params, 0x00, sizeof(struct appScriptParams));

    /* script */
    JNode jsct = j.child("script");
    if (jsct.is_valid())  {
        params->mem_sz = jsct.child_int("mem-size");
    }
    else    {
        params->mem_sz = 0;
    }
}

appInitParams* app_config_load(const char* cfg_jsonfile)
{
    appInitParams* params = (appInitParams*)ALLOC(sizeof(appInitParams), 0);
    ASSERT(params);
    memset(params, 0x00, sizeof(appInitParams));

    char* buffer = util_readtextfile(cfg_jsonfile, mem_heap());
    if (buffer == nullptr) {
        err_printf(__FILE__, __LINE__, "Loading config file '%s' failed", cfg_jsonfile);
        FREE(params);
        return nullptr;
    }
    JNode root = JNode(json_parsestring(buffer));
    FREE(buffer);

    if (!root.is_valid())  {
        err_printf(__FILE__, __LINE__, "Parsing config file '%s' failed: Invalid json",
            cfg_jsonfile);
        FREE(params);
        return nullptr;
    }

    // General/Engine
    JNode general = root.child("general");
    if (general.is_valid())   {
        if (general.child_bool("debug"))
            BIT_ADD(params->flags, (uint)appEngineFlags::DEBUG);
        if (general.child_bool("dev"))
            BIT_ADD(params->flags, (uint)appEngineFlags::CONSOLE);
        if (general.child_bool("console"))
            BIT_ADD(params->flags, (uint)appEngineFlags::CONSOLE);
        if (general.child_bool("no-physics"))
            BIT_ADD(params->flags, (uint)appEngineFlags::DISABLE_PHYSICS);
        if (general.child_bool("optimize-mem"))
            BIT_ADD(params->flags, (uint)appEngineFlags::OPTIMIZE_MEMORY);
        if (general.child_bool("no-bgload"))
            BIT_ADD(params->flags, (uint)appEngineFlags::DISABLE_BGLOAD);
        params->console_lines_max = general.child_int("console-lines", 1000);
    }	else	{
        params->console_lines_max = 1000;
    }

    // Graphics
    gfx_parseparams(&params->gfx, root);

    // Physics
    phx_parseparams(&params->phx, root);

    // Script (LUA)
    sct_parseparams(&params->sct, root);

    // Developer
    JNode dev = root.child("dev");
    if (dev.is_valid())   {
        params->dev.webserver_port = dev.child_int("webserver-port", 8888);
        params->dev.buffsize_data = dev.child_int("buffsize-data");
        params->dev.buffsize_tmp = dev.child_int("buffsize-tmp");
    }
    else    {
        params->dev.webserver_port = 8888;
    }

    // Additional console commands
    JNode console = root.child("console");
    if (console.is_valid())   {
        params->console_cmds_cnt = console.array_item_count();
        if (params->console_cmds_cnt)   {
            params->console_cmds = (char*)ALLOC(params->console_cmds_cnt*128, 0);
            ASSERT(params->console_cmds != nullptr);
            for (uint i = 0; i < params->console_cmds_cnt; i++) {
                char* data = params->console_cmds + i*128;
                str_safecpy(data, 128, console.array_item(i).to_str());
            }
        }
    }

    root.destroy();
    return params;
}

void app_config_unload(appInitParams* cfg)
{
    if (cfg->console_cmds != nullptr)
        FREE(cfg->console_cmds);
    FREE(cfg);
}

appInitParams* app_config_default()
{
    appInitParams* params = (appInitParams*)ALLOC(sizeof(appInitParams), 0);
    ASSERT(params);
    memset(params, 0x00, sizeof(appInitParams));

    params->console_lines_max = 1000;
    params->dev.webserver_port = 8888;

    params->gfx.width = 1280;
    params->gfx.height = 720;
    params->gfx.refresh_rate = 60;
    params->phx.substeps_max = 2;

    return params;
}

void app_config_addconsolecmd(appInitParams* cfg, const char* cmd)
{
    if (cfg->console_cmds != nullptr)  {
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

