#version 450

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec3 inColor;

layout (location = 0) out vec4 vertColor;

void main()
{
    gl_PointSize = 8.0;
    gl_Position = vec4(inPos.xy, 1.0, 1.0);
    vertColor = vec4(inColor, 1.0);
}
