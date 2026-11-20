#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec4 inColor;

layout(push_constant) uniform Push {
    mat4 viewProj;
    vec4 viewDir;
} pc;

layout(location = 0) out vec4 outColor;

void main()
{
    // Headlight: light comes from the camera. Two-sided, because STEP faces
    // are not always oriented consistently.
    vec3 n = normalize(inNormal);
    vec3 v = -normalize(pc.viewDir.xyz);
    float ndotv = abs(dot(n, v));
    float diffuse = 0.25 + 0.75 * ndotv;
    float specular = pow(ndotv, 40.0) * 0.25;
    outColor = vec4(inColor.rgb * diffuse + vec3(specular), inColor.a);
}
