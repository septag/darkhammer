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

#ifndef __CMPTYPES_H__
#define __CMPTYPES_H__

/**
 * @defgroup cmp Component Manager
 * Component manager is the low-level core functionality of engine's component system\n
 * Each object in the game consists of components created and managed by component system
 */

#include "dhcore/types.h"
#include "dhcore/linked-list.h"
#include "dhcore/vec-math.h"
#include "engine-api.h"

/* fwd declaration */
struct cmp_value;
struct gfx_view_params;

/**
 * Component Handle (64bit integer), each handle contains these values :  \n
 * 1. type of component: (16 bits - 63~68) \n
 * 2. index to component (in cmp-mgr::cmps) (16bits - 47~32) \n
 * 3. index to component data (in cmp-mgr::indexes) (32bits - 31~0) \n
 * Use macros: @b CMP_GET_TYPE(cmp_hdl), @b CMP_GET_INDEX(cmp_hdl), @b CMP_GET_INSTANCEINDEX(cmp_hdl)
 * to fetch these values
 * @ingroup cmp
 */
typedef uint64 cmphandle_t;

/**
 * Component type (16bit integer), Identifies a unique type for each component
 * @ingroup cmp
 */
typedef uint16 cmptype_t;

 /**
  * Component chain linked_list (owned by objects). data is cmp_chain_node
  * @ingroup cmp
  */
typedef struct linked_list* cmp_chain;

struct cmp_component;

/**
 * Component itself. opaque type
 * @ingroup cmp
 */
typedef struct cmp_component* cmp_t;

/* update stages
 * update stages are applied sequentially
 * stage #4 comes right before render and have cmdqueue as 'param'
 * stage #5 comes after render
 */
#define CMP_UPDATE_STAGE1   0
#define CMP_UPDATE_STAGE2   1
#define CMP_UPDATE_STAGE3   2
#define CMP_UPDATE_STAGE4   3
#define CMP_UPDATE_STAGE5   4
#define CMP_UPDATE_MAXSTAGE 5

/* component handle macros */
#define CMP_GET_TYPE(hdl)       (((hdl)>>48)&0xffff)
#define CMP_GET_INDEX(hdl)      (((hdl)>>32)&0xffff)
#define CMP_GET_INSTANCEINDEX(hdl)  ((hdl)&0xffffffff)
#define CMP_MAKE_HANDLE(type, c_idx, i_idx) ( (((uint64)((type)&0xffff))<<48) | \
    (((uint64)((c_idx)&0xffff))<<32) | ((uint64)((i_idx)&0xffffffff)) )

/* object flags */
enum cmp_objflag
{
    CMP_OBJFLAG_VISIBLE = (1<<0), /* temp flag: object is visible in current frame */
    CMP_OBJFLAG_STATIC = (1<<3),    /* static flag: it means that it cannot be normally deleted */
    CMP_OBJFLAG_SPATIALVISIBLE = (1<<4), /* temp flag: for culling to dismiss duplicate vis obj */
    CMP_OBJFLAG_SPATIALADD = (1<<5)    /* temp flag: for scene-mgr to dismiss duplicate add */
};

struct cmp_chain_node
{
    cmphandle_t hdl;
    struct linked_list node;
};

/**
 * Object types, there are couple of main object types in the engine, which defines there core
 * functionality
 * @ingroup cmp
 */
enum cmp_obj_type
{
	CMP_OBJTYPE_UNKNOWN = 0,    /**< Unknown object type (custom?) */
	CMP_OBJTYPE_MODEL = (1<<0), /**< Model objects, must have "model" component */
	CMP_OBJTYPE_PARTICLE = (1<<1), /**< Particle objects, must have "particle" component */
	CMP_OBJTYPE_LIGHT = (1<<2), /**< Light objects, must have "light" component */
	CMP_OBJTYPE_DECAL = (1<<3), /**< Decal objects, must have "decal" component */
	CMP_OBJTYPE_CAMERA = (1<<4), /**< Camera object, must have "Camera" component */
    CMP_OBJTYPE_TRIGGER = (1<<5), /**< Trigger object, must have "trigger" component */
    CMP_OBJTYPE_ENV = (1<<6) /**< Environmental objects like terrain, sky, ocean, etc.. */
};

/**
 * Main game object\n
 * Game objects are main entities which are created and added to the scene\n
 * They consist of collection of components which defines their behavior in the engine
 * @ingroup cmp
 */
struct cmp_obj
{
    char name[32];  /**< Name of the engine, it is recommended to make unique names for each scene */
    uint id; /**< Object ID in the scene */
    uint scene_id;	/**< Object's owner scene ID (=0 if object does not belong to any scene) */
    enum cmp_obj_type type; /**< Object type, @see cmp_obj_type */
    uint flags;   /**< Object flags (cmp_objflag) */
    uint tmp_flags; /**< Perframe Temp flags (cmp_objflag) */

    cmp_chain chain;  /**< Component chain (item=cmp_chain_node) */

    /* commonly used component handles */
    cmphandle_t xform_cmp;  /**< Transform component handle (for fast/easy access) */
    cmphandle_t bounds_cmp; /**< Bounds component handle (for fast/easy access) */
    cmphandle_t model_cmp; /**< Model component handle (for fast/easy access) */
    cmphandle_t animchar_cmp; /**< Animation component handle (for fast/easy access) */
    cmphandle_t rbody_cmp; /**< Rigid-body physics component handle (for fast/easy access) */
    cmphandle_t model_shadow_cmp; /**< Shadow model component handle */
    cmphandle_t trigger_cmp; /**< Trigger component handle (for fast/easy access) */
    cmphandle_t attachdock_cmp; /**< Attachdock component handle (for fast/easy access) */
    cmphandle_t attach_cmp; /**< Attachment component handle (for fast/easy access) */
};


/**
 * @b Create callback: this callback is registered on component creation and is called right after
 * each instance is created. @see cmp_create_instance
 * @param obj Owner object
 * @param data Component data, should be casted to whatever data-type is used for component instance
 * @param cur_hdl Current instance handle
 * @return Returns RET_FAIL if an error occured on instance creation, otherwise returns RET_OK
 * @ingroup cmp
 */
typedef result_t (*pfn_cmp_create)(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl);

/**
 * @b Destroy callback: this callback is registered on component creation and is called right before
 * each instance is destroyed. @see cmp_destroy_instance
 * @param obj Owner object
 * @param data Component data, should be casted to whatever data-type is used for component instance
 * @param cur_hdl Current instance handle
 * @ingroup cmp
 */
typedef void (*pfn_cmp_destroy)(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl);

/**
 * @b Modify callback: this callback is defined in value descriptor of component and is called
 * when the specified value is modified using cmp_modifyXXXX functions or in editor/serialization.
 * @param obj Owner object
 * @param alloc Permanent allocator to store modified data
 * @param tmp_alloc Temp allocator for internal modify usage
 * @param data Component data, should be casted to whatever data-type is used for component instance
 * @param cur_hdl Current instance handle
 * @return Returns RET_FAIL if an error occured on instance creation, otherwise returns RET_OK
 * @ingroup cmp
 */
typedef result_t (*pfn_cmp_modify)(struct cmp_obj* obj, struct allocator* alloc,
struct allocator* tmp_alloc, void* data, cmphandle_t cur_hdl);

/**
 * @b Debug callback: this callback is registered on component creation and is called when instance is
 * added to debug list. The callback is called between canvas3d begin/end procedures, so
 * implementations should only use @e gfx_canvas_XXXX functions for visual debugging.
 * @param obj Owner object
 * @param data Component data, should be casted to whatever data-type is used for component instance
 * @param cur_hdl Current instance handle
 * @param dt Delta-time from the previous frame, for possible animation/timer debugging
 * @param gfx_view_params Current view parameters. @see gfx_view_params
 * @ingroup cmp
 */
typedef void (*pfn_cmp_debug)(struct cmp_obj* obj, void* data, cmphandle_t cur_hdl, float dt,
    const struct gfx_view_params* params);

/**
 * @b Update callback: Update callbacks are reigstered on component creation, each component can have
 * multiple update callbacks (one for each "update stage"), These callbacks are called with batch
 * processing in mind, so the callback does not have single instance handle. Implmentation should
 * fetch instances that needs to be updated. @see cmp_get_updateinstances
 * @param c Component itself
 * @param dt Time progress from the last frame.
 * @param params custom parameters, but always gfx_cmdqueue on update stage 4 (CMP_UPDATE_STAGE4)
 * @ingroup cmp
 */
typedef void (*pfn_cmp_update)(cmp_t c, float dt, void* params);

/**
 * Component value type
 * @ingroup cmp
 */
enum cmp_valuetype
{
    CMP_VALUE_UNKNOWN = 0,
    CMP_VALUE_INT,
    CMP_VALUE_UINT,
    CMP_VALUE_INT2,
    CMP_VALUE_BOOL,
    CMP_VALUE_FLOAT,
    CMP_VALUE_FLOAT2,
    CMP_VALUE_FLOAT3,
    CMP_VALUE_FLOAT4,
    CMP_VALUE_MATRIX,
    CMP_VALUE_STRING,
    CMP_VALUE_STRINGARRAY
};

/**
 * Component value descriptor, implementions of each component should contain some kind of data
 * structure to hold instance data.\n
 * The data structure's interface is defined by a collection of cmp_value(s) which then can be
 * accessed by serialization/script system.\n
 * The example below demostrates a simple component that have a single @e 'health' value as it's
 * interface : \n
 * Each instance of this component will be created with a @e cmp_health structure, which only
 * @e health member will be accessible by external sources. @e health must be defined in more
 * detail by filling @e cmp_value structure for it, and pass the array of them to
 * @e cmp_register_component function
 * @code
 * // our component data
 * struct cmp_health {
 *   float health;   // we only interface this value
 *   int some_internaldata[32];
 * };
 * static const struct cmp_value cmp_health_values[] = {
 *   {"health", CMP_VALUE_FLOAT, offsetof(struct cmp_health, health), sizeof(float),
 *   1, cmp_health_modify, ""}
 * };
 * @endcode
 * Now for example in Lua the script, @e health will be accessed by :
 * @code
 * obj["health_component"]["health"] = 0.2
 * @endcode
 * @see cmp_register_component
 * @see cmp_createparams
 * @ingroup cmp
 */
struct cmp_value
{
    const char* name; /**< Value name that can be accessed with script/serialization */
    enum cmp_valuetype type; /**< Value type. @see cmp_valuetype */
    uint offset; 	/**< Offset of the value in the structure , in bytes (can use offsetof macro) */
    uint stride;  /**< Size in bytes of each array element or size of one element if not array */
    uint elem_cnt;    /**< Element count in array, =1 if it's not an array */
    pfn_cmp_modify modify_func; /**< Modify callback func, @see pfn_cmp_modify */
    const char* annot; /**< Annotations to connect UI controls and special data behavior */
};

/**
 * Macro that returns value count for an array of descriptors \n
 * **Note** that @e values parameter should be a static variable
 * @ingroup cmp
 */
#define CMP_VALUE_CNT(values)   sizeof((values))/sizeof(struct cmp_value)

/**
 * Component instance creation flags
 * @see cmp_create_instance
 * @ingroup cmp
 */
enum cmp_instanceflag
{
    CMP_INSTANCEFLAG_INDIRECTHOST = (1<<0) /**< Component has no object host, thus user should provide
                                             * phdl parameter in @e cmp_create_instance */
};

/* Component creation flags
 * @see cmp_register_component
 * @ingroup cmp
 */
enum cmp_flag
{
    CMP_FLAG_ALWAYSUPDATE = (1<<0),  /**< Updates all instances every frame unless un-triggered manually */
    CMP_FLAG_SINGLETON = (1<<1),    /**< Only one instance could be created from this component  */
    CMP_FLAG_DEFERREDMODIFY = (1<<2) /**< Modify functions are called in deferred mode.
                                       * This is useful for components that their init/modify needs
                                       * fully loaded scene or are dependent on other objects */
};

/**
 * Instance descriptor, this structure is returned with some functions like cmp_get_updateinstances
 * @ingroup cmp
 */
struct cmp_instance_desc
{
    struct cmp_obj* host; /**< Associated object host */
    //cmphandle_t* phdl; /**< pointer to handle, use *phdl to fetch actual handle */
    uint8* data;    /**< Associated pointer to instance data, must be casted to proper data type */
    uint updatelist_idx;  /**< Index in update list (=INVALID_INDEX if not in update list) */
    uint flags;   /**< Instance flags, @see cmp_instanceflag */
    cmphandle_t hdl; /**< Handle to the component instance */
    cmphandle_t parent_hdl; /* possible parent component (for indirect components) */
    cmp_chain childs;  /* possible child components (children are indirect components) */
    uint offset_in_parent;    /* offset in the parent component data (for indirect components) */
};

/**
 * Component creation/registeration parameters
 * @see cmp_register_component
 * @ingroup cmp
 */
struct cmp_createparams
{
    const char* name;   /**< Component name */
    cmptype_t type; /**< Component type, should be a unique 16bit number */
    uint flags;   /**< Component creations flags. @see cmp_flag */
    pfn_cmp_create create_func; /**< Create callback function. @see pfn_cmp_create */
    pfn_cmp_destroy destroy_func; /**< Destroy callback function. @see pfn_cmp_destroy */
    pfn_cmp_update update_funcs[CMP_UPDATE_MAXSTAGE];  /**< Update callbacks.
                                                        * For each callback stage, we can have one
                                                        * callback (or NULL). @see pfn_cmp_update */
    pfn_cmp_debug debug_func; /**< Debug callback. @see pfn_cmp_debug */
    uint stride; /**< Strides between each component instance data, sizeof(struct mycmp_data) */
    uint initial_cnt; /**< Initial instance count */
    uint grow_cnt; /**< Growth instance count, if initial count limit is reached */
    uint value_cnt;  /**< Number of (public) values inside component data. @see cmp_value */
    const struct cmp_value* values; /**< Actual value descriptors for component data. @see cmp_value */
};


#endif /* __CMPTYPES_H__ */
