# Lua Engine module
*eng* is a lua module that wraps parts of *darkHammer's* engine library.  
**Note:** all of below functions and types should start with *eng* prefix. Example: `eng.setSunIntensity(2)`

## Functions

| Function | Parameters | Returns | Description
|---|---|---|---
| *setSunDir(x, y, z)* | x:float, y:float, z:float | - | Sets sun direction
| *setSunDir(v)* | v:Vector | - | Sets sun direction
| *setSunColor(r, g, b)* | r:float, g:float, b:float | - | Sets sun color
| *setSunColor(c)* | c:Color | - | Sets sun color
| *setSunIntensity(i)* | i:float | - | Sets sun intensity (Default = 1) 
| *setAmbientIntensity(a)* | a:float | - | Sets world's ambient intensity value
| *setAmbientSky(r, g, b)* | r:float, g:float, b:float | - | Sets sky's ambient color
| *setAmbientSky(c)* | c:Color | - | Sets sky's ambient color
| *setAmbientGround(r, g, b)* | r:float, g:float, b:float | - | Sets ground's ambient color
| *setAmbientSky(c)* | c:Color | - | Sets sky's ambient color
| *addTimer(timeout, funcname, single_shot)* | timeout:int, funcname:string, single_shot:bool | int | Starts a timer. *timeout* is in milliseconds, *funcname* is a name string of the timer's callback function, *single_shot* (default = false) defines if the timer should be called one time only, see below for more on timer, Returns Id of the registered timer
| *removeTimer(id)* | id:int | - | Stops and removes a registered timer
| *setMemThreshold(mem_sz)* | mem_sz:int | - | This is an advanced function, to control scripts' memory management

## Classes

### Component
Components are parts of scene objects that defines their behaviour and other properties in the world.  
Each component may have multiple values inside them, for example many of low-level components have *"filepath"* value. Setting that value loads a specific file related to that component type.  
For more information about components and their values, see [Components reference](../dev/components.md).

| Method | Parameters | Returns | Description
| --- | --- | --- | ---
| *name()* | - | string | Returns the name of current component
| *debug()* | - | - | Triggers component debug visualization
| *undebug()* | - | - | Turns off component debug visualization
| [value] (operator) | value:string | - | Sets a value inside the component, each component have multiple values which are referenced by their names

### CharacterAnim
Character animation controller is a speciall object, that resides in *"animchar"* component of the object/This object controls the parameters of the animation controller.

| Method | Parameters | Returns | Description
| --- | --- | --- | ---
| CharacterAnim() | - | - | Default constructor
| setParam(name, value) | name:string, value:number | - | Sets a parameter value (number) for the controller, parameters are referenced by their names
| setParam(name, value) | name:string, value:bool | - | Sets a parameter value (boolean) for the controller, parameters are referenced by their names
| getParam(name) | - | number | Returns parameter value referenced by it's name
| getParamBool(name) | - | bool | Returns a boolean parameter value, referenced by it's name


### Object
Objects are main classes that populate the scene, they consist of multiple components that define their behaviour in the world.

| Method | Parameters | Returns | Description
| --- | --- | --- | ---
| *Object()* | - | - | Default constructor
| *Object(o)* | - | - | Copy constructor
| *move(x, y, z)* | x:float, y:float, z:float | - | Moves the object to specified x, y, z position
| *move(v)* | v:Vector | - | Moves the object to specified vector position
| *rotate(rx, ry, rz)* | rx:float, ry:float, rz:float | - | Rotates the object, by their rotation around each axis, rotations are in *Degrees*
| *rotate(q)* | q:Quat | - | Rotates the object by a quaternion
| *addComponent(name)* | name:string | - | Adds a component to the object by it's name, remember that objects can't have multiple components of the same type
| *loadRigidBody(h3dpfile)* | h3dpfile:string | - | Creates a rigid-body ("rbody") component, and loads a physics file into it
| *unloadRigidBody()* | - | - | Unloads and removes rigid-body ("rbody") component from the object
| *loadAttachDock(dock1, dock2, dock3, dock4)* | dock1, dock2, dock3, dock4: string | - | Creates "AttachDock" component, and assigns docks by their names, docks are actual nodes in the model that other objects can be attached to.
| *attach(obj, dock_name)* | obj:Object, dock_name:string | - | Creates "attachment" component, and attaches the object to another object with the given dock_name
| *detach()* | - | - | Detaches current object from previously attached object
| *loadAnimation(h3da_file)* | h3da_file:string | - | Creates "anim" component, and loads animation reel file into it
| *loadCharacterAnim(ctrl_file)* | ctrl_file:string | - | Creates "animchar" component and loads animation controller into it, animation controllers can be controlled via *CharacterAnim* class
| *getCharacterAnim()* | - | CharacterAnim | Returns animation controller for the object, animation controller must be loaded with *loadCharacterAnim*
| *addForce(force)* | force:Vector | - | Applies a force vector to an object
| *addForce(fx, fy, fz)* | fx:float, fy:float, fz:float | - | Applies a force (x, y, z) to an object
| *addTorque(torque)* | torque:Vector | - | Applies a torque vector to an object
| *addTorque(tx, ty, tz)* | fx:float, fy:float, fz:float | - | Applies a torque (x, y, z) to an object
| *name()* | - | string | Returns the name of the object
| *[cmpname] (operator)* | - | Component | Component accessor, Retrieves components of the object by their names
| *isNull()* | - | bool | Returns *true* if object is invalid 

### Scene
Scenes are populated with objects, they manage objects and physics, AI properties.

| Method | Parameters | Returns | Description
| --- | --- | --- | ---
| *Scene()* | - | - | Default constructor, fetches the default active scene
| *Scene(name)* | - | - | Constructs scene object from it's existing name
| *clear()* | - | - | Clears the scene of any objects
| *find(name)* | - | Object | Finds objects by their names
| *getObject(id)* | id:int | Object | Returns object by it's Id
| *createModelLod(name, h3dm_hi, h3dm_md, h3dm_lo) | name:string, h3dm_hi:string, h3dm_md:string, h3dm_lo:string | Object | Creates an object and loads Lod models for it, LOD models usually have multiple details categorized "hi" (high), "md (medium)" and "lo" (low)
| *createModel(name, h3dmfile)* | name:string, h3dmfile:string | Object | Creates an object and loads a single model file for it
| *createPointLight(name)* | name:string | Object | Creates a point light object
| *createSpotLight(name)* | name:string | Object | Creates a spot light object
| *createTrigger(name, bx, by, bz, funcname)* | name:string, bx:float, by:float, bz:float, funcname:string | - | Creates a trigger object, (bx, by, bz) = (width, height, depth) defines the dimensions of the trigger's box, *funcname* defines the callback function for the trigger (see examples below)
| *setMax(x, y, z)* | x:float, y:float, z:float | - | Changes scene's boundary maximum point
| *setMin(x, y, z)* | x:float, y:float, z:float | - | Changes scane's boundary minimum point
| *setGravity(gx, gy, gz)* | gx:float, gy:float, gz:float | - | Sets scene gravity vector
| *setGravity(gravity)* | gravity:Vector | - | Sets scene gravity vector
| *createPhysicsPlane()* | - | - | Mainly used for debugging physics, creates an infinite XZ physics plane on the origin

### Input
Input class instance can be used to read input data from devices such as keyboard and mouse

| Method | Parameters | Returns | Description
| --- | --- | --- | ---
| *Input()* | - | - | Default constructor
| *keyPressed(key)* | key:InputKey | bool | Checks if a keyboard key is pressed, see InputKey for keys
| *mouseLeftPressed()* | - | bool | Checks if mouse left key is pressed
| *mouseRightPressed()* | - | bool | Checks if mouse right key is pressed
| *mouseMiddlePressed()* | - | bool | Checks if mouse middle key is pressed
| *mousePos()* | - | Vector2D | Returns pixel position of the mouse cursor

## Examples

### Timer
Timers are callback functions inside the script that are registered with *addTimer* function. The callbacks have one parameter named *id* which defines the registered Id of the timer.  
Here's an example that demonstrates the use of timers inside the script :  
```
function timer_print_something(id)
    core.printcon("This timer prints every 5 seconds")
end

eng.addTimer(5000, "timer_print_something", false)
```

### Components
This example creates a model *(test.h3dm)* into the default active scene and excludes it from shadows, then triggers "model" component debug visualization.  

```
s = eng.Scene()
obj = s:createModel("myobject", "test.h3dm")
model_cmp = obj["model"]
model_cmp["exclude_shadows"] = true
model_cmp:debug()
```

### Triggers
In this example, I create a trigger on point (5, 0, 5) , with box dimensions of (width=1, height=2, depth=1). And when the trigger happens, print the input object's name.  
```
s = eng.Scene()
trigger = s:createTrigger("mytrigger", 1, 2, 1, "mytrigger_callback")
trigger:move(5, 0, 5)

function mytrigger_callback(trigger_id, obj_id, state)
    if state == eng.TRIGGER_IN then
        core.printcon(s:getObject(obj_id):name())
    end
end
```