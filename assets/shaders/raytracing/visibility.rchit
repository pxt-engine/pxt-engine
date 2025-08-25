#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "../common/payload.glsl"

layout(location = VisibilityPayloadLocation) rayPayloadInEXT VisibilityPayload p_visibility;

hitAttributeEXT vec2 barycentrics;

void main() {
    p_visibility.instance = gl_InstanceCustomIndexEXT;
    p_visibility.primitiveId = gl_PrimitiveID;
    p_visibility.barycentrics = barycentrics;
    p_visibility.hitDistance = gl_HitTEXT;
}