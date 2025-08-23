#ifndef _BSDF_
#define _BSDF_

#include "../../common/math.glsl"

struct SurfaceData {
    mat3 tbn;
    bool isBackFace;
    vec3 albedo;
    float metalness;
    float roughness;
    float ior;
    float transmission;

    vec3 reflectance;

	float diffuseWeight;
	float metalWeight;
	float transmissionWeight;

    float diffuseProbability;
    float metalProbability;
    float transmissionProbability;
};

/**
 * @brief Calculates the luminance of a given color.
 *
 * This function converts an RGB color to a single luminance value,
 * which can be useful for weighting or perceptual calculations.
 * Uses standard ITU-R BT.709 coefficients.
 *
 * @param color The input vec3 color in RGB space.
 * @return The luminance value of the color.
 */
float luminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

float schlickWeight(float cosTheta) {
    float m = clamp(1.0 - cosTheta, 0.0, 1.0);
    return pow5(m);
}

/**
 * @brief Calculates the probability of sampling a specular lobe versus a diffuse lobe or a transmission lobe.
 *
 * This probability is used in hybrid sampling techniques to determine whether
 * to sample the specular, diffuse or transmission component of a BSDF. It's based on
 * the relative energy of the specular and diffuse reflections.
 *
 * @param surface A SurfaceData struct containing material properties (reflectance, albedo, metalness, transmission).
 */
void calculateProbabilities(inout SurfaceData surface, vec3 outLightDir) {
    float diffuseWeight = (1.0 - surface.metalness) * (1.0 - surface.transmission);
    float metalWeight = surface.metalness;
    float transmissionWeight = (1.0 - surface.metalness) * surface.transmission;
    float schlick = schlickWeight(outLightDir.z);

	float diffuseProbability = diffuseWeight * luminance(surface.albedo);
	float metalProbability = metalWeight * luminance(mix(surface.albedo, vec3(1.0), schlick));
	float transmissionProbability = transmissionWeight;

	const float totalWeight = diffuseProbability + metalProbability + transmissionProbability;

    if (totalWeight > 0.0) {
		surface.diffuseWeight = diffuseWeight;
		surface.metalWeight = metalWeight;
		surface.transmissionWeight = transmissionWeight;

        surface.diffuseProbability = diffuseProbability / totalWeight;
		surface.metalProbability = metalProbability / totalWeight;
		surface.transmissionProbability = transmissionProbability / totalWeight;
    } else {
        surface.diffuseWeight = 1.0;
		surface.metalWeight = 0.0;
		surface.transmissionWeight = 0.0;

		surface.diffuseProbability = 1.0;
		surface.metalProbability = 0.0;
		surface.transmissionProbability = 0.0;
    }
}

/**
 * @brief Calculates the base reflectance (F0) of a material, accounting for transmission.
 *
 * For opaque dielectrics, F0 is a fixed value (0.04).
 * For transmissive dielectrics, F0 is calculated from the Index of Refraction (IOR).
 * For metals, F0 is equal to the albedo color.
 * This function interpolates between these states based on the metalness and transmission parameters.
 *
 * @param albedo The albedo color of the material.
 * @param metalness The metalness of the material (0.0 for dielectric, 1.0 for metal).
 * @param transmission The transmission of the material (0.0 for opaque, 1.0 for fully transmissive).
 * @param ior The Index of Refraction of the material, used for transmissive objects.
 * @return The Fresnel F0 (reflectance at normal incidence) as a vec3 color.
 */
vec3 calculateReflectance(vec3 albedo, float metalness, float transmission, float ior) {
    // Calculate F0 for dielectrics using the Fresnel equation with the given IOR.
    // This is physically accurate for transmissive materials like glass or water.
    // Assumes the surrounding medium is air (IOR = 1.0).
    float f = pow2((1.0 - ior) / (1.0 + ior));
    vec3 F0_from_ior = vec3(f);

    // For opaque objects (transmission = 0), use the standard dielectric F0 of 0.04.
    // For transmissive objects (transmission > 0), use the IOR-based calculation.
    vec3 F0_dielectric = transmission == 0.0 ? vec3(0.04) : F0_from_ior;

    // Metals are opaque (not transmissive) and their F0 is their albedo.
    // The final F0 is an interpolation between the calculated dielectric F0 and the
    // metallic F0 (albedo) based on the material's metalness.
    return lerp(F0_dielectric, albedo, metalness);
}

/**
 * @brief Samples a direction from a cosine-weighted hemisphere.
 *
 * This function generates a random direction on the hemisphere above the surface normal,
 * with a probability distribution proportional to the cosine of the angle with the normal.
 * This is suitable for diffuse reflection.
 *
 * @param u A vec2 with two uniform random numbers in [0,1) for sampling.
 * @return A vec3 representing the sampled direction in tangent space, where Z is the normal.
 */
vec3 sampleCosineWeightedHemisphere(vec2 u) {
    const float r = sqrt(u.x);
    const float theta = TWO_PI * u.y;

    // disk sample
    const vec2 d = r * vec2(cos(theta), sin(theta));

    return vec3(d.x, d.y, sqrt(1.0 - d.x * d.x - d.y * d.y));
}

/**
 * @brief Evaluates the GGX Normal Distribution Function (NDF).
 *
 * The GGX NDF describes the distribution of microfacet normals on a surface.
 * Higher values indicate more microfacets aligned with the half-vector.
 *
 * @param NoH The cosine of the angle between the surface normal and the half-vector (N dot H).
 * @param roughness The roughness parameter of the material (0.0 for smooth, 1.0 for rough).
 * @return The value of the GGX NDF at the given angle and roughness.
 */
float D_GGX(float NoH, float roughness) {
    const float a2 = pow2(roughness);
    const float d = pow2(NoH) * (a2 - 1.0) + 1.0;

    return a2 / (PI * pow2(d));
}

/**
 * @brief Evaluates the Schlick-GGX Geometric Shadowing function for a single direction.
 *
 * This function approximates the shadowing and masking effects of microfacets for
 * a single direction (incoming or outgoing). It is a component of the Smith G function.
 *
 * @param cosTheta The cosine of the angle between the surface normal and the light/view direction.
 * @param roughness The roughness parameter of the material.
 * @return The value of the Schlick-GGX G1 term.
 */
float G_Schlick_GGX(float cosTheta, float roughness) {
    const float r = (pow2(roughness) + 1.0);
    const float k = pow2(r) / 8.0; // k for direct lighting

    return cosTheta / (cosTheta * (1.0 - k) + k);
}

/**
 * @brief Evaluates the Smith Geometric Shadowing function with the Schlick-GGX approximation.
 *
 * The Smith G function accounts for shadowing and masking of microfacets,
 * preventing light from reaching or leaving parts of the surface. It is the
 * product of G1 terms for incoming and outgoing directions.
 *
 * @param NoO The cosine of the angle between the surface normal and the outgoing direction.
 * @param NoI The cosine of the angle between the surface normal and the incoming direction.
 * @param roughness The roughness parameter of the material.
 * @return The value of the Smith G function.
 */
float G_Smith(float NoO, float NoI, float roughness) {
    return G_Schlick_GGX(NoO, roughness) * G_Schlick_GGX(NoI, roughness);
}

/**
 * @brief Evaluates the Schlick's approximation for the Fresnel equation.
 *
 * The Fresnel equation describes how the reflectance of light changes with the
 * angle of incidence. Schlick's approximation provides a good balance between
 * accuracy and performance.
 *
 * @param f0 The Fresnel reflectance at normal incidence (F0).
 * @param HoO The cosine of the angle between the half-vector and the outgoing direction.
 * @return The Fresnel term as a vec3 color.
 */
vec3 F_Schlick(vec3 f0, float HoO) {
    return f0 + (vec3(1.0) - f0) * pow5(1.0 - HoO);
}

float DielectricFresnel(float cosThetaI, float eta)
{
    float sinThetaTSq = eta * eta * (1.0f - cosThetaI * cosThetaI);

    // Total internal reflection
    if (sinThetaTSq > 1.0)
        return 1.0;

    float cosThetaT = sqrt(max(1.0 - sinThetaTSq, 0.0));

    float rs = (eta * cosThetaT - cosThetaI) / (eta * cosThetaT + cosThetaI);
    float rp = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);

    return 0.5f * (rs * rs + rp * rp);
}

/**
 * @brief Calculates the PDF for the GGX Normal Distribution Function.
 *
 * This is the probability density function for sampling a microfacet normal
 * based on the GGX distribution.
 *
 * @param NoH The cosine of the angle between the surface normal and the half-vector (N dot H).
 * @param roughness The roughness parameter of the material.
 * @return The PDF value for the GGX NDF.
 */
float pdfD_GGX(float NoH, float roughness) {
    return D_GGX(NoH, roughness) * NoH;
}

/**
 * @brief Calculates the PDF for a cosine-weighted hemisphere distribution.
 *
 * This is the probability density function for sampling directions on a hemisphere
 * with a cosine distribution, suitable for diffuse reflection.
 *
 * @param cosTheta The cosine of the angle between the sampled direction and the normal.
 * @return The PDF value for the cosine-weighted hemisphere.
 */
float pdfCosineWeightedHemisphere(float cosTheta) {
    return cosTheta * INV_PI;
}

/**
 * @brief Importance samples a microfacet normal based on the GGX distribution.
 *
 * This function generates a random half-vector that is distributed according to
 * the GGX NDF, which is efficient for sampling specular reflections.
 * The generated vector is in tangent space, where Z is the normal.
 *
 * @param r A vec2 with two uniform random numbers in [0,1) for sampling.
 * @param roughness The roughness parameter of the material.
 * @return A vec3 representing the importance sampled half-vector in tangent space.
 */
vec3 importanceSampleGGX(vec2 r, float roughness) {
    const float alpha2 = pow2(roughness);
    const float phi = TWO_PI * r.x;
    const float cosTheta = sqrt((1.0 - r.y) / (1.0 + (alpha2 - 1.0 + FLT_EPSILON) * r.y));
    const float sinTheta = sqrt(1.0 - pow2(cosTheta));

    return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

vec3 evaluateDiffuse(SurfaceData surface, vec3 outLightDir, vec3 inLightDir, vec3 halfVector, out float pdf) {
	const float NoI = cosThetaTangent(inLightDir);
    const float HoO = cosTheta(halfVector, outLightDir);

    const vec3  F = F_Schlick(surface.reflectance, HoO);

    pdf = pdfCosineWeightedHemisphere(NoI);

    return surface.albedo * INV_PI;
}

/**
 * @brief Evaluates the PBR BRDF (Bidirectional Reflectance Distribution Function).
 *
 * This function combines the diffuse (Lambertian) and specular (GGX) components
 * of the BRDF to calculate the outgoing radiance for a given incoming light direction.
 *
 * @param surface A SurfaceData struct containing material properties (normal, albedo, roughness, metalness, reflectance).
 * @param outLightDir The outgoing (view) direction from the surface point.
 * @param inLightDir The incoming light direction (towards the light).
 * @param halfVector The half-vector between the outgoing and incoming light directions.
 * @return The BRDF value as a vec3 color.
 */
vec3 evaluateReflectance(SurfaceData surface, vec3 outLightDir, vec3 inLightDir, vec3 halfVector, vec3 F, out float pdf) {
    const float NoH = cosThetaTangent(halfVector);
    const float NoI = cosThetaTangent(inLightDir);
    const float NoO = cosThetaTangent(outLightDir);

    // if we are inside an object and reflect we disable this
    //if (NoO < 0.0) { 
    //    return vec3(0.0);
    //}

    const float HoO = cosTheta(halfVector, outLightDir);
	const float IoH = cosTheta(inLightDir, halfVector);

    // G1 and G2 factor for geometry term
    const float G1 = G_Schlick_GGX(NoO, surface.roughness);
    const float G2 = G_Schlick_GGX(NoI, surface.roughness);

    const float D = D_GGX(NoH, surface.roughness);
    const float G = G1 * G2;
    //const vec3  F = F_Schlick(surface.reflectance, HoO);

    const float specularDenominator = 4.0 * NoO * NoI + FLT_EPSILON;

    pdf = D * NoH / max(4.0 * IoH, FLT_EPSILON);

    return D * F * G / specularDenominator;
}

/**
 * @brief Evaluates the PBR BTDF (Bidirectional Transmittance Distribution Function).
 *
 * @param surface A SurfaceData struct containing material properties (normal, albedo, roughness, metalness, reflectance).
 * @param outLightDir The outgoing (view) direction from the surface point.
 * @param inLightDir The incoming light direction (towards the light).
 * @param halfVector The half-vector between the outgoing and incoming light directions.
 * @return The BTDF value as a vec3 color.
 */
vec3 evaluateTransmittance(SurfaceData surface, vec3 outLightDir, vec3 inLightDir, vec3 halfVector, vec3 F, out float pdf) {
    const float NoH = cosThetaTangent(halfVector);
    const float NoI = cosThetaTangent(inLightDir);
    const float NoO = cosThetaTangent(outLightDir);

    const float HoO = cosTheta(halfVector, outLightDir);
    const float HoI = cosTheta(halfVector, inLightDir);
    
    // G1 and G2 factor for geometry term
    const float G1 = G_Schlick_GGX(NoO, surface.roughness);
    const float G2 = G_Schlick_GGX(NoI, surface.roughness);

    float eta = NoO > 0.0 ? 1.0 / surface.ior : surface.ior;

    const float D = D_GGX(NoH, surface.roughness);
    const float G = G1 * G2;
    //float F = DielectricFresnel(abs(HoO), eta);

    float denom = pow2(HoI + HoO * eta);
    float eta2 = pow2(eta);
    float jacobian = abs(HoI) / denom;

    pdf = G1 * max(0.0, HoO) * D * jacobian / NoO;

    return pow(surface.albedo, vec3(0.5)) * (1.0 - F) * D * G * abs(HoO) * jacobian * eta2 / abs(NoI * NoO);
}

/**
 * @brief Evaluates the PBR BSDF (Bidirectional Scattering Distribution Function).
 *
 * This function handles both reflection (BRDF) and transmission (BTDF) for a material.
 * It determines whether to calculate reflection or refraction based on the direction of
 * the light vectors relative to the surface normal.
 */
vec3 evaluateBSDF(SurfaceData surface, vec3 outLightDir, vec3 inLightDir, vec3 halfVector, out float pdf) {

    vec3 totalEval = vec3(0.0);
    float tempPdf = 0.0;
    pdf = 0.0;

    float NoO = cosThetaTangent(outLightDir);
    float NoI = cosThetaTangent(inLightDir);
    float HoO = abs(cosTheta(halfVector, outLightDir));

    // If NoI and NoO have the same sign, both vectors are in the same hemisphere in respect
    // to the normal -> Reflection.
    bool isReflection = NoI * NoO > 0.0;

    if (surface.diffuseProbability > 0.0 && isReflection) {
        totalEval += evaluateDiffuse(surface, outLightDir, inLightDir, halfVector, tempPdf)* surface.diffuseWeight;

        pdf += tempPdf * surface.diffuseProbability;
    }

    if (surface.metalProbability > 0.0 && isReflection) {
        vec3 F = F_Schlick(surface.reflectance, HoO);

        totalEval += evaluateReflectance(surface, outLightDir, inLightDir, halfVector, F, tempPdf)
            * surface.metalWeight;

        pdf += tempPdf * surface.metalProbability;
    }

    if (surface.transmissionProbability > 0.0) {

        float eta = NoO > 0.0 ? 1.0 / surface.ior : surface.ior;
        float F = DielectricFresnel(HoO, eta);

        if (isReflection) {
            // Reflection case
            totalEval += evaluateReflectance(surface, outLightDir, inLightDir, halfVector, vec3(F), tempPdf)
                * surface.transmissionWeight;

            pdf += tempPdf * surface.transmissionProbability * F;
        } else {
            // Transmission case
            totalEval += evaluateTransmittance(surface, outLightDir, inLightDir, halfVector, vec3(F), tempPdf)
                * surface.transmissionWeight;

			pdf += tempPdf * surface.transmissionProbability * (1.0 - F);
        }
    }

	return totalEval;
}

/**
 * @brief Samples a direction from the BSDF (Bidirectional Scattering Distribution Function).
 *
 * This function randomly chooses between sampling the specular or diffuse lobe
 * based on the calculated specular probability and returns a sampled incoming
 * light direction. It also outputs the PDF of the chosen sample and the
 * evaluated BRDF for that direction.
 *
 * @param surface A SurfaceData struct containing material properties.
 * @param outLightDir The outgoing (view) direction from the surface point.
 * @param inLightDir Output: The sampled incoming light direction.
 * @param pdf Output: The PDF of the sampled direction.
 * @param seed Inout: A seed for the random number generator, updated by the function.
 * @return The evaluated BSDF value for the sampled direction, weighted by cosine and inverse PDF.
 *
 * @note implemented looking partially at https://cseweb.ucsd.edu/~tzli/cse272/wi2023/homework1.pdf
 */
vec3 sampleBSDF(SurfaceData surface, vec3 outLightDir, out vec3 inLightDir, out float pdf, out bool isSpecular, inout uint seed, vec2 samplingNoise) {
    // Half vector between the outgoing light direction and the incoming light direction
    vec3 halfVector;

    float rand = randomFloat(seed);

    isSpecular = false;

    // calculate cdf to sample specular, diffuse or transmission
    float cdf[2];
    cdf[0] = surface.metalProbability; // Specular
    cdf[1] = cdf[0] + surface.transmissionProbability; // Specular + Transmission
	//cdf[2] = cdf[1] + surface.diffuseProbability; // Specular + Transmission + Diffuse = 1.0

    if (rand < cdf[0]) {
        // Sample specular reflection
        halfVector = importanceSampleGGX(randomVec2(seed), surface.roughness);
        inLightDir = reflect(-outLightDir, halfVector);
        isSpecular = true;
    } else if (rand < cdf[1]) {
        // Sample specular reflection
        halfVector = importanceSampleGGX(randomVec2(seed), surface.roughness);
        float eta = outLightDir.z > 0.0 ? 1.0 / surface.ior : surface.ior;
        float F = DielectricFresnel(abs(dot(outLightDir, halfVector)), eta);
        
        // We don't want to generate a new random number because it breaks the
        // stratification property. We can remap the random number to reuse it.
        float newRand = (rand - cdf[0]) / (cdf[1] - cdf[0]);

        if (newRand < F) {
            // Reflect
            inLightDir = reflect(-outLightDir, halfVector);
        } else {
            // Refract
            inLightDir = refract(-outLightDir, halfVector, eta);
        }
        
        isSpecular = true;
    } else { // cdf[2] = 1.0
        // Sample diffuse reflection
        inLightDir = sampleCosineWeightedHemisphere(randomVec2(seed));
        halfVector = normalize(outLightDir + inLightDir);
    }

    const float cosTheta = abs(inLightDir.z);

    const vec3 bsdf = evaluateBSDF(surface, outLightDir, inLightDir, halfVector, pdf);

    if (pdf < FLT_EPSILON) {
        return vec3(1.0, 0.0, 1.0);
    }

    return bsdf * cosTheta / pdf;
}

#endif