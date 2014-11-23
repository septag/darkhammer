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
#include <stdarg.h>

#include "dhcore/core.h"
#include "dhcore/array.h"
#include "dhcore/file-io.h"
#include "dhcore/stack-alloc.h"
#include "dhcore/task-mgr.h"

#include "mem-ids.h"
#include "engine.h"

#include "gfx-shader.h"
#include "gfx-device.h"
#include "gfx-cmdqueue.h"

#define CACHE_SIGN 0x48534843   /* HSHC */
#if defined(_D3D_)
#define SHADER_SUFFIX "hlsl"
#define CACHE_FILENAME "dh_shaders_d3d"
#elif defined(_GL_)
#define SHADER_SUFFIX "glsl"
#define CACHE_FILENAME "dh_shaders_gl"
#endif

#define HSEED 4354

/* prepend vertex element IDs as input for all shaders */
const char* shader_prepend_string = "\n"
    "#define INPUT_ID_POSITION 0\n"
    "#define INPUT_ID_NORMAL 1\n"
    "#define INPUT_ID_TEXCOORD0 2\n"
    "#define INPUT_ID_TANGENT 3\n"
    "#define INPUT_ID_BINORMAL 4\n"
    "#define INPUT_ID_BLENDINDEX 5\n"
    "#define INPUT_ID_BLENDWEIGHT 6\n"
    "#define INPUT_ID_TEXCOORD1 7\n"
    "#define INPUT_ID_TEXCOORD2 8\n"
    "#define INPUT_ID_TEXCOORD3 9\n"
    "#define INPUT_ID_COLOR 10\n";

/*************************************************************************************************
 * types
 */
struct shader_cache_item
{
	hash_t vs_hash;
	hash_t ps_hash;
	hash_t gs_hash;
	uint defines_hash;
	struct gfx_shader_binary_data bin;
};

struct shader_load_item
{
	char vs_filepath[DH_PATH_MAX];
	char ps_filepath[DH_PATH_MAX];
	char gs_filepath[DH_PATH_MAX];
    char* include_code;
	struct allocator* alloc;
};

struct shader_mgr
{
	int disable_cache;
	struct array cache;	/* item = shader_cache_item */
    struct array shaders;   /* item = gfx_shader* */
	uint cur_driver_hash;
	struct shader_load_item load_item;
};

/* column major matrixes for submitting to shader */
struct ALIGN16 mat3f_cm
{
    union   {
        struct {
            float m11, m21, m31, m41;    /* column #1 */
            float m12, m22, m32, m42;    /* column #2 */
            float m13, m23, m33, m43;    /* column #3 */
        };

        struct {
            float col1[4];
            float col2[4];
            float col3[4];
        };

        float    f[12];
    };
};

struct ALIGN16 mat4f_cm
{
    union   {
        struct {
            float m11, m21, m31, m41;    /* column #1 */
            float m12, m22, m32, m42;    /* column #2 */
            float m13, m23, m33, m43;    /* column #3 */
            float m14, m24, m34, m44;    /* column #4 */
        };

        struct {
            float col1[4];
            float col2[4];
            float col3[4];
            float col4[4];
        };

        float f[16];
    };
};

/*************************************************************************************************
 * globals
 */
struct shader_mgr g_shader_mgr;

/*************************************************************************************************
 * inlines
 */
INLINE struct mat3f_cm* mat3f_togpu(struct mat3f_cm* rm, const struct mat3f* m)
{
    rm->m11 = m->m11;   rm->m21 = m->m21;   rm->m31 = m->m31;   rm->m41 = m->m41;
    rm->m12 = m->m12;   rm->m22 = m->m22;   rm->m32 = m->m32;   rm->m42 = m->m42;
    rm->m13 = m->m13;   rm->m23 = m->m23;   rm->m33 = m->m33;   rm->m43 = m->m43;
    return rm;
}

INLINE struct mat4f_cm* mat4f_togpu(struct mat4f_cm* rm, const struct mat4f* m)
{
    rm->m11 = m->m11;   rm->m21 = m->m21;   rm->m31 = m->m31;   rm->m41 = m->m41;
    rm->m12 = m->m12;   rm->m22 = m->m22;   rm->m32 = m->m32;   rm->m42 = m->m42;
    rm->m13 = m->m13;   rm->m23 = m->m23;   rm->m33 = m->m33;   rm->m43 = m->m43;
    rm->m14 = m->m14;   rm->m24 = m->m24;   rm->m34 = m->m34;   rm->m44 = m->m44;
    return rm;
}

INLINE const struct gfx_constant_desc* shader_find_constantcb(struct gfx_cblock* cb,
		uint name_hash)
{
	struct hashtable_item* item = hashtable_fixed_find(&cb->ctable, name_hash);
	if (item != NULL)
		return &cb->constants[item->value];
	else
		return NULL;
}

/* add hlsl/glsl to the last directory path
 * and add .hlsl/.glsl extenstion to the file
 * for example, "shaders/test" transforms into "shaders/hlsl/test.hlsl" for d3d
 */
INLINE const char* shader_makepath(char* outpath, const char* inpath)
{
	char filename[64];
    return path_join(outpath, path_getdir(outpath, inpath), SHADER_SUFFIX,
        strcat(path_getfullfilename(filename, inpath), "." SHADER_SUFFIX), NULL);
}

/*************************************************************************************************
 * forward delarations
 */
/* implemented in platform specific (d3d/gl) gfx-shader.c/cpp */
_EXTERN_ void shader_init_cblocks(struct gfx_shader* shader);
_EXTERN_ void shader_destroy_cblocks(struct gfx_shader* shader);
_EXTERN_ void shader_init_samplers(struct gfx_shader* shader);
_EXTERN_ void shader_destroy_samplers(struct gfx_shader* shader);
_EXTERN_ void shader_init_constants(struct gfx_shader* shader);
_EXTERN_ void shader_destroy_constants(struct gfx_shader* shader);
_EXTERN_ result_t shader_init_metadata(struct gfx_shader* shader);
_EXTERN_ void shader_destroy_metadata(struct gfx_shader* shader);

/* */
const struct shader_cache_item* shader_find_cache_item(hash_t vs_hash,
		hash_t ps_hash, hash_t gs_hash, uint defines_hash);

void shader_add_cache_item(hash_t vs_hash,
		hash_t ps_hash, hash_t gs_hash, uint defines_hash,
		const struct gfx_shader_binary_data* bin);

void make_define_string(char* define_str, const struct gfx_shader_define* defines,
		uint define_cnt);
struct gfx_shader* shader_load_fromcache(struct allocator* alloc,
		struct allocator* tmp_alloc, const struct shader_cache_item* cache,
        const struct gfx_input_element_binding* bindings, uint binding_cnt);
struct gfx_shader* shader_load_fromscratch(struct allocator* alloc,
		struct allocator* tmp_alloc,
		const struct gfx_shader_data* source_data,
		const struct gfx_input_element_binding* bindings, uint binding_cnt,
		const struct gfx_shader_define* defines, uint define_cnt,
		struct gfx_shader_binary_data* bin);
void shader_unload(struct gfx_shader* shader);
void shader_loadcache();
void shader_saveunloadcache();
void* shader_includesource(struct allocator* alloc, const char* filepath, const char* include_code,
                           void* src, uint src_size, OUT uint* outsize);

/*************************************************************************************************/
void gfx_shader_zero()
{
	memset(&g_shader_mgr, 0x00, sizeof(struct shader_mgr));
}

result_t gfx_shader_initmgr(int disable_cache)
{
	log_print(LOG_INFO, "init shader-mgr ...");

	if (IS_FAIL(arr_create(mem_heap(), &g_shader_mgr.shaders,
            sizeof(struct gfx_shader*), 50, 50, MID_GFX)))
	{
		err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
		return RET_OUTOFMEMORY;
	}

    /* save driver version string hash */
    const char* driver_info = gfx_get_driverstr();
    g_shader_mgr.cur_driver_hash = hash_str(driver_info);

    if (!disable_cache)	{
		if (IS_FAIL(arr_create(mem_heap(), &g_shader_mgr.cache,
					sizeof(struct shader_cache_item), 50, 50, MID_GFX)))
		{
			err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
			return RET_OUTOFMEMORY;
		}

		/* load cache file */
		log_print(LOG_INFO, "\tloading shader cache ...");
		shader_loadcache();
	}	else	{
		log_print(LOG_INFO, "\tshader cache is disabled (debug mode).");
		g_shader_mgr.disable_cache = TRUE;
	}

	return RET_OK;
}

void gfx_shader_releasemgr()
{
    if (!g_shader_mgr.disable_cache)	{
    	log_print(LOG_INFO, "\tsaving shader cache ...");
    	shader_saveunloadcache();
    	arr_destroy(&g_shader_mgr.cache);
    }

    /* unload, remaining shaders */
    for (int i = 0; i < g_shader_mgr.shaders.item_cnt; i++)  {
        struct gfx_shader* shader = ((struct gfx_shader**)g_shader_mgr.shaders.buffer)[i];
        if (shader != NULL)
            shader_unload(shader);
    }

    arr_destroy(&g_shader_mgr.shaders);
}

struct gfx_cblock* gfx_shader_create_cblockraw(struct allocator* alloc, const char* block_name,
    const struct gfx_constant_desc* constants, uint cnt)
{
    /* estimate buffer size, and create cpu_buffer */
    uint size = 0;
    for (uint i = 0; i < cnt; i++)    {
        if (constants[i].arr_size == 1) {
            size += constants[i].elem_size;
        }   else    {
            size += constants[i].arr_size*constants[i].arr_stride;
        }
    }

    /* create stack memory for allocations */
    struct stack_alloc stack_mem;
    struct allocator stack_alloc;
    size_t total_sz =
        sizeof(struct gfx_cblock) +
        cnt*sizeof(struct gfx_constant_desc) +
        size +
        hashtable_fixed_estimate_size(cnt);
    if (IS_FAIL(mem_stack_create(alloc, &stack_mem, total_sz, MID_GFX)))    {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return NULL;
    }
    mem_stack_bindalloc(&stack_mem, &stack_alloc);

    /* */
    struct gfx_cblock* cb = (struct gfx_cblock*)A_ALLOC(&stack_alloc, sizeof(struct gfx_cblock),
        MID_GFX);
    ASSERT(cb);
    memset(cb, 0x00, sizeof(struct gfx_cblock));
    cb->alloc = alloc;
    cb->name_hash = hash_str(block_name);

    cb->constant_cnt = cnt;
    cb->constants = (struct gfx_constant_desc*)A_ALLOC(&stack_alloc,
        sizeof(struct gfx_constant_desc)*cnt, MID_GFX);
    ASSERT(cb->constants);
    memcpy(cb->constants, constants, sizeof(struct gfx_constant_desc)*cnt);

    /* push constants into ctable */
    hashtable_fixed_create(&stack_alloc, &cb->ctable, cnt, MID_GFX);

    for (uint i = 0; i < cnt; i++)
        hashtable_fixed_add(&cb->ctable, hash_str(constants[i].name), i);

    cb->cpu_buffer = (uint8*)A_ALLOC(&stack_alloc, size, MID_GFX);
    ASSERT(cb->cpu_buffer);
    memset(cb->cpu_buffer, 0x00, size);
    cb->buffer_size = size;
    cb->end_offset = size;

    return cb;
}


void shader_loadcache()
{
	char cache_filepath[DH_PATH_MAX];
    char cache_filename[64];
    strcpy(cache_filename, CACHE_FILENAME);
    if (BIT_CHECK(eng_get_params()->gfx.flags, appGfxFlags::DEBUG))
        strcat(cache_filename, "-dbg");
    strcat(cache_filename, ".bin");

	path_join(cache_filepath, util_gettempdir(cache_filepath), cache_filename, NULL);
    log_printf(LOG_INFO, "Loading shader cache '%s' ...", cache_filepath);
	FILE* f = fopen(cache_filepath, "rb");
    if (f == NULL)  {
        path_join(cache_filepath, util_getexedir(cache_filepath), cache_filename, NULL);
        log_printf(LOG_INFO, "Loading shader cache '%s' ...", cache_filepath);
        f = fopen(cache_filepath, "rb");
    }

	if (f != NULL)	{
		uint sign;
		fread(&sign, sizeof(sign), 1, f);
		if (sign != CACHE_SIGN)	{
			log_print(LOG_WARNING, "loading shader cache failed: invalid file format");
			fclose(f);
			return;
		}

		/* if driver_hash is changed, don't load cache*/
		uint driver_hash;
		fread(&driver_hash, sizeof(uint), 1, f);
		if (driver_hash != g_shader_mgr.cur_driver_hash)	{
			log_print(LOG_INFO, "shader cache: driver/platform changed, recompiling shaders ...");
			fclose(f);
			return;
		}

        uint item_cnt;
		fread(&item_cnt, sizeof(uint), 1, f);
		log_printf(LOG_INFO, "\tloading %d item(s) from shader cache ...", item_cnt);
		for (uint i = 0; i < item_cnt; i++)	{
			struct shader_cache_item* item = (struct shader_cache_item*)arr_add(&g_shader_mgr.cache);
			ASSERT(item);
			fread(item, sizeof(struct shader_cache_item), 1, f);

			/* read program/shaders data */
#if defined(_GL_)
			if (item->bin.prog_size > 0)	{
				item->bin.prog_data = ALLOC(item->bin.prog_size, MID_GFX);
				ASSERT(item->bin.prog_data);
				fread(item->bin.prog_data, item->bin.prog_size, 1, f);
			}
#elif defined(_D3D_)
			if (item->bin.vs_size > 0)	{
				item->bin.vs_data = ALLOC(item->bin.vs_size, MID_GFX);
				ASSERT(item->bin.vs_data);
				fread(item->bin.vs_data, item->bin.vs_size, 1, f);
			}
			if (item->bin.ps_size > 0)	{
				item->bin.ps_data = ALLOC(item->bin.ps_size, MID_GFX);
				ASSERT(item->bin.ps_data);
				fread(item->bin.ps_data, item->bin.ps_size, 1, f);
			}
			if (item->bin.gs_size > 0)	{
				item->bin.gs_data = ALLOC(item->bin.gs_size, MID_GFX);
				ASSERT(item->bin.gs_data);
				fread(item->bin.gs_data, item->bin.gs_size, 1, f);
			}
#endif
		}

		fclose(f);
    }   else    {
        log_printf(LOG_INFO, "Could not locate shader cache file, recompiling shaders ...");
    }
}

void shader_saveunloadcache()
{
    char cache_filename[64];
    strcpy(cache_filename, CACHE_FILENAME);
    if (BIT_CHECK(eng_get_params()->gfx.flags, appGfxFlags::DEBUG))
        strcat(cache_filename, "-dbg");
    strcat(cache_filename, ".bin");

	/* save cache file */
	uint item_cnt = g_shader_mgr.cache.item_cnt;
	if (item_cnt > 0)	{
		char cache_filepath[DH_PATH_MAX];
		path_join(cache_filepath, util_gettempdir(cache_filepath), cache_filename, NULL);
		FILE* f = fopen(cache_filepath, "wb");
		if (f != NULL)	{
            uint sign = CACHE_SIGN;
			fwrite(&sign, sizeof(sign), 1, f);
			fwrite(&g_shader_mgr.cur_driver_hash, sizeof(uint), 1, f);
			fwrite(&item_cnt, sizeof(uint), 1, f);

			struct shader_cache_item* items = (struct shader_cache_item*)g_shader_mgr.cache.buffer;
			for (uint i = 0; i < item_cnt; i++)	{
				fwrite(&items[i], sizeof(struct shader_cache_item), 1, f);

#if defined(_GL_)
				if (items[i].bin.prog_size > 0 && items[i].bin.prog_data != NULL)	{
					fwrite(items[i].bin.prog_data, items[i].bin.prog_size, 1, f);
					FREE(items[i].bin.prog_data);
				}
#elif defined(_D3D_)
				if (items[i].bin.vs_size > 0 && items[i].bin.vs_data != NULL)	{
					fwrite(items[i].bin.vs_data, items[i].bin.vs_size, 1, f);
					FREE(items[i].bin.vs_data);
				}
				if (items[i].bin.ps_size > 0 && items[i].bin.ps_data != NULL)	{
					fwrite(items[i].bin.ps_data, items[i].bin.ps_size, 1, f);
					FREE(items[i].bin.ps_data);
				}
				if (items[i].bin.gs_size > 0 && items[i].bin.gs_data != NULL)	{
					fwrite(items[i].bin.gs_data, items[i].bin.gs_size, 1, f);
					FREE(items[i].bin.gs_data);
				}
#endif
			}

			fclose(f);
		}
	}
}

void gfx_shader_beginload(struct allocator* alloc, const char* vs_filepath, const char* ps_filepath,
		const char* gs_filepath, uint include_cnt, ...)
{
	g_shader_mgr.load_item.alloc = alloc;
	ASSERT(g_shader_mgr.load_item.vs_filepath != NULL);

	strcpy(g_shader_mgr.load_item.vs_filepath, vs_filepath);
	if (ps_filepath != NULL)
		strcpy(g_shader_mgr.load_item.ps_filepath, ps_filepath);
	if (gs_filepath != NULL)
		strcpy(g_shader_mgr.load_item.gs_filepath, gs_filepath);

    /* include files */
    char* total_inc = NULL;
    if (include_cnt > 0)    {
        size_t total_size = 0;
        char filepath[DH_PATH_MAX];
        char dir_buf[DH_PATH_MAX+20];

        va_list args;
        va_start(args, include_cnt);
        for (uint i = 0; i < include_cnt; i++)    {
            const char* inc_file = va_arg(args, const char*);
            ASSERT(inc_file);

            /* load include file and append it to include_code */
            file_t f = fio_openmem(mem_heap(),
                shader_makepath(filepath, inc_file), FALSE, MID_GFX);
            if (f == NULL)	{
                log_printf(LOG_WARNING, "shader '%s' load failed: "
                    "could not open include file '%s'", vs_filepath, inc_file);
                if (total_inc != NULL)
                    FREE(total_inc);
                return;
            }

            size_t inc_size;
            char* inc_code = (char*)fio_detachmem(f, &inc_size, NULL);
            ASSERT(inc_size > 0);
            fio_close(f);

#if defined(_D3D_)
            sprintf(dir_buf, "#line 1 \"%s\"\n", filepath);
#else
            strcpy(dir_buf, "#line 1\n");
#endif
            size_t dir_size = strlen(dir_buf);
            if (total_inc != NULL)  {
                char* tmp = (char*)ALLOC(total_size + inc_size + dir_size, MID_GFX);
                ASSERT(tmp);
                memcpy(tmp, total_inc, total_size);
                FREE(total_inc);
                total_inc = tmp;
                memcpy(total_inc + total_size - 1, dir_buf, dir_size);
                memcpy(total_inc + total_size + dir_size - 1, inc_code, inc_size);
                total_size += inc_size + dir_size;
            }   else    {
                total_inc = (char*)ALLOC(inc_size + dir_size + 1, MID_GFX);
                ASSERT(total_inc);
                memcpy(total_inc, dir_buf, dir_size);
                memcpy(total_inc + dir_size, inc_code, inc_size);
                total_size += (inc_size + dir_size + 1);
            }
            total_inc[total_size-1] = 0;   /* close the string */

            FREE(inc_code);
        } /* foreach: include file */
        va_end(args);
    }

    g_shader_mgr.load_item.include_code = total_inc;
}

uint gfx_shader_add(const char* alias, uint binding_cnt, uint define_cnt, ...)
{
	ASSERT(g_shader_mgr.load_item.alloc != NULL);

	struct gfx_input_element_binding bindings[GFX_INPUTELEMENT_ID_CNT];
	struct gfx_shader_define defines[16];

	va_list args;
	va_start(args, define_cnt);

	/* make bindings */
	ASSERT(binding_cnt < GFX_INPUTELEMENT_ID_CNT);
	for (uint i = 0; i < binding_cnt; i++)	{
		bindings[i].id = (enum gfx_input_element_id)va_arg(args, unsigned int);
		bindings[i].var_name = va_arg(args, const char*);
        bindings[i].vb_idx = va_arg(args, uint);
        bindings[i].elem_offset = GFX_INPUT_OFFSET_PACKED;
	}

	/* make defines */
	ASSERT(define_cnt < 16);
	for (uint i = 0; i < define_cnt; i++)	{
		defines[i].name = va_arg(args, const char*);
		defines[i].value = va_arg(args, const char*);
	}

	va_end(args);

	return gfx_shader_load(alias, g_shader_mgr.load_item.alloc,
			g_shader_mgr.load_item.vs_filepath[0] != 0 ? g_shader_mgr.load_item.vs_filepath : NULL,
			g_shader_mgr.load_item.ps_filepath[0] != 0 ? g_shader_mgr.load_item.ps_filepath : NULL,
			g_shader_mgr.load_item.gs_filepath[0] != 0 ? g_shader_mgr.load_item.gs_filepath : NULL,
			bindings, binding_cnt, defines, define_cnt, g_shader_mgr.load_item.include_code);
}

void gfx_shader_endload()
{
    if (g_shader_mgr.load_item.include_code != NULL)
        FREE(g_shader_mgr.load_item.include_code);

	memset(&g_shader_mgr.load_item, 0x00, sizeof(struct shader_load_item));
}

uint gfx_shader_load(const char* alias, struct allocator* alloc,
		const char* vs_filepath, const char* ps_filepath, const char* gs_filepath,
		const struct gfx_input_element_binding* bindings, uint binding_cnt,
		OPTIONAL const struct gfx_shader_define* defines, uint define_cnt,
        OPTIONAL const char* include_code)
{
	char filepath[DH_PATH_MAX];
	struct gfx_shader_data source_data;
	struct gfx_shader* shader = NULL;
    hash_t vs_hash;	hash_zero(&vs_hash);
    hash_t ps_hash;	hash_zero(&ps_hash);
    hash_t gs_hash;	hash_zero(&gs_hash);
    uint defines_hash = 0;
    char define_code[1024];
    const struct shader_cache_item* item;
    uint shader_id = 0;

	struct allocator* tmp_alloc = tsk_get_tmpalloc(0);
	A_SAVE(tmp_alloc);

	memset(&source_data, 0x00, sizeof(struct gfx_shader_data));

	/* get files and calculate hash values */
	if (vs_filepath != NULL)	{
		file_t f = fio_openmem(tmp_alloc,
				shader_makepath(filepath, vs_filepath), FALSE, MID_GFX);
		if (f == NULL)	{
			err_printf(__FILE__, __LINE__, "shader '%s' load failed: "
					"could not open vertex-shader '%s'", alias, vs_filepath);
			goto cleanup;
		}
		source_data.vs_source = fio_detachmem(f, &source_data.vs_size, NULL);
	    fio_close(f);

        /* add prepend string to includes (for vertex shader only) */
        size_t prepend_sz = strlen(shader_prepend_string);
        char* include_code_vs;
        if (include_code == NULL)   {
            include_code_vs = (char*)ALLOC(prepend_sz + 1, MID_GFX);
            ASSERT(include_code_vs);
            memcpy(include_code_vs, shader_prepend_string, prepend_sz);
            include_code_vs[prepend_sz] = 0;
        }   else    {
            size_t include_sz = strlen(include_code);
            include_code_vs = (char*)ALLOC(prepend_sz + include_sz + 1, MID_GFX);
            ASSERT(include_code_vs);
            memcpy(include_code_vs, shader_prepend_string, prepend_sz);
            memcpy(include_code_vs + prepend_sz, include_code, include_sz);
            include_code_vs[prepend_sz + include_sz] = 0;
        }

        source_data.vs_source = shader_includesource(tmp_alloc, filepath, include_code_vs,
            source_data.vs_source, (uint)source_data.vs_size, (uint*)&source_data.vs_size);

        FREE(include_code_vs);
	}

	if (ps_filepath != NULL)	{
		file_t f = fio_openmem(tmp_alloc, shader_makepath(filepath, ps_filepath), FALSE,
            MID_GFX);
		if (f == NULL)	{
			err_printf(__FILE__, __LINE__, "shader '%s' load failed: "
					"could not open pixel-shader '%s'", alias, ps_filepath);
			goto cleanup;
		}
		source_data.ps_source = fio_detachmem(f, &source_data.ps_size, NULL);
	    fio_close(f);

        if (include_code != NULL)  {
            source_data.ps_source = shader_includesource(tmp_alloc, filepath, include_code,
                source_data.ps_source, (uint)source_data.ps_size, (uint*)&source_data.ps_size);
        }
	}

	if (gs_filepath != NULL)	{
		file_t f = fio_openmem(tmp_alloc, shader_makepath(filepath, gs_filepath), FALSE,
            MID_GFX);
		if (f == NULL)	{
			err_printf(__FILE__, __LINE__, "shader '%s' load failed: "
					"could not open geometry-shader '%s'", alias, gs_filepath);
			goto cleanup;
		}
		source_data.gs_source = fio_detachmem(f, &source_data.gs_size, NULL);
	    fio_close(f);

        if (include_code != NULL)  {
            source_data.gs_source = shader_includesource(tmp_alloc, filepath, include_code,
                source_data.gs_source, (uint)source_data.gs_size, (uint*)&source_data.gs_size);
        }
	}

	/* check for shader cache enabled
	 * if not just load and return
	 */
	if (g_shader_mgr.disable_cache)	{
        log_printf(LOG_INFO, "compiling shader '%s' ...", alias);
		shader = shader_load_fromscratch(alloc, tmp_alloc, &source_data,
				bindings, binding_cnt, defines, define_cnt, NULL);
		goto cleanup;
	}

	make_define_string(define_code, defines, define_cnt);
	defines_hash = hash_str(define_code);

	if (source_data.vs_source != NULL)
		vs_hash = hash_murmur128(source_data.vs_source, source_data.vs_size, HSEED);
	if (source_data.ps_source != NULL)
		ps_hash = hash_murmur128(source_data.ps_source, source_data.ps_size, HSEED);
	if (source_data.gs_source != NULL)
		gs_hash = hash_murmur128(source_data.gs_source, source_data.gs_size, HSEED);

	item = shader_find_cache_item(vs_hash, ps_hash, gs_hash, defines_hash);
	if (item != NULL)	{
		/* use shader from cache */
		shader = shader_load_fromcache(alloc, tmp_alloc, item, bindings, binding_cnt);
	}	else	{
		struct gfx_shader_binary_data bin_data;
		memset(&bin_data, 0x00, sizeof(bin_data));

		/* create and load shader */
        log_printf(LOG_INFO, "compiling shader '%s' ...", alias);
		shader = shader_load_fromscratch(alloc, tmp_alloc, &source_data,
				bindings, binding_cnt, defines, define_cnt, &bin_data);
		if (shader != NULL)
			shader_add_cache_item(vs_hash, ps_hash, gs_hash, defines_hash, &bin_data);
	}

cleanup:
	if (shader != NULL)	{
        /* save bindings */
        for (uint i = 0; i < binding_cnt; i++)
            shader->bindings[i] = (uint)bindings[i].id;
        shader->binding_cnt = binding_cnt;
        /* */
        shader_init_metadata(shader);
		shader_init_cblocks(shader);
		shader_init_samplers(shader);
		shader_init_constants(shader);

#if defined(_DEBUG_)
		strcpy(shader->name, alias);
#endif
        /* add to shaders and fetch ID */
        struct gfx_shader** pshader = (struct gfx_shader**)arr_add(&g_shader_mgr.shaders);
        ASSERT(pshader);
        *pshader = shader;
        shader_id = g_shader_mgr.shaders.item_cnt;
	}

	/* cleanup */
	if (source_data.vs_source != NULL)
		A_FREE(tmp_alloc, source_data.vs_source);
	if (source_data.ps_source != NULL)
		A_FREE(tmp_alloc, source_data.ps_source);
	if (source_data.gs_source != NULL)
		A_FREE(tmp_alloc, source_data.gs_source);
	A_LOAD(tmp_alloc);

    if (shader == NULL)
        err_printf(__FILE__, __LINE__, "gfx-shader load error: loading shader '%s' failed", alias);
	return shader_id;
}

void make_define_string(char* define_str,
		const struct gfx_shader_define* defines, uint define_cnt)
{
	char line[128];
	define_str[0] = 0;

	for (uint i = 0; i < define_cnt; i++)	{
		sprintf(line, "%s=%s\n", defines[i].name, defines[i].value);
		strcat(define_str, line);
	}
}

/* returns NULL if no hash in database is found */
const struct shader_cache_item* shader_find_cache_item(hash_t vs_hash,
		hash_t ps_hash, hash_t gs_hash, uint defines_hash)
{
	uint item_cnt = g_shader_mgr.cache.item_cnt;
	struct shader_cache_item* items = (struct shader_cache_item*)g_shader_mgr.cache.buffer;

	for (uint i = 0; i < item_cnt; i++)	{
		if (items[i].defines_hash == defines_hash &&
			hash_isequal(items[i].vs_hash, vs_hash) &&
			hash_isequal(items[i].ps_hash, ps_hash) &&
			hash_isequal(items[i].gs_hash, gs_hash))
		{
			return &items[i];
		}
	}

	return NULL;
}

struct gfx_shader* shader_load_fromcache(struct allocator* alloc,
		struct allocator* tmp_alloc, const struct shader_cache_item* cache,
        const struct gfx_input_element_binding* bindings, uint binding_cnt)
{
	struct gfx_shader* shader = (struct gfx_shader*)A_ALLOC(alloc, sizeof(struct gfx_shader),
        MID_GFX);
	if (shader == NULL)		{
		err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
		return NULL;
	}
	memset(shader, 0x00, sizeof(struct gfx_shader));
	shader->alloc = alloc;

    struct gfx_program_bin_desc bindesc;

    /* binary data is different for each API */
#if defined(_GL_)
    bindesc.data = cache->bin.prog_data;
    bindesc.fmt = cache->bin.fmt;
    bindesc.size = cache->bin.prog_size;
#else
    bindesc.vs = cache->bin.vs_data;
    bindesc.ps = cache->bin.ps_data;
    bindesc.gs = cache->bin.gs_data;
    bindesc.vs_sz = cache->bin.vs_size;
    bindesc.ps_sz = cache->bin.ps_size;
    bindesc.gs_sz = cache->bin.gs_size;
    bindesc.inputs = bindings;
    bindesc.input_cnt = binding_cnt;
#endif

	shader->prog = gfx_create_program_bin(&bindesc);
    if (shader->prog == NULL)	{
        err_printf(__FILE__, __LINE__, "shader load failed: could not create shader program");
        shader_unload(shader);
        return NULL;
    }

	return shader;
}

struct gfx_shader* shader_load_fromscratch(struct allocator* alloc,
		struct allocator* tmp_alloc,
		const struct gfx_shader_data* source_data,
		const struct gfx_input_element_binding* bindings, uint binding_cnt,
		const struct gfx_shader_define* defines, uint define_cnt,
		struct gfx_shader_binary_data* bin)
{
	struct gfx_shader* shader = (struct gfx_shader*)A_ALLOC(alloc, sizeof(struct gfx_shader),
        MID_GFX);
	if (shader == NULL)		{
		err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
		return NULL;
	}
	memset(shader, 0x00, sizeof(struct gfx_shader));
	shader->alloc = alloc;

	shader->prog = gfx_create_program(source_data, bindings, binding_cnt, defines, define_cnt, bin);
	if (shader->prog == NULL)	{
		err_printf(__FILE__, __LINE__, "shader load failed: could not create shader program");
		shader_unload(shader);
		return NULL;
	}

	return shader;
}

void gfx_shader_unload(uint shader_id)
{
    ASSERT(shader_id <= (uint)g_shader_mgr.shaders.item_cnt);
    struct gfx_shader* shader = ((struct gfx_shader**)g_shader_mgr.shaders.buffer)[shader_id-1];
    ASSERT(shader != NULL);

    shader_unload(shader);

    ((struct gfx_shader**)g_shader_mgr.shaders.buffer)[shader_id-1] = NULL;
}

void shader_unload(struct gfx_shader* shader)
{
    struct allocator* alloc = shader->alloc;

    if (shader->prog != NULL)
        gfx_destroy_program(shader->prog);

    shader_destroy_samplers(shader);
    shader_destroy_cblocks(shader);
    shader_destroy_constants(shader);
    shader_destroy_metadata(shader);

    memset(shader, 0x00, sizeof(struct gfx_shader));
    A_FREE(alloc, shader);
}

void gfx_shader_updatecblock(gfx_cmdqueue cmdqueue, struct gfx_cblock* cblock)
{
    ASSERT(cblock->gpu_buffer);
    gfx_buffer_update(cmdqueue, cblock->gpu_buffer, cblock->cpu_buffer, cblock->end_offset);
}


void gfx_shader_bindcblock_shared(gfx_cmdqueue cmdqueue, struct gfx_shader* shader,
                                  const struct gfx_cblock* cblock,
                                  gfx_buffer buff, uint offset, uint size, uint idx)
{
    struct hashtable_item* item = hashtable_fixed_find(&shader->cblock_bindtable, cblock->name_hash);
    if (item != NULL)   {
        gfx_program_bindcblock_range(cmdqueue, shader->prog, GFX_SHADER_NONE, buff, (uint)item->value,
            idx, offset, size);
    }
}
void shader_add_cache_item(hash_t vs_hash,
		hash_t ps_hash, hash_t gs_hash, uint defines_hash,
		const struct gfx_shader_binary_data* bin)
{
	struct shader_cache_item* item = (struct shader_cache_item*)arr_add(&g_shader_mgr.cache);
	ASSERT(item != NULL);
	hash_set(&item->vs_hash, vs_hash);
	hash_set(&item->ps_hash, ps_hash);
	hash_set(&item->gs_hash, gs_hash);
	item->defines_hash = defines_hash;
	memcpy(&item->bin, bin, sizeof(struct gfx_shader_binary_data));
}

void gfx_shader_bind(gfx_cmdqueue cmdqueue, struct gfx_shader* shader)
{
    gfx_program_setbindings(cmdqueue, shader->bindings, shader->binding_cnt);
    gfx_program_set(cmdqueue, shader->prog);
}

/*************************************************************************************************/
int gfx_cb_isvalid(struct gfx_cblock* cb, uint name_hash)
{
    return shader_find_constantcb(cb, name_hash) != NULL;
}

void gfx_cb_set4m(struct gfx_cblock* cb, uint name_hash, const struct mat4f* m)
{
	const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
	ASSERT(c);
	ASSERT(c->type == gfxUniformType::MAT4x4);

    struct mat4f_cm* mcm = (struct mat4f_cm*)(cb->cpu_buffer + c->offset);
    mat4f_togpu(mcm, m);
}

void gfx_cb_set3m(struct gfx_cblock* cb, uint name_hash, const struct mat3f* m)
{
	const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
	ASSERT(c);
	ASSERT(c->type == gfxUniformType::MAT4x3);

    struct mat3f_cm* mcm = (struct mat3f_cm*)(cb->cpu_buffer + c->offset);
    mat3f_togpu(mcm, m);
}

void gfx_cb_set4f(struct gfx_cblock* cb, uint name_hash, const float* fv)
{
	const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
	ASSERT(c);
	ASSERT(c->type == gfxUniformType::FLOAT4);

    float* buff = (float*)(cb->cpu_buffer + c->offset);
    buff[0] = fv[0];
    buff[1] = fv[1];
    buff[2] = fv[2];
    buff[3] = fv[3];
}

void gfx_cb_set3f(struct gfx_cblock* cb, uint name_hash, const float* fv)
{
	const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
	ASSERT(c);
	ASSERT(c->type == gfxUniformType::FLOAT3);
    float* buff = (float*)(cb->cpu_buffer + c->offset);
    buff[0] = fv[0];
    buff[1] = fv[1];
    buff[2] = fv[2];
}

void gfx_cb_set2f(struct gfx_cblock* cb, uint name_hash, const float* fv)
{
	const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
	ASSERT(c);
	ASSERT(c->type == gfxUniformType::FLOAT2);
    float* buff = (float*)(cb->cpu_buffer + c->offset);
    buff[0] = fv[0];
    buff[1] = fv[1];
}

void gfx_cb_setf(struct gfx_cblock* cb, uint name_hash, float f)
{
	const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
	ASSERT(c);
	ASSERT(c->type == gfxUniformType::FLOAT);
	*((float*)(cb->cpu_buffer + c->offset)) = f;
}

void gfx_cb_setfv(struct gfx_cblock* cb, uint name_hash, const float* fv, uint cnt)
{
    const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
    ASSERT(c);
    ASSERT(c->type == gfxUniformType::FLOAT);
    for (uint i = 0; i < cnt; i++)
        *((float*)(cb->cpu_buffer + c->offset + c->arr_stride*i)) = fv[i];
}

void gfx_cb_set4ivn(struct gfx_cblock* cb, uint name_hash, const int* nv, uint cnt)
{
    const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
    ASSERT(c);
    ASSERT(c->type == gfxUniformType::INT4);
    memcpy(cb->cpu_buffer + c->offset, nv, sizeof(int)*cnt);
}


void gfx_cb_set4i(struct gfx_cblock* cb, uint name_hash, const int* nv)
{
	const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
	ASSERT(c);
	ASSERT(c->type == gfxUniformType::INT4);
    int* buff = (int*)(cb->cpu_buffer + c->offset);
    buff[0] = nv[0];
    buff[1] = nv[1];
    buff[2] = nv[2];
    buff[3] = nv[3];
}

void gfx_cb_set3i(struct gfx_cblock* cb, uint name_hash, const int* nv)
{
	const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
	ASSERT(c);
	ASSERT(c->type == gfxUniformType::INT3);
    int* buff = (int*)(cb->cpu_buffer + c->offset);
    buff[0] = nv[0];
    buff[1] = nv[1];
    buff[2] = nv[2];
}

void gfx_cb_set3ui(struct gfx_cblock* cb, uint name_hash, const uint* nv)
{
    const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
    ASSERT(c);
    ASSERT(c->type == gfxUniformType::INT3);
    uint* buff = (uint*)(cb->cpu_buffer + c->offset);
    buff[0] = nv[0];
    buff[1] = nv[1];
    buff[2] = nv[2];
}


void gfx_cb_set2i(struct gfx_cblock* cb, uint name_hash, const int* nv)
{
	const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
	ASSERT(c);
	ASSERT(c->type == gfxUniformType::INT2);
    int* buff = (int*)(cb->cpu_buffer + c->offset);
    buff[0] = nv[0];
    buff[1] = nv[1];
}

void gfx_cb_seti(struct gfx_cblock* cb, uint name_hash, int n)
{
	const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
	ASSERT(c);
	ASSERT(c->type == gfxUniformType::INT);
	*((int*)(cb->cpu_buffer + c->offset)) = n;
}

void gfx_cb_setiv(struct gfx_cblock* cb, uint name_hash, const int* ns, uint cnt)
{
    const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
    ASSERT(c);
    ASSERT(c->type == gfxUniformType::INT);
    cnt = minui(c->arr_size, cnt);
    for (uint i = 0; i < cnt; i++)    {
        *((int*)(cb->cpu_buffer + c->offset + c->arr_stride*i)) = ns[i];
    }
}

void gfx_cb_setui(struct gfx_cblock* cb, uint name_hash, uint n)
{
	const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
	ASSERT(c);
	ASSERT(c->type == gfxUniformType::UINT);
	*((uint*)(cb->cpu_buffer + c->offset)) = n;
}

void gfx_cb_set3mv(struct gfx_cblock* cb, uint name_hash, const struct mat3f* mv, uint cnt)
{
	const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
	ASSERT(c);
	ASSERT(c->type == gfxUniformType::MAT4x3);

	uint mat_cnt = minui(c->arr_size, cnt);
    uint8* buff = cb->cpu_buffer + c->offset;
	for (uint i = 0; i < mat_cnt; i++)	{
        struct mat3f_cm* mcm = (struct mat3f_cm*)buff + i;
        mat3f_togpu(mcm, &mv[i]);
	}
}

void gfx_cb_set3mv_offset(struct gfx_cblock* cb, uint name_hash, const struct mat3f* mats,
    uint mat_cnt, uint offset)
{
    ASSERT((offset + mat_cnt*sizeof(struct mat3f_cm)) < cb->buffer_size);

    if (cb->is_tbuff)   {
        uint8* buff = cb->cpu_buffer + offset;
        for (uint i = 0; i < mat_cnt; i++)    {
            struct mat3f_cm* mcm = (struct mat3f_cm*)buff + i;
            mat3f_togpu(mcm, &mats[i]);
        }
    }   else    {
        const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
        ASSERT(c);
        ASSERT(c->type == gfxUniformType::STRUCT);

        mat_cnt = minui(c->arr_size, mat_cnt);
        uint8* buff = cb->cpu_buffer + c->offset + offset;
        for (uint i = 0; i < mat_cnt; i++)    {
            struct mat3f_cm* mcm = (struct mat3f_cm*)buff + i;
            mat3f_togpu(mcm, &mats[i]);
        }
    }
    cb->end_offset = offset + mat_cnt*sizeof(struct mat3f_cm);
}

void gfx_cb_set3mvp(struct gfx_cblock* cb, uint name_hash, const struct mat3f** mvp, uint cnt)
{
	const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
	ASSERT(c);
	ASSERT(c->type == gfxUniformType::MAT4x3);

	uint mat_cnt = minui(c->arr_size, cnt);
    uint8* buff = cb->cpu_buffer + c->offset;
	for (uint i = 0; i < mat_cnt; i++)	{
        struct mat3f_cm* mcm = (struct mat3f_cm*)buff + i;
        mat3f_togpu(mcm, mvp[i]);
	}
}

void gfx_cb_set4mv(struct gfx_cblock* cb, uint name_hash, const struct mat4f* mv, uint cnt)
{
	const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
	ASSERT(c);
	ASSERT(c->type == gfxUniformType::MAT4x4);

	uint mat_cnt = minui(c->arr_size, cnt);
    uint8* buff = cb->cpu_buffer + c->offset;
	for (uint i = 0; i < mat_cnt; i++)	{
        struct mat4f_cm* mcm = (struct mat4f_cm*)buff + i;
        mat4f_togpu(mcm, &mv[i]);
	}
}

void gfx_cb_set4fv(struct gfx_cblock* cb, uint name_hash, const struct vec4f* vv, uint cnt)
{
	const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
	ASSERT(c);
	ASSERT(c->type == gfxUniformType::FLOAT4);
	cnt = minui(c->arr_size, cnt);
	for (uint i = 0; i < cnt; i++)	{
        float* v = (float*)(cb->cpu_buffer + c->offset + c->arr_stride*i);
        v[0] = vv[i].x;
        v[1] = vv[i].y;
        v[2] = vv[i].z;
        v[3] = vv[i].w;
	}
}

void gfx_cb_setp(struct gfx_cblock* cb, uint name_hash, const void* sdata, uint size)
{
    const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
    ASSERT(c);
    ASSERT(c->type == gfxUniformType::STRUCT);

    uint s = minui(size, c->arr_stride*c->arr_size);
    memcpy(cb->cpu_buffer + c->offset, sdata, s);
}

void gfx_cb_setpv_offset(struct gfx_cblock* cb, uint name_hash, const void* sdata, uint size,
    uint offset)
{
    ASSERT((offset + size) < cb->buffer_size);

    if (cb->is_tbuff)   {
        memcpy(cb->cpu_buffer + offset, sdata, size);
        cb->end_offset = offset + size;
    }   else    {
        const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
        ASSERT(c);
        ASSERT(c->type == gfxUniformType::STRUCT);

        uint s = minui(size, c->elem_size);
        memcpy(cb->cpu_buffer + c->offset + offset, sdata, s);
        cb->end_offset = offset + s;
    }
}

void gfx_cb_setpv(struct gfx_cblock* cb, uint name_hash, const void* sdata, uint size,
    uint cnt)
{
    const struct gfx_constant_desc* c = shader_find_constantcb(cb, name_hash);
    ASSERT(c);
    ASSERT(c->type == gfxUniformType::STRUCT);

    uint s = minui(size, c->elem_size);
    cnt = minui(cnt, c->arr_size);
    for (uint i = 0; i < cnt; i++)
        memcpy(cb->cpu_buffer + c->offset + c->arr_stride*i, (const uint8*)sdata + i*size, s);
}

void gfx_cb_set_endoffset(struct gfx_cblock* cb, uint offset)
{
    cb->end_offset = minui(offset, cb->buffer_size);
}

struct gfx_shader* gfx_shader_get(uint shader_id)
{
    ASSERT(shader_id <= (uint)g_shader_mgr.shaders.item_cnt && shader_id != 0);
    return ((struct gfx_shader**)g_shader_mgr.shaders.buffer)[shader_id-1];
}

void* shader_includesource(struct allocator* alloc, const char* filepath, const char* include_code,
                           void* src, uint src_size, OUT uint* outsize)
{
    ASSERT(include_code);
    size_t s;
    void* buffer;

    if (include_code[0] != 0)   {
        char dir_buf[DH_PATH_MAX+20];

#if defined(_D3D_)
        sprintf(dir_buf, "#line 1 \"%s\"\n", filepath);
#else
        strcpy(dir_buf, "#line 1\n");
#endif
        size_t dir_size = strlen(dir_buf);
        size_t incl_size = strlen(include_code);

        s = incl_size + dir_size + src_size;
        buffer = A_ALLOC(alloc, s, MID_GFX);
        if (buffer != NULL) {
            memcpy(buffer, include_code, incl_size);
            memcpy((uint8*)buffer + incl_size, dir_buf, dir_size);
            memcpy((uint8*)buffer + incl_size + dir_size, src, src_size);
            A_FREE(alloc, src);
        }
    }   else    {
        buffer = src;
        s = src_size;
    }

    *outsize = (uint)s;
    return buffer;
}

/* tbuffers are special kind of cblocks where memory resides in global texture space
 * we limit the structures to one variable (can be array) only with a fixed given name and size
 * so the implementation is same in d3d/gl because there is no need to query variables from shader
 */
struct gfx_cblock* gfx_shader_create_cblock_tbuffer(struct allocator* alloc,
    struct gfx_shader* shader, const char* tb_name, uint tb_size)
{
    size_t total_sz = sizeof(struct gfx_cblock) + tb_size;
    uint8* buff = (uint8*)A_ALIGNED_ALLOC(alloc, total_sz, MID_GFX);
    if (buff == NULL)   {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return NULL;
    }

    struct gfx_cblock* cb = (struct gfx_cblock*)buff;
    buff += sizeof(struct gfx_cblock);
    memset(cb, 0x0, sizeof(struct gfx_cblock));
    cb->alloc = alloc;

    /* name hash */
    cb->name_hash = hash_str(tb_name);
    cb->is_tbuff = TRUE;

    /* buffers (gpu/cpu) */
    cb->cpu_buffer = buff;
    cb->gpu_buffer = gfx_create_buffer(gfxBufferType::SHADER_TEXTURE, gfxMemHint::DYNAMIC, tb_size,
        NULL, 0);
    if (cb->cpu_buffer == NULL || cb->gpu_buffer == NULL)		{
        gfx_shader_destroy_cblock(cb);
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return NULL;
    }
    cb->buffer_size = tb_size;
    cb->end_offset = tb_size;
    memset(cb->cpu_buffer, 0x00, tb_size);

    return cb;
}
