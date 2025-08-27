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
#include "./common/push.glsl"
#include "./common/bindings.glsl"
#include "./common/surface.glsl"
#include "./common/nee.glsl"

layout(location = PathTracePayloadLocation) rayPayloadInEXT PathTracePayload p_pathTrace;


// For triangles, this implicitly receives barycentric coordinates.
hitAttributeEXT vec2 barycentrics;


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
    
    sampleRandomEmitter(surface.tbn, worldPosition, emitterSample, p_pathTrace);

    if (emitterSample.pdf == 0 || emitterSample.radiance == vec3(0.0)) return;

    vec3 transmittance = evaluateTransmittance(emitterSample, worldPosition, p_pathTrace.mediumIndex);

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
        setFlag(p_pathTrace, FLAG_DONE);
        return;
    }
    
    if (isSpecular) {
        setFlag(p_pathTrace, FLAG_SPECULAR);
    }

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
    }

    if (instance.materialIndex == UINT_MAX) {
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
        else if (hasFlag(p_pathTrace, FLAG_SPECULAR)) {
            vec3 prevBouncePos = p_pathTrace.origin - p_pathTrace.direction * p_pathTrace.hitDistance;

            uint emitterIndex = instance.emitterIndex;
            EmitterSample emitterSample;
            sampleEmitter(emitterIndex, prevBouncePos, emitterSample, p_pathTrace.seed);

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
        setFlag(p_pathTrace, FLAG_DONE);
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