#version 450

// Test shader for preprocessor definitions
// This shader uses #ifdef directives to test preprocessor support

#ifdef USE_COLOR
    #define OUTPUT_COLOR vec4(1.0, 0.0, 0.0, 1.0)
#else
    #define OUTPUT_COLOR vec4(0.0, 1.0, 0.0, 1.0)
#endif

#ifdef SCALE_FACTOR
    #define POSITION_SCALE SCALE_FACTOR
#else
    #define POSITION_SCALE 1.0
#endif

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(location = 0) out vec4 outColor;

void main() {
    gl_Position = vec4(inPosition * POSITION_SCALE, 1.0);
    outColor = OUTPUT_COLOR;
}
