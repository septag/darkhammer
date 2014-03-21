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

#include "gfx.h"
#include "phx.h"
#include "script.h"

struct init_params* app_load_config(const char* cfg_jsonfile)
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

void app_unload_config(struct init_params* cfg)
{
    if (cfg->console_cmds != NULL)
        FREE(cfg->console_cmds);

    FREE(cfg);
}

struct init_params* app_defaultconfig()
{
    struct init_params* params = (struct init_params*)ALLOC(sizeof(struct init_params), 0);
    ASSERT(params);
    memset(params, 0x00, sizeof(struct init_params));

    params->console_lines_max = 1000;
    params->dev.webserver_port = 8888;

    params->gfx.width = 1280;
    params->gfx.height = 720;
    params->gfx.refresh_rate = 60;

#if defined(_OSX_)
    params->gfx.hwver = GFX_HWVER_GL3_2;
#endif

    return params;
}

void app_config_add_consolecmd(struct init_params* cfg, const char* cmd)
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