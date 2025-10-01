#ifndef _PAYLOAD_
#define _PAYLOAD_

#define PathTracePayloadLocation  0
#define VisibilityPayloadLocation 1
#define DistancePayloadLocation   2


// Payload Flags
const uint FLAG_DONE = 1 << 0; // Path is done, no more bounces.
const uint FLAG_SPECULAR = 1 << 1; // Last bounce was specular.

struct PathTracePayload {
    // Accumulated color and energy along the path.
    vec3 radiance;

    // The accumulated material reflectance/transmittance. It's multiplied at each bounce.
    vec3 throughput;

    // Current number of bounces. Used to limit path length and for Russian Roulette.
    int depth;

    // The origin of the ray in world space.
    vec3 origin;

    // The direction of the ray in world space.
    vec3 direction;

	// The instance that was hit by the ray. -1 if no hit.
	float hitDistance;

    // The medium (volume) the ray is currently in. (index)
    // If -1, we are in the void.
    int mediumIndex;

    // A seed for the random number generator, updated at each bounce.
    uint seed;

    // A Noise value used for sampling. Can be white/blue etc...
    vec2 samplingNoise;

    // The PDF value of the last BSDF or phase function sampling
    float pdf;

    uint flags;
};

bool hasFlag(in PathTracePayload payload, in uint flag) {
    return (payload.flags & flag) != 0u;
}

void setFlag(inout PathTracePayload payload, in uint flag) {
    payload.flags |= flag;
}

void removeFlag(inout PathTracePayload payload, in uint flag) {
    payload.flags &= ~flag;
}



struct VisibilityPayload {
    int instance;
    float hitDistance;
    int primitiveId;
    vec2 barycentrics;
};

#endif