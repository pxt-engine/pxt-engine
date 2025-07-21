#ifndef _MATERIAL_DATA_
#define _MATERIAL_DATA_

struct Material {
	vec4 albedoColor;
    vec4 emissiveColor;
	int albedoMapIndex;
	int normalMapIndex;
	int ambientOcclusionMapIndex;
	int metallicMapIndex;
	int roughnessMapIndex;
    int emissiveMapIndex;
};

struct MeshInstanceDescription {
    uint64_t vertexAddress;  
    uint64_t indexAddress;   
    uint materialIndex;
    uint volumeIndex;
    float textureTilingFactor;
    vec4 textureTintColor;
    mat4 objectToWorld;
    mat4 worldToObject;
};

struct Emitter {
    uint instanceIndex;
    uint numberOfFaces;
};

#endif