02 - Models
===========

In this tutorial, you will learn how to setup simple scenes and populate them
with models. Also I will introduce a new and higher level camera model, that
eases camera handling a bit more.

Source files for this section are in `[tutorials_folder]/tut02 - models`
directory.

For basic stuff like setting up the project and initializing application and
engine read [hello world][1] tutorial.

[1]: <../tut01-helloworld/helloworld.md>

![](<preview.jpg>)

To move around in this tutorial, use *WASD* or *arrow keys*

Bring up console with *"~"* key. Type help or help command in the console to
inspect some internals of the engine.

Use *"1"* and *"2"* keys to change sun light direction.



FPS Camera
----------

I will introduce a new camera which as the name suggests, is an FPS camera. It's
still pretty low-level and isn't presented as game object that can be attached,
animated and have physical props. But it's going to be good enough for most of
our demos.

First you have to define it's object and initialize it:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
camera_fps cam;
cam_fps_init(&cam, &cam_pos, &cam_target, CAM_NEAR, CAM_FAR, math_torad(CAM_FOV);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Just like initializing normal camera. But here we have more parameters to
customize, fps camera controls user input (keyboard/mouse) internally by itself,
and have smoothing and speed capabilities. All of these parameters can be
customized via `cam_fps_XXXX` functions.

For example, by default, the camera works with both *WASD* and *Arrow keys* for
movement (forward/backward/strafe). To change that you can use
`cam_fps_set_keys_fwd, cam_fps_set_keys_backwd, cam_fps_set_keys_strafeleft,
cam_fps_set_keys_straferight` functions to define new keys for those. Each
movement direction can receive two keys.

And for example, to change smoothness values, use `cam_fps_set_smoothing_values`
to change smoothing values for mouse and movement. which normally is between
*50-100.*

For more info on fps camera functions check out the reference docs.



Data Directory
--------------

In this tutorial, I will load a simple 200x200 unit plane and throw some barrel
objects in it. The directory for our model data is under `[tutorials]/data`
directory. So before anything else we have to define it as project data root for
the engine:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
char datadir[DH_PATH_MAX];
util_getexedir(datadir)
path_join(data, path_goup(datadir, datadir), "data", NULL);

io_addvdir(datadir, FALSE);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

First we construct our directory, I used couple of engine's utility functions to
resolve that path without any absolute paths, I get exe directory which is
supposed to be under `[tutorials]/bin`, with `util_getexedir`, then I use
`path_join` and `path_goup` to go up one level and concat it to data. (Notice
the `NULL` parameter at the end of each `path_join` call is required)

After the path is resolved, I used `io_addvdir` to add the directory to engine's
file system search directories. From this point on, every resource file is
referenced relative to our *data directory*.



Loading models
--------------

Main structure for scene objects are `cmp_obj`.

Objects in the engine are nothing but a collection of *components*. To add or
remove capabilities to objects, you should add different *component* types to
it.

Each object may have only one *component *type attached. For example you can't
have an object with two "model" components attached to it.

Each *component *have multiple values that can be modified, they are referenced
by their names. Modifications are applied immediately. For example if you modify
component's *"filepath"* value, it will automatically load whatever file you
have assigned to the value for the component.

For more information about the component system, please visit the component
guide.

Let's start by adding objects with model components, which can be rendered:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
uint scene_id = scn_getactive();
cmp_obj* obj = scn_create_obj(scene_id, "obj_name", CMP_OBJTYPE_MODEL);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The first line gets active scene, which in the previous tutorial we assigned it
by `scn_setactive` function.

Second line creates the object with the most basic components pre-attached to
it.

First parameter is the *scene_id* which we want to add our objects to. Second
parameter is the object's name, object name can be duplicated, but in that case,
you will get into trouble finding the object by name, so it is recommended that
you choose unique names to scene objects.

Third parameter is the object's type. Object types are very basic types of
objects (like model in our example), there are for example *particle, light,
trigger, decal, etc*. Defining the type also populates the object with a set of
predefined components, as for model, we will have *transform, bounds, model*
components already added and attached to object for us.

Now that you have all the needed components for our objects, you have to set
modify the *model's filepath *value in order to load a model file (with
extension .h3dm) for it:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
result_t r = cmp_modifys(cmp_findinstance_inobj(obj, "model"), "filepath", "barrel.h3dm");
if (IS_FAIL(r))
    return FALSE;
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Component's value modifications can be done with `cmp_modifyXX` functions, XX
defines what type of value we are dealing with, *s* means string, *f* means
float, *4f* means 4-component float vector and so on.

To find the component inside the object I used `cmp_findinstance_inobj`, and I
sent the object itself and the component name which is "model". There are faster
ways to find components but this one is easier and you have to read the
component system guide and reference docs for more information on component
functions.

The second parameter of `cmp_modifys` is value's name, in our case it's
*"filepath"* of the model.

Third parameter is the actual value, which is the filepath of our model file.
The functions returns RET_FAIL if model could not be loaded or some other error
occurred.



Adding rigid body physics
-------------------------

In this tutorial, I will add a simple physical rigid component to our barrel
objects. The objects don't have *rigid * *body (rbody) * component pre-attached,
so we have to attach it ourselves. I use the most simple function for this
purpose which is `cmp_create_instance_forobj` :

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
cmphandle_t rbody_hdl = cmp_create_instance_forobj("rbody", obj);
ASSERT(rbody_hdl != INVALID_HANDLE);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This function creates and attaches a rigid body (aka "rbody") component instance
for our object and returns the handle to it. So from now on whatever we want to
do with the component, we use that handle.

After creating the component, I load my physics file into it:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
cmp_modifys(rbody_hdl, "filepath", "barrel.h3dp");
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Much like modifying model component described above.



Moving objects
--------------

After we have setup our barrel objects, we can move them around the scene before
starting engine's simulation starts. I will place each object somewhere random
with a little height to demonstrate the physics of the barrels:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
cmp_xform_setposf(obj, rand_getf(-5.0f, 5.0f), rand_getf(10.0f, 16.0f), rand_getf(-5.0f, 5.0f));
cmp_xform_setrot(obj, rand_getf(0, PI), 0, rand_getf(0, PI));
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

First one sets object position of the object and second one sets rotation
randomly.



Changing the light
------------------

Global directional light (aka sun), it's color, intensity and rotation can be
modifed via low-level `scn_world_setvarXXX` and `scn_world_getvarXXX` functions.

These are world variables remains for every scene, no matter if you switch
scenes or not.

There are couple of pre-defined global variables like what this tutorial uses :

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
vec3f sundir;
vec3_setvp(&sundir, scn_world_getvarfv("sun_dir"));
/* transform the light direction by inputs */
scn_world_setvar3f("sun_dir", sundir.f);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

By setting "sun_dir" variable, we change the direction of the sun, as you can
see in the tutorial, shadows and lighting change respected to this value. For
more world variables and how to register new ones, read world variables.



Cleaning up
-----------

Finally, after we are done with our objects we can release them :

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
scn_destroy_obj(obj);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

This function destroys object, removes it from it's owner scene, and also
destroys all components (and resources) we have loaded with it.
