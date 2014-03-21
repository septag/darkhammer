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

#ifndef CONSOLE_H_
#define CONSOLE_H_

#include "dhcore/types.h"
#include "dhcore/log.h"
#include "engine-api.h"

/**
 * command callback function
 * @param argc number of arguments (command itself does not included)
 * @param argv array of arguments
 */
typedef result_t (*pfn_con_execcmd)(uint argc, const char ** argv, void* param);

void con_zero();
result_t con_init(uint lines_max);
void con_release();

/* callback for log */
void con_log(enum log_type type, const char* text, void* param);

result_t con_exec(const char* cmd);

/* used by hud */
const char* con_get_line(uint idx, OUT enum log_type* type);
uint con_get_linecnt();

/* register command */
ENGINE_API result_t con_register_cmd(const char* name, pfn_con_execcmd pfn_cmdfunc, void* param,
    const char* helpstr);

#endif /* CONSOLE_H_ */
