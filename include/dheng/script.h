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

#ifndef __SCRIPT_H__
#define __SCRIPT_H__

#include "dhcore/types.h"
#include "dhcore/json.h"

#include "cmp-types.h"
#include "engine-api.h"

/* fwd */
struct appScriptParams;
struct sct_trigger_event;

struct sct_memstats
{
    size_t buff_max;
    size_t buff_alloc;
};

/**
 * script instance, which is loaded by res-mgr \n
 * internally, it is lua_state object
 */
typedef void* sct_t;

/**
 * Loads and runs script file
 * @param lua_filepath path to script file on disk or virtual file-system
 */
ENGINE_API result_t sct_runfile(const char* lua_filepath);

/* internal */
_EXTERN_BEGIN_

void sct_zero();
result_t sct_init(const struct appScriptParams* params, int monitor);
void sct_release();
void sct_parseparams(struct appScriptParams* params, json_t j);

void sct_update();

uint sct_addtimer(uint timeout, const char* funcname, int single_shot);
void sct_removetimer(uint tid);

void sct_addtrigger(cmphandle_t trigger_cmp, const char* funcname);
void sct_removetrigger(struct sct_trigger_event* te);
void sct_removetrigger_byfuncname(const char* funcname);

void sct_throwerror(const char* fmt, ...);  /* used by wrappers */
sct_t sct_load(const char* lua_filepath, uint thread_id);   /* used by res-mgr */
void sct_unload(sct_t s);
void sct_reload(const char* filepath, reshandle_t hdl, int manual);
void sct_getmemstats(struct sct_memstats* stats);
void sct_setthreshold(int mem_sz);

_EXTERN_END_

#endif /* __SCRIPT_H__ */
