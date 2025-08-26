#ifndef _BINDINGS_RT_
#define _BINDINGS_RT_

layout(set = 1, binding = 0) uniform accelerationStructureEXT TLAS;

layout(set = 2, binding = 0) uniform sampler2D textures[];

// Declare the output storage image
// The format qualifier (e.g., rgba8, rgba32f) should match how the image was created in Vulkan.
// rgba8 is common for 8-bit per channel normalized output. Use rgba32f for HDR float output.
layout(set = 3, binding = 0, rgba16f) uniform image2D outputImage;

layout(set = 4, binding = 0) readonly buffer materialsSSBO {
    Material m[];
} materials;

layout(set = 6, binding = 0, std430) readonly buffer meshInstancesSSBO {
    MeshInstanceDescription i[];
} meshInstances;

layout(set = 7, binding = 0, std430) readonly buffer emittersSSBO {
    uint numEmitters;
    Emitter e[];
} emitters;

layout(set = 8, binding = 0, std430) readonly buffer volumesSSBO {
    Volume volumes[];
} volumes;

layout(set = 9, binding = 0, std430) readonly buffer blueNoiseSSBO {
    uint indeces[]; // Indices of the blue noise textures in the texture array
} blueNoiseTextures;

#endif