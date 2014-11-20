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

#include "dhapp/app.h"

#include "dhcore/core.h"

#include "dhapp/input.h"

// Types
struct InputMgr
{
	uint keymap[(int)inKey::COUNT];
	int mousex;
	int mousey;
    int mousex_locked;    // -1 if it's not locked 
    int mousey_locked;    // -1 if it's not locked
    uint8 kb_locked[inKey::COUNT];
    uint8 mouse_keylocked[5];
};

// Globals
static InputMgr *g_in = nullptr;

#if defined(_GL_)
  typedef GLFWwindow* wplatform_t;
#elif defined(_D3D_)
  #include "dhcore/win.h"
  typedef HWND wplatform_t;
#endif

// Multiplatform functions that are implemented in their own .c files (see platform/ sources)
void input_make_keymap_platform(uint keymap[inKey::COUNT]);
void input_mouse_getpos_platform(wplatform_t wnd_hdl, OUT int* x, OUT int* y);
void input_mouse_setpos_platform(wplatform_t wnd_hdl, int x, int y);
int input_mouse_getkey_platform(wplatform_t wnd_hdl, inMouseKey mkey);
int input_kb_getkey_platform(wplatform_t wnd_hdl, const uint keymap[inKey::COUNT],
                                inKey key);
wplatform_t app_window_getplatform_w();

//
result_t input_init()
{
    g_in = mem_new<InputMgr>();
    ASSERT(g_in);
    memset(g_in, 0x00, sizeof(InputMgr));

    log_print(LOG_TEXT, "Init input ...");

	// Assign keys mapping
    input_make_keymap_platform(g_in->keymap);

    g_in->mousex_locked = -1;
    g_in->mousey_locked = -1;

    return RET_OK;
}

void input_release()
{
    if (g_in)   {
        FREE(g_in);
        g_in = nullptr;
    }
}

void input_update()
{
    if (!g_in)
        return;

    /* update mouse, convert x,y to relative */
    int mx, my;
    input_mouse_getpos_platform(app_window_getplatform_w(), &mx, &my);

    if (g_in->mousex_locked != -1 && g_in->mousey_locked != -1) {
        g_in->mousex += mx - g_in->mousex_locked;
        g_in->mousey += my - g_in->mousey_locked;

        input_mouse_setpos_platform(app_window_getplatform_w(), g_in->mousex_locked, 
            g_in->mousey_locked);
    }   else    {
        g_in->mousex = mx;
        g_in->mousey = my;
    }
}

bool input_kb_getkey(inKey key, bool once)
{
    ASSERT(g_in);

    if (!app_window_isactive())
        return false;

    uint idx = (uint)key;
    uint8 lock_flag = g_in->kb_locked[idx];
    int keypressed = input_kb_getkey_platform(app_window_getplatform_w(), g_in->keymap, key);

    if (!once)   {
        return ((!(lock_flag & 0x1) && keypressed) || ((lock_flag >> 1) & 0x1));
    }   else if (keypressed)   {
        if (BIT_CHECK(lock_flag, 0x4)) {
            return false;
        }   else    {
            BIT_ADD(g_in->kb_locked[idx], 0x4);
            return true;
        }
    }   else {
        BIT_REMOVE(g_in->kb_locked[idx], 0x4);
        return false;
    }
}

void input_mouse_getpos(int *mx, int *my)
{
    ASSERT(g_in);
    *mx = g_in->mousex;
    *my = g_in->mousey;
}

bool input_mouse_getkey(inMouseKey mkey, bool once)
{
    ASSERT(g_in);

    if (!app_window_isactive())
        return true;

    uint idx = (uint)mkey;
    uint8 lock_flag = g_in->mouse_keylocked[idx];
    int keypressed = input_mouse_getkey_platform(app_window_getplatform_w(), mkey);

    if (!once)  {
        return ((!(lock_flag & 0x1) && keypressed) || ((lock_flag >> 1) & 0x1));
    }   else if (keypressed)    {
        if (BIT_CHECK(lock_flag, 0x4)) {
            return false;
        }   else    {
            BIT_ADD(g_in->mouse_keylocked[idx], 0x4);
            return true;
        }
    }   else    {
        BIT_REMOVE(g_in->mouse_keylocked[idx], 0x4);
        return false;
    }
}

inKey input_kb_translatekey(uint vkey)
{
    if (!g_in)
        return inKey::COUNT;

	for (int i = 0; i < (int)inKey::COUNT; i++)	{
		if (g_in->keymap[i] == vkey)
			return (inKey)i;
	}

	return inKey::COUNT;
}

/* using decay function described here:
 * http://en.wikipedia.org/wiki/Exponential_decay
 */
void input_mouse_smooth(INOUT float* rx, INOUT float* ry, float real_x, float real_y, 
                        float springiness, float dt)
{
    ASSERT(g_in);

    fl64 d = 1.0 - exp(log(0.5)*springiness*dt);

    float x = *rx;
    float y = *ry;

    *rx = (float)(x + (real_x - x)*d);
    *ry = (float)(y + (real_y - y)*d);
}

void input_mouse_lockcursor(int x, int y)
{
    ASSERT(g_in);
    g_in->mousex_locked = x;
    g_in->mousey_locked = y;
}

void input_mouse_unlockcursor()
{
    ASSERT(g_in);
    g_in->mousex_locked = -1;
    g_in->mousey_locked = -1;
}

void input_kb_lockkey(inKey key, bool pressed)
{
    ASSERT(g_in);
    ASSERT(key != inKey::COUNT);
    g_in->kb_locked[(uint)key] = (uint8)(((int)pressed << 1) | 1);
}

void input_kb_unlockkey(inKey key)
{
    ASSERT(g_in);
    ASSERT(key != inKey::COUNT);
    g_in->kb_locked[(uint)key] = 0;
}

void input_mouse_lockkey(inMouseKey key, bool pressed)
{
    ASSERT(g_in);
    g_in->mouse_keylocked[(uint)key] = (uint8)(((int)pressed << 1) | 1);
}

void input_mouse_unlockkey(inMouseKey key)
{
    ASSERT(g_in);
    g_in->mouse_keylocked[(uint)key] = 0;
}

void input_kb_resetlocks()
{
    ASSERT(g_in);
    memset(g_in->kb_locked, 0x00, sizeof(g_in->kb_locked));
}

void input_mouse_resetlocks()
{
    ASSERT(g_in);
    memset(g_in->mouse_keylocked, 0x00, sizeof(g_in->mouse_keylocked));
    g_in->mousex_locked = -1;
    g_in->mousey_locked = -1;
}