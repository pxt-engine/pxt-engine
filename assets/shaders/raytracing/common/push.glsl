#ifndef _PUSH_RT_
#define _PUSH_RT_

layout(push_constant) uniform RayTracingPushConstantData {
	uint noiseType; // Type of noise to use (0: tea noise, 1: blue noise)
	uint blueNoiseBaseIndex; // Index of the first blue noise texture in the texture registry
} push;

#endif