#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "../common/payload.glsl"

layout(location = VisibilityPayloadLocation) rayPayloadInEXT VisibilityPayload p_visibility;

void main() {
	p_visibility.instance = -1;
}