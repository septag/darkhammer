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
enum class inMouseKey : int
{
	LEFT = 0, /**< indicates that left button is pressed */
	RIGHT, /**< indicates that right button is pressed */
	MIDDLE, /**< indicates that middle button is pressed */
	PGUP, /**< indicates that page-up button is pressed */
	PGDOWN /**< indicates that page-down button is pressed */
};

/**
 * keyboard enumerator, represents keys on the keyboard
 * @see input_get_kbhit
 * @ingroup input
 */
enum class inKey : int
{
	ESC = 0,
	F1,
	F2,
	F3,
	F4,
	F5,
	F6,
	F7,
	F8,
	F9,
	F10,
	F11,
	F12,
	PRINTSCREEN,
	BREAK,
	TILDE,
	K1,
	K2,
	K3,
	K4,
	K5,
	K6,
	K7,
	K8,
	K9,
	K0,
	DASH,
	EQUAL,
	BACKSPACE,
	TAB,
	Q,
	W,
	E,
	R,
	T,
	Y,
	U,
	I,
	O,
	P,
	BRACKET_OPEN,
	BRACKET_CLOSE,
	BACKSLASH,
	CAPS,
	A,
	S,
	D,
	F,
	G,
	H,
	J,
	K,
	L,
	SEMICOLON,
	QUOTE,
	ENTER,
	LSHIFT,
	Z,
	X,
	C,
	V,
	B,
	N,
	M,
	COMMA,
	DOT,
	SLASH,
	RSHIFT,
	LCTRL,
	LALT,
	SPACE,
	RALT,
	RCTRL,
	DEL,
	INSERT,
	HOME,
	END,
	PGUP,
	PGDWN,
	UP,
	DOWN,
	LEFT,
	RIGHT,
	NK_SLASH,
	NK_MULTIPLY,
	NK_MINUS,
	NK_PLUS,
	NK_ENTER,
	NK_DOT,
	NK1,
	NK2,
	NK3,
	NK4,
	NK5,
	NK6,
	NK7,
	NK8,
	NK9,
	NK0,
	NK_LOCK,
	COUNT   /* count of input key enums */
};

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
APP_API void input_mouse_getpos(OUT int *mx, OUT int *my);

/**
 * Checks if specified mouse key is pressed
 * @param once Determines if pressed key should return once only, If this parameter is set to TRUE,
 * the function will only return TRUE once if the key is pressed until the user re-presses the key
 * @ingroup input
 * @see input_mouse_key
 * @see input_update
 */
APP_API bool input_mouse_getkey(inMouseKey mkey, bool once = false);

/**
 * Receives keyboard hit state
 * @param keycode key code which the pressed state want to be returned. see win-keycodes.h or x11-keycodes.h for list of key codes
 * @return TRUE if specified key in `keycode` parameter is pressed
 * @ingroup input
 */
APP_API bool input_kb_getkey(inKey key, bool once = false);

/**
 * Translates os-dependant keycode to familiat engine key enum
 * @param vkey Virtual key code, OS dependant value, for example in windows, they are @e VK_ defines
 * @return Engine's own key enumerator
 * @ingroup input
 */
APP_API inKey input_kb_translatekey(uint vkey);

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
APP_API void input_kb_lockkey(inKey key, bool pressed = true);

/**
 * @see input_get_kbhit
 * @ingroup input
 */
APP_API void input_kb_unlockkey(inKey key);

/**
 * @ingroup input
 */
APP_API void input_kb_resetlocks();

/**
 * @see input_get_mouse
 * @ingroup input
 */
APP_API void input_mouse_lockkey(inMouseKey key, bool pressed = true);

/**
 * @see input_get_mouse
 * @ingroup input
 */
APP_API void input_mouse_unlockkey(inMouseKey key);

/**
 * @ingroup input
 */
APP_API void input_mouse_resetlocks();

/* internal */
result_t input_init();
void input_release();

#endif /* INPUT_H_ */
