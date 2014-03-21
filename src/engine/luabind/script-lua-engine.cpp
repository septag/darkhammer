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
#include "dhcore/vec-math.h"

#include <stdio.h>

#include "luaengine.i"
#include "cmp-mgr.h"
#include "scene-mgr.h"
#include "mem-ids.h"
#include "script.h"
#include "gfx.h"
#include "phx-device.h"
#include "phx.h"
#include "res-mgr.h"
#include "input.h"
#include "engine.h"
#include "world-mgr.h"

#include "components/cmp-light.h"
#include "components/cmp-rbody.h"
#include "components/cmp-trigger.h"
#include "components/cmp-attachment.h"
#include "components/cmp-animchar.h"

#define PROTECT_OBJECT()    if (o_ == NULL) {   \
                                sct_throwerror("Object is null");  \
                            return;    }

#define PROTECT_OBJECT_R(r)    if (o_ == NULL) {   \
    sct_throwerror("Object is null");  \
    return r;    }

#define PROTECT_COMPONENT()    if (hdl_ == INVALID_HANDLE) {   \
    sct_throwerror("Component is null");  \
    return;    }

#define PROTECT_COMPONENT_R(r)    if (hdl_ == INVALID_HANDLE) {   \
    sct_throwerror("Component is null");  \
    return r;    }

#define PROTECT_SCENE()    if (id_ == 0) {   \
    sct_throwerror("Scene is undefined");  \
    return;    }

#define PROTECT_SCENE_R(r)    if (id_ == 0) {   \
    sct_throwerror("Scene is undefined");  \
    return (r);    }


/*************************************************************************************************
 * Helpers
 */
cmphandle_t get_component(struct cmp_obj* obj, const char* cmp_name)
{
    cmp_t c = cmp_findname(cmp_name);
    if (c == NULL)  {
        sct_throwerror("get_component - could not find Component '%s'", cmp_name);
        return INVALID_HANDLE;
    }

    cmptype_t type = cmp_gettype(c);
    cmphandle_t hdl = cmp_findinstance(obj->chain, type);

    if (hdl == INVALID_HANDLE) {
        sct_throwerror("get_component - could not find Component '%s' in Object '%s'", cmp_name,
            obj->name);
    }

    return hdl;
}

cmphandle_t add_component(struct cmp_obj* obj, const char* cmp_name)
{
    cmp_t c = cmp_findname(cmp_name);
    if (c == NULL)  {
        sct_throwerror("add_component - could not find Component '%s'", cmp_name);
        return INVALID_HANDLE;
    }

    cmphandle_t hdl = cmp_findinstance(obj->chain, cmp_gettype(c));
    if (hdl != INVALID_HANDLE)
        return hdl;

    hdl = cmp_create_instance(c, obj, 0, INVALID_HANDLE, 0);
    if (hdl == INVALID_HANDLE)  {
        sct_throwerror("add_component - could create Component '%s' instance", cmp_name);
        return INVALID_HANDLE;
    }

    return hdl;
}

/*************************************************************************************************
 * Component
 */
Component::Component()
{
    hdl_ = INVALID_HANDLE;
}

Component::Component(cmphandle_t hdl)
{
    hdl_ = hdl;
}

const char* Component::__str__()
{
    static char text[64];

    const char* name;
    if (hdl_ != INVALID_HANDLE) {
        cmp_t c = cmp_getbyhdl(hdl_);
        name = cmp_getname(c);
    }    else   {
        name = "[None]";
    }

    sprintf(text, "Component: %s", name);
    return text;
}

const char* Component::name()
{
    if (hdl_ != INVALID_HANDLE)
        return cmp_getname(cmp_getbyhdl(hdl_));
    else
        return "[None]";
}

void Component::__setitem__(const char* valuename, const char* value)
{
    PROTECT_COMPONENT();
    cmp_value_sets(hdl_, valuename, value);
}

void Component::__setitem__(const char* valuename, fl64 value)
{
    PROTECT_COMPONENT();

    cmp_valuetype type = cmp_getvaluetype(hdl_, valuename);
    if (type == CMP_VALUE_UNKNOWN)  {
        sct_throwerror("Value '%s' not found within Component", valuename);
        return;
    }

    switch (type)   {
    case CMP_VALUE_INT:
        cmp_value_seti(hdl_, valuename, (int)value);
        break;
    case CMP_VALUE_UINT:
        cmp_value_setui(hdl_, valuename, (uint)fabs(value));
        break;
    case CMP_VALUE_FLOAT:
        cmp_value_setf(hdl_, valuename, (float)value);
    default:
        break;
    }
}

void Component::__setitem__(const char* valuename, const Vector& value)
{
    PROTECT_COMPONENT();
    cmp_value_set3f(hdl_, valuename, value.v_.f);
}

void Component::__setitem__(const char* valuename, const Color& value)
{
    PROTECT_COMPONENT();
    cmp_value_set4f(hdl_, valuename, value.c_.f);
}

void Component::__setitem__(const char* valuename, bool value)
{
    PROTECT_COMPONENT();
    cmp_value_setb(hdl_, valuename, value ? TRUE : FALSE);
}

Object Component::getHost()
{
    PROTECT_COMPONENT_R(Object());
    return Object(cmp_getinstancehost(hdl_));
}

void Component::debug()
{
    PROTECT_COMPONENT();
    cmp_debug_add(hdl_);
}

void Component::undebug()
{
    PROTECT_COMPONENT();
    cmp_debug_remove(hdl_);
}

/*************************************************************************************************
 * CharacterAnim
 */
CharacterAnim::CharacterAnim()
{
    hdl_ = INVALID_HANDLE;
    inst_ = NULL;
}

CharacterAnim::CharacterAnim(cmphandle_t hdl)
{
    if (hdl != INVALID_HANDLE)  {
        struct cmp_animchar* ch = (struct cmp_animchar*)cmp_getinstancedata(hdl);
        hdl_ = ch->ctrl_hdl;
        inst_ = (void**)&ch->inst;
    }   else    {
        hdl_ = INVALID_HANDLE;
        inst_ = NULL;
    }
}

const char* CharacterAnim::__str__()
{
    static char text[64];
    const char* name;

    if (hdl_ != INVALID_HANDLE)
        name = rs_get_filepath(hdl_);
    else
        name = "[None]";

    sprintf(text, "Character Animation: %s", name);
    return text;
}

void CharacterAnim::setParam(const char* name, fl64 value)
{
    if (hdl_ == INVALID_HANDLE || inst_ == NULL) {
        sct_throwerror("Character animation is empty");
        return;
    }

    anim_ctrl_inst inst = *((anim_ctrl_inst*)inst_);
    anim_ctrl ctrl = rs_get_animctrl(hdl_);
    if (ctrl == NULL || inst == NULL)
        return;

    enum anim_ctrl_paramtype type = anim_ctrl_get_paramtype(ctrl, inst, name);
    if (type == ANIM_CTRL_PARAM_UNKNOWN)    {
        sct_throwerror("Animation controller parameter '%s' does not exist", name);
        return;
    }

    switch (type)   {
    case ANIM_CTRL_PARAM_FLOAT:
        anim_ctrl_set_paramf(ctrl, inst, name, (float)value);
        break;
    case ANIM_CTRL_PARAM_INT:
        anim_ctrl_set_parami(ctrl, inst, name, (int)value);
        break;
    default:
        break;
    }
}

void CharacterAnim::setParam(const char* name, bool value)
{
    if (hdl_ == INVALID_HANDLE || inst_ == NULL) {
        sct_throwerror("Character animation is empty");
        return;
    }

    anim_ctrl_inst inst = *((anim_ctrl_inst*)inst_);
    anim_ctrl ctrl = rs_get_animctrl(hdl_);
    if (ctrl == NULL || inst == NULL)
        return;

    anim_ctrl_set_paramb(ctrl, inst, name, (bool_t)value);
}

fl64 CharacterAnim::getParam(const char* name)
{
    if (hdl_ == INVALID_HANDLE || inst_ == NULL) {
        sct_throwerror("Character animation is empty");
        return 0.0;
    }

    anim_ctrl_inst inst = *((anim_ctrl_inst*)inst_);
    anim_ctrl ctrl = rs_get_animctrl(hdl_);
    if (ctrl == NULL || inst == NULL)
        return 0.0;

    enum anim_ctrl_paramtype type = anim_ctrl_get_paramtype(ctrl, inst, name);
    if (type == ANIM_CTRL_PARAM_UNKNOWN)    {
        sct_throwerror("Animation controller parameter '%s' does not exist", name);
        return 0.0;
    }

    switch (type)   {
    case ANIM_CTRL_PARAM_FLOAT:
        return (fl64)anim_ctrl_get_paramf(ctrl, inst, name);
    case ANIM_CTRL_PARAM_BOOLEAN:
        return (fl64)anim_ctrl_get_paramb(ctrl, inst, name);
    case ANIM_CTRL_PARAM_INT:
        return (fl64)anim_ctrl_get_parami(ctrl, inst, name);
    default:
        return 0.0;
    }
}

bool CharacterAnim::getParamBool(const char* name)
{
    if (hdl_ == INVALID_HANDLE || inst_ == NULL) {
        sct_throwerror("Character animation is empty");
        return false;
    }

    anim_ctrl_inst inst = *((anim_ctrl_inst*)inst_);
    anim_ctrl ctrl = rs_get_animctrl(hdl_);
    if (ctrl == NULL || inst == NULL)
        return false;

    return anim_ctrl_get_paramb(ctrl, inst, name) ? true : false;
}

/*************************************************************************************************
 * Object
 */
Object::Object()
{
    o_ = NULL;
}

Object::Object(const Object& o)
{
    this->o_ = o.o_;
}

Object::Object(cmp_obj* o)
{
    this->o_ = o;
}

bool Object::isNull() const
{
    return (o_ == NULL) ? true : false;
}

const char* Object::__str__()
{
    static char text[64];
    if (o_ != NULL)
        sprintf(text, "Object(%s)", o_->name);
    else
        strcpy(text, "Object(null)");
    return text;
}


void Object::move(float x, float y, float z)
{
    PROTECT_OBJECT();
    cmphandle_t c = o_->xform_cmp;
    if (c != INVALID_HANDLE)    {
        struct mat3f m;
        cmp_value_get3m(&m, c, "transform");
        mat3_set_transf(&m, x, y, z);
        cmp_value_set3m(c, "transform", &m);
    }
}

void Object::rotate(float rx, float ry, float rz)
{
    PROTECT_OBJECT();
    cmphandle_t c = o_->xform_cmp;
    if (c != INVALID_HANDLE)    {
        struct mat3f m;
        cmp_value_get3m(&m, c, "transform");
        mat3_set_roteuler(&m, math_torad(rx), math_torad(ry), math_torad(rz));
        cmp_value_set3m(c, "transform", &m);
    }
}

void Object::move(const Vector& pos)
{
    this->move(pos.v_.x, pos.v_.y, pos.v_.z);
}

void Object::rotate(const Quat& rot)
{
    PROTECT_OBJECT();
    cmphandle_t c = o_->xform_cmp;
    if (c != INVALID_HANDLE)    {
        struct mat3f m;
        cmp_value_get3m(&m, c, "transform");
        mat3_set_rotquat(&m, &rot.q_);
        cmp_value_set3m(c, "transform", &m);
    }
}

const char* Object::name() const
{
    if (o_ == NULL) {
        sct_throwerror("Object is null");
        return NULL;
    }
    return o_->name;
}

void Object::loadRigidBody(const char* h3dpfile)
{
    cmphandle_t rbody_hdl = add_component(o_, "rbody");
    if (rbody_hdl != INVALID_HANDLE)
        cmp_value_sets(rbody_hdl, "filepath", h3dpfile);
}

void Object::unloadRigidBody()
{
    if (this->o_->rbody_cmp != INVALID_HANDLE)
        cmp_destroy_instance(this->o_->rbody_cmp);
}

void Object::addForce(const Vector& force)
{
    PROTECT_OBJECT();

    cmphandle_t c = o_->rbody_cmp;
    if (c != INVALID_HANDLE)    {
        struct cmp_rbody* rb = (struct cmp_rbody*)cmp_getinstancedata(c);
        if (rb->rbody != NULL)  {
            phx_rigid_applyforce_localpos(rb->rbody, &force.v_, PHX_FORCE_IMPULSE,
                TRUE, &g_vec3_zero);
        }
    }
}

void Object::addForce(float fx, float fy, float fz)
{
    PROTECT_OBJECT();
    addForce(Vector(fx, fy, fz));
}

void Object::addTorque(const Vector& torque)
{
    PROTECT_OBJECT();
    cmphandle_t c = o_->rbody_cmp;
    if (c != INVALID_HANDLE)    {
        struct cmp_rbody* rb = (struct cmp_rbody*)cmp_getinstancedata(c);
        if (rb->rbody != NULL)
            phx_rigid_applytorque(rb->rbody, &torque.v_, PHX_FORCE_IMPULSE, TRUE);
    }
}

void Object::addTorque(float tx, float ty, float tz)
{
    PROTECT_OBJECT();
    addTorque(Vector(tx, ty, tz));
}

void Object::loadAttachDock(const char* dock1, const char* dock2, const char* dock3,
    const char* dock4)
{
    PROTECT_OBJECT();

    cmphandle_t hdl = add_component(this->o_, "attachdock");
    if (hdl != INVALID_HANDLE)  {
        const char* docks[] = {dock1, dock2, dock3, dock4};
        cmp_value_setsvp(hdl, "bindto", 4, docks);
    }
}

void Object::loadAnimation(const char* h3da_filepath)
{
    PROTECT_OBJECT();

    cmphandle_t hdl = add_component(this->o_, "anim");
    if (hdl != INVALID_HANDLE)  {
        cmp_value_sets(hdl, "filepath", h3da_filepath);
    }
}

void Object::loadCharacterAnim(const char* ctrl_filepath)
{
    PROTECT_OBJECT();
    cmphandle_t hdl = add_component(this->o_, "animchar");
    if (hdl != INVALID_HANDLE)
        cmp_value_sets(hdl, "filepath", ctrl_filepath);
}

CharacterAnim Object::getCharacterAnim()
{
    PROTECT_OBJECT_R(CharacterAnim());

    cmphandle_t hdl = get_component(o_, "animchar");
    if (hdl != INVALID_HANDLE)
        return CharacterAnim(hdl);
    return CharacterAnim();
}

void Object::attach(const Object& obj, const char* dock_name)
{
    PROTECT_OBJECT();

    cmphandle_t hdl = add_component(this->o_, "attachment");
    if (hdl != INVALID_HANDLE)
        cmp_attachment_attach(this->o_, obj.o_, dock_name);
}

void Object::detach()
{
    PROTECT_OBJECT();

    if (this->o_->attach_cmp != INVALID_HANDLE) {
        cmp_attachment_detach(this->o_);
        cmp_destroy_instance(this->o_->attach_cmp);
    }
}

Component Object::__getitem__(const char* cmpname)
{
    PROTECT_OBJECT_R(Component());

    cmphandle_t hdl = get_component(this->o_, cmpname);
    if (hdl != INVALID_HANDLE)
        return Component(hdl);

    return Component();
}

Component Object::addComponent(const char* cmpname)
{
    PROTECT_OBJECT_R(Component());

    cmphandle_t hdl = add_component(o_, cmpname);
    if (hdl != INVALID_HANDLE)  {
        return Component(hdl);
    }   else    {
        sct_throwerror("Failed to create Component '%s'", cmpname);
        return Component();
    }
}

/*************************************************************************************************
 * Scene
 */
Scene::Scene()
{
    id_ = scn_getactive();
}

Scene::Scene(const char* name)
{
    id_ = scn_findscene(name);
}

const char* Scene::__str__()
{
    static char text[64];
    if (id_ != 0)
        sprintf(text, "Scene(#%d)", id_);
    else
        strcpy(text, "Scene(undefined)");
    return text;
}

void Scene::clear()
{
    PROTECT_SCENE();
    scn_clear(id_);
}

Object Scene::find(const char* name)
{
    PROTECT_SCENE_R(Object());

    uint id = scn_findobj(id_, name);
    if (id == 0)  {
        sct_throwerror("Object '%s' not found in Scene", name);
        return Object();
    }

    return Object(scn_getobj(id_, id));
}

Object Scene::getObject(uint obj_id)
{
    PROTECT_SCENE_R(Object());

    if (obj_id == 0)  {
        sct_throwerror("invalid Object id");
        return Object();
    }

    return Object(scn_getobj(id_, obj_id));
}

Object Scene::createModel(const char* name, const char* h3dmfile)
{
    PROTECT_SCENE_R(Object());

    struct cmp_obj* obj = scn_create_obj(id_, name, CMP_OBJTYPE_MODEL);
    if (obj == NULL)    {
        sct_throwerror("createModel - could not create model Object '%s'", name);
        return Object();
    }

    cmphandle_t model_hdl = get_component(obj, "model");
    if (model_hdl != INVALID_HANDLE)
        cmp_value_sets(model_hdl, "filepath", h3dmfile);

    return Object(obj);
}

Object Scene::createModelLod(const char* name, const char* h3dm_hi, const char* h3dm_md,
    const char* h3dm_lo)
{
    PROTECT_SCENE_R(Object());

    struct cmp_obj* obj = scn_create_obj(id_, name, CMP_OBJTYPE_MODEL);
    if (obj == NULL)    {
        sct_throwerror("createModelLod - could not create model Object '%s'", name);
        return Object();
    }

    cmphandle_t model_hdl = get_component(obj, "model");
    if (model_hdl != INVALID_HANDLE)    {
        if (str_isempty(h3dm_hi))   {
            sct_throwerror("createModelLod - high detail model should not be empty");
            return Object();
        }

        cmp_value_sets(model_hdl, "model_high", h3dm_hi);
        if (!str_isempty(h3dm_md))
            cmp_value_sets(model_hdl, "model_medium", h3dm_md);
        if (!str_isempty(h3dm_lo))
            cmp_value_sets(model_hdl, "model_low", h3dm_lo);
    }

    return Object(obj);
}


Object Scene::createPointLight(const char* name)
{
    PROTECT_SCENE_R(Object());

    struct cmp_obj* obj = scn_create_obj(id_, name, CMP_OBJTYPE_LIGHT);
    if (obj == NULL)    {
        sct_throwerror("create light - could not create light Object '%s'", name);
        return Object();
    }

    cmphandle_t light_hdl = get_component(obj, "light");
    if (light_hdl != INVALID_HANDLE)
        cmp_value_setui(light_hdl, "type", (uint)CMP_LIGHT_POINT);

    return Object(obj);
}

Object Scene::createTrigger(const char* name, float bx, float by, float bz, const char* funcname)
{
    PROTECT_SCENE_R(Object());

    struct cmp_obj* obj = scn_create_obj(id_, name, CMP_OBJTYPE_TRIGGER);
    if (obj == NULL)    {
        sct_throwerror("create light - could not create trigger Object '%s'", name);
        return Object();
    }

    add_component(obj, "transform");

    cmphandle_t trigger_hdl = add_component(obj, "trigger");
    if (trigger_hdl != INVALID_HANDLE)  {
        float bf[] = {bx, by, bz};
        cmp_value_set3f(trigger_hdl, "box", bf);
        sct_addtrigger(trigger_hdl, funcname);
    }

    return Object(obj);
}

Object Scene::createSpotLight(const char* name)
{
    PROTECT_SCENE_R(Object());

    struct cmp_obj* obj = scn_create_obj(id_, name, CMP_OBJTYPE_LIGHT);
    if (obj == NULL)    {
        sct_throwerror("create light - could not create light Object '%s'", name);
        return Object();
    }

    add_component(obj, "transform");
    add_component(obj, "bounds");
    cmphandle_t light_hdl = add_component(obj, "light");
    if (light_hdl != INVALID_HANDLE)
        cmp_value_setui(light_hdl, "type", (uint)CMP_LIGHT_SPOT);

    return Object(obj);
}

void Scene::setMax(float x, float y, float z)
{
    PROTECT_SCENE();

    struct vec3f minpt, maxpt;
    scn_getsize(id_, &minpt, &maxpt);
    scn_setsize(id_, &minpt, vec3_setf(&maxpt, x, y, z));
}

void Scene::setMin(float x, float y, float z)
{
    PROTECT_SCENE();

    struct vec3f minpt, maxpt;
    scn_getsize(id_, &minpt, &maxpt);
    scn_setsize(id_, vec3_setf(&minpt, x, y, z), &maxpt);
}

void Scene::setGravity(float gx, float gy, float gz)
{
    PROTECT_SCENE();
    if (!BIT_CHECK(eng_get_params()->flags, ENG_FLAG_DISABLEPHX))
        setGravity(Vector(gx, gy, gz));
}

void Scene::setGravity(const Vector& gravity)
{
    PROTECT_SCENE();
    if (!BIT_CHECK(eng_get_params()->flags, ENG_FLAG_DISABLEPHX))
        phx_scene_setgravity(scn_getphxscene(id_), &gravity.v_);
}

void Scene::createPhysicsPlane()
{
    if (!BIT_CHECK(eng_get_params()->flags, ENG_FLAG_DISABLEPHX))
        phx_create_debugplane(0.5f, 0.1f);
}

/*************************************************************************************************
 * world
 */
void setSunDir(float x, float y, float z)
{
    struct variant v;
    uint sec_light = wld_find_section("light");
    wld_set_var(sec_light, wld_find_var(sec_light, "dir"), var_set3f(&v, x, y, z));
}

void setSunDir(const Vector& v)
{
    struct variant var;
    uint sec_light = wld_find_section("light");
    wld_set_var(sec_light, wld_find_var(sec_light, "dir"), var_set3fv(&var, v.v_.f));
}

void setSunColor(float r, float g, float b)
{
    struct variant v;
    uint sec_light = wld_find_section("light");
    wld_set_var(sec_light, wld_find_var(sec_light, "color"),
        var_set4f(&v, r/255.0f, g/255.0f, b/255.0f, 1.0f));
}

void setSunColor(const Color& c)
{
    struct variant var;
    uint sec_light = wld_find_section("light");
    wld_set_var(sec_light, wld_find_var(sec_light, "color"), var_set4fv(&var, c.c_.f));
}

void setSunIntensity(float i)
{
    struct variant v;
    uint sec_light = wld_find_section("light");
    wld_set_var(sec_light, wld_find_var(sec_light, "intensity"), var_setf(&v, i));
}

void setAmbientIntensity(float a)
{
    struct variant v;
    uint sec_light = wld_find_section("ambient");
    wld_set_var(sec_light, wld_find_var(sec_light, "intensity"), var_setf(&v, a));
}

void setAmbientSky(float r, float g, float b)
{
    struct variant v;
    uint sec_light = wld_find_section("ambient");
    wld_set_var(sec_light, wld_find_var(sec_light, "sky-color"),
        var_set4f(&v, r/255.0f, g/255.0f, b/255.0f, 1.0f));
}

void setAmbientGround(float r, float g, float b)
{
    struct variant v;
    uint sec_light = wld_find_section("ambient");
    wld_set_var(sec_light, wld_find_var(sec_light, "ground-color"),
        var_set4f(&v, r/255.0f, g/255.0f, b/255.0f, 1.0f));
}

void setAmbientSky(const Color& c)
{
    struct variant var;
    uint sec_light = wld_find_section("ambient");
    wld_set_var(sec_light, wld_find_var(sec_light, "sky-color"), var_set4fv(&var, c.c_.f));
}

void setAmbientGround(const Color& c)
{
    struct variant var;
    uint sec_light = wld_find_section("ambient");
    wld_set_var(sec_light, wld_find_var(sec_light, "ground-color"), var_set4fv(&var, c.c_.f));
}

/*************************************************************************************************/
uint addTimer(uint timeout, const char* funcname, bool single_shot)
{
    return sct_addtimer(timeout, funcname, single_shot ? TRUE : FALSE);
}

void removeTimer(uint id)
{
    sct_removetimer(id);
}

void setMemThreshold(int mem_sz)
{
    sct_setthreshold(mem_sz);
}

/*************************************************************************************************
 * Input
 */
Input::Input()
{
}

bool Input::keyPressed(enum InputKey key)
{
    return input_kb_getkey((enum input_key)key, FALSE) ? true : false;
}

bool Input::mouseLeftPressed()
{
    return input_mouse_getkey(INPUT_MOUSEKEY_LEFT, FALSE) ? true : false;
}

bool Input::mouseRightPressed()
{
    return input_mouse_getkey(INPUT_MOUSEKEY_RIGHT, FALSE) ? true : false;
}

bool Input::mouseMiddlePressed()
{
    return input_mouse_getkey(INPUT_MOUSEKEY_MIDDLE, FALSE) ? true : false;
}

Vector2D Input::mousePos()
{
    Vector2D p;
    input_mouse_getpos(&p.v_);
    return p;
}

