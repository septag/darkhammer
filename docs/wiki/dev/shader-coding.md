# Shader coding

Writing shaders for *darkHammer *must follow certain guidelines to improve code readability and also to follow some standards that engine forces upon developer.

## Coding standards

Overall coding style is just like C coding style described in [coding style][]. but there are some additions and differences described below:

-   **Texture Objects:** They are currently used in *HLSL*, and they should always start with *t\_* prefix: `Texture<float4> t_diffuse_tex;`

-   **Sampler Objects:** They should always have *s\_* prefix: `SamplerState s_diffuse_tex;`

-   **Shader's input/output vars:** Every shader type has a name abbreviation, for example, *vs=vertex-shader, ps-pixel-shader, gs=geometry-shader* and so on... And for input values you should add an *i *character to the prefix. For output vars add *o* character:

        /* GLSL example */
        vec4 vsi_pos; /* input position */
        vec4 vso_pos; /* output position */

-   **Constants: **Shader constants (uniforms) have *c\_* prefix: `mat4 c_viewproj;`

-   **Constant Blocks: **Uniform blocks (in GL) and Constant buffers (in D3D) have *cb\_* prefix: `uniform cb_perframe { ... };`

-   **Texture Buffers:** Texture buffers have *tb\_* prefix: `tbuffer tb_test { ... };`

## Coding guide

For writing engine compatible shaders, you should follow these rules:

-   Every shader program should reside in a single file. For example vertex-shader and pixel-shader should *not *be written in a single file.

-   All shader programs should have *main()* function as their entry point.

-   Output variable name if each shader stage should be equal to the input variable of the next shader stage in the pipeline.

-   Shader file names *must* include *.hlsl* extension for *HLSL* shaders. And *.glsl* for *GLSL* shaders.

-   Shader files for each API (*HLSL/GLSL)* should be saved inside their related directory. For example all *HLSL* shaders are inside */data/shaders/hlsl* directory. But inside the engine, they are referenced without any *hlsl* words, in order for the engine to be able to load both *glsl/hlsl* shaders on API change. Example:

        /* shader saved in /data/shaders/hlsl/myshader.ps.hlsl */
        gfx_load_shader("data/shaders/myshader.ps");

-   *HLSL* shaders should always have one-to-one relationship for *SamplerState* and* Texture*. Also their names (without prefixes) should be equal. Example:

        SamplerState s_diffuse_map;
        Texture t_diffuse_map;
        /* Because their names are the same, on shader load, engine automatically detects the relationship between the SampleState and it's texture */

  [coding style]: coding-style.md
