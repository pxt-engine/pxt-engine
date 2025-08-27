#ifndef _SURFACE_RT_
#define _SURFACE_RT_

#include "bindings.glsl"
#include "../../common/math.glsl"
#include "../../common/material.glsl"
#include "../../material/pbr/bsdf.glsl"

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
	float roughness = material.roughness;

    if (material.roughnessMapIndex != UINT_MAX) {
        roughness = texture(textures[nonuniformEXT(material.roughnessMapIndex)], uv).r
            * material.roughness;
	}

	return clamp(pow2(roughness), 0.0001, 1.0);
}

float getMetalness(const Material material, const vec2 uv) {
	float metallic = material.metallic;
	
    if (material.metallicMapIndex != UINT_MAX) {
        metallic = texture(textures[nonuniformEXT(material.metallicMapIndex)], uv).r
            * material.metallic;
    }

	return clamp(metallic, 0.0, 1.0);
}

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

#endif