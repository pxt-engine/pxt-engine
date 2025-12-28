#version 460
#extension GL_GOOGLE_include_directive : require

#include "ubo/global_ubo.glsl"

layout(location = 0) in vec4 position;

layout(push_constant) uniform Push {
	mat4 modelMatrix;
} push;


void main() {
	vec4 positionWorld = push.modelMatrix * position;
	gl_Position = ubo.projectionMatrix * ubo.viewMatrix * positionWorld;
}