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

#ifndef __CMPMGR_H__
#define __CMPMGR_H__

#include "dhcore/types.h"
#include "engine-api.h"
#include "cmp-types.h"

struct mat3f;


/**
 * Initialize and registers a component
 * @param alloc Permanent allocator for component data
 * @param params Creation parameters.
 * @return Returns @e RET_OK on success
 * @see cmp_createparams
 * @ingroup cmp
 */
ENGINE_API result_t cmp_register_component(struct allocator* alloc,
    const struct cmp_createparams* params);

/**
 * Creates component instance, returns handle\n
 * The last two parameters should only be provided with @e indirect host components, where created component
 * is owned (created) by another component.
 * @param c Component object
 * @param obj Parent host object (can be NULL if instance is indirect)
 * @param flags Combination of @e cmp_instanceflag
 * @param parent_hdl Handle to the owner component, if handle is created indirect (CMP_INSTANCEFLAG_INDIRECTHOST flag), otherwise pass INVALID_HANDLE
 * @param offset_in_parent Offset to the handle of the child component (CMP_INSTANCEFLAG_INDIRECTHOST flag) in bytes
 * @return handle to newly created component
 * @see cmp_instanceflag
 * @see cmp_create_instance_forobj
 * @ingroup cmp
 */
ENGINE_API cmphandle_t cmp_create_instance(cmp_t c, struct cmp_obj* obj, uint flags,
    OPTIONAL cmphandle_t parent_hdl, OPTIONAL uint offset_in_parent);


/**
 * Creates component instance for object only (higher level cmp_create_instance), returns handle\n
 * Note that objects only accept single component instances for each component type
 * @param cmpname Component's name, which you want to create an instance for the object
 * @param obj Parent host object
 * @return Handle to newly created component
 * @see cmp_create_instance
 * @ingroup cmp
 */
ENGINE_API cmphandle_t cmp_create_instance_forobj(const char* cmpname, struct cmp_obj* obj);


/**
 * Destroys component instance, and removes it from component chain of object
 * @param hdl Handle to component instance
 * @see cmp_create_instance
 * @ingroup cmp
 */
ENGINE_API void cmp_destroy_instance(cmphandle_t hdl);

/**
 * Find component by it's type, searches the list of registered components for the specific component type
 * @param type Component type, components types usually are defined in their own headers
 * @return Valid component object or NULL if no such type found
 * @ingroup cmp
 */
ENGINE_API cmp_t cmp_findtype(cmptype_t type);

/**
 * Find component by it's registered name
 * @param name Component name
 * @return Valid component object or NULL if no such name found
 * @ingroup cmp
 */
ENGINE_API cmp_t cmp_findname(const char* name);

/**
 * Returns component's registered name
 * @ingroup cmp
 */
ENGINE_API const char* cmp_getname(cmp_t c);

/**
 * Get total registered components in the component system
 * @ingroup cmp
 */
ENGINE_API uint cmp_getcount();

/**
 * Find component instance in component chain by it's type
 * @param chain Component chain, which mostly belongs to game-objects.
 * @param type Component type to find in the chain
 * @return Valid component handle or INVALID_HANDLE if not found
 * @see cmp_obj
 * @see cmp_findinstance_inobj
 * @ingroup cmp
 */
ENGINE_API cmphandle_t cmp_findinstance(cmp_chain chain, cmptype_t type);

/**
 * Find component instance in object by it's name, this is a higher level (and slower) cmp_findinstance
 * @param obj Owner object for the desired component instance
 * @param cmpname Component name which you find in object
 * @return Valid component handle or INVALID_HANDLE if it's not found
 * @see cmp_obj
 * @see cmp_findinstance
 * @ingroup cmp
 */
ENGINE_API cmphandle_t cmp_findinstance_inobj(struct cmp_obj* obj, const char* cmpname);

/**
 * Find component instance in object by it's type
 * @param obj Component instance's owner object
 * @param type Component type to find in the chain
 * @return Valid component handle or @e INVALID_HANDLE if not found
 * @see cmp_obj
 * @see cmp_findinstance
 * @ingroup cmp
 */
ENGINE_API cmphandle_t cmp_findinstance_bytype_inobj(struct cmp_obj* obj, cmptype_t type);

/**
 * Get component's type
 * @ingroup cmp
 */
ENGINE_API cmptype_t cmp_gettype(cmp_t c);

/**
 * Get instance's owner component object
 * @see cmp_getbyidx
 * @ingroup cmp
 */
ENGINE_API cmp_t cmp_getbyhdl(cmphandle_t hdl);

/**
 * Get component by it's index, to get index of the component from component handle,
 * use CMP_GET_INDEX(cmp_hdl) macro
 * @see cmphandle_t
 * @see cmp_getbyhdl
 * @ingroup cmp
 */
ENGINE_API cmp_t cmp_getbyidx(uint idx);

/**
 * Get component's value type
 * @param hdl Component instance handle
 * @param name Value name inside the component, components have multiple values embedded inside them
 * @return Component value type, CMP_VALUE_UNKNOWN if specified value is not found
 * @see cmp_valuetype
 * @ingroup cmp
 */
ENGINE_API enum cmp_valuetype cmp_getvaluetype(cmphandle_t hdl, const char* name);

/**
 * Get component data from it's instance handle, data for components are defined as a structure inside
 * each component's header file
 * @param hdl Component instance handle
 * @return Pointer to component's data structure, must be casted to it's actual type.
 * @ingroup cmp
 */
ENGINE_API void* cmp_getinstancedata(cmphandle_t hdl);

/**
 * Get component instance flags
 * @return Combination of component instance flags
 * @see cmp_instanceflag
 * @ingroup cmp
 */
ENGINE_API uint cmp_getinstanceflags(cmphandle_t hdl);

/**
 * Get component instance host object (game-object)
 * @param hdl Valid handle for component instance, must not be INVALID_HANDLE
 * @return Component's owner object
 * @see cmp_obj
 * @ingroup cmp
 */
ENGINE_API struct cmp_obj* cmp_getinstancehost(cmphandle_t hdl);

/**
 * Sends component instance to update list. Update list will be reset in the next frame \n
 * @b Note: if user updates an instance in each update stage (update callbacks), it will be updated in
 * all stages that come after it in the current frame.
 * @see pfn_cmp_update
 * @ingroup cmp
 */
ENGINE_API void cmp_updateinstance(cmphandle_t hdl);

/**
 * Removes component instance from update list
 * @ingroup cmp
 */
ENGINE_API void cmp_updateinstance_reset(cmphandle_t hdl);

/**
 * Adds component instance to debug list, it will be permanent until component is destroyed or
 * @e cmp_debug_remove is called
 * @see cmp_debug_remove
 * @see pfn_cmp_debug
 * @ingroup cmp
 */
ENGINE_API void cmp_debug_add(cmphandle_t hdl);

/**
 * Removes component instance
 * @see cmp_debug_add
 * @ingroup cmp
 */
ENGINE_API void cmp_debug_remove(cmphandle_t hdl);

/*************************************************************************************************/
/* helpers */
/**
 * Set global alloc/tmp_alloc for modify functions @e cmp_modifyXXX
 * @ingroup cmp
 */
ENGINE_API void cmp_set_globalalloc(struct allocator* alloc, struct allocator* tmp_alloc);

/*************************************************************************************************/
/* modify different types of value for component instance */

/**
 * Modify string value inside a component instance. if value is registered with @e 'modify' callback,
 * it will be called @e after new value is set.
 * @param hdl Handle of the component instance
 * @param name Name of the component value which we want to modify
 * @param value New value to be set
 * @see cmp_value @see pfn_cmp_modify
 * @ingroup cmp
 */
ENGINE_API result_t cmp_value_sets(cmphandle_t hdl, const char* name, const char* value);

/**
 * Gets string value inside a component instance
 * @param rs Returned string, you must provide a valid uninitialized string buffer
 * @param rs_sz Maximum size of the string buffer
 * @param hdl Handle of the component instance
 * @param name Value name
 * @see cmp_value
 * @ingroup cmp
 */
ENGINE_API result_t cmp_value_gets(OUT char* rs, uint rs_sz, cmphandle_t hdl,
                                   const char* name);

/**
 * Modify float value inside a component instance. if value is registered with @e 'modify' callback,
 * it will be called @e after new value is set.
 * @param hdl Handle of the component instance
 * @param name Name of the component value which we want to modify
 * @param value New value to be set
 * @see cmp_value @see pfn_cmp_modify
 * @ingroup cmp
 */
ENGINE_API result_t cmp_value_setf(cmphandle_t hdl, const char* name, float value);

/**
 * Gets float value inside a component instance
 * @param rf Pointer to returning float value
 * @param hdl Handle of the component instance
 * @param name Value name
 * @see cmp_value
 * @ingroup cmp
 */
ENGINE_API result_t cmp_value_getf(OUT float* rf, cmphandle_t hdl, const char* name);

/**
 * Modify integer value inside a component instance. if value is registered with @e 'modify' callback,
 * it will be called @e after new value is set.
 * @param hdl Handle of the component instance
 * @param name Name of the component value which we want to modify
 * @param value New value to be set
 * @see cmp_value @see pfn_cmp_modify
 * @ingroup cmp
 */
ENGINE_API result_t cmp_value_seti(cmphandle_t hdl, const char* name, int value);

/**
 * Gets integer value inside a component instance
 * @param rn Pointer to returning integer value
 * @param hdl Handle of the component instance
 * @param name Value name
 * @see cmp_value
 * @ingroup cmp
 */
ENGINE_API result_t cmp_value_geti(OUT int* rn, cmphandle_t hdl, const char* name);

/**
 * Modify unsigned integer value inside a component instance. if value is registered with
 * 'modify' callback, it will be called @e after new value is set.
 * @param hdl Handle of the component instance
 * @param name Name of the component value which we want to modify
 * @param value New value to be set
 * @see cmp_value @see pfn_cmp_modify
 * @ingroup cmp
 */
ENGINE_API result_t cmp_value_setui(cmphandle_t hdl, const char* name, uint value);

/**
 * Gets unsigned integer value inside a component instance
 * @param rn Pointer to returning unsigned integer value
 * @param hdl Handle of the component instance
 * @param name Value name
 * @see cmp_value
 * @ingroup cmp
 */
ENGINE_API result_t cmp_value_getui(OUT uint* rn, cmphandle_t hdl, const char* name);

/**
 * Modify 4 component float value inside a component instance. if value is registered with
 * 'modify' callback, it will be called @e after new value is set.
 * @param hdl Handle of the component instance
 * @param name Name of the component value which we want to modify
 * @param value New value to be set
 * @see cmp_value @see pfn_cmp_modify
 * @ingroup cmp
 */
ENGINE_API result_t cmp_value_set4f(cmphandle_t hdl, const char* name, const float* value);

/**
 * Gets 4-component float value inside a component instance
 * @param rfv Float array to be filled with value data
 * @param hdl Handle of the component instance
 * @param name Value name
 * @see cmp_value
 * @ingroup cmp
 */
ENGINE_API result_t cmp_value_get4f(OUT float* rfv, cmphandle_t hdl, const char* name);

/**
 * Modify 3 component float value inside a component instance. if value is registered with
 * 'modify' callback, it will be called @e after new value is set.
 * @param hdl Handle of the component instance
 * @param name Name of the component value which we want to modify
 * @param value New value to be set
 * @see cmp_value @see pfn_cmp_modify
 * @ingroup cmp
 */
ENGINE_API result_t cmp_value_set3f(cmphandle_t hdl, const char* name, const float* value);

/**
 * Gets 3-component float value inside a component instance
 * @param rfv Float array to be filled with value data
 * @param hdl Handle of the component instance
 * @param name Value name
 * @see cmp_value
 * @ingroup cmp
 */
ENGINE_API result_t cmp_value_get3f(OUT float* rfv, cmphandle_t hdl, const char* name);

/**
 * Modify 2 component float value inside a component instance. if value is registered with
 * 'modify' callback, it will be called @e after new value is set.
 * @param hdl Handle of the component instance
 * @param name Name of the component value which we want to modify
 * @param value New value to be set
 * @see cmp_value @see pfn_cmp_modify
 * @ingroup cmp
 */
ENGINE_API result_t cmp_value_set2f(cmphandle_t hdl, const char* name, const float* value);

/**
 * Gets 2-component float value inside a component instance
 * @param rfv Float array to be filled with value data
 * @param hdl Handle of the component instance
 * @param name Value name
 * @see cmp_value
 * @ingroup cmp
 */
ENGINE_API result_t cmp_value_get2f(OUT float* rfv, cmphandle_t hdl, const char* name);

/**
 * Modify boolean value inside a component instance. if value is registered with
 * 'modify' callback, it will be called @e after new value is set.
 * @param hdl Handle of the component instance
 * @param name Name of the component value which we want to modify
 * @param value New value to be set
 * @see cmp_value @see pfn_cmp_modify
 * @ingroup cmp
 */
ENGINE_API result_t cmp_value_setb(cmphandle_t hdl, const char* name, bool_t value);

/**
 * Gets boolean value inside a component instance
 * @param rb Pointer to returning boolean value
 * @param hdl Handle of the component instance
 * @param name Value name
 * @see cmp_value
 * @ingroup cmp
 */
ENGINE_API result_t cmp_value_getb(OUT bool_t* rb, cmphandle_t hdl, const char* name);

/**
 * Modify matrix value inside a component instance. if value is registered with
 * 'modify' callback, it will be called @e after new value is set.
 * @param hdl Handle of the component instance
 * @param name Name of the component value which we want to modify
 * @param value New value to be set
 * @see cmp_value @see pfn_cmp_modify
 * @ingroup cmp
 */
ENGINE_API result_t cmp_value_set3m(cmphandle_t hdl, const char* name, const struct mat3f* value);

/**
 * Fetch matrix value from a component instance.
 * @param r Returned matrix
 * @param hdl Handle of the component instance
 * @param name Name of the component value which we want to modify
 * @see cmp_value @see pfn_cmp_modify
 * @ingroup cmp
 */
ENGINE_API result_t cmp_value_get3m(OUT struct mat3f* rm, cmphandle_t hdl, const char* name);

/**
 * Modify a single component of string array value inside a component instance.
 * if value is registered with 'modify' callback, it will be called @e after new value is set.
 * @param hdl Handle of the component instance
 * @param name Name of the component value which we want to modify
 * @param idx Index of the string inside string array
 * @param value New value to be set
 * @see cmp_value @see pfn_cmp_modify
 * @ingroup cmp
 */
ENGINE_API result_t cmp_value_setsvi(cmphandle_t hdl, const char* name, uint idx,
                                     const char* value);

/**
 * Gets a single component of string array value inside a component instance
 * @param rs String buffer to be filled with string value
 * @param hdl Handle of the component instance
 * @param name Value name
 * @param idx Index of the string inside string array
 * @see cmp_value
 * @ingroup cmp
 */
ENGINE_API result_t cmp_value_getsvi(OUT char* rs, uint rs_sz, cmphandle_t hdl, const char* name,
                                     uint idx);

/**
 * Modify the whole string array inside a component instance. if value is registered with
 * 'modify' callback, it will be called @e after new value is set.
 * @param hdl Handle of the component instance
 * @param name Name of the component value which we want to modify
 * @param cnt Number of elements in string array
 * @param values array of string values to be set
 * @see cmp_value @see pfn_cmp_modify
 * @ingroup cmp
 */
ENGINE_API result_t cmp_value_setsvp(cmphandle_t hdl, const char* name, uint cnt,
                                     const char** values);

/*************************************************************************************************/
/* internal */
void cmp_zero();
result_t cmp_initmgr();
void cmp_releasemgr();

result_t cmp_prepare_deferredinstances(struct allocator* alloc, struct allocator* tmp_alloc);

struct cmp_chain_node* cmp_create_chainnode();
void cmp_destroy_chainnode(struct cmp_chain_node* chnode);
void cmp_zeroobj(struct cmp_obj* obj);
const struct cmp_instance_desc** cmp_get_updateinstances(cmp_t c, OUT uint* cnt);
const struct cmp_instance_desc** cmp_get_allinstances(cmp_t c, OUT uint* cnt,
    struct allocator* alloc);

void cmp_debug(float dt, const struct gfx_view_params* params);
void cmp_update(float dt, uint stage_id);
void cmp_clear_updates();


#endif /* __CMPMGR_H__ */
