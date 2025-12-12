#version 460
#extension GL_GOOGLE_include_directive : require

#include "ubo/global_ubo.glsl"

layout(push_constant) uniform Push {
	mat4 modelMatrix;
	vec4 objPickingColor;
} push;

layout(location = 0) out vec4 outColor;

void main() {
	outColor = push.objPickingColor;
}