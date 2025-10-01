#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../common/math.glsl"
#include "../common/ray.glsl"
#include "../common/geometry.glsl"
#include "../common/material.glsl"
#include "../ubo/global_ubo.glsl"
#include "../material/surface_normal.glsl"
#include "../lighting/blinn_phong_lighting.glsl"
#include "../common/volume.glsl"
#include "./common/surface.glsl"
#include "./common/bindings.glsl"


// Define the ray payload structure. Must match the raygen and miss shaders.
layout(location = 0) rayPayloadInEXT struct RayPayload {
    vec4 color; // The color accumulated along the ray
    float t;    // The hit distance (t-value)
} payload;

// payload used for shadow calculations
layout(location = 1) rayPayloadEXT bool isShadowed;


// Define the ray attributes structure.
// layout(location = 0) hitAttributeEXT is used to receive attributes from the intersection.
// For triangles, this implicitly receives barycentric coordinates.
hitAttributeEXT vec2 HitAttribs;

void main()
{
    MeshInstanceDescription instance = meshInstances.i[gl_InstanceCustomIndexEXT];

    IndexBuffer indices = IndexBuffer(instance.indexAddress);
    VertexBuffer vertices = VertexBuffer(instance.vertexAddress);
    Material material = materials.m[instance.materialIndex];

    // Retrieve the indices of the triangle being hit.
    uint i0 = indices.i[gl_PrimitiveID * 3 + 0];
    uint i1 = indices.i[gl_PrimitiveID * 3 + 1];
    uint i2 = indices.i[gl_PrimitiveID * 3 + 2];

    // Retrieve the vertices of the triangle using the indices.
    Vertex v0 = vertices.v[i0];
    Vertex v1 = vertices.v[i1];
    Vertex v2 = vertices.v[i2]; 
    // Interpolate the vertex attributes using barycentric coordinates.
    const vec4 position = barycentricLerp(v0.position, v1.position, v2.position, HitAttribs);
    const vec4 objectNormal = barycentricLerp(v0.normal, v1.normal, v2.normal, HitAttribs);
    const vec4 objectTangent = barycentricLerp(v0.tangent, v1.tangent, v2.tangent, HitAttribs);
    const vec2 uv = barycentricLerp(v0.uv.xy, v1.uv.xy, v2.uv.xy, HitAttribs) * instance.textureTilingFactor;

    // Normal Matrix (or Model-View Matrix) used to trasform from object space to world space.
    // its just the inverse of the gl_ObjectToWorld3x4EXT
    const mat3 normalMatrix = transpose(mat3(gl_WorldToObjectEXT));
    vec3 worldNormal = normalize(normalMatrix * objectNormal.xyz);

    // Calculate the Tangent-Bitangent-Normal (TBN) matrix and the surface normal in world space.
    const mat3 TBN = calculateTBN(objectNormal, objectTangent, normalMatrix);
    const vec3 surfaceNormal = calculateSurfaceNormal(textures[nonuniformEXT(material.normalMapIndex)], uv, TBN);

    // calculate world position
    const vec3 worldPosition = vec3(gl_ObjectToWorldEXT * position);

    // compute diffuse and specular
    vec3 specularLight, diffuseLight;
    const vec3 viewDirection = -normalize(gl_WorldRayDirectionEXT);
    float specularIntensity = material.blinnPhongSpecularIntensity;
    float specularShininess = material.blinnPhongSpecularShininess;
    computeBlinnPhongLighting(surfaceNormal, viewDirection, worldPosition, specularIntensity, specularShininess, diffuseLight, specularLight);

    vec3 albedo = getAlbedo(material, uv, instance.textureTintColor);

    // shadow
    float attenuation = 1.0;

    // Tracing shadow ray only if the light is visible from the surface
    vec3 lightPosition = ubo.pointLights[0].position.xyz;
    vec3 vecToLight = lightPosition - worldPosition;
    float lightDistance = length(vecToLight);
    vec3 dirToLight = normalize(vecToLight);

    // We assume by default that everything is in shadow
    // then we cast a ray and set to false if the ray misses
    isShadowed = true; 

    if(dot(worldNormal, dirToLight) > 0) {
        // A small bias to avoid the shadow terminator problem
        const float bias = 0.0005;

        Ray shadowRay;
        shadowRay.origin = worldPosition + worldNormal * bias;
        shadowRay.direction = dirToLight;
        float tMin   = RAY_T_MIN;
        float tMax   = lightDistance - FLT_EPSILON;
        uint  flags  = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
        
        traceRayEXT(TLAS,        // acceleration structure
                    flags,       // rayFlags
                    0xFF,        // cullMask
                    0,           // sbtRecordOffset
                    0,           // sbtRecordStride
                    1,           // missIndex
                    shadowRay.origin,      
                    tMin,        // ray min range
                    shadowRay.direction,      
                    tMax,        // ray max range
                    1            // payload (location = 1)
        );
    }

    if(isShadowed) {
        attenuation = 0.4;
    }

    // Apply lighting
    //TODO: specualr is broken, because of something? maybe normals????
    vec3 finalColor = (diffuseLight + vec3(0.0)) * albedo * attenuation;
    
    payload.color = vec4(finalColor, 1.0);
    payload.t = gl_HitTEXT;
}
