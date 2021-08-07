#version 450

layout (location = 0) in vec2 inPos;
layout (location = 1) in vec3 inColor;

layout (location = 0) out vec4 vertColor;

layout( push_constant ) uniform constants
{
    float timer;
} PushConstants;

void main()
{
    gl_PointSize = 1.0;
    
    gl_Position = vec4(inPos, 1.0, 1.0);
    vertColor = vec4(inColor * fract(PushConstants.timer), 1.0);
}
