#version 460
#extension GL_GOOGLE_include_directive : enable

#include "HostDevice.h"

layout(location = 0) out vec4 DepthNormal;
layout(location = 1) out vec4 AlbedoMatIdx;

layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 vNorm;
layout(location = 2) in vec2 vTexUV;
layout(location = 3) in float vDepth;

layout(set = CameraDescSet, binding = 1) uniform sampler2D uTexture;

void main() {
    DepthNormal = vec4(vDepth, vNorm);
    AlbedoMatIdx = vec4(vPos, vTexUV.x);
}