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

#include "dheng/cmp-mgr.h"

#include "dhcore/core.h"
#include "dhcore/array.h"
#include "dhcore/pool-alloc.h"
#include "dhcore/linked-list.h"
#include "dhcore/hash-table.h"

#include "share/mem-ids.h"
#include "dheng/gfx.h"
#include "dheng/prf-mgr.h"
#include "dheng/scene-mgr.h"
#include "dheng/console.h"
#include "dheng/engine.h"

#define CHAIN_POOLSIZE  1000

/*************************************************************************************************
 * types
 */
struct cmp_component
{
    char name[32];
    uint id;  /* unique id */
    cmptype_t type;
    uint flags;   /* cmp_flag */
    pfn_cmp_create create_func;
    pfn_cmp_destroy destroy_func;
    pfn_cmp_update update_funcs[CMP_UPDATE_MAXSTAGE]; /* for each stage (each elem can be NULL) */
    pfn_cmp_debug debug_func;

    uint stride; /* single data size */
    uint cur_idx; /* end index of the data_buff */
    uint instance_cnt;
    uint grow_cnt;
    uint update_cnt;  /* number of updates in update_refs */
    struct allocator* alloc;
    uint* indexes; /* indexes to valid data slots */
    struct cmp_instance_desc* instances;
    struct cmp_instance_desc** update_refs; /* instances that needs to be updated */
    uint8* data_buff;

    uint value_cnt;   /* number of each component values */
    const struct cmp_value* values; /* pointer to static values defined in each component header */
    struct hashtable_fixed value_table;    /* hashtable to access values fast */
};

struct cmp_mgr
{
    struct array cmps;  /* item: cmp_t */
    struct pool_alloc chainnode_pool; /* item: cmp_chain_node */
    struct array deferred_instances;    /* item: cmphandle_t */
    cmp_chain debug_list;   /* debug items */

    struct allocator* alloc;    /* used for modify */
    struct allocator* tmp_alloc; /* used for modify */
};

/*************************************************************************************************
 * fwd declarations
 */
result_t cmp_register_main_components();    /* implemented in cmp-register-main.c */
uint8* cmp_get_valuedata(cmphandle_t hdl, const char* name, struct cmp_obj** phost,
    pfn_cmp_modify* pmod_fn, const struct cmp_value** pcval);
cmp_t cmp_create_component(struct allocator* alloc, const struct cmp_createparams* params);
void cmp_destroy_component(cmp_t c);
result_t cmp_grow_component(cmp_t c);
void cmp_setcommonhdl(struct cmp_obj* obj, cmphandle_t hdl, cmptype_t type);

result_t cmp_console_debug(uint argc, const char ** argv, void* param);
result_t cmp_console_undebug(uint argc, const char ** argv, void* param);
void cmp_update_hdl_inchain(cmp_chain chain, cmphandle_t cur_hdl, cmphandle_t new_hdl);

/*************************************************************************************************
 * globals
 */
struct cmp_mgr g_cmp;

/*************************************************************************************************
 * inlines
 */
INLINE struct cmp_instance_desc* cmp_get_inst(cmphandle_t hdl)
{
    uint16 idx = CMP_GET_INDEX(hdl);
    uint i_idx = CMP_GET_INSTANCEINDEX(hdl);
    cmp_t c = ((cmp_t*)g_cmp.cmps.buffer)[idx];
    return &c->instances[c->indexes[i_idx]];
}


/*************************************************************************************************/
void cmp_zero()
{
    memset(&g_cmp, 0x00, sizeof(g_cmp));
}

result_t cmp_initmgr()
{
    result_t r;

    log_print(LOG_TEXT, "init component-mgr ...");

    g_cmp.alloc = mem_heap();
    g_cmp.tmp_alloc = mem_heap();

    r = arr_create(mem_heap(), &g_cmp.cmps, sizeof(cmp_t), 100, 100, MID_CMP);
    if (IS_FAIL(r)) {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return RET_OUTOFMEMORY;
    }

    r = mem_pool_create(mem_heap(), &g_cmp.chainnode_pool, sizeof(struct cmp_chain_node),
        CHAIN_POOLSIZE, MID_CMP);
    if (IS_FAIL(r)) {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return RET_OUTOFMEMORY;
    }

    r = arr_create(mem_heap(), &g_cmp.deferred_instances, sizeof(cmphandle_t), 100, 100, MID_CMP);
    if (IS_FAIL(r)) {
        err_printn(__FILE__, __LINE__, RET_OUTOFMEMORY);
        return RET_OUTOFMEMORY;
    }

    log_print(LOG_INFO, "\tregistering main components ...");
    r = cmp_register_main_components();
    if (IS_FAIL(r)) {
        err_print(__FILE__, __LINE__, "cmp-mgr init failed: could not register main components");
        return RET_FAIL;
    }

    /* console commands */
    if (BIT_CHECK(eng_get_params()->flags, static_cast<uint>(appEngineFlags::CONSOLE)))   {
        con_register_cmd("debug", cmp_console_debug, NULL, "debug [obj-name] [component-name]");
        con_register_cmd("undebug", cmp_console_undebug, NULL, "undebug [obj-name] [component-name]");
    }

    return RET_OK;
}

void cmp_releasemgr()
{
	/* release registered components */
	for (int i = 0; i < g_cmp.cmps.item_cnt; i++)	{
		cmp_t c = ((cmp_t*)g_cmp.cmps.buffer)[i];
		cmp_destroy_component(c);
	}

	arr_destroy(&g_cmp.cmps);
	arr_destroy(&g_cmp.deferred_instances);
	mem_pool_destroy(&g_cmp.chainnode_pool);

    cmp_zero();

    log_print(LOG_TEXT, "component-mgr released.");
}

result_t cmp_register_component(struct allocator* alloc, const struct cmp_createparams* params)
{
    cmp_t c = cmp_create_component(alloc, params);
    if (c == NULL)  {
        err_printf(__FILE__, __LINE__, "cmp-register failed: could not register component '%s'",
        		params->name);
        return RET_FAIL;
    }

    /* add to component array */
    cmp_t* pc = (cmp_t*)arr_add(&g_cmp.cmps);
    if (pc == NULL)  {
        err_print(__FILE__, __LINE__, "cmp-register failed: out of memory");
        return RET_OUTOFMEMORY;
    }

    /* assign ID (index+1) */
    c->id = g_cmp.cmps.item_cnt;
    *pc = c;
    return RET_OK;
}

cmp_t cmp_create_component(struct allocator* alloc, const struct cmp_createparams* params)
{
    cmp_t c = (cmp_t)A_ALLOC(alloc, sizeof(struct cmp_component), MID_CMP);
    if (c == NULL)
        return NULL;
    memset(c, 0x00, sizeof(struct cmp_component));

    /* save params */
    strcpy(c->name, params->name);
    c->type = params->type;
    c->flags = params->flags;
    c->create_func = params->create_func;
    c->destroy_func = params->destroy_func;
    c->debug_func = params->debug_func;

    for (uint i = 0; i < CMP_UPDATE_MAXSTAGE; i++)
        c->update_funcs[i] = params->update_funcs[i];
    c->alloc = alloc;
    c->stride = params->stride;
    c->grow_cnt = params->grow_cnt;
    c->instance_cnt = params->initial_cnt;
    c->value_cnt = params->value_cnt;
    c->values = params->values;

    /* create buffers */
    uint cnt = params->initial_cnt;
    c->indexes = (uint*)A_ALLOC(alloc, sizeof(uint)*cnt, MID_CMP);
    c->instances = (struct cmp_instance_desc*)A_ALLOC(alloc, sizeof(struct cmp_instance_desc)*cnt,
        MID_CMP);
    c->update_refs = (struct cmp_instance_desc**)A_ALLOC(alloc,
        sizeof(struct cmp_instance_desc*)*cnt, MID_CMP);
    c->data_buff = (uint8*)A_ALIGNED_ALLOC(alloc, params->stride*cnt, MID_CMP);
    if (c->indexes == NULL || c->instances == NULL || c->data_buff == NULL) {
        cmp_destroy_component(c);
        return NULL;
    }

    /* fillup indexes and instances */
    for (uint i = 0; i < cnt; i++)    {
        c->indexes[i] = i;
        c->instances[i].host = NULL;
        c->instances[i].data = c->data_buff + i*params->stride;
        c->instances[i].updatelist_idx = INVALID_INDEX;
    }

    /* value table */
    if (IS_FAIL(hashtable_fixed_create(alloc, &c->value_table, params->value_cnt, MID_CMP)))   {
        cmp_destroy_component(c);
        return NULL;
    }
    for (uint i = 0; i < params->value_cnt; i++)
        hashtable_fixed_add(&c->value_table, hash_str(c->values[i].name), i);

    return c;
}

void cmp_destroy_component(cmp_t c)
{
    ASSERT(c);
    ASSERT(c->alloc);

    struct allocator* alloc = c->alloc;

    if (c->indexes != NULL)
        A_FREE(alloc, c->indexes);
    if (c->instances != NULL)
        A_FREE(alloc, c->instances);
    if (c->update_refs != NULL)
        A_FREE(alloc, c->update_refs);
    if (c->data_buff != NULL)
        A_ALIGNED_FREE(alloc, c->data_buff);

    hashtable_fixed_destroy(&c->value_table);

    A_FREE(alloc, c);
}

result_t cmp_grow_component(cmp_t c)
{
    /* realloc */
    uint nsize = c->instance_cnt + c->grow_cnt;
    struct allocator* alloc = c->alloc;

    uint* indexes = (uint*)A_ALLOC(alloc, sizeof(uint)*nsize, MID_CMP);
    struct cmp_instance_desc* instances = (struct cmp_instance_desc*)A_ALLOC(alloc,
        sizeof(struct cmp_instance_desc)*nsize, MID_CMP);
    struct cmp_instance_desc** update_refs = (struct cmp_instance_desc**)A_ALLOC(alloc,
        sizeof(struct cmp_instance_desc*)*nsize, MID_CMP);
    void* buffer = A_ALIGNED_ALLOC(alloc, c->stride*nsize, MID_CMP);
    if (buffer == NULL || indexes == NULL || instances == NULL)
        return RET_OUTOFMEMORY;

    memcpy(indexes, c->indexes, sizeof(uint)*c->instance_cnt);
    memcpy(instances, c->instances, sizeof(struct cmp_instance_desc)*c->instance_cnt);
    memcpy(buffer, c->data_buff, c->stride*c->instance_cnt);
    uint stride = c->stride;

    /* rebuild current data which is invalidated */
    for (uint i = 0; i < c->instance_cnt; i++)    {
        uint idx = indexes[i];
        instances[idx].data = (uint8*)buffer + idx*stride;
    }

    for (uint i = 0; i < c->update_cnt; i++)  {
        uint idx = (uint)(c->update_refs[i] - c->instances);
        update_refs[i] = &instances[idx];
    }

    /* reassign grown buffer data */
    for (uint i = c->instance_cnt; i < nsize; i++)    {
        indexes[i] = i;
        instances[i].host = NULL;
        instances[i].data = (uint8*)buffer + i*stride;
        instances[i].updatelist_idx = INVALID_INDEX;
    }

    A_FREE(alloc, c->indexes);
    A_FREE(alloc, c->instances);
    A_FREE(alloc, c->update_refs);
    A_ALIGNED_FREE(alloc, c->data_buff);

    c->indexes = indexes;
    c->instances = instances;
    c->update_refs = update_refs;
    c->data_buff = (uint8*)buffer;

    /* increase number of instances */
    c->instance_cnt = nsize;

    return RET_OK;
}

cmphandle_t cmp_create_instance_forobj(const char* cmpname, struct cmp_obj* obj)
{
    cmp_t c = cmp_findname(cmpname);
    if (c != NULL)  {
        return cmp_create_instance(c, obj, 0, INVALID_HANDLE, 0);
    }   else    {
        return INVALID_HANDLE;
    }
}


cmphandle_t cmp_create_instance(cmp_t c, struct cmp_obj* obj, uint flags,
    OPTIONAL cmphandle_t parent_hdl, OPTIONAL uint offset_in_parent)
{
    result_t r = RET_OK;
    uint idx = c->cur_idx;
    if (idx == c->instance_cnt) {
        if (IS_FAIL(cmp_grow_component(c)))
            return INVALID_HANDLE;
    }

    uint r_idx = c->indexes[idx];
    struct cmp_instance_desc* inst = &c->instances[r_idx];

    cmphandle_t hdl = CMP_MAKE_HANDLE(c->type, c->id-1, idx);

    /* initialize instance description */
    memset(inst->data, 0x00, c->stride);
    inst->host = obj;
    inst->flags = flags;
    inst->updatelist_idx = INVALID_INDEX;
    inst->hdl = hdl;
    inst->parent_hdl = INVALID_HANDLE;
    inst->childs = NULL;
    inst->offset_in_parent = offset_in_parent;

    struct cmp_chain_node* chain_node = (struct cmp_chain_node*)
        mem_pool_alloc(&g_cmp.chainnode_pool);
    if (chain_node == NULL)
        return INVALID_HANDLE;
    chain_node->hdl = hdl;

    if (!BIT_CHECK(flags, CMP_INSTANCEFLAG_INDIRECTHOST))   {
        ASSERT(obj != NULL);
        list_add(&obj->chain, &chain_node->node, chain_node);
    }   else    {
        ASSERT(parent_hdl != INVALID_HANDLE);
        inst->parent_hdl = parent_hdl;
        struct cmp_instance_desc* parent_inst = cmp_get_inst(parent_hdl);
        list_add(&parent_inst->childs, &chain_node->node, chain_node);
    }

    c->cur_idx ++;

    /* call create callback function */
    if (c->create_func != NULL)
        r = c->create_func(obj, inst->data, hdl);

    /* add to update list */
    cmp_updateinstance(hdl);

    /* if component is deferred-modify, add to deferred update list */
    if (BIT_CHECK(c->flags, CMP_FLAG_DEFERREDMODIFY))   {
        cmphandle_t* pdefhdl = (cmphandle_t*)arr_add(&g_cmp.deferred_instances);
        *pdefhdl = hdl;
    }

    return (r == RET_OK) ? hdl : INVALID_HANDLE;
}

void cmp_destroy_instance(cmphandle_t hdl)
{
    uint c_idx = CMP_GET_INDEX(hdl);
    ASSERT(c_idx < (uint)g_cmp.cmps.item_cnt);

    cmp_updateinstance_reset(hdl);

#if !defined(_RETAIL_)
    cmp_debug_remove(hdl);
#endif

    cmp_t c = ((cmp_t*)g_cmp.cmps.buffer)[c_idx];
    uint idx = CMP_GET_INSTANCEINDEX(hdl);
    uint r_idx = c->indexes[idx];

    struct cmp_instance_desc* instance = &c->instances[r_idx];

    /* call destroy callback */
    if (c->destroy_func != NULL)
        c->destroy_func(instance->host, instance->data, hdl);

    if (!BIT_CHECK(instance->flags, CMP_INSTANCEFLAG_INDIRECTHOST)) {
        struct cmp_obj* host_obj = instance->host;
        ASSERT(host_obj != NULL);

        /* move through the list and find the node with specified handle, then remove it */
        struct linked_list* node = host_obj->chain;
        while (node != NULL)    {
            struct cmp_chain_node* chnode = (struct cmp_chain_node*)node->data;
            if (chnode->hdl == hdl) {
                list_remove(&host_obj->chain, node);
                mem_pool_free(&g_cmp.chainnode_pool, chnode);
                break;
            }
            node = node->next;
        }
    }   else    {
        ASSERT(instance->parent_hdl != INVALID_HANDLE);

        struct cmp_instance_desc* parent_inst = cmp_get_inst(instance->parent_hdl);
        struct linked_list* lnode = parent_inst->childs;
        while (lnode != NULL)   {
            struct cmp_chain_node* chnode = (struct cmp_chain_node*)lnode->data;
            if (chnode->hdl == hdl) {
                list_remove(&parent_inst->childs, lnode);
                mem_pool_free(&g_cmp.chainnode_pool, chnode);
                break;
            }
            lnode = lnode->next;
        }
    }

    /* if component instances is not the last item in the component database
     * do the swap trick to remove it
     * swap trick works by swapping the instance index with the last one
     * .. and we have to update handle of the last instance (because it's index is changed) in all
     * of it's referenced datas
     */
    uint last_idx = c->cur_idx - 1;
    if (idx != last_idx)    {
        /* 1) swap cur value index by the last value index, so last instance become cur index
         * 2) modify cur handle (previously last item) to point to new index
         */
        uint last_ridx = c->indexes[last_idx];
        struct cmp_instance_desc* last_instance = &c->instances[last_ridx];
        swapui(&c->indexes[last_idx], &c->indexes[idx]);

        /* handle of the last instance, should be replaced with current handle
         * check and swap in debug list too */
#if !defined(_RETAIL_)
        cmp_update_hdl_inchain(g_cmp.debug_list, last_instance->hdl, hdl);
#endif

        /* for direct components, we have to set it's possible shortcut handle for object and also
         * change handle inside component chain (wether it's inside indirect or inside object) */
        if (!BIT_CHECK(last_instance->flags, CMP_INSTANCEFLAG_INDIRECTHOST))    {
            ASSERT(last_instance->host);
            cmp_update_hdl_inchain(last_instance->host->chain, last_instance->hdl, hdl);
            cmp_setcommonhdl(last_instance->host, hdl, c->type);
        }    else   {
            ASSERT(last_instance->parent_hdl != INVALID_HANDLE);
            struct cmp_instance_desc* parent_inst = cmp_get_inst(last_instance->parent_hdl);
            cmp_update_hdl_inchain(parent_inst->childs, last_instance->hdl, hdl);
            *((cmphandle_t*)(parent_inst->data + last_instance->offset_in_parent)) = hdl;
        }

        /* change parent handles of children */
        struct linked_list* lnode = last_instance->childs;
        while (lnode != NULL)   {
            struct cmp_chain_node* chnode = (struct cmp_chain_node*)lnode->data;
            struct cmp_instance_desc* child_inst = cmp_get_inst(chnode->hdl);
            child_inst->parent_hdl = hdl;
            lnode = lnode->next;
        }

        last_instance->hdl = hdl;
    }

    /* destroy child components */
    struct linked_list* lnode = instance->childs;
    while (lnode != NULL)   {
        struct cmp_chain_node* chnode = (struct cmp_chain_node*)lnode->data;
        cmp_destroy_instance(chnode->hdl);
        lnode = lnode->next;
    }

    c->cur_idx --;
}

void cmp_updateinstance(cmphandle_t hdl)
{
    uint16 idx = CMP_GET_INDEX(hdl);
    uint i_idx = CMP_GET_INSTANCEINDEX(hdl);
    cmp_t c = ((cmp_t*)g_cmp.cmps.buffer)[idx];
    uint r_idx = c->indexes[i_idx];
    if (c->instances[r_idx].updatelist_idx == INVALID_INDEX)	{
    	c->update_refs[c->update_cnt] = &c->instances[r_idx];
    	c->instances[r_idx].updatelist_idx = c->update_cnt;
    	c->update_cnt ++;
    }
}

void cmp_updateinstance_reset(cmphandle_t hdl)
{
    uint16 idx = CMP_GET_INDEX(hdl);
    uint i_idx = CMP_GET_INSTANCEINDEX(hdl);
    cmp_t c = ((cmp_t*)g_cmp.cmps.buffer)[idx];
    uint r_idx = c->indexes[i_idx];
    if (c->instances[r_idx].updatelist_idx != INVALID_INDEX)	{
    	/* swap trick: swap with the last item in update list, and reassign the last one */
    	uint updatelist_idx = c->instances[r_idx].updatelist_idx;
    	swapptr((void**)&c->update_refs[updatelist_idx], (void**)&c->update_refs[c->update_cnt-1]);
    	c->update_refs[updatelist_idx]->updatelist_idx = updatelist_idx;
    	c->update_cnt --;

    	/* reset removed one from instances array */
    	c->instances[r_idx].updatelist_idx = INVALID_INDEX;
    }
}

void cmp_debug_add(cmphandle_t hdl)
{
	struct cmp_chain_node* chnode = (struct cmp_chain_node*)mem_pool_alloc(&g_cmp.chainnode_pool);
	ASSERT(chnode);
	chnode->hdl = hdl;
	list_add(&g_cmp.debug_list, &chnode->node, chnode);
}

void cmp_debug_remove(cmphandle_t hdl)
{
	struct linked_list* node = g_cmp.debug_list;
	while (node != NULL)	{
		struct cmp_chain_node* chnode = (struct cmp_chain_node*)node->data;
		if (chnode->hdl == hdl)	{
			list_remove(&g_cmp.debug_list, node);
			mem_pool_free(&g_cmp.chainnode_pool, chnode);
			break;
		}
		node = node->next;
	}
}

result_t cmp_prepare_deferredinstances(struct allocator* alloc, struct allocator* tmp_alloc)
{
	result_t r = RET_OK;
	uint cnt = g_cmp.deferred_instances.item_cnt;
	for (uint i = 0; i < cnt; i++)	{
		cmphandle_t hdl = ((cmphandle_t*)g_cmp.deferred_instances.buffer)[i];
		cmp_t c = cmp_getbyhdl(hdl);
		void* data = cmp_getinstancedata(hdl);
		struct cmp_obj* host_obj = cmp_getinstancehost(hdl);
		const struct cmp_value* values = c->values;
		uint value_cnt = c->value_cnt;

		for (uint k = 0; k < value_cnt; k++)	{
			if (values[k].modify_func != NULL &&
				IS_FAIL(values[k].modify_func(host_obj, alloc, tmp_alloc, data, hdl)))
			{
				log_printf(LOG_WARNING, "modify value '%s' for object '%s' failed", values[k].name,
						host_obj->name);
				r = RET_FAIL;
			}
		}

		arr_clear(&g_cmp.deferred_instances);
	}

	return r;
}

cmp_t cmp_findtype(cmptype_t type)
{
    uint cmp_cnt = g_cmp.cmps.item_cnt;
    cmp_t* cmps = (cmp_t*)g_cmp.cmps.buffer;
    for (uint i = 0; i < cmp_cnt; i++)    {
        if (cmps[i]->type == type)
            return cmps[i];
    }
    return NULL;
}

cmp_t cmp_findname(const char* name)
{
    uint cmp_cnt = g_cmp.cmps.item_cnt;
    cmp_t* cmps = (cmp_t*)g_cmp.cmps.buffer;
    for (uint i = 0; i < cmp_cnt; i++)    {
        if (str_isequal_nocase(cmps[i]->name, name))
            return cmps[i];
    }
    return NULL;
}

const char* cmp_getname(cmp_t c)
{
    return c->name;
}

uint cmp_getcount()
{
    return g_cmp.cmps.item_cnt;
}

cmphandle_t cmp_findinstance_inobj(struct cmp_obj* obj, const char* cmpname)
{
    cmp_t c = cmp_findname(cmpname);
    if (c != NULL)
        return cmp_findinstance(obj->chain, c->type);
    else
        return INVALID_HANDLE;
}

cmphandle_t cmp_findinstance_bytype_inobj(struct cmp_obj* obj, cmptype_t type)
{
    return cmp_findinstance(obj->chain, type);
}

cmphandle_t cmp_findinstance(cmp_chain chain, cmptype_t type)
{
    struct linked_list* node = chain;
    while (node != NULL)    {
        struct cmp_chain_node* chnode = (struct cmp_chain_node*)node->data;
        if (type == CMP_GET_TYPE(chnode->hdl))
            return chnode->hdl;
        node = node->next;
    }
    return INVALID_HANDLE;
}

cmp_t cmp_getbyhdl(cmphandle_t hdl)
{
    uint16 idx = CMP_GET_INDEX(hdl);
    return ((cmp_t*)g_cmp.cmps.buffer)[idx];
}

cmp_t cmp_getbyidx(uint idx)
{
    return ((cmp_t*)g_cmp.cmps.buffer)[idx];
}

void* cmp_getinstancedata(cmphandle_t hdl)
{
    return cmp_get_inst(hdl)->data;
}

uint cmp_getinstanceflags(cmphandle_t hdl)
{
    return cmp_get_inst(hdl)->flags;
}

struct cmp_obj* cmp_getinstancehost(cmphandle_t hdl)
{
    return cmp_get_inst(hdl)->host;
}

enum cmp_valuetype cmp_getvaluetype(cmphandle_t hdl, const char* name)
{
    uint16 idx = CMP_GET_INDEX(hdl);
    cmp_t c = ((cmp_t*)g_cmp.cmps.buffer)[idx];
    struct hashtable_item* item = hashtable_fixed_find(&c->value_table, hash_str(name));
    if (item != NULL)   {
        return c->values[item->value].type;
    }

    return CMP_VALUE_UNKNOWN;
}

struct cmp_chain_node* cmp_create_chainnode()
{
	return (struct cmp_chain_node*)mem_pool_alloc(&g_cmp.chainnode_pool);
}

void cmp_destroy_chainnode(struct cmp_chain_node* chnode)
{
	mem_pool_free(&g_cmp.chainnode_pool, chnode);
}

/* note: this function is not optimized for cpu cache (doesn't need it anyway) */
void cmp_debug(float dt, const struct gfx_view_params* params)
{
	struct linked_list* node = g_cmp.debug_list;
	while (node != NULL)	{
		struct cmp_chain_node* chnode = (struct cmp_chain_node*)node->data;
		cmphandle_t hdl = chnode->hdl;

		uint16 idx = CMP_GET_INDEX(hdl);
		cmp_t c = ((cmp_t*)g_cmp.cmps.buffer)[idx];
		if (c->debug_func != NULL)	{
			uint i_idx = CMP_GET_INSTANCEINDEX(hdl);
			uint r_idx = c->indexes[i_idx];
			c->debug_func(c->instances[r_idx].host, c->instances[r_idx].data, hdl, dt, params);
		}

		node = node->next;
	}
}

void cmp_update(float dt, uint stage_id)
{
	void* param = NULL;

#if defined(_PROFILE_)
    char name[32];
    sprintf(name, "Stage #%d", stage_id+1);
    PRF_OPENSAMPLE(name);
#endif

	/* before render stage, we pass active cmdqueue as param */
    switch (stage_id)   {
    case CMP_UPDATE_STAGE4:
        param = gfx_get_cmdqueue(0);
        break;
    default:
        param = NULL;
    }

	uint cnt = g_cmp.cmps.item_cnt;
	for (uint i = 0; i < cnt; i++)	{
		cmp_t c = ((cmp_t*)g_cmp.cmps.buffer)[i];
		if (c->update_funcs[stage_id] != NULL)
			c->update_funcs[stage_id](c, dt, param);
	}

    PRF_CLOSESAMPLE();
}

void cmp_clear_updates()
{
    uint cnt = g_cmp.cmps.item_cnt;
    for (uint i = 0; i < cnt; i++)    {
        cmp_t c = ((cmp_t*)g_cmp.cmps.buffer)[i];
        if (!BIT_CHECK(c->flags, CMP_FLAG_ALWAYSUPDATE)) {
            for (uint k = 0, update_cnt = c->update_cnt; k < update_cnt; k++)
                c->update_refs[k]->updatelist_idx = INVALID_INDEX;

            c->update_cnt = 0;
        }
    }
}

void cmp_set_globalalloc(struct allocator* alloc, struct allocator* tmp_alloc)
{
    g_cmp.alloc = alloc;
    g_cmp.tmp_alloc = tmp_alloc;
}

/*************************************************************************************************/
result_t cmp_value_sets(cmphandle_t hdl, const char* name, const char* value)
{
    const struct cmp_value* cval;
    struct cmp_obj* host;
    pfn_cmp_modify mod_fn;
    uint8* data = cmp_get_valuedata(hdl, name, &host, &mod_fn, &cval);
    if (data != NULL)   {
        ASSERT(cval->type == CMP_VALUE_STRING);
        str_safecpy((char*)data + cval->offset, cval->stride, value);
        if (mod_fn != NULL)
            return mod_fn(host, g_cmp.alloc, g_cmp.tmp_alloc, data, hdl);
        else
            return RET_OK;
    }
    return RET_FAIL;
}

result_t cmp_value_gets(OUT char* rs, uint rs_sz, cmphandle_t hdl, const char* name)
{
    const struct cmp_value* cval;
    struct cmp_obj* host;
    pfn_cmp_modify mod_fn;
    uint8* data = cmp_get_valuedata(hdl, name, &host, &mod_fn, &cval);
    if (data == NULL)
        return RET_FAIL;

    ASSERT(cval->type == CMP_VALUE_STRING);
    str_safecpy(rs, rs_sz, (char*)data + cval->offset);
    return RET_OK;
}

result_t cmp_value_setsvi(cmphandle_t hdl, const char* name, uint idx, const char* value)
{
    const struct cmp_value* cval;
    struct cmp_obj* host;
    pfn_cmp_modify mod_fn;
    uint8* data = cmp_get_valuedata(hdl, name, &host, &mod_fn, &cval);
    if (data != NULL)   {
        ASSERT(cval->type == CMP_VALUE_STRINGARRAY);
        str_safecpy((char*)data + cval->offset + idx*cval->stride, cval->stride, value);
        if (mod_fn != NULL)
            return mod_fn(host, g_cmp.alloc, g_cmp.tmp_alloc, data, hdl);
        else
            return RET_OK;
    }
    return RET_FAIL;
}

result_t cmp_value_getsvi(OUT char* rs, uint rs_sz, cmphandle_t hdl, const char* name, uint idx)
{
    const struct cmp_value* cval;
    struct cmp_obj* host;
    pfn_cmp_modify mod_fn;
    uint8* data = cmp_get_valuedata(hdl, name, &host, &mod_fn, &cval);
    if (data == NULL)
        return RET_FAIL;

    ASSERT(cval->type == CMP_VALUE_STRINGARRAY);
    str_safecpy(rs, rs_sz, (char*)data + cval->offset + idx*cval->stride);
    return RET_OK;
}

result_t cmp_value_setsvp(cmphandle_t hdl, const char* name, uint cnt, const char** values)
{
    const struct cmp_value* cval;
    struct cmp_obj* host;
    pfn_cmp_modify mod_fn;
    uint8* data = cmp_get_valuedata(hdl, name, &host, &mod_fn, &cval);
    if (data != NULL)   {
        ASSERT(cval->type == CMP_VALUE_STRINGARRAY);
        cnt = minui(cval->elem_cnt, cnt);
        for (uint i = 0; i < cnt; i++)
            str_safecpy((char*)data + cval->offset + i*cval->stride, cval->stride, values[i]);
    }

    if (mod_fn != NULL)
        return mod_fn(host, g_cmp.alloc, g_cmp.tmp_alloc, data, hdl);
    else
        return RET_OK;

    return RET_FAIL;
}

result_t cmp_value_setf(cmphandle_t hdl, const char* name, float value)
{
    const struct cmp_value* cval;
    struct cmp_obj* host;
    pfn_cmp_modify mod_fn;
    uint8* data = cmp_get_valuedata(hdl, name, &host, &mod_fn, &cval);
    if (data != NULL)   {
        ASSERT(cval->type == CMP_VALUE_FLOAT);
        *((float*)(data + cval->offset)) = value;
        if (mod_fn != NULL)
            return mod_fn(host, g_cmp.alloc, g_cmp.tmp_alloc, data, hdl);
        else
            return RET_OK;
    }
    return RET_FAIL;
}

result_t cmp_value_getf(OUT float* rf, cmphandle_t hdl, const char* name)
{
    const struct cmp_value* cval;
    struct cmp_obj* host;
    pfn_cmp_modify mod_fn;
    uint8* data = cmp_get_valuedata(hdl, name, &host, &mod_fn, &cval);
    if (data == NULL)
        return RET_FAIL;

    ASSERT(cval->type == CMP_VALUE_FLOAT);
    *rf = *((float*)(data + cval->offset));
    return RET_OK;
}

result_t cmp_value_seti(cmphandle_t hdl, const char* name, int value)
{
    const struct cmp_value* cval;
    struct cmp_obj* host;
    pfn_cmp_modify mod_fn;
    uint8* data = cmp_get_valuedata(hdl, name, &host, &mod_fn, &cval);
    if (data != NULL)   {
        ASSERT(cval->type == CMP_VALUE_INT);
        *((int*)(data + cval->offset)) = value;
        if (mod_fn != NULL)
            return mod_fn(host, g_cmp.alloc, g_cmp.tmp_alloc, data, hdl);
        else
            return RET_OK;
    }
    return RET_FAIL;
}

result_t cmp_value_geti(OUT int* rn, cmphandle_t hdl, const char* name)
{
    const struct cmp_value* cval;
    struct cmp_obj* host;
    pfn_cmp_modify mod_fn;
    uint8* data = cmp_get_valuedata(hdl, name, &host, &mod_fn, &cval);
    if (data == NULL)
        return RET_FAIL;

    ASSERT(cval->type == CMP_VALUE_INT);
    *rn = *((int*)(data + cval->offset));
    return RET_OK;
}

result_t cmp_value_setui(cmphandle_t hdl, const char* name, uint value)
{
    const struct cmp_value* cval;
    struct cmp_obj* host;
    pfn_cmp_modify mod_fn;
    uint8* data = cmp_get_valuedata(hdl, name, &host, &mod_fn, &cval);
    if (data != NULL)   {
        ASSERT(cval->type == CMP_VALUE_UINT);
        *((uint*)(data + cval->offset)) = value;
        if (mod_fn != NULL)
            return mod_fn(host, g_cmp.alloc, g_cmp.tmp_alloc, data, hdl);
        else
            return RET_OK;
    }
    return RET_FAIL;
}

result_t cmp_value_getui(OUT uint* rn, cmphandle_t hdl, const char* name)
{
    const struct cmp_value* cval;
    struct cmp_obj* host;
    pfn_cmp_modify mod_fn;
    uint8* data = cmp_get_valuedata(hdl, name, &host, &mod_fn, &cval);
    if (data == NULL)
        return RET_FAIL;

    ASSERT(cval->type == CMP_VALUE_UINT);
    *rn = *((uint*)(data + cval->offset));
    return RET_OK;
}

result_t cmp_value_set4f(cmphandle_t hdl, const char* name, const float* value)
{
    const struct cmp_value* cval;
    struct cmp_obj* host;
    pfn_cmp_modify mod_fn;
    uint8* data = cmp_get_valuedata(hdl, name, &host, &mod_fn, &cval);
    if (data != NULL)   {
        ASSERT(cval->type == CMP_VALUE_FLOAT4);
        float* fdata = (float*)(data + cval->offset);
        fdata[0] = value[0];    fdata[1] = value[1];    fdata[2] = value[2];    fdata[3] = value[3];
        if (mod_fn != NULL)
            return mod_fn(host, g_cmp.alloc, g_cmp.tmp_alloc, data, hdl);
        else
            return RET_OK;
    }
    return RET_FAIL;
}

result_t cmp_value_get4f(OUT float* rfv, cmphandle_t hdl, const char* name)
{
    const struct cmp_value* cval;
    struct cmp_obj* host;
    pfn_cmp_modify mod_fn;
    uint8* data = cmp_get_valuedata(hdl, name, &host, &mod_fn, &cval);
    if (data == NULL)
        return RET_FAIL;

    ASSERT(cval->type == CMP_VALUE_FLOAT4);
    float* fv = (float*)(data + cval->offset);
    rfv[0] = fv[0];
    rfv[1] = fv[1];
    rfv[2] = fv[2];
    rfv[3] = fv[3];
    return RET_OK;
}

result_t cmp_value_set3f(cmphandle_t hdl, const char* name, const float* value)
{
    const struct cmp_value* cval;
    struct cmp_obj* host;
    pfn_cmp_modify mod_fn;
    uint8* data = cmp_get_valuedata(hdl, name, &host, &mod_fn, &cval);
    if (data != NULL)   {
        ASSERT(cval->type == CMP_VALUE_FLOAT3);
        float* fdata = (float*)(data + cval->offset);
        fdata[0] = value[0];    fdata[1] = value[1];    fdata[2] = value[2];
        if (mod_fn != NULL)
            return mod_fn(host, g_cmp.alloc, g_cmp.tmp_alloc, data, hdl);
        else
            return RET_OK;
    }
    return RET_FAIL;
}

result_t cmp_value_get3f(OUT float* rfv, cmphandle_t hdl, const char* name)
{
    const struct cmp_value* cval;
    struct cmp_obj* host;
    pfn_cmp_modify mod_fn;
    uint8* data = cmp_get_valuedata(hdl, name, &host, &mod_fn, &cval);
    if (data == NULL)
        return RET_FAIL;

    ASSERT(cval->type == CMP_VALUE_FLOAT3);
    float* fv = (float*)(data + cval->offset);
    rfv[0] = fv[0];
    rfv[1] = fv[1];
    rfv[2] = fv[2];
    return RET_OK;
}

result_t cmp_value_set2f(cmphandle_t hdl, const char* name, const float* value)
{
    const struct cmp_value* cval;
    struct cmp_obj* host;
    pfn_cmp_modify mod_fn;
    uint8* data = cmp_get_valuedata(hdl, name, &host, &mod_fn, &cval);
    if (data != NULL)   {
        ASSERT(cval->type == CMP_VALUE_FLOAT2);
        float* fdata = (float*)(data + cval->offset);
        fdata[0] = value[0];    fdata[1] = value[1];
        if (mod_fn != NULL)
            return mod_fn(host, g_cmp.alloc, g_cmp.tmp_alloc, data, hdl);
        else
            return RET_OK;
    }
    return RET_FAIL;
}

result_t cmp_value_get2f(OUT float* rfv, cmphandle_t hdl, const char* name)
{
    const struct cmp_value* cval;
    struct cmp_obj* host;
    pfn_cmp_modify mod_fn;
    uint8* data = cmp_get_valuedata(hdl, name, &host, &mod_fn, &cval);
    if (data == NULL)
        return RET_FAIL;

    ASSERT(cval->type == CMP_VALUE_FLOAT2);
    float* fv = (float*)(data + cval->offset);
    rfv[0] = fv[0];
    rfv[1] = fv[1];
    return RET_OK;
}

result_t cmp_value_setb(cmphandle_t hdl, const char* name, int value)
{
    const struct cmp_value* cval;
    struct cmp_obj* host;
    pfn_cmp_modify mod_fn;
    uint8* data = cmp_get_valuedata(hdl, name, &host, &mod_fn, &cval);
    if (data != NULL)   {
        ASSERT(cval->type == CMP_VALUE_BOOL);
        *((int*)(data + cval->offset)) = value;
        if (mod_fn != NULL)
            return mod_fn(host, g_cmp.alloc, g_cmp.tmp_alloc, data, hdl);
        else
            return RET_OK;
    }
    return RET_FAIL;
}

 result_t cmp_value_getb(OUT int* rb, cmphandle_t hdl, const char* name)
 {
     const struct cmp_value* cval;
     struct cmp_obj* host;
     pfn_cmp_modify mod_fn;
     uint8* data = cmp_get_valuedata(hdl, name, &host, &mod_fn, &cval);
     if (data == NULL)
         return RET_FAIL;

     ASSERT(cval->type == CMP_VALUE_BOOL);
     *rb = *((int*)(data + cval->offset));
     return RET_OK;
 }

result_t cmp_value_set3m(cmphandle_t hdl, const char* name, const struct mat3f* value)
{
    const struct cmp_value* cval;
    struct cmp_obj* host;
    pfn_cmp_modify mod_fn;
    uint8* data = cmp_get_valuedata(hdl, name, &host, &mod_fn, &cval);
    if (data != NULL)   {
        ASSERT(cval->type == CMP_VALUE_MATRIX);
        struct mat3f* mdata = (struct mat3f*)(data + cval->offset);
        mat3_setm(mdata, value);

        if (mod_fn != NULL)
            return mod_fn(host, g_cmp.alloc, g_cmp.tmp_alloc, data, hdl);
        else
            return RET_OK;
    }
    return RET_FAIL;
}

result_t cmp_value_get3m(OUT struct mat3f* rm, cmphandle_t hdl, const char* name)
{
    const struct cmp_value* cval;
    struct cmp_obj* host;
    pfn_cmp_modify mod_fn;
    uint8* data = cmp_get_valuedata(hdl, name, &host, &mod_fn, &cval);
    if (data == NULL)
        return RET_FAIL;

    ASSERT(cval->type == CMP_VALUE_MATRIX);
    mat3_setm(rm, (const struct mat3f*)(data + cval->offset));
    return RET_OK;
}

uint8* cmp_get_valuedata(cmphandle_t hdl, const char* name, struct cmp_obj** phost,
    pfn_cmp_modify* pmod_fn, const struct cmp_value** pcval)
{
    uint16 idx = CMP_GET_INDEX(hdl);
    uint i_idx = CMP_GET_INSTANCEINDEX(hdl);
    cmp_t c = ((cmp_t*)g_cmp.cmps.buffer)[idx];
    struct cmp_instance_desc* inst = &c->instances[c->indexes[i_idx]];

    struct hashtable_item* item = hashtable_fixed_find(&c->value_table, hash_str(name));
    if (item != NULL)   {
        *phost = inst->host;
        *pmod_fn = c->values[item->value].modify_func;
        *pcval = &c->values[item->value];
        return inst->data;
    }

    return NULL;
}

const struct cmp_instance_desc** cmp_get_updateinstances(cmp_t c, OUT uint* cnt)
{
    *cnt = c->update_cnt;
    return (const struct cmp_instance_desc**)c->update_refs;
}

const struct cmp_instance_desc** cmp_get_allinstances(cmp_t c, OUT uint* cnt,
    struct allocator* alloc)
{
    *cnt = c->cur_idx;
    if (*cnt == 0)
        return NULL;

    struct cmp_instance_desc** insts = (struct cmp_instance_desc**)A_ALLOC(alloc,
        sizeof(struct cmp_instance_desc*)*(*cnt), MID_CMP);
    if (insts == NULL)  {
        *cnt = 0;
        return NULL;
    }

    for (uint i = 0, num = *cnt; i < num; i++)
        insts[i] = &c->instances[c->indexes[i]];

    return (const struct cmp_instance_desc**)insts;
}


void cmp_zeroobj(struct cmp_obj* obj)
{
    memset(obj, 0x00, sizeof(struct cmp_obj));

    obj->xform_cmp = INVALID_HANDLE;
    obj->bounds_cmp = INVALID_HANDLE;
    obj->model_cmp = INVALID_HANDLE;
    obj->animchar_cmp = INVALID_HANDLE;
    obj->rbody_cmp = INVALID_HANDLE;
    obj->model_shadow_cmp = INVALID_HANDLE;
    obj->trigger_cmp = INVALID_HANDLE;
    obj->attachdock_cmp = INVALID_HANDLE;
    obj->attach_cmp = INVALID_HANDLE;
}

cmptype_t cmp_gettype(cmp_t c)
{
    return c->type;
}

result_t cmp_console_debug(uint argc, const char ** argv, void* param)
{
    if (argc != 2)
        return RET_INVALIDARG;

    uint obj_id = scn_findobj(scn_getactive(), argv[0]);
    if (obj_id == 0) {
        con_log(LOG_ERROR, "object not found", NULL);
        return RET_FAIL;
    }

    struct cmp_obj* obj = scn_getobj(scn_getactive(), obj_id);

    cmp_t c = cmp_findname(argv[1]);
    if (c == NULL)  {
        con_log(LOG_ERROR, "component not found", NULL);
        return RET_FAIL;
    }

    cmphandle_t hdl = cmp_findinstance(obj->chain, c->type);
    if (hdl == INVALID_HANDLE)  {
        con_log(LOG_ERROR, "component not found in object", NULL);
        return RET_FAIL;
    }
    cmp_debug_add(hdl);

    return RET_OK;
}

result_t cmp_console_undebug(uint argc, const char ** argv, void* param)
{
    if (argc != 2)
        return RET_INVALIDARG;

    uint obj_id = scn_findobj(scn_getactive(), argv[0]);
    if (obj_id == 0) {
        con_log(LOG_ERROR, "object not found", NULL);
        return RET_FAIL;
    }

    struct cmp_obj* obj = scn_getobj(scn_getactive(), obj_id);

    cmp_t c = cmp_findname(argv[1]);
    if (c == NULL)  {
        con_log(LOG_ERROR, "component not found", NULL);
        return RET_FAIL;
    }

    cmphandle_t hdl = cmp_findinstance(obj->chain, c->type);
    if (hdl == INVALID_HANDLE)  {
        con_log(LOG_ERROR, "component not found in object", NULL);
        return RET_FAIL;
    }
    cmp_debug_remove(hdl);

    return RET_OK;
}

void cmp_update_hdl_inchain(cmp_chain chain, cmphandle_t cur_hdl, cmphandle_t new_hdl)
{
    struct linked_list* lnode = chain;
    while (lnode != NULL)   {
        struct cmp_chain_node* chnode = (struct cmp_chain_node*)lnode->data;
        if (chnode->hdl == cur_hdl) {
            chnode->hdl = new_hdl;
            return;
        }
        lnode = lnode->next;
    }
}
