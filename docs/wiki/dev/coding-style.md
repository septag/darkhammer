# Coding Style

Coding style in dark-hammer is pretty much what *Linus Torvalds* describes in linux kernel coding style, with some minor changes that are described below.

Before moving forward, read this [document][]

First of all, 99% of the code is written in *C99 *and *.C *files, the only exceptions of* cpp* are sources that need to access C++ libraries like *DirectX* and *Physx* Layers, and also lua wrapper.

-   Tabs are *4 spaces*

-   Pointer types should have *\** character right before the name, without spaces:

        int* func(const char* text)
        {
            void* p1;
            void* p2;
        }

-   Filenames are *lower-case* and seperated with *'-'* character. Example: `gfx-device.c`

-   Variable and function names are *lower\_case\_underline* Example: `int my_var;`

-   Use abbreviations for some known names and put them at the end of the variable name. Example: `int item_cnt;  int layers_max;`

-   Functions and other types like `struct, typedef, enum` come with their related subsystem prefix, in headers and interfaces:

        /* gfx related header file */
        void gfx_drawmeshes();
        struct gfx_object
        {
            ...
        };

-   Preprocessor and Macros are all *UPPER\_CASE.* Example: `#define RELEASE(x)`

-   Enumerators are *UPPER\_CASE, *But their type names are *lower\_case\_underline* like other type defs:

        enum object_type
        {
            OBJECT_MODEL,
            OBJECT_PARTICLE,
            OBJECT_TRIGGER
        };

-   It's recommended to use opaque typedefs defined in *types.h* in the code, like: `uint32, uint16, fl32, fl64, uptr_t, ...`

-   It's recommended to not use *typedef* for *struct* types.

-   Function parameters that are more than 16 bytes in size, should be passed by their pointers.

-   Function's output parameters come in first arguments:

        struct vec4f* vec4_add(OUT struct vec4f* r, const struct vec4f* v1, const struct vec4f* v2);

-   There is one space between C keywords and parenthesis:

        if (condition) {
           ....
        }

        for (uint32 i = 0; i < 100; i++) {
           ....
        }

-   *If/else* statements should be like this:

        if (condition) {
        } else if (condition2) {
        } else {
        }

-   Single line blocks can have no *{}*:

        for (int32 i = 0; i < n; i++)
            v += i;

-   Lines are limited to *100 characters*. More than 100 character line widths, should break into a new line (with indentation).

  [document]: http://www.kernel.org/doc/Documentation/CodingStyle
