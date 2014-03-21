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
#include "world-mgr.h"
#include "camera.h"

#define CAM_FOV 60.0f
#define CAM_NEAR 0.1f
#define CAM_FAR 1000.0f

/*************************************************************************************************
 * Types
 */
struct wld_var
{
    char name[32];
    struct variant v;
    pfn_wld_varchanged change_fn;
    void* param;
};

struct wld_section
{
    char name[32];
    struct hashtable_open vtable;   /* key: var name, value: index to vars */
    struct array vars;  /* item: wld_var */
};

struct wld_data
{
    struct hashtable_open stable; /* table for sections, key: section-name, value: section-ID */
    struct array sections; /* item: wld_section */
    struct camera* cam;
    struct camera default_cam;
};

/*************************************************************************************************
 * Fwd
 */
void wld_destroy_section(struct wld_section* s);


/*************************************************************************************************
 * Globals
 */
static struct wld_data g_wld;

/*************************************************************************************************/
void wld_zero()
{
    memset(&g_wld, 0x00, sizeof(g_wld));
}

result_t wld_initmgr()
{
    result_t r;
    r = hashtable_open_create(mem_heap(), &g_wld.stable, 5, 10, 0);
    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "Initializing world manager failed");
        return r;
    }

    r = arr_create(mem_heap(), &g_wld.sections, sizeof(struct wld_section), 5, 10, 0);
    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "Initializing world manager failed");
        return r;
    }

    /* camera */
    struct vec3f pos;
    cam_init(&g_wld.default_cam, vec3_setf(&pos, 0.0f, 0.0f, -1.0f), &g_vec3_zero, CAM_NEAR,
        CAM_FAR, math_torad(CAM_FOV));
    g_wld.cam = &g_wld.default_cam;

    return RET_OK;
}

void wld_releasemgr()
{
    for (uint i = 0; i < g_wld.sections.item_cnt; i++)    {
        struct wld_section* s = &((struct wld_section*)g_wld.sections.buffer)[i];
        wld_destroy_section(s);
    }

    arr_destroy(&g_wld.sections);
    hashtable_open_destroy(&g_wld.stable);
    wld_zero();
}

uint wld_register_section(const char* name)
{
    ASSERT(strlen(name) < 32);

    result_t r;
    uint hashval = hash_str(name);
    struct hashtable_item* item = hashtable_open_find(&g_wld.stable, hashval);
    if (item != NULL)   {
        err_printf(__FILE__, __LINE__, "world-mgr: section '%s' already exists", name);
        return 0;
    }

    struct wld_section* section = (struct wld_section*)arr_add(&g_wld.sections);
    if (section == NULL)    {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return 0;
    }

    /* initialize section */
    str_safecpy(section->name, sizeof(section->name), name);
    r = hashtable_open_create(mem_heap(), &section->vtable, 10, 20, 0);
    if (IS_FAIL(r)) {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return 0;
    }
    r = arr_create(mem_heap(), &section->vars, sizeof(struct wld_var), 10, 20, 0);
    if (IS_FAIL(r)) {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        wld_destroy_section(section);
        return 0;
    }

    /* add */
    uint id = g_wld.sections.item_cnt;
    if (IS_FAIL(hashtable_open_add(&g_wld.stable, hashval, id))) {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        wld_destroy_section(section);
        return 0;
    }

    return id;
}

void wld_destroy_section(struct wld_section* s)
{
    hashtable_open_destroy(&s->vtable);
    arr_destroy(&s->vars);
}

uint wld_register_var(uint section_id, const char* name, enum variant_type type,
                        pfn_wld_varchanged change_fn, void* param)
{
    ASSERT(section_id != 0);
    ASSERT(strlen(name) < 32);
    ASSERT(type != VARIANT_NULL);

    struct wld_section* s = &((struct wld_section*)g_wld.sections.buffer)[section_id - 1];

    uint hashval = hash_str(name);
    struct hashtable_item* item = hashtable_open_find(&s->vtable, hashval);
    if (item != NULL)   {
        err_printf(__FILE__, __LINE__, "world-mgr: var '%s' already exists in '%s'", name, s->name);
        return 0;
    }

    struct wld_var* v = (struct wld_var*)arr_add(&s->vars);
    uint id = s->vars.item_cnt;
    if (v == NULL || IS_FAIL(hashtable_open_add(&s->vtable, hashval, id)))  {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return 0;
    }

    memset(v, 0x00, sizeof(struct wld_var));
    str_safecpy(v->name, sizeof(v->name), name);
    v->change_fn = change_fn;
    v->param = param;
    v->v.type = type;

    return id;
}

uint wld_find_section(const char* name)
{
    struct hashtable_item* item = hashtable_open_find(&g_wld.stable, hash_str(name));
    if (item != NULL)
        return (uint)item->value;

    return 0;
}

uint wld_find_var(uint section_id, const char* name)
{
    ASSERT(section_id != 0);
    struct wld_section* s = &((struct wld_section*)g_wld.sections.buffer)[section_id - 1];

    struct hashtable_item* item = hashtable_open_find(&s->vtable, hash_str(name));
    if (item != NULL)
        return (uint)item->value;
    return 0;
}

const struct variant* wld_get_var(uint section_id, uint var_id)
{
    ASSERT(var_id != 0);
    ASSERT(section_id != 0);

    struct wld_section* s = &((struct wld_section*)g_wld.sections.buffer)[section_id - 1];
    return &((struct wld_var*)s->vars.buffer)[var_id - 1].v;
}

void wld_set_var(uint section_id, uint var_id, const struct variant* var)
{
    ASSERT(var_id != 0);
    ASSERT(section_id != 0);

    struct wld_section* s = &((struct wld_section*)g_wld.sections.buffer)[section_id - 1];
    struct wld_var* v = &((struct wld_var*)s->vars.buffer)[var_id - 1];
    var_setv(&v->v, var);

    if (v->change_fn != NULL)
        v->change_fn(&v->v, v->param);
}

void wld_set_cam(struct camera* cam)
{
    if (cam != NULL)
        g_wld.cam = cam;
    else
        g_wld.cam = &g_wld.default_cam;
}

struct camera* wld_get_cam()
{
    return g_wld.cam;
}
