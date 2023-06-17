#version 460
#extension GL_GOOGLE_include_directive : enable

#include "layouts.glsl"

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 vTexUV;

layout(push_constant) uniform PushConstant {
    ivec2 frameSize;
    uint toneMapping;
} uSettings;

void main() {
    vec3 color = texture(uAlbedoMatIdx, vTexUV).rgb;
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, 1.0);
}