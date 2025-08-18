#ifndef _PUSH_RT_
#define _PUSH_RT_

layout(push_constant) uniform RayTracingPushConstantData {
	uint noiseType; // Type of noise to use (0: tea noise, 1: blue noise)
	uint blueNoiseBaseIndex; // Index of the first blue noise texture in the texture registry
	uint blueNoiseTextureCount; // Number of blue noise textures available
	uint blueNoiseTextureSize; // Size of each blue noise texture (assumed square)
	bool selectSingleTextures; // Whether to select single textures or use 
							   //different blue noise textures every frame
	uint blueNoiseIndex; // Index of the blue noise texture to use in case selectSingleTextures is true
} push;

#endif