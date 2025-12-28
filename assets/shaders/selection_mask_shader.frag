#version 460
#extension GL_GOOGLE_include_directive : require

#include "ubo/global_ubo.glsl"

layout(location = 0) out vec4 outColor;

void main() {
	// for now selection color is white (user does not see this)
	outColor = vec4(1.0, 1.0, 1.0, 1.0);
}