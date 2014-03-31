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
 * @defgroup gfx-canvas Canvas
 * Canvas is mostly used for debug and engine HUD rendering\n
 * There are two kinds of canvas function, 2D and 3D:\n
 * - 2D functions, No matter where you call them, are executed and batched at the end of the render pipeline
 * - 3D functions are executed immediately, but should come between *gfx_canvas_begin3d* and *gfx_canvas_end3d* functions
 */

#ifndef __GFXCANVAS_H__
#define __GFXCANVAS_H__

#include "engine-api.h"
#include "dhcore/types.h"
#include "dhcore/color.h"
#include "dhcore/prims.h"
#include "gfx-font.h"
#include "gfx-types.h"

/* fwd */
struct camera;
struct gfx_model_geo;

/* enums/flags */

/**
 * Text drawing flags
 * @see gfx_canvas_text2dpt
 * @see gfx_canvas_text2drc
 * @ingroup gfx-canvas
 */
enum gfx_text_flags
{
    GFX_TEXT_RIGHTALIGN = (1<<0),   /**< Right align text (*gfx_canvas_text2drc* only) */
    GFX_TEXT_CENTERALIGN = (1<<1), /**< Center align text (*gfx_canvas_text2drc* only) */
    GFX_TEXT_VERTICALALIGN = (1<<2), /**< Centers text vertically (*gfx_canvas_text2drc* only )*/
    GFX_TEXT_RTL = (1<<3), /**< Draws right-to-left text instead of default left-to-right */
    GFX_TEXT_UNICODE = (1<<4) /**< Tells the function that input text is unicode instead of multi-byte */
};

/**
 * Bitmap drawing flags
 * @ingroup gfx-canvas
 */
enum gfx_bmp2d_flags
{
    GFX_BMP2D_NOFIT = (1<<0), /**< Doesn't fit texture to provided *'rc'* parameter */
    GFX_BMP2D_FLIPY = (1<<1) /**< Flips bitmap vertically */
};

/**
 * Rectangle drawing flags
 * @see gfx_canvas_rect2d
 * @ingroup gfx-canvas
 */
enum gfx_rect2d_flags
{
    GFX_RECT2D_HOLLOW = (1<<0) /**< Draws a hallow rectangle (border only) */
};

/**
 * Gradient types
 * @ingroup gfx-canvas
 */
enum gfx_gradient_type
{
    GFX_GRAD_NULL = 0,
    GFX_GRAD_LTR,   /**< left(clr0) to right(clr1) */
    GFX_GRAD_RTL,   /**< right to left */
    GFX_GRAD_TTB,   /**< top to bottom */
    GFX_GRAD_BTT    /**< bottom to top */
};

/**
 * Used for drawing different levels of detail for debug sphere
 * @ingroup gfx-canvas
 */
enum gfx_sphere_detail
{
	GFX_SPHERE_HIGH = 0,    /**< High detail */
	GFX_SPHERE_MEDIUM = 1, /**< Medium detail */
	GFX_SPHERE_LOW = 2 /**< Low detail */
};

void gfx_canvas_zero();
result_t gfx_canvas_init();
void gfx_canvas_release();
void gfx_canvas_render2d(gfx_cmdqueue cmdqueue, gfx_rendertarget rt, float rt_width, float rt_height);

/**
 * Sets active font handle for the canvas
 * @param hdl Handle of the font, or INVALID_HANDLE to set default font
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_setfont(fonthandle_t hdl);

/**
 * Sets fill color, fill color is used for solid debug objects
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_setfillcolor_solid(const struct color* c);

/**
 * Sets fill gradient color
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_setfillcolor_gradient(const struct color* clr0, const struct color* clr1,
    enum gfx_gradient_type grad);

/**
 * Sets line color, line color is used for wireframe debug objects
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_setlinecolor(const struct color* c);

/**
 * Sets text color
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_settextcolor(const struct color* c);

/**
 * Sets wireframe and culling drawing mode
 * @param enable Enable/disable wireframe drawing
 * @param cull Enable/Disable cull backfaces
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_setwireframe(bool_t enable, bool_t cull);

/**
 * Enable/Disable Z-test
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_setztest(bool_t enable);

/**
 * Sets clipping rectangle for the drawing functions that follows
 * @param enable Enables/Disables clipping
 * @param x Start X screen position of the clipping rect, in pixels
 * @param y Start Y screen position of the clippping rect, in pixels
 * @param w Width of the clipping rect, in pixels
 * @param h Height of the clipping rect, in pixels
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_setclip2d(bool_t enable, int x, int y, int w, int h);

/**
 * Sets alpha (transparency) for the following draw objects
 * @param alpha Alpha value which is in [0, 1], default=1
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_setalpha(float alpha);

/**
 * Returns current canvas' font line height
 * @ingroup gfx-canvas
 */
ENGINE_API uint16 gfx_canvas_getfontheight();

/* 2d drawing */

/**
 * Draws text on the specified point in the screen (2D)
 * @param text Null-terminated text, text can be either multi-byte or unicode (defined by flags)
 * @param x X screen position, in pixels
 * @param y Y screen position, in pixels
 * @param flags Combination of gfx_text_flags
 * @see gfx_text_flags
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_text2dpt(const void* text, int x, int y, uint flags);

/**
 * Draws a text by specified rectangle in the screen (2D)
 * @param text Null-terminated text, text can be eigher multi-byte or unicode (defined by flags)
 * @param rc Screen rectangle of the text, in pixels
 * @param flags Combination of gfx_text_flags
 * @see gfx_text_flags
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_text2drc(const void* text, const struct rect2di* rc, uint flags);

/**
 * Draws a rectangle on the screen (2D)
 * @param rc Screen rectangle, in pixels
 * @param line_width width of the rectangle border, set zero for no border
 * @param flags Combination of gfx_rect2d_flags
 * @see gfx_rect2d_flags
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_rect2d(const struct rect2di* rc, int line_width, uint flags);

/**
 * Draws a bitmap on the screen (2D)
 * @param tex Texture to draw as a 2D bitmap
 * @param width Width of the texture, set zero to draw with texture's default width
 * @param height Height of the texture, set zero to draw with texture's default height
 * @param rc Screen rectangle that you wish texture to fit into
 * @param flags Combination of gfx_bmp2d_flags
 * @see gfx_bmp2d_flags
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_bmp2d(gfx_texture tex, uint width, uint height,
		const struct rect2di* rc, uint flags);

/**
 * Draws a line on the screen (2D)
 * @param x0 X of the line's start position
 * @param y0 Y of the line's start position
 * @param x1 X of the line's end position
 * @param y1 Y of the line's end position
 * @param line_width Line width, in pixels
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_line2d(int x0, int y0, int x1, int y1, int line_width);

/* 3d drawing */
/**
 * Draws a solid box (3D)
 * @param b AABB box, without transformation
 * @param world Transformation matrix
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_box(const struct aabb* b, const struct mat3f* world);

/**
 * Draws a bounding AABB box (3D)
 * @param b AABB box, you must also transform it before drawing
 * @param viewproj Camera's View(x)Projection matrix, used for drawing text info
 * @param show_info Draws box info in text
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_boundaabb(const struct aabb* b, const struct mat4f* viewproj,
                                     bool_t show_info);
/**
 * Draws a solid sphere (3D)
 * @param s Sphere, without transformation
 * @param world Transformation matrix
 * @param detail Sphere detail
 * @see gfx_sphere_detail
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_sphere(const struct sphere* s, const struct mat3f* world,
		enum gfx_sphere_detail detail);
ENGINE_API void gfx_canvas_prism(float width, float height, const struct mat3f* world);
ENGINE_API void gfx_canvas_prism_2pts(const struct vec3f* p0, const struct vec3f* p1, float base_width);

/**
 * Draws a solid capsule (3D)
 * @param radius Radius of the capsule
 * @param half_height Height/2 of the cylinder part of the capsule
 * @param world Transformation matrix
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_capsule(float radius, float half_height, const struct mat3f* world);

/**
 * Draws a bounding sphere (3D)
 * @param s Bounding sphere, must be transformed before drawing
 * @param viewproj Camera's View(x)Projection matrix, used for drawing text info
 * @param view Camera's view matrix
 * @param show_info Draws bounding sphere info in text
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_boundsphere(const struct sphere* s, const struct mat4f* viewproj,
                                       const struct mat3f* view, bool_t show_info);

/**
 * Draws a rectangular bitmap in space, very much like a single billboard (3D)
 * @param tex Texture that holds the bitmap
 * @param pos Position of the bitmap in world-space
 * @param width Width of the texture, set zero for default texture width
 * @param height Height of the texture, set zero for default texture height
 * @param viewproj Camera's view(x)projection matrix
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_bmp3d(gfx_texture tex, const struct vec4f* pos,
                                 int width, int height, const struct mat4f* viewproj);

ENGINE_API void gfx_canvas_plane(const struct plane* plane);

ENGINE_API void gfx_canvas_frustum(const struct vec4f frustum_pts[8]);

/**
 * Draws a text in space (3D)
 * @param text Null-terminated multi-byte text
 * @param pos Position of the text in world-space
 * @param viewproj Camera's view(x)projection matrix
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_text3d(const char* text, const struct vec4f* pos,
                                  const struct mat4f* viewproj);

/**
 * Draws multi-line text, separated by @e '\n', in space (3D)
 * @param text Null-terminated multi-byte text
 * @param pos Position of the text in world-space
 * @param viewproj Camera's view(x)projection matrix
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_text3dmultiline(const char* text, const struct vec4f* pos,
                                         const struct mat4f* viewproj);

/**
 * Draws coordinate axises (3D)
 * @param xform Transformation matrix to retreive our axises, Identity matrix is the default coordinate matrix
 * @param campos Camera position in world-space
 * @param scale Draw scaling of the coords
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_coords(const struct mat3f* xform, const struct vec4f* campos, float scale);

/**
 * Draws an arrow (3D)
 * @param p0 Start position of the arrow's line in world-space
 * @param p1 End position of the arrow's line in world-space
 * @param plane_n Sets the plane normal, which the arrow head is prependicular to.
 * @param width Width of the arrow head
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_arrow3d(const struct vec4f* p0, const struct vec4f* p1,
                                   const struct vec4f* plane_n, float width);

/**
 * Draws an arrow (2D)
 * @param p0 Start position of the arrow's line in screen space
 * @param p1 End position of the arrow's line in screen space
 * @param twoway Defines if we want the arrow to have two heads
 * @param line_width Width of the arrow line
 * @param width Width of the arrow head
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_arrow2d(const struct vec2i* p0, const struct vec2i* p1, bool_t twoway,
                                   uint line_width, float width);
ENGINE_API void gfx_canvas_geo(const struct gfx_model_geo* geo, const struct mat3f* world);
ENGINE_API void gfx_canvas_georaw(gfx_inputlayout il, const struct mat3f* world, const struct color* clr,
    uint tri_cnt, enum gfx_index_type ib_type);
ENGINE_API void gfx_canvas_cam(const struct camera* cam, const struct vec4f* activecam_pos,
		const struct mat4f* viewproj, bool_t show_info);
ENGINE_API void gfx_canvas_worldbounds(const struct vec3f* minpt, const struct vec3f* maxpt,
    float height);

/**
 * Draws a world-aligned 3D grid on X-Z plane, which is usefull for editors and 3D space debug view
 * @param spacing Spacing between grid lines
 * @param depth_max how much grid depth will be shown to user, higher depth value will draw grid 
 * more into the horizon
 * @param cam current viewing camera
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_grid(float spacing, float depth_max, const struct camera* cam);
ENGINE_API void gfx_canvas_line3d(const struct vec4f* p0, const struct vec4f* p1);
ENGINE_API void gfx_canvas_light_pt(const struct vec4f* pos, float atten[2]);
ENGINE_API void gfx_canvas_light_spot(const struct mat3f* xform, float atten[4]);

/**
 * Begins 3D drawing. But in some parts of engine like component debugging callbacks and gfx debug 
 * callback,
 * It is already called, so you won't need to call this in those functions
 * @see cmp_debug_add
 * @see gfx_set_debug_renderfunc
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_begin3d(gfx_cmdqueue cmdqueue, float rt_width, float rt_height,
    const struct mat4f* viewproj);

/**
 * Ends 3D drawing. But in some parts of engine like component debugging callbacks and gfx debug callback,
 * It is already called, so you won't need to call this in those functions
 * @see cmp_debug_add
 * @see gfx_set_debug_renderfunc
 * @ingroup gfx-canvas
 */
ENGINE_API void gfx_canvas_end3d();

#endif /* GFXCANVAS_H */
