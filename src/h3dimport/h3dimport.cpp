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

#include <stdio.h>
#include <stdlib.h>

#include "dhcore/core.h"
#include "dhcore/array.h"
#include "dhcore/commander.h"

#include "h3dimport.h"
#include "model-import.h"
#include "anim-import.h"
#include "texture-import.h"
#include "phx-import.h"

/***********************************************************************************
 * types
 */
struct arg_pair
{
	char arg[32];
	char value[DH_PATH_MAX];
};


/***********************************************************************************
 * globals
 */

/***********************************************************************************
 * inlines
 */
INLINE enum h3d_texture_type import_texture_gettype(const char* value)
{
    if (str_isequal_nocase(value, "diffuse"))
        return H3D_TEXTURE_DIFFUSE;
    else if (str_isequal_nocase(value, "gloss"))
        return H3D_TEXTURE_GLOSS;
    else if (str_isequal_nocase(value, "norm"))
        return H3D_TEXTURE_NORMAL;
    else if (str_isequal_nocase(value, "opacity"))
        return H3D_TEXTURE_ALPHAMAP;
    else if (str_isequal_nocase(value, "emissive"))
        return H3D_TEXTURE_EMISSIVE;
    else if (str_isequal_nocase(value, "reflection"))
        return H3D_TEXTURE_REFLECTION;
    else
        return H3D_TEXTURE_DIFFUSE;
}

/***********************************************************************************
 * forward declarations
 */
int validate_params(const struct import_params* params);
void verbose_param_report(const struct import_params* params);

/* callbacks for arguments */
static void cmdline_model(command_t* cmd)
{   
    struct import_params* p = (struct import_params*)cmd->data;
    p->type = IMPORT_MODEL;
    if (cmd->arg)
        str_safecpy(p->name, sizeof(p->name), cmd->arg);
}
static void cmdline_anim(command_t* cmd)
{   ((struct import_params*)cmd->data)->type = IMPORT_ANIM;  }
static void cmdline_phx(command_t* cmd)
{   
    struct import_params* p = (struct import_params*)cmd->data;
    p->type = IMPORT_PHX;
    if (cmd->arg)
        str_safecpy(p->name, sizeof(p->name), cmd->arg);
}
static void cmdline_tex(command_t* cmd)
{   ((struct import_params*)cmd->data)->type = IMPORT_TEXTURE;  }
static void cmdline_tfast(command_t* cmd)
{  ((struct import_params*)cmd->data)->tex_params.fast_mode = TRUE; }
static void cmdline_tdxt3(command_t* cmd)
{  ((struct import_params*)cmd->data)->tex_params.force_dxt3 = TRUE; }
static void cmdline_toff(command_t* cmd)
{  ((struct import_params*)cmd->data)->toff = TRUE;  }
static void cmdline_ttype(command_t* cmd)
{  ((struct import_params*)cmd->data)->ttype = import_texture_gettype(cmd->arg);  }
static void cmdline_tdiralias(command_t* cmd)
{   
    struct import_params* p = (struct import_params*)cmd->data;
    str_safecpy(p->texture_dir_alias, sizeof(p->texture_dir_alias), cmd->arg);
    path_tounix(p->texture_dir_alias, p->texture_dir_alias);
    size_t tdir_sz = strlen(p->texture_dir_alias);
    if (tdir_sz > 0 && p->texture_dir_alias[tdir_sz-1] == '/')
        p->texture_dir_alias[tdir_sz-1] = 0;
}
static void cmdline_tdir(command_t* cmd)
{   
    struct import_params* p = (struct import_params*)cmd->data;
    str_safecpy(p->texture_dir, sizeof(p->texture_dir), cmd->arg);
    path_norm(p->texture_dir, p->texture_dir);
}
static void cmdline_verbose(command_t* cmd)
{  ((struct import_params*)cmd->data)->verbose = TRUE; }
static void cmdline_listmtls(command_t* cmd)
{  ((struct import_params*)cmd->data)->list_mtls = TRUE; }
static void cmdline_list(command_t* cmd)
{  ((struct import_params*)cmd->data)->list = TRUE; }
static void cmdline_calctangents(command_t* cmd)
{  ((struct import_params*)cmd->data)->calc_tangents = TRUE; }
static void cmdline_animfps(command_t* cmd)
{  ((struct import_params*)cmd->data)->anim_fps = str_toint32(cmd->arg); }
static void cmdline_zup(command_t* cmd)
{  ((struct import_params*)cmd->data)->coord = COORD_RH_ZUP; }
static void cmdline_zinv(command_t* cmd)
{  ((struct import_params*)cmd->data)->coord = COORD_RH_GL; }
static void cmdline_clips(command_t* cmd)
{   
    struct import_params* p = (struct import_params*)cmd->data;
    str_safecpy(p->clips_json_filepath, sizeof(p->clips_json_filepath), cmd->arg);
}
static void cmdline_scale(command_t* cmd)
{  ((struct import_params*)cmd->data)->scale = str_tofl32(cmd->arg); }
static void cmdline_occ(command_t* cmd)
{   
    struct import_params* p = (struct import_params*)cmd->data;
    str_safecpy(p->occ_name, sizeof(p->occ_name), cmd->arg);
}

static void cmdline_infile(command_t* cmd)
{   
    struct import_params* p = (struct import_params*)cmd->data;
    str_safecpy(p->in_filepath, sizeof(p->in_filepath), cmd->arg);
}

static void cmdline_outfile(command_t* cmd)
{   
    struct import_params* p = (struct import_params*)cmd->data;
    str_safecpy(p->out_filepath, sizeof(p->out_filepath), cmd->arg);
}

/***********************************************************************************/
int main(int argc, char** argv)
{
	result_t r;
    int prog_ret = 0;
    int ir = FALSE;

    struct import_params params;
    memset(&params, 0x00, sizeof(params));
    params.scale = 1.0f;

    command_t cmd;
    command_init(&cmd, argv[0], FULL_VERSION);    
    command_option_pos(&cmd, "input_file", "input resource file (geometry/anim/physics)", 0,
        cmdline_infile);
    command_option_pos(&cmd, "output_file", "output h3dx file (h3da/h3dm/h3dp)", 1, cmdline_outfile);
    command_option(&cmd, "-v", "--verbose", "enable verbose mode", cmdline_verbose);
    command_option(&cmd, "-f", "--fps <fps>", "specify fps (frames-per-second) sampling rate of the "
        "animation", cmdline_animfps);
    command_option(&cmd, "-c", "--calc-tangents", "calculate tangents for specified model", 
        cmdline_calctangents);
    command_option(&cmd, "-m", "--model [name]", "import model, must specify it's name inside resource", 
        cmdline_model);
    command_option(&cmd, "-a", "--animation", "import animation", cmdline_anim);
    command_option(&cmd, "-t", "--texture", "import textures only (from model)", cmdline_tex);
    command_option(&cmd, "-p", "--physics [name]", "import physics data, must specify it's name "
        "inside resource", cmdline_phx);
    command_option(&cmd, "-l", "--list", "list models inside resource", cmdline_list);
    command_option(&cmd, "-L", "--list-mtls", "list materials inside specified resource and model "
        "(in JSON format)", cmdline_listmtls);
    command_option(&cmd, "-o", "--occluder <name>", "specify the name of the occluder "
        "inside resource (for models)", cmdline_occ);
    command_option(&cmd, "-d", "--texture-dir <path>", "output texture directory "
        "(relative path on disk)", cmdline_tdir);
    command_option(&cmd, "-s", "--scale <scale>", "set scale multiplier (default=1)", cmdline_scale);
    command_option(&cmd, "-D", "--texture-alias-dir <path>", "texture alias directory "
        "(relative path inside h3dm file)", cmdline_tdiralias);
    command_option(&cmd, "-C", "--clips <clip_file>", "animation clips JSON file", cmdline_clips);
    command_option(&cmd, "-Y", "--zup", "Z is up (3dsmax default coords)", cmdline_zup);
    command_option(&cmd, "-Z", "--zinv", "Z is inverse (OpenGL default coords)", cmdline_zinv);
    command_option(&cmd, "-F", "--tfast", "fast texture compression", cmdline_tfast);
    command_option(&cmd, "-I", "--tignore", "ignore texture compression", cmdline_toff);
    command_option(&cmd, "-T", "--texture-name <name>", "name of the texture to import "
        "(diffuse, gloss, norm, opacity, emissive, reflection)", cmdline_ttype);
    command_option(&cmd, "-X", "--texture-dxt3 <name>", "force dxt3 texture compression "
        "instead of dxt5", cmdline_tdxt3);
    cmd.data = &params;
    command_parse(&cmd, argc, argv);
    command_free(&cmd);

	r = core_init(CORE_INIT_ALL);
	if (IS_FAIL(r))
		return -1;

	log_outputconsole(TRUE);

    /* for list-mode just print out the list */
    if (params.list)    {
        if (params.type == IMPORT_MODEL)
            ir = import_list(&params);
        else if (params.type == IMPORT_PHX)
            ir = import_phx_list(&params);
        goto cleanup;
    }

	/* validate */
	if (!validate_params(&params))	{
		printf(TERM_BOLDRED "Error: invalid arguments\n" TERM_RESET);
        ir = FALSE;
		goto cleanup;
	}


	/* import model */
    printf(TERM_BOLDWHITE "%s ...\n" TERM_RESET, params.in_filepath);
    switch (params.type)    {
    case IMPORT_MODEL:
        ir = !params.list_mtls ? import_model(&params) : import_listmtls(&params);;
        break;
    case IMPORT_ANIM:
        ir = import_anim(&params);
        break;
    case IMPORT_TEXTURE:
        ir = import_texture(&params);
        break;
    case IMPORT_PHX:
        ir = import_phx(&params);
        break;
    default:
        ASSERT(0);
    }


cleanup:
    prog_ret = ir ? 0 : -1;

#if defined(_DEBUG_)
	core_release(TRUE);
#else
	core_release(FALSE);
#endif
	return prog_ret;
}

int validate_params(const struct import_params* params)
{
    if (params->type == IMPORT_UNKNOWN)
        return FALSE;
	if (str_isempty(params->in_filepath))
		return FALSE;
	if (!params->list_mtls && params->type != IMPORT_TEXTURE && str_isempty(params->out_filepath))
		return FALSE;

    /* check model/physics name */
	if ((params->type == IMPORT_MODEL || params->type == IMPORT_PHX)
        && str_isempty(params->name))
    {
		return FALSE;
    }

	return TRUE;
}

