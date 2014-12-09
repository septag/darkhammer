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

#ifndef DEBUG_HUD_H_
#define DEBUG_HUD_H_

#include "dhcore/types.h"

#include "dhapp/input.h"

#include "gfx-types.h"
#include "gui.h"

/**
 * callback for rendering label items
 * @param line_stride height of each line that can be used when rendering multiple lines
 * @return current y position (y + line_stride*num_lines_drawn)
 */
typedef int (*pfn_hud_render_label)(gfx_cmdqueue cmqueue, int x, int y, int line_stride,
    void* param);

/**
 * callback for rendering graphs
 * @param widget graph widget which is passed for updating/drawing/etc..
 * @param update is TRUE if we have to update the data of the graph
 * @return current y position (y + graph_height)
 */
typedef int (*pfn_hud_render_graph)(gfx_cmdqueue cmdqueue, ui_widget widget, int x, int y,
    int update, void* param);

void hud_zero();
result_t hud_init(int init_console);
void hud_release();

void hud_render(gfx_cmdqueue cmdqueue);
void hud_send_input(char c, inKey key);
void hud_console_scroll();

ENGINE_API int hud_console_isactive();

ENGINE_API void hud_add_label(const char* alias, pfn_hud_render_label render_fn,
    OPTIONAL void* param);
ENGINE_API void hud_remove_label(const char* alias);
ENGINE_API void hud_add_graph(const char* alias, pfn_hud_render_graph render_fn,
    ui_widget widget, OPTIONAL void* param);
ENGINE_API void hud_remove_graph(const char* alias);
ENGINE_API void hud_add_image(const char* alias, gfx_texture img_tex, int fullscreen,
    uint width, uint height, const char* caption);
ENGINE_API void hud_remove_image(const char* alias);

#endif /* DEBUG_HUD_H_ */
