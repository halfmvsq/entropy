#version 330 core

layout (location = 0) in vec2 clipPos;

uniform mat4 view_T_clip;
uniform float clipDepth;
uniform vec2 clipMin;
uniform vec2 clipMax;

void main()
{
    gl_Position = view_T_clip * vec4( clipPos, clipDepth, 1.0 );
    gl_Position.xy = clamp( gl_Position.xy, clipMin, clipMax );
}
