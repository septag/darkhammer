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
#include "app.h"
#include "input.h"

/* types */
struct input_mgr
{
	uint keymap[INPUT_KEY_CNT];
	int mousex;
	int mousey;
    int mousex_locked;    /* -1 if it's not locked */
    int mousey_locked;    /* -1 if it's not locked */
    uint8 kb_locked[INPUT_KEY_CNT];
    uint8 mouse_keylocked[5];
};

/* globals */
static struct input_mgr g_input;

/*************************************************************************************************
 * fwd
 */
/* multiplatform functions that are implemented in their own .c files */
void input_make_keymap_platform(uint keymap[INPUT_KEY_CNT]);
void input_mouse_getpos_platform(void* wnd_hdl, OUT int* x, OUT int* y);
void input_mouse_setpos_platform(void* wnd_hdl, int x, int y);
bool_t input_mouse_getkey_platform(void* wnd_hdl, enum input_mouse_key mkey);
bool_t input_kb_getkey_platform(void* wnd_hdl, const uint keymap[INPUT_KEY_CNT],
                                enum input_key key);

/* */
void input_zero()
{
	memset(&g_input, 0x00, sizeof(g_input));
}

void input_init()
{
    log_print(LOG_TEXT, "init input ...");

	/* assign keys map */
    input_make_keymap_platform(g_input.keymap);

    g_input.mousex_locked = -1;
    g_input.mousey_locked = -1;
}

void input_release()
{
	input_zero();
}

void input_update()
{
    /* update mouse, convert x,y to relative */
    int mx, my;
    input_mouse_getpos_platform(app_get_mainwnd(), &mx, &my);

    if (g_input.mousex_locked != -1 && g_input.mousey_locked != -1) {
        g_input.mousex += mx - g_input.mousex_locked;
        g_input.mousey += my - g_input.mousey_locked;

        input_mouse_setpos_platform(app_get_mainwnd(), g_input.mousex_locked, g_input.mousey_locked);
    }   else    {
        g_input.mousex = mx;
        g_input.mousey = my;
    }
}

bool_t input_kb_getkey(enum input_key key, bool_t once)
{
    uint idx = (uint)key;
    uint8 lock_flag = g_input.kb_locked[idx];
    bool_t keypressed = input_kb_getkey_platform(app_get_mainwnd(), g_input.keymap, key);

    if (!once)   {
        return ((!(lock_flag & 0x1) && keypressed) || ((lock_flag >> 1) & 0x1));
    }   else if (keypressed)   {
        if (BIT_CHECK(lock_flag, 0x4)) {
            return FALSE;
        }   else    {
            BIT_ADD(g_input.kb_locked[idx], 0x4);
            return TRUE;
        }
    }   else {
        BIT_REMOVE(g_input.kb_locked[idx], 0x4);
        return FALSE;
    }
}

struct vec2i* input_mouse_getpos(struct vec2i* pos)
{
    return vec2i_seti(pos, g_input.mousex, g_input.mousey);
}

bool_t input_mouse_getkey(enum input_mouse_key mkey, bool_t once)
{
    uint idx = (uint)mkey;
    uint8 lock_flag = g_input.mouse_keylocked[idx];
    bool_t keypressed = input_mouse_getkey_platform(app_get_mainwnd(), mkey);

    if (!once)  {
        return ((!(lock_flag & 0x1) && keypressed) || ((lock_flag >> 1) & 0x1));
    }   else if (keypressed)    {
        if (BIT_CHECK(lock_flag, 0x4)) {
            return FALSE;
        }   else    {
            BIT_ADD(g_input.mouse_keylocked[idx], 0x4);
            return TRUE;
        }
    }   else    {
        BIT_REMOVE(g_input.mouse_keylocked[idx], 0x4);
        return FALSE;
    }
}

enum input_key input_kb_translatekey(uint vkey)
{
	for (uint i = 0; i < INPUT_KEY_CNT; i++)	{
		if (g_input.keymap[i] == vkey)
			return (enum input_key)i;
	}
	return INPUT_KEY_CNT;
}

/* using decay function described here:
 * http://en.wikipedia.org/wiki/Exponential_decay
 */
void input_mouse_smooth(INOUT float* rx, INOUT float* ry, float real_x, float real_y, float springiness,
                        float dt)
{
    fl64 d = 1.0 - exp(log(0.5)*springiness*dt);

    float x = *rx;
    float y = *ry;

    *rx = (float)(x + (real_x - x)*d);
    *ry = (float)(y + (real_y - y)*d);
}

void input_mouse_lockcursor(int x, int y)
{
    g_input.mousex_locked = x;
    g_input.mousey_locked = y;
}

void input_mouse_unlockcursor()
{
    g_input.mousex_locked = -1;
    g_input.mousey_locked = -1;
}

void input_kb_lockkey(enum input_key key, bool_t pressed)
{
    ASSERT(key != INPUT_KEY_CNT);
    g_input.kb_locked[(uint)key] = (uint8)((pressed << 1) | TRUE);
}

void input_kb_unlockkey(enum input_key key)
{
    ASSERT(key != INPUT_KEY_CNT);
    g_input.kb_locked[(uint)key] = FALSE;
}

void input_mouse_lockkey(enum input_mouse_key key, bool_t pressed)
{
    g_input.mouse_keylocked[(uint)key] = (uint8)((pressed << 1) | TRUE);
}

void input_mouse_unlockkey(enum input_mouse_key key)
{
    g_input.mouse_keylocked[(uint)key] = FALSE;
}

void input_kb_resetlocks()
{
    memset(g_input.kb_locked, 0x00, sizeof(g_input.kb_locked));
}

void input_mouse_resetlocks()
{
    memset(g_input.mouse_keylocked, 0x00, sizeof(g_input.mouse_keylocked));
    g_input.mousex_locked = -1;
    g_input.mousey_locked = -1;
}