#version 330 core

#define N 2

layout (location = 0) in vec2 clipPos;

// View transformation data:
uniform mat4 view_T_clip;
uniform mat4 world_T_clip;
uniform float clipDepth;

// Image transformation data:
uniform mat4 imgTexture_T_world[N];
uniform mat4 segTexture_T_world[N];

// Vertex shader outputs:
out VS_OUT
{
    vec3 ImgTexCoords[N]; // Image texture coords
    vec3 SegTexCoords[N]; // Seg texture coords
} vs_out;


void main()
{
    vec4 clipPos3d = vec4( clipPos, clipDepth, 1.0 );
    gl_Position = view_T_clip * clipPos3d;

    vec4 worldPos = world_T_clip * clipPos3d;

    for ( int i = 0; i < N; ++i )
    {
        vec4 imgTexPos = imgTexture_T_world[i] * worldPos;
        vec4 segTexPos = segTexture_T_world[i] * worldPos;

        vs_out.ImgTexCoords[i] = vec3( imgTexPos / imgTexPos.w );
        vs_out.SegTexCoords[i] = vec3( segTexPos / segTexPos.w );
    }
}
