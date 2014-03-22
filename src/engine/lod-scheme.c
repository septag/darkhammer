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
#include "dhcore/hash-table.h"
#include "dhcore/json.h"
#include "dhcore/file-io.h"

#include "lod-scheme.h"
#include "mem-ids.h"

/*************************************************************************************************
 * types
 */
struct lod_mgr
{
    int model_cnt;   /* number of model schemes */
    int light_cnt;   /* number of light schemes */
    struct hashtable_fixed model_table;
    struct lod_model_scheme* model_schemes;
    struct hashtable_fixed light_table;
    struct lod_light_scheme* light_schemes;
};

/*************************************************************************************************
 * globals
 */
struct lod_mgr g_lod;

/*************************************************************************************************/
void lod_zero()
{
    memset(&g_lod, 0x00, sizeof(struct lod_mgr));
}

result_t lod_initmgr()
{
    result_t r;
    bool_t has_default;

    /* open lod-scheme.json file and read scheme data */
    file_t f = fio_openmem(mem_heap(), "lod-scheme.json", FALSE, MID_BASE);
    if (f == NULL) {
        err_print(__FILE__, __LINE__, "lod-init failed: could not open lod-scheme.json");
        return RET_FAIL;
    }

    json_t jroot = json_parsefile(f, mem_heap());
    fio_close(f);

    if (jroot == NULL)  {
        err_print(__FILE__, __LINE__, "lod-init failed: invalid lod-scheme.json");
        return RET_FAIL;
    }

    /**********************************************************************************************/
    /* model schemes */
    json_t jmodel = json_getitem(jroot, "model");
    if (jmodel == NULL || json_getarr_count(jmodel) == 0)    {
        json_destroy(jroot);
        err_print(__FILE__, __LINE__, "lod-init failed: at least 1 model scheme should exist");
        return RET_FAIL;
    }

    g_lod.model_cnt = json_getarr_count(jmodel);
    r = hashtable_fixed_create(mem_heap(), &g_lod.model_table, g_lod.model_cnt, MID_BASE);
    g_lod.model_schemes = (struct lod_model_scheme*)
        ALLOC(sizeof(struct lod_model_scheme)*g_lod.model_cnt, MID_BASE);
    if (IS_FAIL(r) || g_lod.model_schemes == NULL) {
        json_destroy(jroot);
        return RET_OUTOFMEMORY;
    }
    has_default = FALSE;
    for (int i = 0; i < g_lod.model_cnt; i++)    {
        json_t js = json_getarr_item(jmodel, i);
        const char* name = json_gets_child(js, "name", "");
        has_default = str_isequal(name, "default");

        struct lod_model_scheme* s = &g_lod.model_schemes[i];
        str_safecpy(s->name, sizeof(s->name), name);
        s->high_range = maxf(json_getf_child(js, "high-range", 1.0f), 1.0f);
        s->medium_range = maxf(json_getf_child(js, "medium-range", 2.0f), 2.0f);
        s->low_range = maxf(json_getf_child(js, "low-range", 3.0f), 3.0f);

        hashtable_fixed_add(&g_lod.model_table, hash_str(name), i+1);
    }
    if (!has_default)   {
        json_destroy(jroot);
        err_print(__FILE__, __LINE__, "lod-init failed: model 'default' scheme does not exist");
        return RET_FAIL;
    }

    /**********************************************************************************************/
    /* light schemes */
    json_t jlight = json_getitem(jroot, "light");
    if (jlight == NULL || json_getarr_count(jlight) == 0)  {
        json_destroy(jroot);
        err_print(__FILE__, __LINE__, "lod-init failed: at least 1 model scheme should exist");
        return RET_FAIL;
    }
    g_lod.light_cnt = json_getarr_count(jlight);
    r = hashtable_fixed_create(mem_heap(), &g_lod.light_table, g_lod.light_cnt, MID_BASE);
    g_lod.light_schemes = (struct lod_light_scheme*)
        ALLOC(sizeof(struct lod_light_scheme)*g_lod.light_cnt, MID_BASE);
    if (IS_FAIL(r) || g_lod.light_schemes == NULL)  {
        json_destroy(jroot);
        return RET_OUTOFMEMORY;
    }
    has_default = FALSE;
    for( int i = 0; i < g_lod.light_cnt; i++) {
        json_t js = json_getarr_item(jlight, i);
        const char* name = json_gets_child(js, "name", "");
        has_default = str_isequal(name, "default");

        struct lod_light_scheme* l = &g_lod.light_schemes[i];
        str_safecpy(l->name, sizeof(l->name), name);
        l->vis_range = maxf(json_getf_child(js, "vis-range", 1.0f), 1.0f);

        hashtable_fixed_add(&g_lod.light_table, hash_str(name), i+1);
    }
    if (!has_default)   {
        json_destroy(jroot);
        err_print(__FILE__, __LINE__, "lod-init failed: light 'default' scheme does not exist");
        return RET_FAIL;
    }

    json_destroy(jroot);
    return RET_OK;
}

void lod_releasemgr()
{
    if (g_lod.model_schemes != NULL)
        FREE(g_lod.model_schemes);
    if (g_lod.light_schemes != NULL)
        FREE(g_lod.light_schemes);

    hashtable_fixed_destroy(&g_lod.model_table);
    hashtable_fixed_destroy(&g_lod.light_table);

    lod_zero();
}

uint lod_findmodelscheme(const char* name)
{
    struct hashtable_item* item = hashtable_fixed_find(&g_lod.model_table, hash_str(name));
    if (item != NULL)
        return (uint)item->value;

    return 0;
}

const struct lod_model_scheme* lod_getmodelscheme(uint id)
{
    ASSERT(id > 0 && id <= (uint)g_lod.model_cnt);
    return &g_lod.model_schemes[id - 1];
}

uint lod_findlightscheme(const char* name)
{
    struct hashtable_item* item = hashtable_fixed_find(&g_lod.light_table, hash_str(name));
    if (item != NULL)
        return (uint)item->value;

    return 0;
}

const struct lod_light_scheme* lod_getlightscheme(uint id)
{
    ASSERT(id > 0 && id <= (uint)g_lod.light_cnt);
    return &g_lod.light_schemes[id - 1];
}
