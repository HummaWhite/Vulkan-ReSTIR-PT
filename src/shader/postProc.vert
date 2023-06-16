#version 460
#extension GL_GOOGLE_include_directive : enable

#include "HostDevice.h"

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexUV;

layout(location = 0) out vec2 vTexUV;

void main() {
    gl_Position = vec4(aPos - 0.5, 0.0, 1.0);
    vTexUV = aTexUV;
}