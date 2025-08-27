#ifndef _NEXT_EVENT_ESTIMATION_RT_
#define _NEXT_EVENT_ESTIMATION_RT_

#include "../../common/ray.glsl"
#include "../../common/payload.glsl"
#include "../../common/geometry.glsl"
#include "../../common/random.glsl"
#include "sky.glsl"

layout(location = VisibilityPayloadLocation) rayPayloadEXT VisibilityPayload p_visibility;

#define NEE_MAX_BOUNCES 8

struct EmitterSample {
    vec3 radiance;
    vec3 inLightDirWorld;
    float lightDistance;
    float emitterCosTheta;
    float pdf;
    vec3 normalWorld;
};

vec3 evaluateTransmittance(EmitterSample emitterSample, vec3 worldPosition, int currentMediumIndex) {
    vec3 transmittance = vec3(1.0);

    Ray ray;
    ray.origin = worldPosition;
    ray.direction = emitterSample.inLightDirWorld;

    // we need to keep track of the last surface we hit to know
    // if we are exiting an object inside a volume.
    // think of a donut inside a volume, we need to account
    // for transmittance of the volume in the hole of the donut.
    int currentSurfaceIndex = -1;
    // we also need to know how many times we hit the same surface
    int currentSurfaceHitCount = 0;

    float tMax = max(0.0, emitterSample.lightDistance - FLT_EPSILON);

    int depth;
    for (depth = 0; depth < NEE_MAX_BOUNCES; depth++) {
        traceRayEXT(
            TLAS,
            gl_RayFlagsTerminateOnFirstHitEXT, // Ray Flags           
            0xFF,  // Cull Mask         
            1,
            0,
            1,
            ray.origin,
            RAY_T_MIN,
            ray.direction,
            tMax,
            VisibilityPayloadLocation
        );

        int instanceIndex = p_visibility.instance;
        if (instanceIndex == -1) {
            // There are no more objects in between the point and the light source
            break;
        }

        const MeshInstanceDescription instance = meshInstances.i[instanceIndex];

        bool isInsideSurface = currentSurfaceHitCount % 2 == 1; // I dont know if (bool)intValue conversion is the same
        bool isInMedium = currentMediumIndex != -1;

        if (isInMedium) {
            const Volume volume = volumes.volumes[currentMediumIndex];

            // We have travelled the medium from start to finish
            // we need to account for the absorption
            vec3 sigma_t = volume.absorption.rgb + volume.scattering.rgb;
            if (maxComponent(sigma_t) > 0.0) {
                // Beer-Lambert law for transmittance
                transmittance *= exp(-sigma_t * p_visibility.hitDistance);
            }
        }

        // if we hit a volume
        if (instance.volumeIndex != UINT_MAX) {
            // Update the current medium index
            currentMediumIndex = int(instance.volumeIndex);
        }

        // if we hit a surface that has a material (maybe volume inside)
            // if we hit the same surface again, we are inside a transparent object
            // we just need to go through (no refraction is accounted for now).
        if (currentSurfaceIndex != instanceIndex && instance.materialIndex != UINT_MAX) {
            currentSurfaceIndex = instanceIndex;
            currentSurfaceHitCount = 1;

            const Material material = materials.m[instance.materialIndex];
            float transmission = material.transmission;

            // if the surface is opaque we stop
            if (transmission == 0.0) {
                transmittance = vec3(0.0);
                break;
            }

            const Triangle triangle = getTriangle(instance.indexAddress, instance.vertexAddress, p_visibility.primitiveId);

            const vec2 uv = getTextureCoords(triangle, p_visibility.barycentrics) * instance.textureTilingFactor;

            vec3 albedo = getAlbedo(material, uv, instance.textureTintColor);
            float metalness = getMetalness(material, uv);
            float roughness = getRoughness(material, uv);

            // Even if it's not physically correct we use this as an approximation of how much
            // light reaches the point. Metallic and rough surfaces reflects the light.
            transmittance *= (1.0 - metalness) * (1.0 - roughness) * transmission * pow(albedo, vec3(0.5));

        } else {
            currentSurfaceHitCount++;
        }

        // Prepare for the next segment of the ray
        float offset = p_visibility.hitDistance + RAY_T_MIN;
        ray.origin += ray.direction * offset;
        tMax -= offset;

        if (tMax <= 0.0) {
            break; // Ray has reached its maximum distance
        }
    }

    if (depth == NEE_MAX_BOUNCES) return vec3(0.0);

    return transmittance;
}

/**
 * Samples a random emitter (either a mesh emitter or the sky) and returns the sample.
 * The function samples a mesh emitter or the sky based on the provided seed.
 * It calculates the radiance, light distance, PDF, and visibility of the sampled emitter.
 *
 * @param surface The SurfaceData containing geometric and material properties of the hit point.
 * @param worldPosition The world position of the surface being sampled.
 * @param smpl Output parameter to store the sampled emitter data.
 */
void sampleEmitter(uint emitterIndex, vec3 worldPosition, out EmitterSample smpl, inout uint seed) {
    smpl.radiance = vec3(0.0);
    smpl.lightDistance = RAY_T_MAX;
    smpl.pdf = 0.0;

    vec3 worldInLightDir = vec3(0.0);

    const uint numEmitters = uint(emitters.numEmitters);
    const uint totalSamplableEmitters = numEmitters + USE_SKY_AS_NEE_EMITTER;

    // Sample a mesh emitter
    const Emitter emitter = emitters.e[emitterIndex];
    const MeshInstanceDescription emitterInstance = meshInstances.i[emitter.instanceIndex];
    const Material material = materials.m[emitterInstance.materialIndex];
        
    const uint faceIndex = nextUint(seed, emitter.numberOfFaces);

    // Generate barycentric coordinates for the triangle
    vec2 emitterBarycentrics = sampleTrianglePoint(seed);
    
    const Triangle emitterTriangle = getTriangle(emitterInstance.indexAddress, emitterInstance.vertexAddress, faceIndex);
    const vec2 uv = getTextureCoords(emitterTriangle, emitterBarycentrics) * emitterInstance.textureTilingFactor;
        
    smpl.radiance = getEmission(material, uv);

    if (smpl.radiance == vec3(0.0)) {
        return; 
    }

    const vec3 emitterObjPosition = getPosition(emitterTriangle, emitterBarycentrics);
    const vec3 emitterObjNormal = getNormal(emitterTriangle, emitterBarycentrics);

    const mat4 emitterObjectToWorld = mat4(emitterInstance.objectToWorld);
    // The upper 3x3 of the world-to-object matrix is the normal matrix
    const mat3 emitterNormalMatrix = mat3(emitterInstance.worldToObject);

    const vec3 emitterPosition = vec3(emitterObjectToWorld * vec4(emitterObjPosition, 1.0));
    const vec3 emitterNormal = normalize(emitterNormalMatrix * emitterObjNormal);

    // vector from emitter the surface to the emitter
    vec3 outLightVec = worldPosition - emitterPosition;

    smpl.lightDistance = length(outLightVec);

    const float areaWorld = calculateWorldSpaceTriangleArea(emitterTriangle, mat3(emitterInstance.objectToWorld));

    if (areaWorld <= 0.0 || smpl.lightDistance <= 0) {
        return;
    }

    vec3 outLightDir = outLightVec / smpl.lightDistance;

    smpl.emitterCosTheta = cosTheta(emitterNormal, outLightDir);
    if (smpl.emitterCosTheta <= 0.0) {
        return;
    }

    worldInLightDir = -outLightDir; 
    smpl.inLightDirWorld = worldInLightDir;
    smpl.pdf = pow2(smpl.lightDistance) / (smpl.emitterCosTheta * areaWorld * totalSamplableEmitters * emitter.numberOfFaces);
}

/**
 * Samples a random emitter (either a mesh emitter or the sky) and returns the sample.
 * The function samples a mesh emitter or the sky based on the provided seed.
 * It calculates the radiance, light distance, PDF, and visibility of the sampled emitter.
 *
 * @param surface The SurfaceData containing geometric and material properties of the hit point.
 * @param worldPosition The world position of the surface being sampled.
 * @param smpl Output parameter to store the sampled emitter data.
 */
void sampleRandomEmitter(mat3 tbn, vec3 worldPosition, out EmitterSample smpl, inout PathTracePayload payload) {
    smpl.radiance = vec3(0.0);
    smpl.lightDistance = RAY_T_MAX;
    smpl.pdf = 0.0;

    const uint numEmitters = uint(emitters.numEmitters);

    if (numEmitters == 0) {
        return;
    }

    // We add one extra emitters for the sky
    const uint totalSamplableEmitters = numEmitters + USE_SKY_AS_NEE_EMITTER;

    const uint emitterIndex = nextUint(payload.seed, totalSamplableEmitters);

    if (emitterIndex == numEmitters) {
        // Sample the sky as an emitter
        vec3 inLightDirTangent = sampleCosineWeightedHemisphere(payload.samplingNoise);

        smpl.inLightDirWorld = tangentToWorld(tbn, inLightDirTangent);

        smpl.pdf = pdfCosineWeightedHemisphere(max(inLightDirTangent.z, 0)) / totalSamplableEmitters;
        smpl.radiance = getSkyRadiance(smpl.inLightDirWorld);

        if (smpl.radiance == vec3(0.0)) return;

    }

    sampleEmitter(emitterIndex, worldPosition, smpl, payload.seed);
}

/**
 * Power Heuristic for combining multiple sampling strategies.
 * This heuristic is used to balance the contributions of different sampling methods
 * based on their probability density functions (PDFs).
 *
 * The generic power heurisitc is: w_i = pow(pdf_i, beta) / sum(pow(pdf_j, beta))
 *
 * The power heuristic, particularly with beta=2 was extensively studied and empirically shown to be
 * highly effective by Eric Veach in his Ph.D. thesis.
 * While not always strictly "optimal" in a mathematical sense for every single scenario, it provides
 * a very robust and generally well-performing solution across a wide range of rendering situations.
 * @see https://graphics.stanford.edu/papers/veach_thesis/thesis.pdf
 *
 * @param pdfA The PDF of the first sampling method.
 * @param pdfB The PDF of the second sampling method.

 * @return The weight for the first sampling method.
 */
float powerHeuristic(float pdfA, float pdfB) {
    const float pdfASq = pow2(pdfA);
    const float pdfBSq = pow2(pdfB);

    return pdfASq / (pdfASq + pdfBSq);
}

#endif