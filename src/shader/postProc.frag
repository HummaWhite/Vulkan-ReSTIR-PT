#version 460
#extension GL_GOOGLE_include_directive : enable

#include "HostDevice.h"

layout(location = 0) out vec4 FragColor;

layout(location = 0) in vec2 vTexUV;

layout(set = SwapchainStorageDescSet, binding = 0) uniform sampler2D uDepthNormal;
layout(set = SwapchainStorageDescSet, binding = 1) uniform sampler2D uAlbedoMatIdx;

layout(push_constant) uniform PushConstant {
    ivec2 frameSize;
    uint toneMapping;
} uSettings;

void main() {
    vec3 color = texture(uDepthNormal, vTexUV).rgb;
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, 1.0);
}