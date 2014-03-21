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

/**
 * Engine's swig interface file for lua bindings
 * Note that I change styles from C style to C++ style for more convenient lua bindings
 */

#if defined(SWIG)
%module eng

%{
#include "dhcore/core.h"
#include "luabind/script-lua-common.h"
#include "luaengine.i"
%}

%import "luacore.i"
%import "../../../include/core/types.h"

#else
#include "luacore.i"
#include "dhcore/types.h"
#include "cmp-types.h"
#endif

#ifdef __cplusplus
#ifndef SWIG
struct cmp_obj;
#endif

class Object;

/*************************************************************************************************
 * types/enums
 */
enum TriggerState
{
    TRIGGER_UNKNOWN = 0,
    TRIGGER_IN = 1,
    TRIGGER_OUT = 2
};

enum LightType
{
	LIGHT_POINT = 2,
	LIGHT_SPOT = 3
};

enum InputKey	{
	KEY_ESC = 0,
	KEY_F1,
	KEY_F2,
	KEY_F3,
	KEY_F4,
	KEY_F5,
	KEY_F6,
	KEY_F7,
	KEY_F8,
	KEY_F9,
	KEY_F10,
	KEY_F11,
	KEY_F12,
	KEY_PRINTSCREEN,
	KEY_BREAK,
	KEY_TILDE,
	KEY_1,
	KEY_2,
	KEY_3,
	KEY_4,
	KEY_5,
	KEY_6,
	KEY_7,
	KEY_8,
	KEY_9,
	KEY_0,
	KEY_DASH,
	KEY_EQUAL,
	KEY_BACKSPACE,
	KEY_TAB,
	KEY_Q,
	KEY_W,
	KEY_E,
	KEY_R,
	KEY_T,
	KEY_Y,
	KEY_U,
	KEY_I,
	KEY_O,
	KEY_P,
	KEY_BRACKET_OPEN,
	KEY_BRACKET_CLOSE,
	KEY_BACKSLASH,
	KEY_CAPS,
	KEY_A,
	KEY_S,
	KEY_D,
	KEY_F,
	KEY_G,
	KEY_H,
	KEY_J,
	KEY_K,
	KEY_L,
	KEY_SEMICOLON,
	KEY_QUOTE,
	KEY_ENTER,
	KEY_LSHIFT,
	KEY_Z,
	KEY_X,
	KEY_C,
	KEY_V,
	KEY_B,
	KEY_N,
	KEY_M,
	KEY_COMMA,
	KEY_DOT,
	KEY_SLASH,
	KEY_RSHIFT,
	KEY_LCTRL,
	KEY_LALT,
	KEY_SPACE,
	KEY_RALT,
	KEY_RCTRL,
	KEY_DELETE,
	KEY_INSERT,
	KEY_HOME,
	KEY_END,
	KEY_PGUP,
	KEY_PGDWN,
	KEY_UP,
	KEY_DOWN,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_NUM_SLASH,
	KEY_NUM_MULTIPLY,
	KEY_NUM_MINUS,
	KEY_NUM_PLUS,
	KEY_NUM_ENTER,
	KEY_NUM_DOT,
	KEY_NUM_1,
	KEY_NUM_2,
	KEY_NUM_3,
	KEY_NUM_4,
	KEY_NUM_5,
	KEY_NUM_6,
	KEY_NUM_7,
	KEY_NUM_8,
	KEY_NUM_9,
	KEY_NUM_0,
	KEY_NUM_LOCK,
	KEY_CNT   /* count of input key enums */
};

/*************************************************************************************************
 * components
 */
class Component : public BaseAlloc
{
private:
    cmphandle_t hdl_;

public:
    Component();

#ifndef SWIG
    Component(cmphandle_t hdl);
#endif

    const char* __str__();
    const char* name();

    Object getHost();

    void debug();
    void undebug();

    void __setitem__(const char* valuename, fl64 value);
    void __setitem__(const char* valuename, bool value);
    void __setitem__(const char* valuename, const char* value);
    void __setitem__(const char* valuename, const Vector& value);
    void __setitem__(const char* valuename, const Color& value);
};

/*************************************************************************************************
 * components
 */
class CharacterAnim : public BaseAlloc
{
private:
    reshandle_t hdl_;
    void** inst_;

public:
    CharacterAnim();
#ifndef SWIG
    CharacterAnim(cmphandle_t hdl);
#endif
    const char* __str__();

    void setParam(const char* name, fl64 value);
    void setParam(const char* name, bool value);
    fl64 getParam(const char* name);
    bool getParamBool(const char* name);
};

/*************************************************************************************************
 * Object
 */
class Object : public BaseAlloc
{
private:
    cmp_obj* o_;

public:
    Object();
    Object(const Object& o);
    const char* __str__();

#ifndef SWIG
    Object(cmp_obj* o);
#endif

    void move(float x, float y, float z);
    void move(const Vector& pos);
    void rotate(float rx, float ry, float rz);
    void rotate(const Quat& rot);

    Component addComponent(const char* cmpname);

    void loadRigidBody(const char* h3dpfile);
    void unloadRigidBody();

    void loadAttachDock(const char* dock1, const char* dock2 = "", const char* dock3 = "",
        const char* dock4 = "");
    void attach(const Object& obj, const char* dock_name);
    void detach();

    void loadAnimation(const char* h3da_filepath);
    void loadCharacterAnim(const char* ctrl_filepath);
    CharacterAnim getCharacterAnim();

    void addForce(const Vector& force);
    void addForce(float fx, float fy, float fz);
    void addTorque(const Vector& torque);
    void addTorque(float tx, float ty, float tz);

    const char* name() const;
    bool isNull() const;

    Component __getitem__(const char* cmpname);
};

/*************************************************************************************************
 * Scene
 */
class Scene : public BaseAlloc
{
private:
    uint id_;

public:
    Scene();
    Scene(const char* name);
    const char* __str__();

    void clear();
    Object find(const char* name);
    Object getObject(uint obj_id);

    Object createModelLod(const char* name, const char* h3dm_hi, const char* h3dm_md = "",
        const char* h3dm_lo = "");
    Object createModel(const char* name, const char* h3dmfile);
    Object createPointLight(const char* name);
    Object createSpotLight(const char* name);
    Object createTrigger(const char* name, float bx, float by, float bz, const char* funcname);

    void setMax(float x, float y, float z);
    void setMin(float x, float y, float z);
    void setGravity(float gx, float gy, float gz);
    void setGravity(const Vector& gravity);
    void createPhysicsPlane();
};

/*************************************************************************************************
 * input
 */
class Input : public BaseAlloc
{
public:
    Input();

    bool keyPressed(enum InputKey key);

    bool mouseLeftPressed();
    bool mouseRightPressed();
    bool mouseMiddlePressed();
    Vector2D mousePos();
};


/* world props */
void setSunDir(float x, float y, float z);
void setSunDir(const Vector& v);
void setSunColor(float r, float g, float b);
void setSunColor(const Color& c);
void setSunIntensity(float i);
void setAmbientIntensity(float a);
void setAmbientSky(float r, float g, float b);
void setAmbientSky(const Color& c);
void setAmbientGround(float r, float g, float b);
void setAmbientGround(const Color& c);

uint addTimer(uint timeout, const char* funcname, bool single_shot = FALSE);
void removeTimer(uint id);
void setMemThreshold(int mem_sz);

#endif

