#version 460
#extension GL_GOOGLE_include_directive : enable

#include "layouts.glsl"

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec2 vTexUV;

layout(push_constant) uniform _PushConstant {
	uint uToneMapping;
    uint uCorrectGamma;
    uint noDirect;
    uint noIndirect;
};

vec3 calcFilmic(vec3 c) {
    return (c * (c * 0.22 + 0.03) + 0.002) / (c * (c * 0.22 + 0.3) + 0.06) - 1.0 / 30.0;
}

vec3 filmic(vec3 c) {
    return calcFilmic(c * 1.6) / calcFilmic(vec3(11.2));
}

vec3 ACES(vec3 color) {
    return (color * (color * 2.51 + 0.03)) / (color * (color * 2.43 + 0.59) + 0.14);
}

void main() {
    ivec2 coord = ivec2(vec2(textureSize(uGBufferThisA, 0) - 1) * vTexUV);
    vec3 color = vec3(0.0);

    if (noDirect == 0) {
        color += imageLoad(uDirectOutput, coord).rgb;
    }
    if (noIndirect == 0) {
        color += imageLoad(uIndirectOutput, coord).rgb;
    }
    color = (uToneMapping == 0) ? color :
        (uToneMapping == 1) ? filmic(color) : ACES(color);

    if (uCorrectGamma != 0) {
        color = pow(color, vec3(1.0 / 2.2));
    }
    FragColor = vec4(color, 1.0);
}