01 - Hello World
================

In this tutorial, we create the most basic application by initializing window
and the engine itself. The tutorial also contains some basic input device
(mouse/keyboard) controls and a basic low-level camera.

Source files for this section are in `[tutorials_folder]/helloworld` directory.

![](<preview.jpg>)



Compiler Setup
--------------

-   **Include files** are in *path-to-darkhammer/include* directory

-   **Library files** are in *path-to-darkhammer/lib* directory

-   You should also add */bin* directory to your %PATH% environment variables,
    or if you don't prefer that, copy the engine's binary files from that folder
    to your executable directory.

-   Link to *dh_core (dh_core-dbg for debug)* library

-   Link to *dh_engine (dh_engine-dbg for debug)* library



Initialization
--------------

First thing we have to do is initialize engine's components.

The most essential library for every engine's app is *core* library, so we have
to initialize it before everything else. *core* library contains engine's basic
functionalities like *File manager, Memory manager, Timers, Parsers, Containers,
..*

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
core_init(TRUE);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

First parameter *trace_mem *indicates that we want to trace memory leaks.



We can set logging options and initialize the logger. There are different
outputs for logger:

-   *Console: *Outputs to application console (only in console apps)

-   *File: *Outputs to a text file

-   *Debugger: *Outputs to debugger's output (only in debug mode)

-   *Custom function: *Outputs to user definable callback function



Here we output to console and text file

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
log_outputconsole(TRUE);
log_outputfile(TRUE, logfile);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~



For Engine and App initialization we have to create and fill `init_params`*
*structure. There are many options available in this structure that is beyond
the scope of this article, I suggest you read the reference documentation for
initialization parameters.



~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
init_params* params = app_config_default();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Creates default parameters, After this you can add your own custom parameters
and pass it to app and engine for initialization. You can also use json files
for loading config by using `app_config_load` function.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
params->flags |= ENG_FLAG_CONSOLE | ENG_FLAG_DEV;
params->data_dir = eng_datadir;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Here we set *Dev *and *Console* flags for engine, which indicates that we want
to use development mode and need an in-game command console.

Development config *(ENG_FLAG_DEV)* is pretty important for every non-release
initialization, because some important internal engine optimizations will be
off. For example in Dev mode, resource manager uses dynamic allocator for scene
data, but in non-dev mode, resource manager uses a very fast stack allocator for
scene data, which also doesn't have the capability of unloading single resources
during runtime.

*data_dir* is also important to override, because we are using a different path
for engine's essential data files, and we have to override it.

You can also set renderer width/height by setting *params->gfx.width* and
*params->gfx.height* parameters.



Now everything is set, we have to initialize the application. Application
initialization should come before engine init, because graphics device (GL/D3D)
and main rendering window will be created by *app*.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
app_init(APP_NAME, params)
eng_init(params)
app_config_unload(params)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

After successful initialization of the engine, we can free *init_params* and
move on to setting up a basic scene.



Initializing the scene
----------------------

Our scene in this tutorials is actually empty with a simple text and a debug
grid on XZ plane.

We need a low-level camera (because we don't wan't to create higher level camera
object, that will be the context of another tutorial) for basic movement and an
empty scene.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
cam_init(&cam, &cam_pos, &cam_target, CAM_NEAR, CAM_FAR, math_torad(CAM_FOV));
scene_id = scn_create_scene("main");

scn_setactive(scene_id);
scn_setcam(&cam);

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

After creating the scene and initializing the camera, We have to activate it for
scene manager and set the camera (*scn_setactive* and *scn_setcam)*.



Application callbacks
---------------------

To receive events from window system and update the frame, we have to define
callback functions and set them by *app_set_XXXXfunc* functions.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
app_window_setupdatefn(update_callback);
app_window_setkeypressfn(keypress_callback);
app_window_setactivefn(activate_callback);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

-   *Update* callback is called on every frame in event loop. In update loop we
    have to update input system, camera and also engine itself so it can render
    the frame.

-   *Keypress* callback is called when user presses a key, it is used to capture
    user keystrokes and send them for GUI.

-   *Activate* callback is called when application gains focus or unfocused. In
    this callback we pause/resume the engine simulation (rendering will
    continue).



Camera Update
-------------

Because we are using low-level camera for this tutorial, we have to update it on
every frame (*update*). We start by fetching mouse and keyboard movement, so
WASD keys corresponds to camera movement and mouse left-key + movement operates
on camera rotation.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
input_get_mouse(&mx, &my, &mousekeys);
input_get_kbhit(INPUT_KEY_XXXX);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

*input_get_mouse* retrieves mouse state (x, y, key combination) and
*input_get_kbhit* tests for keystroke.

To rotate the camera we call `cam_yaw` and `cam_pitch` to rotate around Y and X
axis respectively.

To move the camera around the scene, we test for key strokes and use `cam_fwd
`and `cam_strafe `functions to move forward/backward (negative values), and to
the sides.

After all of this you have to call `cam_update(&cam)` to reflect the changes in
the camera.



Debug Drawing
-------------

We use debug drawing to draw a simple snapped grid on the XZ ground plane with a
axis vectors on the origin.

The first thing we have to do is to override the debugging callback function

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
gfx_set_debug_renderfunc(debug_view_callback);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This sets callback function for debug drawing, and the engine calls it on every
frame. Inside this function you can use any 2D/3D canvas drawing calls:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
gfx_canvas_setlinecolor(&g_color_white);
gfx_canvas_grid(5.0f, 70.0f, params->cam);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

These two lines draws a white grid on XZ plane, with 5 units spacing between
cells and 70 units view range.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
gfx_canvas_coords(&ident, &params->cam_pos, 1.0f);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This line draws 3 axises of our coordinate space (identity matrix).

**Note **that the incoming *params* in *debug_view_callback* includes basic
information about the current active camera (and it's position) and other
essential matrices for rendering (like view/projection matrix).

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
gfx_canvas_text2dpt("Hello World", 10, 10, 0);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This function draws a "hello world" text on (x = 10, y = 10) position. 2D
coordinates are in pixels and the point (0, 0) is on the top-left corner of the
screen



Update loop
-----------

We set the update callback earlier, so in order to initiate the event loop and
run the engine, we have to run the following function:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
app_window_run();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

*app_window_run* runs the even loop of the window and calls our own update callback
on every frame. In update callback (*update_callback* function), first we update
the input system, and camera, then call the engine's *eng_update* function to
process and render the frame.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void update_callback()
{
    input_update();
    update_camera();
    eng_update();
}
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Now the engine will run until you close the window.



Cleaning up
-----------

In order to cleanup everything, just call the release functions in the reverse
order that we initialized the engine's components:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
/* cleanup */
release_scene();
eng_release();
app_release();
core_release(TRUE);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The first parameter of `core_release` function indicates that we want to see the
*memory leak report*.



For more details check out the source files for this tutorial and read reference
docs for each function used.


