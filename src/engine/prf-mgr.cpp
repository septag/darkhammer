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

#include "dhcore/core.h"
#include "dhcore/array.h"
#include "dhcore/hash.h"
#include "dhcore/json.h"
#include "dhcore/timer.h"
#include "dhcore/stack-alloc.h"
#include "dhcore/mt.h"
#include "dhcore/file-io.h"

#include "mongoose.h"

#include "prf-mgr.h"
#include "mem-ids.h"
#include "engine.h"
#include "gfx-device.h"
#include "scene-mgr.h"
#include "camera.h"
#include "script.h"
#include "phx-device.h"
#include "world-mgr.h"

#define AJAX_HEADER	"/json/"
#define HSEED	2874
#define SAMPLES_BUFFER_SIZE (64*1024)
#define PROTECT_CMD() if (!g_prf.init)   return NULL;
#define WEB_ROOTDIR "web"

/*************************************************************************************************
 * types
 */
typedef json_t (*pfn_ajax_cmd)(const char* param1, const char* param2);

struct prf_cmd_desc
{
	const char* name;
	uint hash;
	pfn_ajax_cmd cmd_fn;
};

struct prf_node
{
    char name[32];
    uint64 start_tick;
    fl64 start_tm;
    fl64 tm;

    struct linked_list l;   /* linked list node */
    struct linked_list* childs; /* children linked-list */
    struct prf_node* parent;
};

struct prf_samples
{
    struct stack_alloc alloc; /* stack allocator */
    struct linked_list* nodes; /* performance nodes, data=prf_node */
    struct prf_node* node_cur; /* cursor node */
    float duration;
};

struct prf_mgr
{
    long volatile init;
	struct mg_server* server;
	struct array cmds; /* commands (item: cmd_desc)*/
    struct prf_samples* samples_back; /* the one that is being created by engine */
    struct prf_samples* samples_front; /* the one that is presentable to user */
    mt_mutex samples_mtx;    /* mutex for front-buffer protection */
};

/*************************************************************************************************
 * globals
 */
struct prf_mgr g_prf;

/*************************************************************************************************
 * fwd declarations
 */
result_t webserver_init(int port);
void webserver_release();
void* webserver_malloc(size_t s);
void webserver_free(void* ptr);
void* webserver_realloc(void* ptr, size_t s);
int webserver_request_handler(struct mg_connection* conn, enum mg_event ev);
void webserver_runajax(struct mg_connection* conn, const char* cmd, const char* param1,
		const char* param2);
void prf_register_cmd(const char* name, pfn_ajax_cmd callback_fn);
pfn_ajax_cmd prf_find_cmd(const char* name);
json_t prf_create_node_json(json_t parent, struct prf_node* node);
struct prf_samples* prf_create_samples();
void prf_destroy_samples(struct prf_samples* s);

/*************************************************************************************************
 * commands (in form of pfn_ajax_cmd signature)
 * these commands run in multi-threaded mode (web-service), so they must be thread-safe
 */
json_t prf_cmd_heapinfo(const char* param1, const char* param2);
json_t prf_cmd_heapsubinfo(const char* param1, const char* param2);
json_t prf_cmd_gpumem(const char* param1, const char* param2);
json_t prf_cmd_profilergantt(const char* param1, const char* param2);
json_t prf_cmd_buffersmem(const char* param1, const char* param2);
json_t prf_cmd_getcaminfo(const char* param1, const char* param2);

/*************************************************************************************************
 * inlines
 */
 json_t prf_cmd_createkeyvalue_n(const char* key, fl64 value, int isgraph)
{
	json_t p = json_create_obj();
	json_additem_toobj(p, "key", json_create_str(key));
	json_additem_toobj(p, "value", json_create_num(value));
    if (isgraph)
        json_additem_toobj(p, "isgraph", json_create_bool(TRUE));
	return p;
}


INLINE json_t prf_cmd_createkeyvalue_s(const char* key, const char* value)
{
	json_t p = json_create_obj();
	json_additem_toobj(p, "key", json_create_str(key));
	json_additem_toobj(p, "value", json_create_str(value));
	return p;
}

INLINE json_t prf_cmd_createkeyvalue_b(const char* key, int value)
{
	json_t p = json_create_obj();
	json_additem_toobj(p, "key", json_create_str(key));
	json_additem_toobj(p, "value", json_create_bool(value));
	return p;
}

INLINE struct prf_node* prf_create_node(const char* name, const char* file, uint line,
    struct prf_node* parent)
{
    struct prf_samples* s = (struct prf_samples*)g_prf.samples_back;

    struct prf_node* node = (struct prf_node*)mem_stack_alloc(&s->alloc, sizeof(struct prf_node),
        MID_PRF);
    ASSERT(node);
    memset(node, 0x00, sizeof(struct prf_node));

    strcpy(node->name, name);

    /* add to hierarchy */
    if (parent != NULL) {
        node->parent = parent;
        list_addlast(&parent->childs, &node->l, node);
    }   else    {
        list_addlast(&s->nodes, &node->l, node);
    }

    return node;
}

INLINE void prf_destroy_node(struct prf_node* node)
{
    struct prf_samples* s = (struct prf_samples*)g_prf.samples_back;

    /* remove from hiearchy */
    if (node->parent != NULL)   {
        list_remove(&node->parent->childs, &node->l);
        node->parent = NULL;
    }   else    {
        list_remove(&s->nodes, &node->l);
    }
}

/*************************************************************************************************/
void prf_zero()
{
	memset(&g_prf, 0x00, sizeof(g_prf));
}

result_t prf_initmgr()
{
	result_t r;

    /* samples */
    g_prf.samples_back = prf_create_samples();
    g_prf.samples_front = prf_create_samples();
    if (g_prf.samples_back == NULL || g_prf.samples_front == NULL)
        return RET_FAIL;

    mt_mutex_init(&g_prf.samples_mtx);

	/* web server */
	r = webserver_init(eng_get_params()->dev.webserver_port);
	if (IS_FAIL(r))
		return RET_FAIL;

	/* commands */
	r = arr_create(mem_heap(), &g_prf.cmds, sizeof(struct prf_cmd_desc), 20, 20, MID_PRF);
	if (IS_FAIL(r))
		return RET_OUTOFMEMORY;

	/* register commands */
	prf_register_cmd("mem-heap", prf_cmd_heapinfo);
    prf_register_cmd("mem-heap-sub", prf_cmd_heapsubinfo);
    prf_register_cmd("mem-gpu", prf_cmd_gpumem);
    prf_register_cmd("prf-gantt", prf_cmd_profilergantt);
    prf_register_cmd("mem-buffers", prf_cmd_buffersmem);
    prf_register_cmd("info-cam", prf_cmd_getcaminfo);

    MT_ATOMIC_SET(g_prf.init, TRUE);
	return RET_OK;
}

void prf_releasemgr()
{
    MT_ATOMIC_SET(g_prf.init, FALSE);
    mt_mutex_release(&g_prf.samples_mtx);
	webserver_release();
	arr_destroy(&g_prf.cmds);
    if (g_prf.samples_front != NULL)
        prf_destroy_samples((struct prf_samples*)g_prf.samples_front);
    if (g_prf.samples_back != NULL)
        prf_destroy_samples((struct prf_samples*)g_prf.samples_back);
	prf_zero();
}

result_t webserver_init(int port)
{
	char port_str[32];
	str_itos(port_str, port);
    struct mg_server* server = mg_create_server(NULL, webserver_request_handler);
    if (server == NULL) {
        log_printf(LOG_WARNING, "starting httpd server failed - service will not be available.");
        return RET_FAIL;
    }
    mg_set_option(server, "listening_port", port_str);
    g_prf.server = server;

	log_printf(LOG_TEXT, "httpd debug server started on port '%s'", port_str);

	return RET_OK;
}

void webserver_release()
{
	if (g_prf.server != NULL)	{
        mg_destroy_server(&g_prf.server);
		g_prf.server = NULL;
		log_print(LOG_TEXT, "httpd debug server closed.");
	}
}

void* webserver_malloc(size_t s)
{
	return ALLOC(s, MID_NET);
}

void webserver_free(void* ptr)
{
	FREE(ptr);
}

void* webserver_realloc(void* ptr, size_t s)
{
	void* tmp = ALLOC(s, MID_NET);
	if (tmp == NULL)
		return NULL;
	memcpy(tmp, ptr, mem_size(ptr));
	FREE(ptr);
	return tmp;
}

int webserver_request_handler(struct mg_connection* conn, enum mg_event ev)
{
	if (ev == MG_REQUEST)	{
		/* check ajax json requests
		 * else just send the file to the client */
		if (strstr(conn->uri, AJAX_HEADER) == conn->uri)	{
			char param1[64];
			char param2[64];

			const char* cmd = conn->uri + strlen(AJAX_HEADER);

			/* read POST data for parameters */
			mg_get_var(conn, "p1", param1, sizeof(param1));
			mg_get_var(conn, "p2", param2, sizeof(param2));

			webserver_runajax(conn, cmd, param1, param2);
		}	else	{
			/* set default 'index.html' for urls with no file references */
			char path[DH_PATH_MAX];
            strcpy(path, WEB_ROOTDIR);
            strcat(path, conn->uri);
			if (util_pathisdir(path))	{
				if (path[strlen(path)-1] != '/')
					strcat(path, "/index.html");
				else
					strcat(path, "index.html");
			}

			/* send back the file to the client */
            file_t f = fio_openmem(mem_heap(), path, FALSE, MID_PRF);
            if (f != NULL)  {
                size_t size;
                void* data = fio_detachmem(f, &size, NULL);
                mg_send_data(conn, data, (uint)size);
                fio_close(f);
            }   else {
                mg_send_status(conn, 404);
            }

		}

        return MG_TRUE;
	}

    return MG_FALSE;
}

void webserver_runajax(struct mg_connection* conn, const char* cmd, const char* param1,
		const char* param2)
{
	pfn_ajax_cmd cmd_fn = prf_find_cmd(cmd);
	if (cmd_fn != NULL)	{
		json_t j = cmd_fn(param1, param2);
		if (j != NULL)	{
			size_t json_size;
			char* json_data = json_savetobuffer(j, &json_size, TRUE);
			if (json_data != NULL)	{
				mg_write(conn, json_data, (uint)json_size);
				json_deletebuffer(json_data);
			}
			json_destroy(j);
		}	else	{
			mg_send_status(conn, 400);
		}
	}	else	{
		mg_send_status(conn, 404);
	}
}

void prf_register_cmd(const char* name, pfn_ajax_cmd callback_fn)
{
	struct prf_cmd_desc* c = (struct prf_cmd_desc*)arr_add(&g_prf.cmds);
	ASSERT(c);
	c->name = name;
	c->hash = hash_murmur32(name, strlen(name), HSEED);
	c->cmd_fn = callback_fn;
}

pfn_ajax_cmd prf_find_cmd(const char* name)
{
	uint hash = hash_murmur32(name, strlen(name), HSEED);

	struct prf_cmd_desc* cmds = (struct prf_cmd_desc*)g_prf.cmds.buffer;
	for (uint i = 0, cnt = g_prf.cmds.item_cnt; i < cnt; i++)	{
		if (cmds[i].hash == hash)
			return cmds[i].cmd_fn;
	}
	return NULL;
}

json_t prf_cmd_heapinfo(const char* param1, const char* param2)
{
    PROTECT_CMD();

	json_t root = json_create_obj();
	json_t data = json_create_arr();

    /* data is an array of key,value items */
	json_additem_toobj(root, "data", data);

	struct mem_stats stats;
	mem_getstats(&stats);

	json_additem_toarr(data, prf_cmd_createkeyvalue_n("alloc-cnt", (fl64)stats.alloc_cnt, FALSE));
	json_additem_toarr(data, prf_cmd_createkeyvalue_n("alloc-total", (fl64)stats.alloc_bytes, TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("max-limit", (fl64)stats.limit_bytes, FALSE));
	json_additem_toarr(data, prf_cmd_createkeyvalue_n("trace-total", (fl64)stats.tracer_alloc_bytes,
        TRUE));
	return root;
}

json_t prf_cmd_buffersmem(const char* param1, const char* param2)
{
    PROTECT_CMD();

    struct eng_mem_stats stats;
    struct sct_memstats sm_stats;
    struct phx_memstats px_stats;

    eng_get_memstats(&stats);
    sct_getmemstats(&sm_stats);
    phx_getmemstats(&px_stats);

    json_t root = json_create_obj();
    json_t data = json_create_arr();

    json_additem_toobj(root, "data", data);

    json_additem_toarr(data, prf_cmd_createkeyvalue_n("LSR-max", (fl64)stats.lsr_max, TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("LSR-alloc", (fl64)stats.lsr_size, TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("data-max", (fl64)stats.data_max, TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("data-alloc", (fl64)stats.data_size, TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("tmp0-max", (fl64)stats.tmp0_total, TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("tmp0-maxalloc", (fl64)stats.tmp0_max, TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("tmp0-maxframeid",
        (fl64)stats.tmp0_max_frameid, FALSE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("script", (fl64)sm_stats.buff_alloc, TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("script-max", (fl64)sm_stats.buff_max, TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("physics", (fl64)px_stats.buff_alloc, TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("physics-max", (float)px_stats.buff_max, TRUE));

    return root;
}


json_t prf_cmd_heapsubinfo(const char* param1, const char* param2)
{
    PROTECT_CMD();

    json_t root = json_create_obj();
    json_t data = json_create_arr();

    /* data is an array of key,value items */
    json_additem_toobj(root, "data", data);

	struct mem_stats stats;
	mem_getstats(&stats);

    json_additem_toarr(data, prf_cmd_createkeyvalue_n("total", (fl64)stats.alloc_bytes, TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("base", (fl64)mem_sizebyid(MID_BASE), TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("graphics", (fl64)mem_sizebyid(MID_GFX), TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("res-mgr", (fl64)mem_sizebyid(MID_RES), TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("gui", (fl64)mem_sizebyid(MID_GUI), TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("scene-mgr", (fl64)mem_sizebyid(MID_SCN), TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("network", (fl64)mem_sizebyid(MID_NET), TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("components", (fl64)mem_sizebyid(MID_CMP), TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("profiler", (fl64)mem_sizebyid(MID_PRF), TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("script", (fl64)mem_sizebyid(MID_SCT), TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("data", (fl64)mem_sizebyid(MID_DATA), TRUE));
    return root;
}

json_t prf_cmd_gpumem(const char* param1, const char* param2)
{
    PROTECT_CMD();

    json_t root = json_create_obj();
    json_t data = json_create_arr();

    /* data is an array of key,value items */
    json_additem_toobj(root, "data", data);

    const struct gfx_gpu_memstats* stats = gfx_get_memstats();

    json_additem_toarr(data, prf_cmd_createkeyvalue_n("texture-cnt", stats->texture_cnt, FALSE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("texture-total", (fl64)stats->textures, TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("render-target-cnt", stats->rttexture_cnt,
    		FALSE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("render-target-total", (fl64)stats->rt_textures,
    		TRUE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("buffer-cnt", stats->buffer_cnt, FALSE));
    json_additem_toarr(data, prf_cmd_createkeyvalue_n("buffer-total", (fl64)stats->buffers, TRUE));
    return root;
}

json_t prf_cmd_profilergantt(const char* param1, const char* param2)
{
    PROTECT_CMD();

    /* protect front buffer during data creation */
    mt_mutex_lock(&g_prf.samples_mtx);
    struct prf_samples* s = (struct prf_samples*)g_prf.samples_front;

    json_t root = json_create_obj();
    json_t data = json_create_obj();
    json_additem_toobj(root, "data", data);

    json_t jsamples = json_create_arr();
    json_additem_toobj(data, "duration", json_create_num(s->duration));
    json_additem_toobj(data, "samples", jsamples);

    struct linked_list* l = s->nodes;
    while (l != NULL)   {
        prf_create_node_json(jsamples, (struct prf_node*)l->data);
        l = l->next;
    }

    mt_mutex_unlock(&g_prf.samples_mtx);
    return root;
}

json_t prf_create_node_json(json_t parent, struct prf_node* node)
{
    PROTECT_CMD();

    json_t jnode = json_create_obj();
    json_additem_toarr(parent, jnode);

    json_additem_toobj(jnode, "name", json_create_str(node->name));
    json_additem_toobj(jnode, "start", json_create_num(node->start_tm));
    json_additem_toobj(jnode, "duration", json_create_num(node->tm));

    /* recurse for children */
    struct linked_list* child = node->childs;
    if (child != NULL)  {
        json_t jchilds = json_create_arr();
        json_additem_toobj(jnode, "childs", jchilds);

        while (child != NULL)   {
            prf_create_node_json(jchilds, (struct prf_node*)child->data);
            child = child->next;
        }
    }

    return jnode;
}

void prf_opensample(const char* name, const char* file, uint line)
{
    if (g_prf.samples_back == NULL)
        return;

    struct prf_samples* s = (struct prf_samples*)g_prf.samples_back;
    struct prf_node* n = prf_create_node(name, file, line, s->node_cur);

    /* save time */
    n->start_tick = timer_querytick();
    n->start_tm = timer_calctm(eng_get_framestats()->start_tick, n->start_tick)*1000.0;

    s->node_cur = n;
}

void prf_closesample()
{
    if (g_prf.samples_back == NULL)
        return;

    struct prf_samples* s = (struct prf_samples*)g_prf.samples_back;
    struct prf_node* n = s->node_cur;
    ASSERT(n != NULL);

    /* calculate time elapsed since start_tick */
    n->tm = timer_calctm(n->start_tick, timer_querytick())*1000.0;

    /* go back to parent */
    s->node_cur = n->parent;
}

struct prf_samples* prf_create_samples()
{
    struct prf_samples* s = (struct prf_samples*)ALLOC(sizeof(struct prf_samples), MID_PRF);
    if (s == NULL)
        return NULL;
    memset(s, 0x00, sizeof(struct prf_samples));

    result_t r;

    r = mem_stack_create(mem_heap(), &s->alloc, SAMPLES_BUFFER_SIZE, MID_PRF);
    if (IS_FAIL(r)) {
        prf_destroy_samples(s);
        return NULL;
    }

    return s;
}

void prf_destroy_samples(struct prf_samples* s)
{
    mem_stack_destroy(&s->alloc);
    FREE(s);
}

void prf_presentsamples(fl64 ft)
{
    if (!g_prf.init)
        return;

    /* save whole frame duration for built sampels */
    g_prf.samples_back->duration = (float)(ft*1000.0);

    /* block presenting front buffer until we are done with json data creation */
    if (mt_mutex_try(&g_prf.samples_mtx))   {
        swapptr((void**)&g_prf.samples_back, (void**)&g_prf.samples_front);
        mt_mutex_unlock(&g_prf.samples_mtx);
    }

    /* reset back buffer stack allocator */
    struct prf_samples* s = g_prf.samples_back;
    mem_stack_reset(&s->alloc);
    s->node_cur = NULL;
    s->nodes = NULL;
}

json_t prf_cmd_getcaminfo(const char* param1, const char* param2)
{
    PROTECT_CMD();

    Camera* cam = wld_get_cam();

    json_t jroot = json_create_obj();
    json_t jcam = json_create_obj();

    /* data is an array of key,value items */
    json_additem_toobj(jroot, "cam", jcam);
    if (cam == NULL)
        return jroot;

    json_additem_toobj(jcam, "x", json_create_num(cam->pos.x));
    json_additem_toobj(jcam, "y", json_create_num(cam->pos.y));
    json_additem_toobj(jcam, "z", json_create_num(cam->pos.z));

    json_additem_toobj(jcam, "qx", json_create_num(cam->rot.x));
    json_additem_toobj(jcam, "qy", json_create_num(cam->rot.y));
    json_additem_toobj(jcam, "qz", json_create_num(cam->rot.z));
    json_additem_toobj(jcam, "qw", json_create_num(cam->rot.w));

    return jroot;
}
