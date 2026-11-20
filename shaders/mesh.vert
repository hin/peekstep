#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inColor;

layout(push_constant) uniform Push {
    mat4 viewProj;
    vec4 viewDir; // camera forward, world space
} pc;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec4 outColor;

void main()
{
    gl_Position = pc.viewProj * vec4(inPos, 1.0);
    outNormal = inNormal;
    outColor = inColor;
}
