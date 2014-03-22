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
#include "console.h"
#include "dhcore/core.h"
#include "dhcore/array.h"
#include "dhcore/hash-table.h"
#include "mem-ids.h"
#include "debug-hud.h"

/*************************************************************************************************
 * types
 */

struct console_line
{
	enum log_type type;
	char text[256];
};

struct console_cmd
{
	char name[32];
	pfn_con_execcmd cmd_func;
	void* param;
	char help_text[128];
};

struct console
{
    struct array cmds;  /* registered commands item: console_cmd */
	struct hashtable_open cmdtable;	/* cmdname -> index in cmds */
	struct array logs;	/* type = struct console_line */
	uint cur_logidx;
	uint lines_max;
    bool_t last_limitidx;
    bool_t init;
};

/*************************************************************************************************
 * forward declarations
 */

/* commands ('pfn_con_execcmd' callbacks) */
result_t con_help(uint argc, const char** argv, void* param);

/* */
void con_addline(enum log_type type, char* text);
void con_respond(result_t r, const char* cmdname, const char* desc);
void con_respond_rawtext(const char* text);
void con_parse_args(char* cmd, struct array* args);

/*************************************************************************************************
 * globals
 */
static const struct console_cmd g_con_cmds[] = {
		{"help", con_help, NULL, "help [command_name]"}
};
static const uint g_con_cnt = 1;

static struct console g_con;

/*************************************************************************************************
 * inlines
 */
INLINE void con_trim_arg(char* arg)
{
    str_trim(arg, (uint)strlen(arg) + 1, arg, "\"");
}

/*************************************************************************************************/
void con_zero()
{
	memset(&g_con, 0x00, sizeof(g_con));
}

result_t con_init(uint lines_max)
{
	result_t r;
    r = arr_create(mem_heap(), &g_con.cmds, sizeof(struct console_cmd), g_con_cnt, 10, MID_BASE);
    if (IS_FAIL(r))
        return err_printn(__FILE__, __LINE__, r);

	r = hashtable_open_create(mem_heap(), &g_con.cmdtable, g_con_cnt, 10, MID_BASE);
	if (IS_FAIL(r))
		return err_printn(__FILE__, __LINE__, r);

    /* add predefined commands */
	for (uint i = 0; i < g_con_cnt; i++)  {
        struct console_cmd* c = (struct console_cmd*)arr_add(&g_con.cmds);
        memcpy(c, &g_con_cmds[i], sizeof(struct console_cmd));
		hashtable_open_add(&g_con.cmdtable, hash_str(g_con_cmds[i].name), i);
    }

	r = arr_create(mem_heap(), &g_con.logs, sizeof(struct console_line), 100, 100, MID_BASE);
	if (IS_FAIL(r))
		return err_printn(__FILE__, __LINE__, r);

	g_con.lines_max = lines_max;
    g_con.init = TRUE;

	return RET_OK;
}

void con_release()
{
    arr_destroy(&g_con.cmds);
	arr_destroy(&g_con.logs);
	hashtable_open_destroy(&g_con.cmdtable);
	con_zero();
}

result_t con_exec(const char* cmd)
{
	/* parse command by spaces */
	char ncmd[256];
	struct array args_arr;

	if (str_isempty(cmd))	{
		con_respond(RET_INVALIDCALL, "", "");
		return RET_INVALIDCALL;
	}

	arr_create(mem_heap(), &args_arr, sizeof(char*), 5, 5, MID_BASE);
	str_trim(ncmd, sizeof(ncmd), strcpy(ncmd, cmd), "\n\t");
	con_parse_args(ncmd, &args_arr);

	if (arr_isempty(&args_arr)) {
        arr_destroy(&args_arr);
        con_respond(RET_INVALIDCALL, "", "");
        return RET_INVALIDCALL;
    }

	/* seach in commands to find it, and execute it */
	char** args = (char**)args_arr.buffer;
	const char* name = args[0];
	struct hashtable_item* item = hashtable_open_find(&g_con.cmdtable, hash_str(name));
	if (item == NULL)	{
		arr_destroy(&args_arr);
		con_respond(RET_INVALIDCALL, name, cmd);
		return RET_INVALIDCALL;
	}
	const struct console_cmd* c = &((struct console_cmd*)g_con.cmds.buffer)[item->value];
	result_t r = c->cmd_func(args_arr.item_cnt - 1, (const char**)args + 1, c->param);
	arr_destroy(&args_arr);

	con_respond(r, name, cmd);
	return r;
}

void con_parse_args(char* cmd, struct array* args)
{
    /* tokenize spaces, but ignore spaces that come between '"' characters */
    while (cmd[0] == ' ')
        cmd++;
    char* token = strchr(cmd, ' ');
    while (token != NULL)   {
        char* dblquote = strchr(cmd, '"');
        if (dblquote != NULL && dblquote < token)   {
            dblquote = strchr(dblquote+1, '"');
            if (dblquote != NULL)
                token = strchr(dblquote+1, ' ');
        }

        if (token != NULL)  {
            /* close the token and save the argument */
            *token = 0;
            char** parg = (char**)arr_add(args);
            ASSERT(parg);
            con_trim_arg(cmd);
            *parg = cmd;

            /* move cmd start to token and continue */
            cmd = token + 1;
            while (cmd[0] == ' ')
                cmd++;
            token = strchr(cmd, ' ');
        }
    }

    if (cmd[0] != 0)    {
        char** parg = (char**)arr_add(args);
        ASSERT(parg);
        con_trim_arg(cmd);
        *parg = cmd;
    }
}


result_t con_help(uint argc, const char** argv, void* param)
{
	if (argc == 0)	{
		/* list commands */
		con_addline(LOG_TEXT, "console commands: ");
		for (uint i = 0; i < g_con.cmds.item_cnt; i++)    {
            struct console_cmd* c = &((struct console_cmd*)g_con.cmds.buffer)[i];
			con_addline(LOG_INFO, c->help_text);
        }
	}	else	{
		const char* cmdname = argv[0];
		struct hashtable_item* item = hashtable_open_find(&g_con.cmdtable, hash_str(cmdname));
		if (item != NULL)	{
            struct console_cmd* c = &((struct console_cmd*)g_con.cmds.buffer)[item->value];
			con_addline(LOG_INFO, c->help_text);
		}	else	{
			char text[128];
			sprintf(text, "console command \"%s\" is invalid", cmdname);
			con_addline(LOG_WARNING, text);
		}
	}
	return RET_OK;
}

void con_log(enum log_type type, const char* text, void* param)
{
	char text2[2048];
	strcpy(text2, text);

	/* seperate by '\n' */
	char* token = strtok(text2, "\n");
	while (token != NULL)	{
		/* add a line for each token */
		con_addline(type, token);
		token = strtok(NULL, "\n");
	}
}

void con_addline(enum log_type type, char* text)
{
	uint idx = g_con.cur_logidx;
	struct console_line* l;

	if (idx >= g_con.lines_max)	{
		idx = idx % g_con.lines_max;
		l = ((struct console_line*)g_con.logs.buffer + idx);
        g_con.last_limitidx = g_con.cur_logidx + 1;
	}	else	{
		l = (struct console_line*)arr_add(&g_con.logs);
		ASSERT(l);
	}
	memset(l, 0x00, sizeof(struct console_line));

	l->type = type;
	/* convert first character tabs to "  " */
	if (text[0] == '\t')    {
		strcpy(l->text, "   ");
		text += 1;
	}

	str_safecat(l->text, sizeof(l->text), text);

	g_con.cur_logidx ++;
	hud_console_scroll();
}

const char* con_get_line(uint idx, OUT enum log_type* type)
{
	ASSERT(idx < g_con.logs.item_cnt);
    idx = (idx + g_con.last_limitidx) % g_con.logs.item_cnt;
	struct console_line* l = (struct console_line*)g_con.logs.buffer + idx;
	*type = l->type;
	return l->text;
}

uint con_get_linecnt()
{
	return g_con.logs.item_cnt;
}

void con_respond(result_t r, const char* cmdname, const char* desc)
{
	char text[512];
	switch (r)	{
	case RET_OK:
		sprintf(text, "console: ok - '%s'", desc);
		con_addline(LOG_INFO, text);
		break;
	case RET_INVALIDARG:
		sprintf(text, "console: invalid arguments - '%s'", desc);
		con_addline(LOG_ERROR, text);
		break;
	case RET_INVALIDCALL:
		sprintf(text, "console: command not found - '%s'", cmdname);
		con_addline(LOG_ERROR, text);
		break;
	case RET_FAIL:
		sprintf(text, "console: '%s' - %s", cmdname, desc);
		con_addline(LOG_ERROR, text);
		break;
	}
}

result_t con_register_cmd(const char* name, pfn_con_execcmd pfn_cmdfunc, void* param,
    const char* helpstr)
{
    if (!g_con.init)
        return RET_FAIL;

    struct console_cmd* c = (struct console_cmd*)arr_add(&g_con.cmds);
    if (c == NULL)
        return RET_OUTOFMEMORY;

    str_safecpy(c->name, sizeof(c->name), name);
    str_safecpy(c->help_text, sizeof(c->help_text), helpstr);
    c->cmd_func = pfn_cmdfunc;
    c->param = param;

    result_t r = hashtable_open_add(&g_con.cmdtable, hash_str(name), g_con.cmds.item_cnt-1);

    return r;
}
