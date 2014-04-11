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

#ifndef GUI_H_
#define GUI_H_

#include "dhcore/types.h"
#include "dhcore/prims.h"
#include "engine-api.h"

/* types/structs */
enum ui_widget_type
{
	UI_WIDGET_GRAPHLINE
};

struct ui_widget_s;
typedef struct ui_widget_s* ui_widget;

/* */
/* graph - line */
ui_widget ui_create_graphline(const char* title, float y_min, float y_max,
		uint sample_cnt, const struct rect2di rc, int enable_variable_yrange);
void ui_destroy_graphline(ui_widget graph);
void ui_graphline_addvalue(ui_widget g, float y);

const struct rect2di* ui_widget_getrect(ui_widget widget);
void ui_widget_move(ui_widget widget, int x, int y);
void ui_widget_resize(ui_widget widget, int w, int h);
void ui_widget_draw(ui_widget widget);

#endif /* GUI_H_ */
