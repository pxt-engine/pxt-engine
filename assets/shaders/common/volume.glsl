#ifndef _VOLUME_
#define _VOLUME_

struct Volume {
    vec4 absorption;
    vec4 scattering;
    // Henyey-Greenstein phase function parameter [-1.0, 1.0].
    // phaseFunctionG = 0.0 for isotropic scattering
    // phaseFunctionG > 0.0 for forward scattering
    // phaseFunctionG < 0.0 for backward scattering
    float phaseFunctionG;
    uint densityTextureId;
    uint detailTextureId; // for edge details of the volume
    uint instanceIndex;
};

// Finds the intersection points with a bounding box along the given ray.
// Returns a vec2 with tNear (distance along the ray to the near intersection),
// and tFar (distance to the far intersection).
// Interpretation of the intersection result:
// No Intersection:
//   - If tNear > tFar: The ray completely missed the box. This occurs if the entry point along one axis is beyond the exit point along another.
//   - If tFar < 0: The AABB is entirely behind the ray's origin.
// Intersection:
//   - If tNear <= tFar && tFar >= 0: A valid intersection occurred.
//     - If tNear < 0: The ray's origin is inside the AABB. The effective entry point is at t=0 (the ray's origin), and tFar is the exit point.
//     - If tNear >= 0: The ray enters the AABB at tNear and exits at tFar.
vec2 intersectAABB(vec3 rayOrigin, vec3 rayDir, vec3 boxMin, vec3 boxMax) {
    vec3 tMin = (boxMin - rayOrigin) / rayDir;
    vec3 tMax = (boxMax - rayOrigin) / rayDir;
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);
    return vec2(tNear, tFar);
}

float evalHenyeyGreenstein(float cosTheta, float g) {
    float denom = 1 + pow2(g) + 2 * g * cosTheta;
    return (1 - pow2(g)) / (4.0 * PI * pow(denom, 1.5));
}

// Henyey-Greenstein phase function sampling.
// Returns a new direction in world space
vec3 sampleHenyeyGreenstein(vec3 wo, float g, inout uint seed) {
    float rand1 = randomFloat(seed);
    float rand2 = randomFloat(seed);

    // To prevent numeric instability when g approx -1 or 1
    clamp(g, -0.99, 0.99);

    float cosTheta;

    // To prevent numeric instability when g approx 0
    if (abs(g) < 1e-4) {
        // Isotropic scattering
        cosTheta = 1.0 - 2.0 * rand1;
    } else {
        cosTheta = -1.0 / (2.0 * g) * (1.0 + pow2(g) - pow2((1 - pow2(g)) / (1.0 + g - 2.0 * g * rand1)));
    }

    float sinTheta = sqrt(max(0.0, 1.0 - pow2(cosTheta)));
    float phi = 2.0 * PI * rand2;

    // Create a local coordinate system
    vec3 w = wo;
    vec3 u = (abs(w.x) > 0.1) ? vec3(0, 1, 0) : vec3(1, 0, 0);
    u = normalize(cross(u, w));
    vec3 v = cross(w, u);

    return normalize(u * cos(phi) * sinTheta + v * sin(phi) * sinTheta + w * cosTheta);
}

#endif