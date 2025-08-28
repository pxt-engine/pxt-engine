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

EmitterSample sampleEmitterAt(uint emitterIndex, uint faceIndex, vec2 barycentrics, vec3 worldPosition) {
    EmitterSample smpl;
    smpl.radiance = vec3(0.0);
    smpl.lightDistance = RAY_T_MAX;
    smpl.pdf = 0.0;

    const Emitter emitter = emitters.e[emitterIndex];
    const MeshInstanceDescription instance = meshInstances.i[emitter.instanceIndex];
    const Material material = materials.m[instance.materialIndex];

    const Triangle triangle = getTriangle(instance.indexAddress, instance.vertexAddress, faceIndex);
    const vec2 uv = getTextureCoords(triangle, barycentrics) * instance.textureTilingFactor;

    smpl.radiance = getEmission(material, uv);

    if (smpl.radiance == vec3(0.0)) {
        return smpl;
    }

    const vec3 emitterObjPosition = getPosition(triangle, barycentrics);
    const vec3 emitterObjNormal = getNormal(triangle, barycentrics);

    const mat4 emitterObjectToWorld = mat4(instance.objectToWorld);
    // The upper 3x3 of the world-to-object matrix is the normal matrix
    const mat3 emitterNormalMatrix = mat3(instance.worldToObject);

    const vec3 emitterPosition = vec3(emitterObjectToWorld * vec4(emitterObjPosition, 1.0));
    const vec3 emitterNormal = normalize(emitterNormalMatrix * emitterObjNormal);

    // vector from emitter the surface to the emitter
    vec3 outLightVec = worldPosition - emitterPosition;

    smpl.lightDistance = length(outLightVec);

    const float areaWorld = calculateWorldSpaceTriangleArea(triangle, mat3(instance.objectToWorld));

    if (areaWorld <= 0.0 || smpl.lightDistance <= 0) {
        return smpl;
    }

    vec3 outLightDir = outLightVec / smpl.lightDistance;

    smpl.emitterCosTheta = cosTheta(emitterNormal, outLightDir);
    if (smpl.emitterCosTheta <= 0.0) {
        return smpl;
    }

    const uint numEmitters = uint(emitters.numEmitters);
    const uint totalSamplableEmitters = numEmitters + USE_SKY_AS_NEE_EMITTER;

    smpl.inLightDirWorld = -outLightDir;
    smpl.pdf = pow2(smpl.lightDistance) / (smpl.emitterCosTheta * areaWorld * totalSamplableEmitters * emitter.numberOfFaces);

    return smpl;
}

vec3 evaluateTransmittance(inout EmitterSample emitterSample, vec3 worldPosition, int currentMediumIndex) {
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
        bool isInMedium = currentMediumIndex != -1 && isInsideSurface;

        if (isInMedium) {
            const Volume volume = volumes.volumes[currentMediumIndex];

            // We have travelled the medium from start to finish
            // we need to account for the absorption
            vec3 sigma_t = volume.absorption.rgb + volume.scattering.rgb;
            if (maxComponent(sigma_t) > 0.0) {
                // TODO: support for heterogeneous media
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

            const Triangle triangle = getTriangle(instance.indexAddress, instance.vertexAddress, p_visibility.primitiveId);

            const vec2 uv = getTextureCoords(triangle, p_visibility.barycentrics) * instance.textureTilingFactor;

            vec3 emission = getEmission(material, uv);

            // We have encountered an emitter during the visibility for the provided emitter
            // We update the emitterSample using the current hit and return the accumulated
            // transmittance which correspond to the transmittance for the found emitter.
            if (maxComponent(emission) > 0.0) {
				uint emitterIndex = instance.emitterIndex;

				emitterSample = sampleEmitterAt(emitterIndex, p_visibility.primitiveId,
                                                p_visibility.barycentrics, worldPosition);

                return transmittance;
            }

            float transmission = material.transmission;

            // if the surface is opaque we stop
            if (transmission == 0.0) {
                transmittance = vec3(0.0);
                break;
            }

            vec3 albedo = getAlbedo(material, uv, instance.textureTintColor);
            float metalness = getMetalness(material, uv);
            float roughness = getRoughness(material, uv);

            // Even if it's not physically correct we use this as an approximation of how much
            // light reaches the point. Metallic and rough surfaces reflect the light.
            // Note: we multiply by albedo and not sqrt(albedo) because the second time
            //       we touch the same transmissive object (on exit - in the else branch) we do no operations.
            transmittance *= (1.0 - metalness) * (1.0 - roughness) * transmission * albedo;

        } else {
            currentSurfaceHitCount++;
        }

        // Prepare for the next segment of the ray
        float offset = p_visibility.hitDistance;
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
    // Sample a mesh emitter
    const Emitter emitter = emitters.e[emitterIndex];
    const MeshInstanceDescription emitterInstance = meshInstances.i[emitter.instanceIndex];
    const Material material = materials.m[emitterInstance.materialIndex];
        
    const uint faceIndex = nextUint(seed, emitter.numberOfFaces);

    // Generate barycentric coordinates for the triangle
    vec2 barycentrics = sampleTrianglePoint(seed);

    smpl = sampleEmitterAt(emitterIndex, faceIndex, barycentrics, worldPosition);
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