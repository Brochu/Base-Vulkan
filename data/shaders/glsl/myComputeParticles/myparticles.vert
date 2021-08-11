#version 450

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec3 inColor;

layout (location = 0) out vec4 vertColor;

layout(std140, binding=0) uniform time
{
    vec4 values;
} Time;

float rand(vec2 co)
{
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    gl_PointSize = 1.0;

    gl_Position = vec4(inPos, 1.0, 1.0);
    
    float r = rand(inPos * Time.values.y);
    float g = rand(inColor.rg * Time.values.y);
    float b = rand(inColor.gb * Time.values.y);
    vertColor = vec4(r, g, b, 1);
}
