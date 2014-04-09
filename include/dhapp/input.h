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

/**
 * @defgroup input Input
 */

#ifndef INPUT_H_
#define INPUT_H_

#include "dhcore/types.h"
#include "dhcore/vec-math.h"
#include "app-api.h"

/**
 * mouse key enumerator
 * @see input_get_mouse
 * @ingroup input
 */
enum input_mouse_key
{
	INPUT_MOUSEKEY_LEFT = 0, /**< indicates that left button is pressed */
	INPUT_MOUSEKEY_RIGHT, /**< indicates that right button is pressed */
	INPUT_MOUSEKEY_MIDDLE, /**< indicates that middle button is pressed */
	INPUT_MOUSEKEY_PGUP, /**< indicates that page-up button is pressed */
	INPUT_MOUSEKEY_PGDOWN /**< indicates that page-down button is pressed */
};

/**
 * keyboard enumerator, represents keys on the keyboard
 * @see input_get_kbhit
 * @ingroup input
 */
enum input_key	{
	INPUT_KEY_ESC = 0,
	INPUT_KEY_F1,
	INPUT_KEY_F2,
	INPUT_KEY_F3,
	INPUT_KEY_F4,
	INPUT_KEY_F5,
	INPUT_KEY_F6,
	INPUT_KEY_F7,
	INPUT_KEY_F8,
	INPUT_KEY_F9,
	INPUT_KEY_F10,
	INPUT_KEY_F11,
	INPUT_KEY_F12,
	INPUT_KEY_PRINTSCREEN,
	INPUT_KEY_BREAK,
	INPUT_KEY_TILDE,
	INPUT_KEY_1,
	INPUT_KEY_2,
	INPUT_KEY_3,
	INPUT_KEY_4,
	INPUT_KEY_5,
	INPUT_KEY_6,
	INPUT_KEY_7,
	INPUT_KEY_8,
	INPUT_KEY_9,
	INPUT_KEY_0,
	INPUT_KEY_DASH,
	INPUT_KEY_EQUAL,
	INPUT_KEY_BACKSPACE,
	INPUT_KEY_TAB,
	INPUT_KEY_Q,
	INPUT_KEY_W,
	INPUT_KEY_E,
	INPUT_KEY_R,
	INPUT_KEY_T,
	INPUT_KEY_Y,
	INPUT_KEY_U,
	INPUT_KEY_I,
	INPUT_KEY_O,
	INPUT_KEY_P,
	INPUT_KEY_BRACKET_OPEN,
	INPUT_KEY_BRACKET_CLOSE,
	INPUT_KEY_BACKSLASH,
	INPUT_KEY_CAPS,
	INPUT_KEY_A,
	INPUT_KEY_S,
	INPUT_KEY_D,
	INPUT_KEY_F,
	INPUT_KEY_G,
	INPUT_KEY_H,
	INPUT_KEY_J,
	INPUT_KEY_K,
	INPUT_KEY_L,
	INPUT_KEY_SEMICOLON,
	INPUT_KEY_QUOTE,
	INPUT_KEY_ENTER,
	INPUT_KEY_LSHIFT,
	INPUT_KEY_Z,
	INPUT_KEY_X,
	INPUT_KEY_C,
	INPUT_KEY_V,
	INPUT_KEY_B,
	INPUT_KEY_N,
	INPUT_KEY_M,
	INPUT_KEY_COMMA,
	INPUT_KEY_DOT,
	INPUT_KEY_SLASH,
	INPUT_KEY_RSHIFT,
	INPUT_KEY_LCTRL,
	INPUT_KEY_LALT,
	INPUT_KEY_SPACE,
	INPUT_KEY_RALT,
	INPUT_KEY_RCTRL,
	INPUT_KEY_DELETE,
	INPUT_KEY_INSERT,
	INPUT_KEY_HOME,
	INPUT_KEY_END,
	INPUT_KEY_PGUP,
	INPUT_KEY_PGDWN,
	INPUT_KEY_UP,
	INPUT_KEY_DOWN,
	INPUT_KEY_LEFT,
	INPUT_KEY_RIGHT,
	INPUT_KEY_NUM_SLASH,
	INPUT_KEY_NUM_MULTIPLY,
	INPUT_KEY_NUM_MINUS,
	INPUT_KEY_NUM_PLUS,
	INPUT_KEY_NUM_ENTER,
	INPUT_KEY_NUM_DOT,
	INPUT_KEY_NUM_1,
	INPUT_KEY_NUM_2,
	INPUT_KEY_NUM_3,
	INPUT_KEY_NUM_4,
	INPUT_KEY_NUM_5,
	INPUT_KEY_NUM_6,
	INPUT_KEY_NUM_7,
	INPUT_KEY_NUM_8,
	INPUT_KEY_NUM_9,
	INPUT_KEY_NUM_0,
	INPUT_KEY_NUM_LOCK,
	INPUT_KEY_CNT   /* count of input key enums */
};

/* api */
/**
 * Updates input system on each frame. should be called within frame progression normally before calling `input_get_XXX` functions
 * @see eng_update
 * @ingroup input
 */
APP_API void input_update();

/**
 * Returns mouse position relative to client area of window
 * @ingroup input
 * @see input_update
 */
APP_API struct vec2i* input_mouse_getpos(struct vec2i* pos);

/**
 * Checks if specified mouse key is pressed
 * @param once Determines if pressed key should return once only, If this parameter is set to TRUE,
 * the function will only return TRUE once if the key is pressed until the user re-presses the key
 * @ingroup input
 * @see input_mouse_key
 * @see input_update
 */
APP_API bool_t input_mouse_getkey(enum input_mouse_key mkey, bool_t once);

/**
 * Receives keyboard hit state
 * @param keycode key code which the pressed state want to be returned. see win-keycodes.h or x11-keycodes.h for list of key codes
 * @return TRUE if specified key in `keycode` parameter is pressed
 * @ingroup input
 */
APP_API bool_t input_kb_getkey(enum input_key key, bool_t once);

/**
 * Translates os-dependant keycode to familiat engine key enum
 * @param vkey Virtual key code, OS dependant value, for example in windows, they are @e VK_ defines
 * @return Engine's own key enumerator
 * @ingroup input
 */
APP_API enum input_key input_kb_translatekey(uint vkey);

/**
 * Smooths mouse movement (x, y)
 * @param rx (input/output) For input pass the previous X of smoothed mouse position,
 * for output, It returns the new smoothed X position
 * @param ry (input/output) For input pass the previous Y of smoothed mouse position,
 * for output, It returns the new smoothed Y position
 * @param real_x Real X position of the mouse on screen, usually retreived by @e input_get_mouse
 * @param real_y Real Y position of the mouse on screen, usually retreived by @e input_get_mouse
 * @param springiness Spriginness values defines the smoothing strength, usually between 50~100
 * @param dt Delta-time from the last @e input_mouse_smooth call, in seconds
 * @ingroup input
 */
APP_API void input_mouse_smooth(INOUT float* rx, INOUT float* ry, float real_x, float real_y,
                                   float springiness, float dt);

/**
 * Locks mouse cursor position to specified X, Y position
 * @ingroup input
 */
APP_API void input_mouse_lockcursor(int x, int y);

/**
 * Unlocks mouse cursor
 * @ingroup input
 */
APP_API void input_mouse_unlockcursor();

/**
 * @see input_get_kbhit
 * @ingroup input
 */
APP_API void input_kb_lockkey(enum input_key key, bool_t pressed);

/**
 * @see input_get_kbhit
 * @ingroup input
 */
APP_API void input_kb_unlockkey(enum input_key key);

/**
 * @ingroup input
 */
APP_API void input_kb_resetlocks();

/**
 * @see input_get_mouse
 * @ingroup input
 */
APP_API void input_mouse_lockkey(enum input_mouse_key key, bool_t pressed);

/**
 * @see input_get_mouse
 * @ingroup input
 */
APP_API void input_mouse_unlockkey(enum input_mouse_key key);

/**
 * @ingroup input
 */
APP_API void input_mouse_resetlocks();

/* internal */
void input_zero();
void input_init();
void input_release();

#endif /* INPUT_H_ */
