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
    if (str_isequal_nocase(value, "diffuse-tex"))
        return H3D_TEXTURE_DIFFUSE;
    else if (str_isequal_nocase(value, "gloss-tex"))
        return H3D_TEXTURE_GLOSS;
    else if (str_isequal_nocase(value, "norm-tex"))
        return H3D_TEXTURE_NORMAL;
    else if (str_isequal_nocase(value, "opacity-tex"))
        return H3D_TEXTURE_ALPHAMAP;
    else if (str_isequal_nocase(value, "emissive-tex"))
        return H3D_TEXTURE_EMISSIVE;
    else if (str_isequal_nocase(value, "reflection-tex"))
        return H3D_TEXTURE_REFLECTION;
    else
        return H3D_TEXTURE_DIFFUSE;
}

/***********************************************************************************
 * forward declarations
 */
void parse_arguments(struct array* pairs, int argc, char** argv);
bool_t validate_params(const struct import_params* params);
void verbose_param_report(const struct import_params* params);
void show_help();

/***********************************************************************************/
int main(int argc, char** argv)
{
	result_t r;
    int prog_ret = 0;
    bool_t ir = FALSE;
    size_t tdir_sz;

	r = core_init(CORE_INIT_ALL);
	if (IS_FAIL(r))
		return -1;

	log_outputconsole(TRUE);

	struct array pairs;
	r = arr_create(mem_heap(), &pairs, sizeof(struct arg_pair), 10, 10, 0);
	ASSERT(IS_OK(r));

	/* read input arguments and read flags */
	parse_arguments(&pairs, argc, argv);

	struct import_params params;
	memset(&params, 0x00, sizeof(params));
    params.scale = 1.0f;

	struct arg_pair* ps = (struct arg_pair*)pairs.buffer;
	for (uint i = 0; i < pairs.item_cnt; i++)	{
		if (str_isequal_nocase(ps[i].arg, "-m"))    {
			strcpy(params.in_filepath, ps[i].value);
            params.type = IMPORT_MODEL;
        }
		else if (str_isequal_nocase(ps[i].arg, "-n"))
			str_safecpy(params.name, sizeof(params.name), ps[i].value);
		else if (str_isequal_nocase(ps[i].arg, "-o"))
			strcpy(params.out_filepath, ps[i].value);
		else if (str_isequal_nocase(ps[i].arg, "-tdxt3"))
			params.tex_params.force_dxt3 = TRUE;
		else if (str_isequal_nocase(ps[i].arg, "-tfast"))
			params.tex_params.fast_mode = TRUE;
		else if (str_isequal_nocase(ps[i].arg, "-tdir"))
			strcpy(params.texture_dir, ps[i].value);
		else if (str_isequal_nocase(ps[i].arg, "-talias"))
			strcpy(params.texture_dir_alias, ps[i].value);
		else if (str_isequal_nocase(ps[i].arg, "-v"))
			params.verbose = TRUE;
		else if (str_isequal_nocase(ps[i].arg, "-a"))   {
			strcpy(params.in_filepath, ps[i].value);
            params.type = IMPORT_ANIM;
        }
        else if (str_isequal_nocase(ps[i].arg, "-p"))   {
            strcpy(params.in_filepath, ps[i].value);
            params.type = IMPORT_PHX;
        }
        else if (str_isequal_nocase(ps[i].arg, "-t"))   {
            strcpy(params.in_filepath, ps[i].value);
            params.type = IMPORT_TEXTURE;
        }
        else if (str_isequal_nocase(ps[i].arg, "-toff"))
            params.toff = TRUE;
        else if (str_isequal_nocase(ps[i].arg, "-ttype"))
            params.ttype = import_texture_gettype(ps[i].value);
        else if (str_isequal_nocase(ps[i].arg, "-lm"))
            params.list_mtls = TRUE;
        else if (str_isequal_nocase(ps[i].arg, "-fps"))
            params.anim_fps = str_toint32(ps[i].value);
		else if (str_isequal_nocase(ps[i].arg, "-calctng"))
			params.calc_tangents = TRUE;
        else if (str_isequal_nocase(ps[i].arg, "-l"))
            params.list = TRUE;
        else if (str_isequal_nocase(ps[i].arg, "-occ"))
            str_safecpy(params.occ_name, sizeof(params.occ_name), ps[i].value);
        else if (str_isequal_nocase(ps[i].arg, "-zup"))
            params.coord = COORD_RH_ZUP;
        else if (str_isequal_nocase(ps[i].arg, "-zinv"))
            params.coord = COORD_RH_GL;
        else if (str_isequal_nocase(ps[i].arg, "-clips"))
            strcpy(params.clips_json_filepath, ps[i].value);
        else if (str_isequal_nocase(ps[i].arg, "-scale"))
            params.scale = str_tofl32(ps[i].value);
		else if (str_isequal_nocase(ps[i].arg, "-h") || str_isequal_nocase(ps[i].arg, "--help")) {
			show_help();
			goto cleanup;
		}
	}

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

    /* remove '/' or '\\' from the end of texture_dir */
    tdir_sz = strlen(params.texture_dir);
    if (tdir_sz > 0 &&
        (params.texture_dir[tdir_sz-1] == '\\' || params.texture_dir[tdir_sz-1] == '/'))
    {
        params.texture_dir[tdir_sz-1] = 0;
    }
    path_norm(params.texture_dir, params.texture_dir);

    path_tounix(params.texture_dir_alias, params.texture_dir_alias);
    tdir_sz = strlen(params.texture_dir_alias);
    if (tdir_sz > 0 && params.texture_dir_alias[tdir_sz-1] == '/')
        params.texture_dir_alias[tdir_sz-1] = 0;

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
	arr_destroy(&pairs);

#if defined(_DEBUG_)
	core_release(TRUE);
#else
	core_release(FALSE);
#endif
	return prog_ret;
}

void parse_arguments(struct array* pairs, int argc, char** argv)
{
	for (int i = 0; i < argc; i++)	{
		const char* arg = argv[i];
		if (arg[0] == '-')	{
			struct arg_pair* pair = (struct arg_pair*)arr_add(pairs);
			ASSERT(pair);
			memset(pair, 0x00, sizeof(struct arg_pair));
			str_safecpy(pair->arg, sizeof(pair->arg), arg);
			while ((i + 1) < argc && argv[i+1][0] != '-')	{
				if (!str_isempty(pair->value))
					str_safecat(pair->value, sizeof(pair->value), " ");
				str_safecat(pair->value, sizeof(pair->value), argv[i+1]);
				i ++;
			}
		}
	}
}

bool_t validate_params(const struct import_params* params)
{
    if (params->type == IMPORT_UNKNOWN)
        return FALSE;
	if (str_isempty(params->in_filepath))
		return FALSE;
	if (!params->list_mtls && params->type != IMPORT_TEXTURE && str_isempty(params->out_filepath))
		return FALSE;

    /* check model name */
	if ((params->type == IMPORT_MODEL || params->type == IMPORT_PHX)
        && str_isempty(params->name))
    {
		return FALSE;
    }

	return TRUE;
}

void show_help()
{
	printf("h3dimport utility: model importer for dark-hammer engine v1.2\n");
	printf("parameters:\n");
	printf( "  -n [name]: input object (model/physics/etc) name (inside the file)\n"
			"  -m [filename]: input model file (.obj, .dae, .x, etc)\n"
            "  -a [filename]: input animation file (.obj, .dae, .x, ...)\n"
            "  -p [filename]: input physics file (.RepX - xml)\n"
            "  -pn [object-name]: physics object name (in the file)\n"
            "  -t [filename]: input texture file\n"
			"  -o [output-h3d-file]: output h3dX/dds file\n"
			"  -tdir [output-texture-directory]: output texture directory (default=current dir)\n"
			"  -talias [output-texture-alias-directory]: directory that is addressed for textures in the h3d file\n"
			"  -tfast: fast texture compression\n"
            "  -ttype [name]: texture type for texture only conversion mode (diffuse-tex, norm-tex, ...)\n"
			"  -tdxt3: force dxtc3 (high-freq) for textures that have alpha channel\n"
            "  -toff: turn off texture compression when importing model\n"
            "  -v: verbose mode\n"
            "  -l: list objects (which can be later referenced by -n flag) \n"
            "  -fps [fps-value]: sets fps for animations (default: 30)\n"
			"  -calctng: calculate tangents.\n"
            "  -occ [occ-name]: model has occluder mesh (defined by it's name inside model-file)\n"
            "  -lm: list all materials for specified model\n"
            "  -zup: defines that up vector is Z (like 3dsmax)\n"
            "  -zinv: Z axis in inverted (right-handed)\n"
            "  -clips: path to clips json file\n"
            "\n"
			"\n");

}
