#version 460
#extension GL_GOOGLE_include_directive : enable

#include "layouts.glsl"

layout(location = 0) in vec3 aPos;
layout(location = 1) in float aTexX;
layout(location = 2) in vec3 aNorm;
layout(location = 3) in float aTexY;

layout(location = 0) out VSOut {
	vec3 pos;
	vec3 norm;
	vec2 uv;
} vsOut;

layout(push_constant) uniform _PushConstant {
	GBufferDrawParam uGBufferDrawParam;
};

void main() {
	GBufferDrawParam param = uGBufferDrawParam;
	vec4 pos = param.model * vec4(aPos, 1.0);
	vsOut.pos = pos.xyz;
	gl_Position = uCamera.projView * pos;

	vsOut.norm = normalize(mat3(param.modelInvT) * aNorm);
	vsOut.uv = vec2(aTexX, aTexY);
}