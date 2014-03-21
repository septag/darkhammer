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

#ifndef __INITPARAMS_H__
#define __INITPARAMS_H__

/**
 * MSAA (multi-sampling) flags, used in initializing renderer
 * @see gfx_params
 * @ingroup eng
 */
enum msaa_mode
{
    MSAA_NONE = 0,
    MSAA_2X = 2,
    MSAA_4X = 4,
    MSAA_8X = 8
};

/**
 * texture quality for renderer. by default it uses the highest texture resolution
 * @see gfx_params
 * @ingroup eng
 */
enum texture_quality
{
    TEXTURE_QUALITY_HIGH = 1, /**< uses 1 lower mip level of the texture for prefabs */
    TEXTURE_QUALITY_NORMAL = 2, /**< uses 2 lower mip level of the texture for prefabs */
    TEXTURE_QUALITY_LOW = 3, /**< uses 3 lower mip level of the texture for prefabs */
    TEXTURE_QUALITY_HIGHEST = 0 /**< (default) uses the highest texture mip level */
};

/**
 * texture filtering for renderer. by default it uses TRILINEAR filtering for models and other objects
 * @see gfx_params
 * @ingroup eng
 */
enum texture_filter
{
    TEXTURE_FILTER_TRILINEAR = 0, /**< (default) uses triliear filtering for scene assets */
    TEXTURE_FILTER_BILINEAR, /**< uses biliear filtering for scene assets */
    TEXTURE_FILTER_ANISO2X, /**< uses 2x anisotropic filtering for scene assets */
    TEXTURE_FILTER_ANISO4X, /**< uses 4x anisotropic filtering for scene assets */
    TEXTURE_FILTER_ANISO8X, /**< uses 8x anisotropic filtering for scene assets */
    TEXTURE_FILTER_ANISO16X /**< uses 16x anisotropic filtering for scene assets */
};

/**
 * shading quality for renderer. by default it uses the best quality for shaders
 * @see gfx_params
 * @ingroup eng
 */
enum shading_quality
{
    SHADING_QUALITY_LOW = 2, /**< loads lowest quality shaders (for low-end cards) */
    SHADING_QUALITY_NORMAL = 1, /**< loads normal quality shaders (for mid-range cards) */
    SHADING_QUALITY_HIGH = 0    /**< (default) loads highest quality shaders (mid-range to high-range cards) */
};

/**
 * Requested hardware version for renderer\n
 * If gfx_hwver is set to UNKNOWN, then the highest gpu capability will be chosen for renderer\n
 * Any other value forces the graphics hardware to downgrade it's capabilities
 * @see gfx_params
 * @ingroup eng
 */
enum gfx_hwver
{
    GFX_HWVER_UNKNOWN = 0,  /**< (default) uses the highest graphics capabilities for current gpu */
    GFX_HWVER_D3D11_0 = 3,  /**< uses D3D11 graphics capabilities for current gpu (must be _D3D_ build) */
    GFX_HWVER_D3D10_1 = 2,  /**< uses D3D10.1 graphics capabilities for current gpu (must be _D3D_ build) */
    GFX_HWVER_D3D10_0 = 1,  /**< uses D3D10 graphics capabilities for current gpu (must be _D3D_ build) */
    GFX_HWVER_D3D11_1 = 4, /**< uses D3D11.1 graphics capabilities for current gpu (must be _D3D_ build) */
    GFX_HWVER_GL4_4 = 12, /**< uses GL4.4 graphics capabilities for current gpu (must be _GL_ build) */
    GFX_HWVER_GL4_3 = 11, /**< uses GL4.3 graphics capabilities for current gpu (must be _GL_ build) */
    GFX_HWVER_GL4_2 = 10, /**< uses GL4.2 graphics capabilities for current gpu (must be _GL_ build) */
    GFX_HWVER_GL4_1 = 9, /**< uses GL4.0 graphics capabilities for current gpu (must be _GL_ build) */
    GFX_HWVER_GL4_0 = 8, /**< uses GL4.0 graphics capabilities for current gpu (must be _GL_ build) */
    GFX_HWVER_GL3_3 = 7, /**< uses GL3.3 graphics capabilities for current gpu (must be _GL_ build) */
    GFX_HWVER_GL3_2 = 6 /**< uses GL3.2 graphics capabilities for current gpu (must be _GL_ build) - not recommended */
};

/**
 * engine init flags, used in init_params::flag value
 * @ingroup eng
 */
enum gfx_flags
{
    GFX_FLAG_FULLSCREEN = (1<<0), /**< sets device to fullscreen view instead of windowed mode */
    GFX_FLAG_VSYNC = (1<<1), /**< enables vertical-sync for device */
    GFX_FLAG_DEBUG = (1<<2), /**< sets renderer device to debug mode, also shaders may be recompiled for debug-mode */
    GFX_FLAG_FXAA = (1<<3), /**< enables FXAA anti-aliasing */
    GFX_FLAG_REBUILDSHADERS = (1<<4) /**< rebuild shaders, clears shader cache and forces recompile on shaders */
};

/**
 * engine init flags, used in init_params::flag value
 * @ingroup eng
 */
enum eng_flags
{
    ENG_FLAG_DEBUG = (1<<0), /**< Enables engine debug facilities */
    ENG_FLAG_DEV = (1<<1), /**< Enables engine developer facilities, like dynamic resource manager and extra console commands */
    ENG_FLAG_EDITOR = (1<<2), /**< Enables engine editor facilities */
    ENG_FLAG_CONSOLE = (1<<3), /**< Enables engine built-in console, log data will also be sent to engine instead of custom logger */
    ENG_FLAG_DISABLEPHX = (1<<4), /**< disabe physics simulation */
    ENG_FLAG_OPTIMIZEMEMORY = (1<<5),   /**< use fixed (but fast) allocators for common buffers,
                                        If you want to set this option, make sure you have profiled
                                        memory usage for each buffer and set them in init_params */
    ENG_FLAG_DISABLEBGLOAD = (1<<6) /**< Disables background loading feature */
};

/**
 * init parameters for the developer, should be filled when initializing the engine or loaded by json file
 * @see init_params
 * @ingroup eng
 */
struct dev_params
{
    int fpsgraph_max; /**< maximum value for 'fps' (frames-per-second) graph */
    int ftgraph_max;  /**< maximum value for 'ft' (frame-time) graph, this should be a value */
    int webserver_port; /**< local port number that profiler webserver will be run */

    uint buffsize_data;   /**< Buffer size for level data (in KB), only applies in DEV mode.
                             * Set this parameter to 0 to allocate default buffer size */
    uint buffsize_tmp;    /**< Buffer size for temp frame data (in KB), only applies in DEV mode.
                             * Set this parameter to 0 to allocate default buffer size */
};

/**
 * physics init flags
 * @see phx_params
 * @ingroup eng
 */
enum phx_flags
{
    PHX_FLAG_TRACKMEM = (1<<0), /**< Track internal physics memory allocations */
    PHX_FLAG_PROFILE = (1<<1) /**< Enable internal physics profiler */
};

/**
 * init parameters for physics system, should be filled when initializing the engine
 * @see init_params
 * @ingroup eng
 */
struct phx_params
{
    uint flags;   /**< Combination of init flags, @see phx_flags */
    uint mem_sz; /**< Fixed custom allocator memory size (in kilobytes), if =0, default size will be set (32mb) */
    uint substeps_max; /**< Maximum sub-steps, if simulation lags behind the desired frame-rate */
    uint scratch_sz; /**< Temp buffer size during physics simulation (in kilobytes) */
};

/**
 * init parameters for script system, should be filled when initializing the engine
 * @see init_params
 * @ingroup eng
 */
struct sct_params
{
    uint mem_sz; /**< Fixed custom allocator memory size (in kilobytes), if =0, default size will be set (8mb) */
};

/**
 * init parameters for the renderer, should be filled when initializing the engine or loaded by json file
 * @see init_params
 * @ingroup eng
 */
struct gfx_params
{
    uint flags;  /**< combination of gfx_flags enum */
    enum msaa_mode msaa;   /**< multisample mode, @see msaa_mode */
    enum texture_quality tex_quality; /**< texture quality */
    enum texture_filter tex_filter;  /**< texture filtering */
    enum shading_quality shading_quality; /**< shading quality */
    enum gfx_hwver hwver; /**< graphics hardware version */
    uint adapter_id; /**< graphics adapter Id, if system has more than one gpu, must be set to adapter ID */
    uint width; /**< render buffer width in pixels */
    uint height; /**< render buffer height in pixels */
    uint refresh_rate; /**< monitor refresh for given resolution */
};

/**
 * init parameters for the engine, should be filled when initializing the engine or loaded by json file
 * @see eng_load_params
 * @see eng_init
 * @ingroup eng
 */
struct init_params
{
    uint flags;  /**< combination of eng_flags enum @see eng_flags */
    uint console_lines_max;   /**< maximum console lines */

    struct gfx_params gfx; /**< graphics parameters @see gfx_params*/
    struct dev_params dev; /**< dev paramters @see dev_params*/
    struct phx_params phx;  /**< physics paramters @see phx_params */
    struct sct_params sct; /**< script parameters @see sct_params */
    char* console_cmds; /**< array of initial console commands (each item is char[128]) */
    uint console_cmds_cnt;

    const char* data_path; /**< data directory or .pak file override, used in distros and tools */
};

#endif /* __INITPARAMS_H__ */
