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

#if defined(_D3D_)

#include "dhcore/core.h"
#include "dhcore/win.h"
#include "win-keycodes.h"
#include "dhapp/input.h"

void input_make_keymap_platform(uint keymap[INPUT_KEY_CNT])
{
    keymap[0] = KEY_ESC;
    keymap[1] = KEY_F1;
    keymap[2] = KEY_F2;
    keymap[3] = KEY_F3;
    keymap[4] = KEY_F4;
    keymap[5] = KEY_F5;
    keymap[6] = KEY_F6;
    keymap[7] = KEY_F7;
    keymap[8] = KEY_F8;
    keymap[9] = KEY_F9;
    keymap[10] = KEY_F10;
    keymap[11] = KEY_F11;
    keymap[12] = KEY_F12;
    keymap[13] = KEY_PRINTSCREEN;
    keymap[14] = KEY_BREAK;
    keymap[15] = KEY_TILDE;
    keymap[16] = KEY_1;
    keymap[17] = KEY_2;
    keymap[18] = KEY_3;
    keymap[19] = KEY_4;
    keymap[20] = KEY_5;
    keymap[21] = KEY_6;
    keymap[22] = KEY_7;
    keymap[23] = KEY_8;
    keymap[24] = KEY_9;
    keymap[25] = KEY_0;
    keymap[26] = KEY_DASH;
    keymap[27] = KEY_EQUAL;
    keymap[28] = KEY_BACKSPACE;
    keymap[29] = KEY_TAB;
    keymap[30] = KEY_Q;
    keymap[31] = KEY_W;
    keymap[32] = KEY_E;
    keymap[33] = KEY_R;
    keymap[34] = KEY_T;
    keymap[35] = KEY_Y;
    keymap[36] = KEY_U;
    keymap[37] = KEY_I;
    keymap[38] = KEY_O;
    keymap[39] = KEY_P;
    keymap[40] = KEY_BRACKET_OPEN;
    keymap[41] = KEY_BEACKET_CLOSE;
    keymap[42] = KEY_BACKSLASH;
    keymap[43] = KEY_CAPS;
    keymap[44] = KEY_A;
    keymap[45] = KEY_S;
    keymap[46] = KEY_D;
    keymap[47] = KEY_F;
    keymap[48] = KEY_G;
    keymap[49] = KEY_H;
    keymap[50] = KEY_J;
    keymap[51] = KEY_K;
    keymap[52] = KEY_L;
    keymap[53] = KEY_SEMICOLON;
    keymap[54] = KEY_QUOTE;
    keymap[55] = KEY_ENTER;
    keymap[56] = KEY_LSHIFT;
    keymap[57] = KEY_Z;
    keymap[58] = KEY_X;
    keymap[59] = KEY_C;
    keymap[60] = KEY_V;
    keymap[61] = KEY_B;
    keymap[62] = KEY_N;
    keymap[63] = KEY_M;
    keymap[64] = KEY_COMMA;
    keymap[65] = KEY_DOT;
    keymap[66] = KEY_SLASH;
    keymap[67] = KEY_RSHIFT;
    keymap[68] = KEY_LCTRL;
    keymap[69] = KEY_LALT;
    keymap[70] = KEY_SPACE;
    keymap[71] = KEY_RALT;
    keymap[72] = KEY_RCTRL;
    keymap[73] = KEY_DELETE;
    keymap[74] = KEY_INSERT;
    keymap[75] = KEY_HOME;
    keymap[76] = KEY_END;
    keymap[77] = KEY_PGUP;
    keymap[78] = KEY_PGDWN;
    keymap[79] = KEY_UP;
    keymap[80] = KEY_DOWN;
    keymap[81] = KEY_LEFT;
    keymap[82] = KEY_RIGHT;
    keymap[83] = KEY_NUM_DIVIDE;
    keymap[84] = KEY_NUM_MULTIPLY;
    keymap[85] = KEY_NUM_SUBTRACT;
    keymap[86] = KEY_NUM_ADD;
    keymap[87] = KEY_NUM_ENTER;
    keymap[88] = KEY_NUM_DOT;
    keymap[89] = KEY_NUM_1;
    keymap[90] = KEY_NUM_2;
    keymap[91] = KEY_NUM_3;
    keymap[92] = KEY_NUM_4;
    keymap[93] = KEY_NUM_5;
    keymap[94] = KEY_NUM_6;
    keymap[95] = KEY_NUM_7;
    keymap[96] = KEY_NUM_8;
    keymap[97] = KEY_NUM_9;
    keymap[98] = KEY_NUM_0;
    keymap[99] = KEY_NUM_LOCK;
}

void input_mouse_getpos_platform(void* wnd_hdl, OUT int* x, OUT int* y)
{
    HWND hwnd = (HWND)wnd_hdl;
    POINT pt;

    GetCursorPos(&pt);
    ScreenToClient(hwnd, &pt);
    *x = pt.x;
    *y = pt.y;
}

void input_mouse_setpos_platform(void* wnd_hdl, int x, int y)
{
    HWND hwnd = (HWND)wnd_hdl;
    POINT pt = {x, y};

    ClientToScreen(hwnd, &pt);
    SetCursorPos(pt.x, pt.y);
}

bool_t input_mouse_getkey_platform(void* wnd_hdl, enum input_mouse_key mkey)
{
    int vkey = 0;
    switch (mkey)   {
    case INPUT_MOUSEKEY_LEFT:
        vkey = VK_LBUTTON;
        break;
    case INPUT_MOUSEKEY_RIGHT:
        vkey = VK_RBUTTON;
        break;
    case INPUT_MOUSEKEY_MIDDLE:
        vkey = VK_MBUTTON;
        break;
    case INPUT_MOUSEKEY_PGUP:
        vkey = VK_XBUTTON1;
        break;
    case INPUT_MOUSEKEY_PGDOWN:
        vkey = VK_XBUTTON2;
        break;
    }

    return (GetAsyncKeyState(vkey)&0x8000);
}

bool_t input_kb_getkey_platform(void* wnd_hdl, const uint keymap[INPUT_KEY_CNT],
                                enum input_key key)
{
    int keycode = (int)keymap[(uint)key];
    return (GetAsyncKeyState(keycode)&0x8000) >> 15;
}

#endif