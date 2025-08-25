#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../common/math.glsl"
#include "../common/ray.glsl"
#include "../common/payload.glsl"
#include "../common/geometry.glsl"
#include "../common/random.glsl"
#include "../common/tone_mapping.glsl"
#include "../common/volume.glsl"
#include "../common/material.glsl"
#include "../ubo/global_ubo.glsl"
#include "../material/surface_normal.glsl"
#include "../material/pbr/bsdf.glsl"
#include "../raytracing/common/push.glsl"
#include "sky.glsl"

#define NEE_MAX_BOUNCES 4

layout(set = 1, binding = 0) uniform accelerationStructureEXT TLAS;

layout(set = 2, binding = 0) uniform sampler2D textures[];

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

// --- Payloads ---
layout(location = PathTracePayloadLocation) rayPayloadInEXT PathTracePayload p_pathTrace;
layout(location = VisibilityPayloadLocation) rayPayloadEXT VisibilityPayload p_visibility;

// For triangles, this implicitly receives barycentric coordinates.
hitAttributeEXT vec2 barycentrics;

vec3 getAlbedo(const Material material, const vec2 uv, const vec4 tintColor) {
    const vec3 albedo = texture(textures[nonuniformEXT(material.albedoMapIndex)], uv).rgb;
    return albedo * tintColor.rgb;
}

vec3 getNormal(const Material material, const vec2 uv) {
    return texture(textures[nonuniformEXT(material.normalMapIndex)], uv).rgb;
}

vec3 getEmission(const Material material, const vec2 uv) {
    const vec3 emissive = texture(textures[nonuniformEXT(material.emissiveMapIndex)], uv).rgb;

    // The alpha channels is used as intensity
    return emissive * material.emissiveColor.rgb * material.emissiveColor.a;
}

float getRoughness(const Material material, const vec2 uv) {
    return max(pow2(texture(textures[nonuniformEXT(material.roughnessMapIndex)], uv).r), 0.0001);
}

float getMetalness(const Material material, const vec2 uv) {
    return texture(textures[nonuniformEXT(material.metallicMapIndex)], uv).r;
}

struct EmitterSample {
    vec3 radiance;
    vec3 inLightDirWorld;
    float lightDistance;
    float emitterCosTheta;
    float pdf;
    vec3 normalWorld;
};

SurfaceData getSurfaceData(const MeshInstanceDescription instance, const Material material, const vec2 uv, const mat3 tbn, bool isBackFace) {
    SurfaceData surface;
    surface.tbn = tbn;
    surface.isBackFace = isBackFace;
    surface.albedo = getAlbedo(material, uv, instance.textureTintColor);
    surface.metalness = getMetalness(material, uv);
    surface.roughness = getRoughness(material, uv);
    surface.transmission = material.transmission;
    surface.ior = material.ior;
    surface.reflectance = calculateReflectance(surface.albedo, surface.metalness, surface.transmission, surface.ior);

    return surface;
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
void sampleEmitter(uint emitterIndex, vec3 worldPosition, out EmitterSample smpl) {
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
        
    const uint faceIndex = nextUint(p_pathTrace.seed, emitter.numberOfFaces);

    // Generate barycentric coordinates for the triangle
    vec2 emitterBarycentrics = sampleTrianglePoint(p_pathTrace.seed);
    
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
void sampleRandomEmitter(mat3 tbn, vec3 worldPosition, out EmitterSample smpl) {
    smpl.radiance = vec3(0.0);
    smpl.lightDistance = RAY_T_MAX;
    smpl.pdf = 0.0;

    const uint numEmitters = uint(emitters.numEmitters);

    if (numEmitters == 0) {
        return;
    }
    
    // We add one extra emitters for the sky
    const uint totalSamplableEmitters = numEmitters + USE_SKY_AS_NEE_EMITTER;

    const uint emitterIndex = nextUint(p_pathTrace.seed, totalSamplableEmitters);

    if (emitterIndex == numEmitters) {
        // Sample the sky as an emitter
        vec3 inLightDirTangent = sampleCosineWeightedHemisphere(p_pathTrace.samplingNoise);

        smpl.inLightDirWorld = tangentToWorld(tbn, inLightDirTangent);

        smpl.pdf = pdfCosineWeightedHemisphere(max(inLightDirTangent.z, 0)) / totalSamplableEmitters;
        smpl.radiance = getSkyRadiance(smpl.inLightDirWorld);

        if (smpl.radiance == vec3(0.0)) return;

    }

    sampleEmitter(emitterIndex, worldPosition, smpl);
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

vec3 evaluateTransmittance(EmitterSample emitterSample, vec3 worldPosition) {
    vec3 transmittance = vec3(1.0);

    Ray ray;
    ray.origin = worldPosition;
    ray.direction = emitterSample.inLightDirWorld;

    int currentMediumIndex = p_pathTrace.mediumIndex;

    // we need to keep track of the last surface we hit to know
    // if we are exiting an object inside a volume.
    // think of a donut inside a volume, we need to account
    // for transmittance of the volume in the hole of the donut.
    int currentSurfaceIndex = -1;
    // we also need to know how many times we hit the same surface
    int currentSurfaceHitCount = 0;
    
    float tMax = max(0.0, emitterSample.lightDistance - FLT_EPSILON); 
    int depth = 0;
    for (; depth < NEE_MAX_BOUNCES; depth++) {
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
        bool isInMedium = currentMediumIndex != -1 && !isInsideSurface;

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
        // else we hit a surface
            // if we hit the same surface again, we are inside a transparent object
            // we just need to go through (no refraction is accounted for now).
        else if (currentSurfaceIndex != instanceIndex) {
            currentSurfaceIndex = instanceIndex;
            currentSurfaceHitCount = 1;

            const Material material = materials.m[instance.materialIndex];
            float transmission = material.transmission;

            

            // if the surface is opaque we stop
            if (transmission == 0.0) {
                transmittance = vec3(0.0);
                break;
            }

            // else we 

            const Triangle triangle = getTriangle(instance.indexAddress, instance.vertexAddress, p_visibility.primitiveId);

            const vec2 uv = getTextureCoords(triangle, p_visibility.barycentrics) * instance.textureTilingFactor;

            float metalness = getMetalness(material, uv);
            float roughness = getRoughness(material, uv);

            

            // Even if it's not physically correct we use this as an approximation of how much
            // light reaches the point. Metallic and rough surfaces reflects the light.
            transmittance *= (1.0 - metalness) * (1.0 - roughness) * transmission;
            
        }
        else {
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
 * @brief Calculates the direct illumination at a given surface point using Next Event Estimation (NEE)
 * and Multiple Importance Sampling (MIS).
 *
 * This function is responsible for computing the radiance received directly from light sources (emitters)
 * at a specified surface point. It employs two complementary sampling strategies for direct lighting
 * and combines their contributions using the Power Heuristic to minimize variance.
 *
 * @param surface The SurfaceData containing geometric and material properties of the hit point.
 * @param worldPosition The world-space coordinates of the surface point where direct lighting is being calculated.
 * @param outLightDir The outgoing light direction from the surface point.
 */
void directLighting(SurfaceData surface, vec3 worldPosition, vec3 outLightDir) {
    
    EmitterSample emitterSample;
    
    sampleRandomEmitter(surface.tbn, worldPosition, emitterSample);

    if (emitterSample.pdf == 0 || emitterSample.radiance == vec3(0.0)) return;

    vec3 transmittance = evaluateTransmittance(emitterSample, worldPosition);

    emitterSample.radiance *= transmittance;

    if (emitterSample.radiance == vec3(0.0)) return;

    vec3 inLightDirTangent = worldToTangent(surface.tbn, emitterSample.inLightDirWorld);

    const vec3 halfVector = normalize(outLightDir + inLightDirTangent);
    const float receiverCos = cosThetaTangent(inLightDirTangent);

    float bsdfPdfSolidAngle;

    const vec3 bsdf = evaluateBSDF(surface, outLightDir, inLightDirTangent, halfVector, bsdfPdfSolidAngle);
        
    // Jacobian for PDF conversion from solid angle to area
    const float G_term = emitterSample.emitterCosTheta / pow2(emitterSample.lightDistance);

    // Convert the BSDF PDF from solid angle to area so it can be used in MIS with the NEE pdf
    // that is already in area units.
    const float bsdfPdfarea = bsdfPdfSolidAngle * G_term;
            
    const vec3 contribution = (emitterSample.radiance * bsdf * receiverCos) / emitterSample.pdf;        

    const float weight = powerHeuristic(emitterSample.pdf, bsdfPdfarea);

    p_pathTrace.radiance += contribution * p_pathTrace.throughput * weight;  
}

/**
 * @brief Prepares for the indirect lighting step by sampling a new direction based on the BSDF and updating path state.
 *
 * This function handles the sampling of an outgoing direction for the next segment of an indirect path
 * and applies Russian Roulette for path termination. It's a crucial part of the recursive path tracing process
 * for indirect illumination.
 *
 * @param surface The SurfaceData containing geometric and material properties of the hit point.
 * @param outLightDir The outgoing light direction from the surface point.
 * @param inLightDir The incoming direction for the next bounce, sampled based on the BSDF.
 *                   This will be used to trace the next ray.
 */
void indirectLighting(SurfaceData surface, vec3 outLightDir, out vec3 inLightDir) {
    float pdf;
    bool isSpecular;
    vec3 bsdfMultiplier = sampleBSDF(surface, outLightDir, inLightDir, pdf, isSpecular, p_pathTrace.seed, p_pathTrace.samplingNoise);

    if (bsdfMultiplier == vec3(0.0)) {
        // No contribution from this surface
        p_pathTrace.done = true;
        return;
    }
    
    p_pathTrace.isSpecularBounce = isSpecular;
    p_pathTrace.throughput *= bsdfMultiplier;
    p_pathTrace.pdf = pdf;
}

void main() {
    const MeshInstanceDescription instance = meshInstances.i[gl_InstanceCustomIndexEXT];
    const Triangle triangle = getTriangle(instance.indexAddress, instance.vertexAddress, gl_PrimitiveID);

     // Tangent, Bi-tangent, Normal (TBN) matrix to transform tangent space to world space
    mat3 tbn = calculateTBN(triangle, mat3(instance.objectToWorld), barycentrics);

    const vec3 geometricNormal = tbn[2];

    const bool isBackFace = dot(gl_WorldRayDirectionEXT, geometricNormal) > 0.0;

    // Check if the hit object is a volume boundary
    if (instance.volumeIndex != UINT_MAX) {
        // HIT A VOLUME BOUNDARY
        if (!isBackFace) {
            p_pathTrace.mediumIndex = int(instance.volumeIndex);
        } else {
            // Exiting the volume. We assume it exits into a vacuum.
            // For overlapping/nested volumes, we'd need a medium stack.
            p_pathTrace.mediumIndex = -1;
        }

        // Update ray origin to continue tracing from the hit point.
        // in the case of volumes we need higher precision in the RAY_T_MIN value.
        // Instead of modifying it directly, we offset the origin backwards so that
        // the new ray begins tracing from the new origin + RAY_T_MIN. is the same as
        // setting the RAY_T_MIN to an ESPILON VALUE before tracing for the volumes.
        // In general we dont want smaller RAY_T_MIN values, for performance.
        // Offset slightly to avoid immediate self-intersection.
        p_pathTrace.origin += p_pathTrace.direction * (gl_HitTEXT - RAY_T_MIN + FLT_EPSILON);

        p_pathTrace.done = false;
        return; 
    }

    const Material material = materials.m[instance.materialIndex];

    const vec2 uv = getTextureCoords(triangle, barycentrics) * instance.textureTilingFactor;

    const vec3 surfaceNormal = calculateSurfaceNormal(textures[nonuniformEXT(material.normalMapIndex)], uv, tbn);
    
    //if (surfaceNormal != geometricNormal) {
     //   tbn = createOrthonormalBasis(surfaceNormal);
    //}

    if (isBackFace) {
        tbn[2] *= -1;
        tbn[1] *= -1;
    }

    SurfaceData surface = getSurfaceData(instance, material, uv, tbn, isBackFace);

    const vec3 emission = getEmission(material, uv);

    if (maxComponent(emission) > 0.0) {
        // Add the light's emission to the total radiance if:
        // 1. It's the first hit (the camera sees the light directly).
        // 2. The ray that hit the light came from a reflection / refarction bounce.
        if (p_pathTrace.depth == 0) {
            p_pathTrace.radiance += emission * p_pathTrace.throughput;
        } 
        // we use the previous bounce BSDF pdf to do MIS
        else if (p_pathTrace.isSpecularBounce) {
            vec3 prevBouncePos = p_pathTrace.origin - p_pathTrace.direction * p_pathTrace.hitDistance;

            uint emitterIndex = instance.emitterIndex;
            EmitterSample emitterSample;
            sampleEmitter(emitterIndex, prevBouncePos, emitterSample);

            // Jacobian for PDF conversion from solid angle to area
            const float G_term = emitterSample.emitterCosTheta / pow2(emitterSample.lightDistance);

            // Convert the BSDF PDF from solid angle to area so it can be used in MIS
            const float bsdfPdfarea = p_pathTrace.pdf * G_term;

            float misWeight = 1.0;

            if (p_pathTrace.pdf > FLT_EPSILON) {
                misWeight = powerHeuristic(bsdfPdfarea, emitterSample.pdf);
            }

            p_pathTrace.radiance += emission * p_pathTrace.throughput * misWeight;
        }
        
        // The path ends at the light source.
        p_pathTrace.done = true;
        return;
    }
    
    const vec3 worldPosition = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_RayTmaxEXT;

    // The ray has the opposite direction to that of the light.
    // The "incoming" ray hitting the surface is actually the outgoing light direction.
    // We perform calculation in tangent space
    vec3 outgoingLightDirection = worldToTangent(tbn, -gl_WorldRayDirectionEXT);
    vec3 incomingLightDirection;

    // Calculate the probabilities for the surface properties for the bsdf
    calculateProbabilities(surface, outgoingLightDirection);

    directLighting(surface, worldPosition, outgoingLightDirection);
    indirectLighting(surface, outgoingLightDirection, incomingLightDirection);    

    // Convert back to world space
    outgoingLightDirection = tangentToWorld(tbn, incomingLightDirection);

    // we offset the origin slightly to avoid self-intersection based on
    // reflection (negative) or refraction (positive).
    const float offsetSign = sign(dot(outgoingLightDirection, geometricNormal));

    // Update the payload for the next bounce
    p_pathTrace.depth++;
    p_pathTrace.origin = worldPosition + geometricNormal * offsetSign * (FLT_EPSILON);
    p_pathTrace.direction = outgoingLightDirection;
    p_pathTrace.hitDistance = gl_HitTEXT;
}