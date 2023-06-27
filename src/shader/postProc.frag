#version 460
#extension GL_GOOGLE_include_directive : enable

#include "layouts.glsl"

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 vTexUV;

void main() {
    ivec2 coord = ivec2(vec2(textureSize(uDepthNormal, 0) - 1) * vTexUV);
    vec3 color = imageLoad(uRayColorOutput, coord).rgb;
    //vec3 color = texture(uAlbedoMatIdx, vTexUV).rgb;
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, 1.0);
}