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

#include "gui.h"
#include "dhcore/core.h"
#include "gfx-canvas.h"
#include "gfx-font.h"
#include "mem-ids.h"
#include "engine.h"
#include <stdio.h>

#define LINEGRAPH_BORDER		5
#define LINEGRAPH_TOPPADDING	15

/*************************************************************************************************
 * types
 */

/* draw callback */
typedef void (*pfn_widget_draw)(ui_widget);

struct ui_widget_s
{
	enum ui_widget_type type;
	char title[32];
	struct rect2di rc;
	pfn_widget_draw draw_func;
	void* data;	/* specific data for each widget */
};

struct ui_graphline_data
{
	uint sample_cnt;
	uint idx;
    float total_cnt;
    float total_sum;
    float average;
	int enable_variable_yrange;
	float y_min;
	float y_max;
	float y_cur_max;
	float y_cur_min;
	int inloop;
	fonthandle_t font;
	struct vec2f* values;
	struct vec2i* pts;	/* points for rendering */
};

/*************************************************************************************************
 * globals
 */

/*************************************************************************************************
 * forward declarations
 */
void ui_graphline_draw(ui_widget g);

/*************************************************************************************************
 * inlines
 */
INLINE ui_widget ui_create_widget(enum ui_widget_type type, const char* title,
		const struct rect2di rc, pfn_widget_draw draw_func, void* data)
{
	ui_widget w = (ui_widget)ALLOC(sizeof(struct ui_widget_s), MID_GUI);
	ASSERT(w);
	memset(w, 0x00, sizeof(struct ui_widget_s));
	w->type = type;
	strcpy(w->title, title);
	w->rc = rc;
	w->draw_func = draw_func;
	w->data = data;
	return w;
}

INLINE void ui_destroy_widget(ui_widget w)
{
	FREE(w);
}

INLINE struct vec2i* ui_transform_coord(struct vec2i* r, float x, float y,
		float x_r, float y_r, const struct rect2df* rc)
{

	return r;
}

/*************************************************************************************************/
ui_widget ui_create_graphline(const char* title, float y_min, float y_max,
		uint sample_cnt, const struct rect2di rc, int enable_variable_yrange)
{
	struct ui_graphline_data* data = (struct ui_graphline_data*)
        ALLOC(sizeof(struct ui_graphline_data), MID_GUI);
	if (data == NULL)
		return NULL;
	ASSERT(y_min != y_max);
	memset(data, 0x00, sizeof(struct ui_graphline_data));
	data->y_cur_max = FL32_MIN;
	data->y_cur_min = FL32_MAX;
	data->sample_cnt = sample_cnt;
	data->enable_variable_yrange = enable_variable_yrange;
	data->y_max = y_max;
	data->y_min = y_min;
	data->font = gfx_font_register(eng_get_lsralloc(), "fonts/monospace12/monospace12.fnt",
			NULL, "monospace", 12, 0);
	data->values = (struct vec2f*)ALLOC(sizeof(struct vec2f)*sample_cnt, MID_GUI);
	data->pts = (struct vec2i*)ALLOC(sizeof(struct vec2i)*sample_cnt, MID_GUI);
	if (data->values == NULL || data->pts == NULL)	{
		FREE(data);
		return NULL;
	}
	memset(data->values, 0x00, sizeof(struct vec2f)*sample_cnt);
	memset(data->pts, 0x00, sizeof(struct vec2i)*sample_cnt);
	return ui_create_widget(UI_WIDGET_GRAPHLINE, title, rc, ui_graphline_draw, data);
}

void ui_destroy_graphline(ui_widget graph)
{
	ASSERT(graph->type == UI_WIDGET_GRAPHLINE);
	if (graph->data != NULL)	{
		struct ui_graphline_data* data = (struct ui_graphline_data*)graph->data;
		if (data->values != NULL)
			FREE(data->values);
		if (data->pts != NULL)
			FREE(data->pts);
		FREE(graph->data);
	}
	ui_destroy_widget(graph);
}

void ui_widget_move(ui_widget widget, int x, int y)
{
	widget->rc.x = x;
	widget->rc.y = y;
}

void ui_widget_resize(ui_widget widget, int w, int h)
{
	widget->rc.w = w;
	widget->rc.h = h;
}

void ui_graphline_addvalue(ui_widget g, float y)
{
	ASSERT(g->type == UI_WIDGET_GRAPHLINE);
	struct ui_graphline_data* data = (struct ui_graphline_data*)g->data;

	uint idx = (data->idx + 1) % data->sample_cnt;
	if (idx == 0 && data->idx != 0)
		data->inloop = TRUE;

	if (y > data->y_cur_max)
		data->y_cur_max = y;
	if (y < data->y_cur_min)
		data->y_cur_min = y;

	if (data->enable_variable_yrange)	{
		if (data->y_max < data->y_cur_max)
			data->y_max = data->y_cur_max;
		if (data->y_min > data->y_cur_min)
			data->y_min = data->y_cur_min;
	}

	data->values[idx].x = (float)idx;
	data->values[idx].y = y;
	data->idx = idx;

    /* calculat average */
    data->total_cnt += 1.0f;
    data->total_sum += y;
    data->average = data->total_sum/data->total_cnt;
}

void ui_graphline_draw(ui_widget g)
{
	ASSERT(g->type == UI_WIDGET_GRAPHLINE);
	struct ui_graphline_data* data = (struct ui_graphline_data*)g->data;
	struct rect2di rc;

	rect2di_seti(&rc, g->rc.x + LINEGRAPH_BORDER, g->rc.y + LINEGRAPH_TOPPADDING + LINEGRAPH_BORDER,
			g->rc.w - LINEGRAPH_BORDER*2, g->rc.h - LINEGRAPH_TOPPADDING - LINEGRAPH_BORDER*2);

	gfx_canvas_setfillcolor_solid(&g_color_black);
	gfx_canvas_rect2d(&g->rc, 0, 0);
	gfx_canvas_setclip2d(TRUE, rc.x - 1, rc.y - 1, rc.w + 1, rc.h + 1);
	gfx_canvas_setlinecolor(&g_color_white);

	/* yaxis */
	gfx_canvas_line2d(rc.x, rc.y, rc.x, rc.y + rc.h, 1);

	/* xaxis */
	gfx_canvas_line2d(rc.x, rc.y + rc.h, rc.x + rc.w, rc.y + rc.h, 1);

	/* grid */
	gfx_canvas_setlinecolor(&g_color_grey);
	int yinterval = rc.h / 5;
	int xinterval = rc.w / 10;
	for (int x = xinterval; x < rc.w; x += xinterval)
		gfx_canvas_line2d(rc.x + x, rc.y, rc.x + x, rc.y + rc.h - 1, 1);
	for (int y = rc.h - yinterval; y >= yinterval; y -= yinterval)
		gfx_canvas_line2d(rc.x, y + rc.y, rc.x + rc.w, y + rc.y, 1);

	/* values */
	/* first, transform all values into points on screen */
	uint sample_cnt = (uint)data->sample_cnt;
	struct rect2df rcf;
	gfx_canvas_setlinecolor(&g_color_green);
	rect2df_setf(&rcf, (float)rc.x, (float)rc.y, (float)rc.w, (float)rc.h);
	float x_r = rcf.w / (float)sample_cnt;
	float y_r = 1.0f / (data->y_max - data->y_min);
	for (uint i = 0; i < sample_cnt; i++)	{
		data->pts[i].x = (int)(data->values[i].x*x_r + rcf.x);
		data->pts[i].y = (int)(rcf.h * (1.0f - data->values[i].y*y_r) + rcf.y);
	}

	uint idx = data->idx;
	for (uint i = 1; i < idx; i++)
		gfx_canvas_line2d(data->pts[i-1].x, data->pts[i-1].y, data->pts[i].x, data->pts[i].y, 1);

	if (data->inloop)	{
		for (uint i = idx + 1; i < sample_cnt; i++)
			gfx_canvas_line2d(data->pts[i-1].x, data->pts[i-1].y, data->pts[i].x, data->pts[i].y, 1);
	}

	/* indicator */
	gfx_canvas_setlinecolor(&g_color_yellow);
	gfx_canvas_line2d(data->pts[idx].x, rc.y, data->pts[idx].x, rc.y + rc.h, 2);

	/* texts */
	gfx_canvas_setfont(data->font);
	gfx_canvas_setclip2d(FALSE, 0, 0, 0, 0);
	struct rect2di textrc;
	rect2di_seti(&textrc, g->rc.x + LINEGRAPH_BORDER, g->rc.y,
			g->rc.w - LINEGRAPH_BORDER*2, LINEGRAPH_TOPPADDING);
	gfx_canvas_settextcolor(&g_color_white);
	gfx_canvas_text2drc(g->title, &textrc, GFX_TEXT_CENTERALIGN | GFX_TEXT_VERTICALALIGN);
	char maxtext[32];
	sprintf(maxtext, "max: %.1f", data->y_cur_max);
	gfx_canvas_text2drc(maxtext, &textrc, GFX_TEXT_VERTICALALIGN);
    char avgtext[32];
    textrc.y += gfx_font_getf(data->font)->line_height + 2;
    sprintf(avgtext, "avg: %.1f", data->average);
    gfx_canvas_text2drc(avgtext, &textrc, GFX_TEXT_VERTICALALIGN);

	/* turn back canvas state */
	gfx_canvas_setfillcolor_solid(&g_color_white);
	gfx_canvas_setlinecolor(&g_color_white);
	gfx_canvas_setfont(INVALID_HANDLE);
}


void ui_widget_draw(ui_widget widget)
{
	ASSERT(widget->draw_func);
	widget->draw_func(widget);
}

const struct rect2di* ui_widget_getrect(ui_widget widget)
{
    return &widget->rc;
}
