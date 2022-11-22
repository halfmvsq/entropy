#version 330 core

// View corner vertex position in Clip space
layout (location = 0) in vec2 clipVertexPos;

uniform mat4 view_T_clip;
uniform mat4 world_T_clip;
uniform float clipDepth;

// Vertex shader outputs:
out VS_OUT
{
    vec3 worldRayDir; // Ray direction in World space (NOT normalized)
} vs_out;


void main()
{
    // Rays are defined by two points:
    // 1) One point on the plane positioned at NDC depth clipDepth
    vec4 clipPos1 = vec4(clipVertexPos, clipDepth, 1.0);
    gl_Position = view_T_clip * clipPos1;

    // 2) Another point on the far clipping plane positioned at NDC depth 1.0
    vec4 clipPos2 = vec4(clipVertexPos, 1.0, 1.0);

    vec4 worldPos1 = world_T_clip * clipPos1;
    worldPos1 /= worldPos1.w;

    vec4 worldPos2 = world_T_clip * clipPos2;
    worldPos2 /= worldPos2.w;

    vs_out.worldRayDir = vec3(worldPos2 - worldPos1);
}
