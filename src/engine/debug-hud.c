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

#include "debug-hud.h"
#include "dhcore/core.h"
#include "dhcore/linked-list.h"
#include "dhcore/timer.h"
#include "dhcore/util.h"

#include "gfx.h"
#include "gfx-canvas.h"
#include "gfx-font.h"
#include "gfx-cmdqueue.h"
#include "engine.h"
#include "mem-ids.h"
#include "console.h"
#include "dhapp/input.h"

#define GRAPH_UPDATE_INTERVAL	0.05f

#define CONSOLE_LINE_SPACING	2
#define CONSOLE_ROW_SPACING		5
#define CONSOLE_BORDER			5
#define CONSOLE_HEIGHT			320
#define CONSOLE_CURSOR_INTERVAL	0.5f
#define CONSOLE_CMD_SIZE		255
#define CONSOLE_CMD_SAVES		10
#define CONSOLE_ROLL_SPEED		1.5f
#define CONSOLE_KEYS_FILE		"dh_keys.dat"

/*************************************************************************************************
 * types
 */
struct debug_label_item
{
    uint name_hash;
	struct linked_list node;
	pfn_hud_render_label render_fn;
    void* param;
};

struct debug_graph_item
{
    uint name_hash;
	struct linked_list node;
	pfn_hud_render_graph render_fn;
	ui_widget widget;
    void* param;
};

struct debug_image_item
{
    uint name_hash;
    struct linked_list node;
    uint width;
    uint height;
    int fullscreen;
    gfx_texture img_tex;
    char caption[32];
    void* param;
};

struct debug_console
{
	fonthandle_t log_font;
	fonthandle_t cmd_font;

	int active;
	int cursor;
	uint line_idx;
	uint cursor_idx;
	int slide_dwn;
	int slide_up;
	int y;
	int prev_y;
	uint lines_perpage;
	char cmd[CONSOLE_CMD_SIZE];
	int cmdcursor_idx;
	uint cmdsave_cnt;
	char lastcmds[CONSOLE_CMD_SAVES][CONSOLE_CMD_SIZE];
};

struct debug_hud
{
	fonthandle_t label_font;
	struct linked_list* labels;
	struct linked_list* graphs;
    struct linked_list* imgs;
	struct timer* update_timer;
	struct debug_console* console;
};

/*************************************************************************************************
 * globals
 */
struct debug_hud g_hud;

/*************************************************************************************************
 * inlines
 */
INLINE struct linked_list* find_label(uint name_hash)
{
	struct linked_list* node = g_hud.labels;
	while (node != NULL)	{
		struct debug_label_item* item = (struct debug_label_item*)node->data;
        if (item->name_hash == name_hash)
            return node;
		node = node->next;
	}
	return NULL;
}

INLINE struct linked_list* find_graph(uint name_hash)
{
    struct linked_list* node = g_hud.graphs;
	while (node != NULL)	{
		struct debug_graph_item* item = (struct debug_graph_item*)node->data;
		if (item->name_hash == name_hash)
            return node;
		node = node->next;
	}
	return NULL;
}

INLINE struct linked_list* find_image(uint name_hash)
{
    struct linked_list* node = g_hud.imgs;
    while (node != NULL)	{
        struct debug_image_item* item = (struct debug_image_item*)node->data;
        if (item->name_hash == name_hash)
            return node;
        node = node->next;
    }
    return NULL;
}

/*************************************************************************************************
 * forward declarations
 */
void hud_render_labels(gfx_cmdqueue cmdqueue);
void hud_render_graphs(gfx_cmdqueue cmdqueue);
void hud_render_images(gfx_cmdqueue cmdqueue);

result_t hud_console_init();
void hud_console_release();
void hud_console_render(gfx_cmdqueue cmdqueue);
void hud_console_update();
void hud_console_input(char c, enum input_key key);
void hud_console_activate(int active);
void hud_console_savecmd(const char* cmd);
uint hud_console_loadcmd(int idx);

/*************************************************************************************************/
void hud_zero()
{
	memset(&g_hud, 0x00, sizeof(g_hud));
}

result_t hud_init(int init_console)
{
	g_hud.label_font = gfx_font_register(eng_get_lsralloc(), "fonts/monospace12/monospace12.fnt",
			NULL, "monospace", 12, 0);
	if (g_hud.label_font == INVALID_HANDLE)	{
		err_print(__FILE__, __LINE__, "debug-hud init failed: could not load"
				" label font 'monospace'");
		return RET_FAIL;
	}

	g_hud.update_timer = timer_createinstance(TRUE);

	if (init_console)	{
		if (IS_FAIL(hud_console_init()))	{
			err_printf(__FILE__, __LINE__, "debug-hud init failed: could not init console");
			return RET_FAIL;
		}
	}

	return RET_OK;
}

void hud_release()
{
	hud_console_release();

	struct linked_list* node = g_hud.labels;
	while (node != NULL)	{
        struct linked_list* next = node->next;
        list_remove(&g_hud.labels, node);
		FREE(node->data);
		node = next;
	}

	node = g_hud.graphs;
	while (node != NULL)	{
        struct linked_list* next = node->next;
        list_remove(&g_hud.labels, node);
        struct debug_graph_item* item = (struct debug_graph_item*)node->data;
        if (item->widget != NULL)
        	ui_destroy_graphline(item->widget);
		FREE(node->data);
		node = next;
	}

    node = g_hud.imgs;
    while (node != NULL)	{
        struct linked_list* next = node->next;
        list_remove(&g_hud.imgs, node);
        FREE(node->data);
        node = next;
    }

	if (g_hud.update_timer != NULL)
		timer_destroyinstance(g_hud.update_timer);

	hud_zero();
}

result_t hud_console_init()
{
	g_hud.console = (struct debug_console*)ALLOC(sizeof(struct debug_console), MID_BASE);
	if (g_hud.console == NULL)
		return RET_OUTOFMEMORY;
	memset(g_hud.console, 0x00, sizeof(struct debug_console));
	struct debug_console* console = g_hud.console;

	console->log_font = gfx_font_register(eng_get_lsralloc(), "fonts/monospace12/monospace12.fnt",
			NULL, "monospace", 12, 0);
	console->cmd_font = gfx_font_register(eng_get_lsralloc(), "font/monospace16/monospace16.fnt",
			NULL, "monospace", 16, 0);
	if (console->log_font == INVALID_HANDLE || console->cmd_font == INVALID_HANDLE)
		return RET_FAIL;

	uint line_height = gfx_font_getf(console->log_font)->line_height;
	uint line_height_cmd = gfx_font_getf(console->cmd_font)->line_height;
	uint console_lineheight = line_height + CONSOLE_LINE_SPACING;
	uint log_height = CONSOLE_HEIGHT - line_height_cmd - CONSOLE_BORDER*2 - 2*CONSOLE_LINE_SPACING;
	console->y = -CONSOLE_HEIGHT;
	console->lines_perpage = log_height / console_lineheight;
	console->cmdcursor_idx = -1;

	/* load saved keys (if available) */
	char keysfile[DH_PATH_MAX];
	path_join(keysfile, util_gettempdir(keysfile), CONSOLE_KEYS_FILE, NULL);
	FILE* f = fopen(keysfile, "rb");
	if (f != NULL)	{
		fread(console->lastcmds, sizeof(console->lastcmds), 1, f);
		fread(&console->cmdsave_cnt, sizeof(console->cmdsave_cnt), 1, f);
		fclose(f);
	}

	return RET_OK;
}

void hud_console_release()
{
	if (g_hud.console != NULL)	{
		/* saves keys */
		char keysfile[DH_PATH_MAX];
		path_join(keysfile, util_gettempdir(keysfile), CONSOLE_KEYS_FILE, NULL);
		FILE* f = fopen(keysfile, "wb");
		if (f != NULL)	{
			fwrite(g_hud.console->lastcmds, sizeof(g_hud.console->lastcmds), 1, f);
			fwrite(&g_hud.console->cmdsave_cnt, sizeof(g_hud.console->cmdsave_cnt), 1, f);
			fclose(f);
		}

		FREE(g_hud.console);
	}
}

void hud_add_label(const char* alias, pfn_hud_render_label render_fn, void* param)
{
    uint name_hash = hash_str(alias);

	struct linked_list* node = find_label(name_hash);
	if (node == NULL)	{
		struct debug_label_item* item = (struct debug_label_item*)
            ALLOC(sizeof(struct debug_label_item), MID_BASE);
		ASSERT(item != NULL);
        item->name_hash = name_hash;
		item->render_fn = render_fn;
        item->param = param;

		list_addlast(&g_hud.labels, &item->node, item);
	}
}

void hud_remove_label(const char* alias)
{
    struct linked_list* node = find_label(hash_str(alias));
	if (node != NULL)   {
		list_remove(&g_hud.labels, node);
        FREE(node->data);
    }
}

void hud_render(gfx_cmdqueue cmdqueue)
{
    hud_render_images(cmdqueue);
	hud_render_labels(cmdqueue);
	hud_render_graphs(cmdqueue);

	if (g_hud.console != NULL && g_hud.console->active)
		hud_console_render(cmdqueue);
}

void hud_render_labels(gfx_cmdqueue cmdqueue)
{
	int x = 5;
	int y = 5;
	struct linked_list* node = g_hud.labels;

	gfx_canvas_setfont(g_hud.label_font);
	int line_height = (int)gfx_font_getf(g_hud.label_font)->line_height;
    int line_stride = line_height + CONSOLE_LINE_SPACING;

	while (node != NULL)	{
		struct debug_label_item* item = (struct debug_label_item*)node->data;
		ASSERT(item->render_fn);
		y = item->render_fn(cmdqueue, x, y, line_stride, item->param);
		node = node->next;
	}
	gfx_canvas_setfont(INVALID_HANDLE);
}

void hud_add_graph(const char* alias, pfn_hud_render_graph render_fn, ui_widget widget, void* param)
{
    ASSERT(widget);

    uint name_hash = hash_str(alias);
	struct linked_list* node = find_graph(name_hash);
	if (node == NULL)	{
		struct debug_graph_item* item = (struct debug_graph_item*)
            ALLOC(sizeof(struct debug_graph_item), MID_BASE);
		ASSERT(item != NULL);
        item->name_hash = name_hash;
		item->render_fn = render_fn;
        item->widget = widget;

		list_addlast(&g_hud.graphs, &item->node, item);
	}
}

void hud_remove_graph(const char* alias)
{
	struct linked_list* node = find_graph(hash_str(alias));
	if (node != NULL)   {
		list_remove(&g_hud.graphs, node);
		struct debug_graph_item* item = (struct debug_graph_item*)node->data;
		if (item->widget != NULL)
			ui_destroy_graphline(item->widget);
        FREE(node->data);
    }
}

void hud_render_graphs(gfx_cmdqueue cmdqueue)
{
	static float graph_tm = 0.0f;
	if (g_hud.graphs != NULL)	{
		int update_values = FALSE;
		graph_tm += g_hud.update_timer->dt;
		if (graph_tm > GRAPH_UPDATE_INTERVAL)	{
			graph_tm = 0.0f;
			update_values = TRUE;
		}

        int width, height;
        int y = 5;
        gfx_get_rtvsize(&width, &height);
		gfx_canvas_setalpha(0.75f);

		struct linked_list* node = g_hud.graphs;
		while (node != NULL)	{
			struct debug_graph_item* item = (struct debug_graph_item*)node->data;
            int x = width - ui_widget_getrect(item->widget)->w - 5;
			y = item->render_fn(cmdqueue, item->widget, x, y, update_values, item->param);
            y += 5;

			node = node->next;
		}

		gfx_canvas_setalpha(1.0f);
	}
}

void hud_console_render(gfx_cmdqueue cmdqueue)
{
#if defined(_GNUC_)
	static const struct color text_colors[] = {
			{.r=1.0f, .g=1.0f, .b=1.0f, .a=1.0f},	/* LOG_TEXT */
			{.r=1.0f, .g=0.0f, .b=0.0f, .a=1.0f},	/* LOG_ERROR */
			{.r=1.0f, .g=1.0f, .b=0.0f, .a=1.0f},	/* LOG_WARNING */
			{.r=0.7f, .g=0.7f, .b=0.7f, .a=1.0f},	/* LOG_INFO */
			{.r=0.4f, .g=0.4f, .b=1.0f, .a=1.0f}	/* LOG_LOAD */
	};
    static const struct color frame_color = {.r=0.2f, .g=0.2f, .b=0.2f, .a=1.0f};
    static const struct color bg_color = {.r=0.0f, .g=0.0f, .b=0.0f, .a=1.0f};
#else
    static const struct color text_colors[] = {
        {1.0f, 1.0f, 1.0f, 1.0f},	/* LOG_TEXT */
        {1.0f, 0.0f, 0.0f, 1.0f},	/* LOG_ERROR */
        {1.0f, 1.0f, 0.0f, 1.0f},	/* LOG_WARNING */
        {0.7f, 0.7f, 0.7f, 1.0f},	/* LOG_INFO */
        {0.4f, 0.4f, 1.0f, 1.0f}	/* LOG_LOAD */
    };
    static const struct color frame_color = {0.2f, 0.2f, 0.2f, 1.0f};
    static const struct color bg_color = {0.0f, 0.0f, 0.0f, 1.0f};
#endif

	struct debug_console* console = g_hud.console;

	/* update */
	hud_console_update();

	/* drawing */
	uint line_height = gfx_font_getf(console->log_font)->line_height;
	uint line_height_cmd = gfx_font_getf(console->cmd_font)->line_height;
	int width, height;

	gfx_get_rtvsize(&width, &height);

	/* */
	struct rect2di rc;
	rect2di_seti(&rc, 0, console->y, width, CONSOLE_HEIGHT);
	gfx_canvas_setalpha(0.8f);
	gfx_canvas_setfillcolor_solid(&frame_color);
	gfx_canvas_rect2d(&rc, 0, 0);
	gfx_canvas_settextcolor(&g_color_white);

	/* log frame */
	struct rect2di logframe_rc;
	rect2di_seti(&logframe_rc, rc.x + CONSOLE_BORDER, rc.y + CONSOLE_BORDER,
			rc.w - 2*CONSOLE_BORDER, rc.h - line_height_cmd - CONSOLE_BORDER*2 - 2*CONSOLE_LINE_SPACING);
	gfx_canvas_setfillcolor_solid(&bg_color);
	gfx_canvas_rect2d(&logframe_rc, 0, 0);

	/* log messages */
	uint line_cnt = con_get_linecnt();
	uint c = 0;
	uint console_lineheight = line_height + CONSOLE_LINE_SPACING;
	uint lines_perpage = console->lines_perpage;
	struct rect2di logline_rc;

	gfx_canvas_setfont(console->log_font);
	rect2di_seti(&logline_rc, logframe_rc.x + CONSOLE_ROW_SPACING,
			logframe_rc.y + CONSOLE_LINE_SPACING, logframe_rc.w - 2*CONSOLE_ROW_SPACING,
			console_lineheight);
	for (uint i = console->line_idx; i < line_cnt && c < lines_perpage; i++, c++)	{
		enum log_type type;
		const char* text = con_get_line(i, &type);
		gfx_canvas_settextcolor(&text_colors[(int)type]);
		gfx_canvas_text2drc(text, &logline_rc, GFX_TEXT_VERTICALALIGN);
		logline_rc.y += (line_height + CONSOLE_LINE_SPACING);
	}

	/* cmd frame */
	struct rect2di cmdframe_rc;
	rect2di_seti(&cmdframe_rc, rc.x + CONSOLE_BORDER,
			rc.y + logframe_rc.h + CONSOLE_BORDER + CONSOLE_LINE_SPACING,
			rc.w - 2*CONSOLE_BORDER, line_height_cmd + CONSOLE_LINE_SPACING);
	gfx_canvas_setfillcolor_solid(&bg_color);
	gfx_canvas_rect2d(&cmdframe_rc, 0, 0);

	/* cmd text */
	struct rect2di cmdtext_rc;
	rect2di_seti(&cmdtext_rc, cmdframe_rc.x + CONSOLE_ROW_SPACING, cmdframe_rc.y,
			cmdframe_rc.w, cmdframe_rc.h);
	gfx_canvas_setfont(console->cmd_font);
	gfx_canvas_settextcolor(&g_color_white);
	gfx_canvas_text2drc(console->cmd, &cmdtext_rc, GFX_TEXT_VERTICALALIGN);

	/* cmd cursor */
	if (console->cursor)	{
		uint cw = gfx_font_getf(console->cmd_font)->char_width;
		struct rect2di cursor_rc;
		rect2di_seti(&cursor_rc, cmdframe_rc.x + console->cursor_idx*cw + CONSOLE_ROW_SPACING,
				cmdframe_rc.y + cmdframe_rc.h - 3, cw, 3);
		gfx_canvas_setfillcolor_solid(&g_color_white);
		gfx_canvas_rect2d(&cursor_rc, 0, 0);
	}

	gfx_canvas_setalpha(1.0f);
	gfx_canvas_setfillcolor_solid(&g_color_white);
	gfx_canvas_settextcolor(&g_color_white);
	gfx_canvas_setfont(INVALID_HANDLE);

}

void hud_console_update()
{
	struct debug_console* console = g_hud.console;
	static float cursor_tm = 0.0f;
	static float roll_tm = 0.0f;

	/* blink cursor */
	cursor_tm += g_hud.update_timer->dt;
	if (cursor_tm > CONSOLE_CURSOR_INTERVAL)	{
		console->cursor = !console->cursor;
		cursor_tm = 0.0f;
	}

	/* roll animation */
	const float u = CONSOLE_ROLL_SPEED;
	if (console->slide_dwn)	{
		float t = roll_tm;
		float s = (float)-console->prev_y;
		float a = -(u*u)*0.5f* s;
		console->y -= (int)(u*t + 0.5f*a*t*t);
		console->y = clampi(console->y, -CONSOLE_HEIGHT, 0);
		if (console->y >= 0)	{
			roll_tm = 0.0f;
			console->slide_dwn = FALSE;
		}
		roll_tm += g_hud.update_timer->dt;
	}

	if (console->slide_up)	{
		float t = roll_tm;
		float s = (float)(-CONSOLE_HEIGHT - console->prev_y);
		float a = -(u*u)*0.5f*s;
		console->y -= (int)(u*t + 0.5f*a*t*t);
		console->y = clampi(console->y, -CONSOLE_HEIGHT, 0);
		if (console->y <= -CONSOLE_HEIGHT)	{
			roll_tm = 0.0f;
			console->slide_up = FALSE;
			console->active = FALSE;
		}

		roll_tm += g_hud.update_timer->dt;
	}
}

void hud_send_input(char c, enum input_key key)
{
	/* console */
	if (g_hud.console != NULL)	{
		/* ~ key for activate/deacvating console */
		if (c == 0x60)
			hud_console_activate(!g_hud.console->active);

		if (g_hud.console->active)
			hud_console_input(c, key);
	}
}

void hud_console_input(char c, enum input_key key)
{
	struct debug_console* console = g_hud.console;
	uint idx = console->cursor_idx;

	/* special characters */
	switch (key)	{
	case INPUT_KEY_BACKSPACE:
		if (idx > 0)
			idx --;	/* then continue to DELETE */
		else
			break;
	case INPUT_KEY_DELETE:
		{
			uint s = (uint)strlen(console->cmd);
			if (idx < s)	{
				char text[CONSOLE_CMD_SIZE];
				strncpy(text, console->cmd, idx);
				text[idx] = 0;
				if (idx + 1 < s)
					strcat(text, console->cmd + idx + 1);
				strcpy(console->cmd, text);
			}
		}
		break;
	case INPUT_KEY_ENTER:
		if (console->cmd[0] != 0)	{
			/* run and save command */
			con_exec(console->cmd);
			hud_console_savecmd(console->cmd);
			console->cmd[0] = 0;
		}
		/* continue to ESC */
	case INPUT_KEY_ESC:
		console->cmd[0] = 0;
		idx = 0;
		break;
	case INPUT_KEY_END:
		idx = (uint)strlen(console->cmd);
		break;
	case INPUT_KEY_HOME:
		idx = 0;
		break;
	case INPUT_KEY_LEFT:
		if (idx > 0)
			idx--;
		break;
	case INPUT_KEY_RIGHT:
		if (idx < strlen(console->cmd))
			idx++;
		break;
	case INPUT_KEY_UP:
		idx = hud_console_loadcmd(++console->cmdcursor_idx);
		break;
	case INPUT_KEY_DOWN:
		idx = hud_console_loadcmd(--console->cmdcursor_idx);
		break;
	case INPUT_KEY_PGUP:
		if (console->line_idx > 0)
			console->line_idx --;
		break;
	case INPUT_KEY_PGDWN:
		{
			console->line_idx++;
			uint line_cnt = con_get_linecnt();
			if (console->line_idx > (line_cnt - console->lines_perpage))
				console->line_idx = line_cnt - console->lines_perpage;
		}
		break;
	default:
		break;
	}

	if (c >= 0x20 && c <= 0x7D && c != 0x60)	{
		uint s = (uint)strlen(console->cmd);
		if (s < sizeof(console->cmd) - 1)	{
			if (s == idx)	{
				console->cmd[idx] = c;
				console->cmd[idx+1] = 0;
				idx ++;
			}	else	{
				/* insert */
				char text[CONSOLE_CMD_SIZE+1];
				strncpy(text, console->cmd, idx);
				text[idx] = c;
				text[idx+1] = 0;
				strcat(text, console->cmd + idx);
				strcpy(console->cmd, text);
				idx ++;
			}
		}
	}

	console->cursor_idx = idx;
}

void hud_console_activate(int active)
{
	struct debug_console* console = g_hud.console;
	if (active)	{
		console->slide_dwn = TRUE;
		console->slide_up = FALSE;
		console->active = TRUE;
	}	else	{
		console->slide_dwn = FALSE;
		console->slide_up = TRUE;
	}
	console->prev_y = console->y;
}

void hud_console_scroll()
{
	struct debug_console* console = g_hud.console;
	if (console != NULL)	{
		uint line_cnt = con_get_linecnt();
		if (line_cnt >= console->lines_perpage)
			console->line_idx = line_cnt - console->lines_perpage;
	}
}

void hud_console_savecmd(const char* cmd)
{
	struct debug_console* console = g_hud.console;
	console->cmdcursor_idx = -1;

	/* shift commands 1-up */
	if (console->cmdsave_cnt < CONSOLE_CMD_SAVES)
		console->cmdsave_cnt ++;
	for (uint i = console->cmdsave_cnt; i > 0 ; i--)	{
		if (i != CONSOLE_CMD_SAVES)
			strcpy(console->lastcmds[i], console->lastcmds[i-1]);
	}
	strcpy(console->lastcmds[0], cmd);
}

uint hud_console_loadcmd(int idx)
{
	struct debug_console* console = g_hud.console;
	if (idx < 0)	{
		console->cmd[0] = 0;
		console->cmdcursor_idx = -1;
	}	else if (idx >= (int)console->cmdsave_cnt)	{
		console->cmdcursor_idx = console->cmdsave_cnt-1;
	}	else	{
		strcpy(console->cmd, console->lastcmds[idx]);
		console->cmdcursor_idx = idx;
	}
	return (uint)strlen(console->cmd);
}

void hud_add_image(const char* alias, gfx_texture img_tex, int fullscreen,
    uint width, uint height, const char* caption)
{
    uint name_hash = hash_str(alias);
    struct linked_list* node = find_image(name_hash);
    if (node == NULL)	{
        struct debug_image_item* item = (struct debug_image_item*)
            ALLOC(sizeof(struct debug_image_item), MID_BASE);
        ASSERT(item != NULL);
        item->name_hash = name_hash;
        item->fullscreen = fullscreen;
        item->width = width;
        item->height = height;
        item->img_tex = img_tex;
        str_safecpy(item->caption, sizeof(item->caption), caption);
        item->param = NULL;

        list_addlast(&g_hud.imgs, &item->node, item);
    }
}

void hud_remove_image(const char* alias)
{
    struct linked_list* node = find_image(hash_str(alias));
    if (node != NULL)   {
        list_remove(&g_hud.imgs, node);
        FREE(node->data);
    }
}

void hud_render_images(gfx_cmdqueue cmdqueue)
{
    if (g_hud.imgs != NULL)	{
        struct rect2di rc;
        int width, height;
        gfx_get_rtvsize(&width, &height);
        gfx_canvas_setfont(g_hud.label_font);
        gfx_canvas_settextcolor(&g_color_yellow);
        int line_height = (int)gfx_font_getf(g_hud.label_font)->line_height;
        int x = 5;

        /* if we have fullscreen images, draw the first one and quit loop */
        struct linked_list* node = g_hud.imgs;
        while (node != NULL)	{
            struct debug_image_item* item = (struct debug_image_item*)node->data;
            if (item->fullscreen)   {
                gfx_canvas_bmp2d(item->img_tex,
                    item->img_tex->desc.tex.width, item->img_tex->desc.tex.height,
                    rect2di_seti(&rc, 0, 0, width, height), 0);
                /* caption */
                if (!str_isempty(item->caption))    {
                    rect2di_seti(&rc, 3, height - line_height - 3, width-6, line_height+3);
                    gfx_canvas_text2drc(item->caption, &rc, GFX_TEXT_CENTERALIGN);
                }

                break;
            }
            node = node->next;
        }

        /* non-fullscreen images */
        node = g_hud.imgs;
        while (node != NULL)	{
            struct debug_image_item* item = (struct debug_image_item*)node->data;

            if (item->fullscreen)   {
                node = node->next;
                continue;
            }

            int y = height - item->height - 5;
            rect2di_seti(&rc, x, y, item->width, item->height);
            gfx_canvas_bmp2d(item->img_tex,
                item->img_tex->desc.tex.width, item->img_tex->desc.tex.height, &rc, 0);
            x += (item->width + 5);

            /* caption */
            if (!str_isempty(item->caption))    {
                rect2di_seti(&rc, rc.x + 3, rc.y + rc.h - line_height - 3, rc.w-6, line_height+3);
                gfx_canvas_text2drc(item->caption, &rc, GFX_TEXT_CENTERALIGN);
            }

            node = node->next;
        }

        gfx_canvas_setalpha(1.0f);
        gfx_canvas_setfont(INVALID_HANDLE);
        gfx_canvas_settextcolor(&g_color_white);
    }
}

int hud_console_isactive()
{
    return (g_hud.console != NULL) && g_hud.console->active;
}
