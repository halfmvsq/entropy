#version 330 core

layout (location = 0) in vec2 clipPos;

// View transformation data:
uniform mat4 view_T_clip;
uniform mat4 world_T_clip;
uniform float clipDepth;

// View aspect ratio (for checkerboarding and flashlight):
uniform float aspectRatio;

// Number of checkerboard squares along the longest view dimension:
uniform float numSquares;

// Image transformation data:
uniform mat4 imgTexture_T_world;
uniform mat4 segTexture_T_world;

// Vertex shader outputs:
out VS_OUT
{
    vec3 ImgTexCoords; // Image texture coords
    vec3 SegTexCoords; // Seg texture coords
    vec2 CheckerCoord; // 2D clip position
    vec2 ClipPos; // 2D clip position
} vs_out;


void main()
{
    vs_out.ClipPos = clipPos;

    vec2 C = numSquares * 0.5 * ( clipPos + vec2( 1.0, 1.0 ) );

    vs_out.CheckerCoord = mix( vec2( C.x, C.y / aspectRatio ),
                               vec2( C.x * aspectRatio, C.y ),
                               float( aspectRatio <= 1.0 ) );

    vec4 clipPos3d = vec4( clipPos, clipDepth, 1.0 );
    gl_Position = view_T_clip * clipPos3d;

    vec4 worldPos = world_T_clip * clipPos3d;

    vec4 imgTexPos = imgTexture_T_world * worldPos;
    vec4 segTexPos = segTexture_T_world * worldPos;

    vs_out.ImgTexCoords = vec3( imgTexPos / imgTexPos.w );
    vs_out.SegTexCoords = vec3( segTexPos / segTexPos.w );
}
