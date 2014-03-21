# Lua Core module

*core* is a lua module that wraps some parts of *darkHammer's core* library.

**Note:** All of the below functions and types should start *core* prefix. Example: `core.printcon("test")`

## Functions

| Function  | Parameters | Returns | Description 
|---|---|---|---
| *printcon(text)*  | text:string | - | Prints a text to program console
| *randFloat(min, max)* | min:float, max:float | float | Returns a random floating point value between two numbers
| *randInt(min, max)* | min:int, max:int | int | Returns random integer value between two numbers

## Classes

### Vector2D

2D vector class that contains (x, y) as integers

| Method | Parameters | Returns | Description
| --- | --- | --- | ---
| *Vector2D()* | - | - | Default constructor, sets (x, y) = (0, 0)
| *Vector2D(x, y)* | x:int, y:int | - | Constructor that sets x and y
| *Vector2D(v)* | v:Vector2D | - | Copy constructor
| *set(x, y)* | x:int, y:int | - | Sets x, y values
| *= (operator)* | v:Vector2D | - | Assigns the vector to another one
| *+ (operator)* | v:Vector2D | Vector | Adds two vectors
| *- (operator)* | v:Vector2D | Vector | Subtracts two vectors
| _* (operator)_ | k:float | Vector | Multiplies a scalar value by the vector
| *[index] (operator)* | index: int | int& | Property accessor, [0]=X, [1]=Y


### Vector

Common 3D vector with (x, y, z) floating point components

| Method | Parameters | Returns | Description
| --- | --- | --- | ---
| *Vector()* | - | - | Default constructor, sets (x, y, z) = (0, 0, 0)
| *Vector(x, y, z)* | x:float, y:float, z:float | - | Constructor that sets (x, y, z)
| *Vector(v)* | v:Vector | - | Copy constructor
| *= (operator)* | v:Vector | - | Assigns the vector to another one
| *+ (operator)* | v:Vector | Vector | Adds two vectors
| *- (operator)* | v:Vector | Vector | Multiplies two vectors
| _* (operator)_ | v:Vector | float | Dot products two vectors
| _* (operator)_ | k:float | Vector | Multiplies the vector to an scalar value
| _/ (operator)_ | k:float | Vector | Divides the vector to an scalar value
| *cross(v)* | v:Vector | Vector | Cross products two vectors
| *set(x, y, z)* | x:float, y:float, z:float | - | Sets (x, y, z) values
| *x()* | - | float | Returns X component
| *y()* | - | float | Returns Y component
| *z()* | - | float | Returns Z component
| *lerp(v1, v2, t)* | x:Vector, y:Vector, t:float | - | Linear interpolates between two vectors, t is the interpolation value and should be in [0, 1] range
| *cubic(v0, v1, v2, v3, t)* | v0, v1, v2, v3:Vector, t:float | - | Cubic interpolates between four vectors, t is the interpolation value and should be in [0, 1] range
| *normalize()* | - | - | Normalizes the vector
| *[index] (operator)* | index:int | float& | Property accessor, [0]=X, [1]=Y, [2]=Z

### Color

Color object that is used to manipulate colors. Contains (r, g, b, a) components and each of them are normalized 0-1 floating point values.

| Method | Parameters | Returns | Description
|---|---|---|---
| *Color()* | - | - | Default constructor, sets color to black
| *Color(r, g, b, a)* | r:float, g:float, b:float, a:float | - | Constructor that sets RGBA values
| *Color(c)* | c:Color | - | Copy constructor
| *set(r, g, b, a)* | r:float, g:float, b:float, a:float | - | Sets color components, each color value should be in [0, 1] range
| *setInt(r, g, b, a)* | r:int, g:int, b:int, a:int | - | Sets color components by integer values, each value should be in [0, 255] range
| *= (operator)* | c:Color | - | Assigns color to another one
| *+ (operator)* | c:Color | - | Adds two colors
| _* (operator)_ | c:Color | - | Multiplies two colors
| _* (operator)_ | k:float | - | Multiplies color into an scalar value
| *r()* | - | float | Returns R (red) component
| *g()* | - | float | Returns G (green) component
| *b()* | - | float | Returns B (blue) component
| *a()* | - | float | Returns A (alpha) component
| *lerp(c1, c2, t)* | c1:Color, c2:Color, t:float | - | Linear interpolates between two colors, t is the interpolation value and should be in [0, 1] range
| *toGamma()* | - | Color | Transforms the color into gamma-space
| *toLinear()* | - | Color | Transforms the color into linear-space
| *toUint()* | - | int | Packs color to 32-bit RGBA value
| *[index] (operator)* | index:int | float& | Property accessor, [0]=R, [1]=G, [2]=B, [3]=A

### Quat

Quaternion class with (x, y, z, w) floating-point components, that is used to define rotations in 3D space.

| Method | Parameters | Returns | Description
|---|---|---|---
| *Quat()* | - | - | Default constructor, sets quaternion to identity
| *Quat(x, y, z, w)* | x:float, y:float, z:float, w:float | - | Constructor that sets (x, y, z, w) values
| *Quat(q)* | q:Quat | - | Copy constructor
| *set(x, y, z, w)* | - | - | Sets (x, y, z, w) values
| *setEuler(rx, ry, rz)* | rx:float, ry:float, rz:float | - | Sets euler angles (in radians) for rotation, rx=Rotate around x-axis, ry=rotate around y-axis, rz=rotate around z-axis
| *getEuler()* | - | Vector | Returns euler angles (in radians) as a three component vector
| *inverse()* | - | - | Inverses the quaternion
| *slerp(q1, q2, t)* | q1:Quat, q2:Quat, t:float | - | Interpolates betweeen two quaternions, t is the interpolation value and should be in [0, 1] range
| *[index] (operator)* | index:int | float& | Property accessor, [0]=X, [1]=Y, [2]=Z, [3]=W


