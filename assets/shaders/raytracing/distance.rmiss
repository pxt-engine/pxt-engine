#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "../common/payload.glsl"
#include "../common/ray.glsl"

layout(location = DistancePayloadLocation) rayPayloadInEXT float p_distance;

void main() {
	p_distance = RAY_T_MAX;
}