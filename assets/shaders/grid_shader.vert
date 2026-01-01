#version 460
#extension GL_GOOGLE_include_directive : require

#include "ubo/global_ubo.glsl"

// inspired from https://vec3.ca/posts/rendering-an-infinite-grid

layout(location = 0) out vec3 fragPosWorld;
layout(location = 1) out vec3 fragPosView;

layout(push_constant) uniform GridPush {
    vec4 xAxisColor;
    vec4 zAxisColor;
    float gridUnitSize;
    // how many grid squares run along a grid group edge
    uint gridMinorsPerMajor;
    float nearFog;
    float farFog;
} push;

void main() {
    // gl_VertexIndex is the id of the vertex being drawn
    // we call the draw for 4 vertices so vertexId [0,3]
    uint vertexId = uint(gl_VertexIndex);

    // bit manipulation to get 4 coordinates:
    // (0,1) , (1,1) , (1,0) , (0,0)
    uint bit0 = vertexId & 0x1u;
    uint bit1 = (vertexId & 0x2u) >> 1u;

    vec2 mPos = vec2(
        float(bit0 ^ bit1), // 0, 1, 1, 0
        1.0 - float(bit1)   // 1, 1, 0, 0
    );

    // scale to get unit cube centered at the origin
    mPos = mPos * 2.0 - 1.0;
    
    // we scale the fog far distance based on the camera's height
	const float cameraHeight = abs(ubo.inverseViewMatrix[3].y);
	const float fogCameraScale = max(1.0, cameraHeight / 10.0);
    const float scaledFarFog = push.farFog * fogCameraScale;
    mPos = mPos * scaledFarFog;
    
    mPos = mPos + vec2(ubo.inverseViewMatrix[3].x, ubo.inverseViewMatrix[3].z);

    vec3 wPos = vec3(mPos.x, 0.0, mPos.y);

    gl_Position = ubo.projectionMatrix * ubo.viewMatrix * vec4(wPos, 1.0);

    fragPosWorld = wPos;
    fragPosView = vec3(ubo.viewMatrix * vec4(wPos, 1.0));
}