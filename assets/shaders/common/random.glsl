#ifndef _RANDOM_
#define _RANDOM_

#include "math.glsl"

/**
 * @brief A pseudo-random number generator based on the Tiny Encryption Algorithm (TEA).
 * @param v0 The first 32-bit integer input.
 * @param v1 The second 32-bit integer input.
 * @return A pseudo-random 32-bit integer hash of the inputs.
 */
uint tea(uint v0, uint v1) {
    uint s0 = 0;
    for (uint n = 0; n < 4; n++) {
        s0 += 0x9e3779b9;
        v0 += ((v1 << 4) + 0xa341316c) ^ (v1 + s0) ^ ((v1 >> 5) + 0xc8013ea4);
        v1 += ((v0 << 4) + 0xad90777d) ^ (v0 + s0) ^ ((v0 >> 5) + 0x7e95761e);
    }
    return v0;
}

uint pcgHash(uint x) {
    uint state = x * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;

    return (word >> 22u) ^ word;
}

float randomFloat(inout uint seed) {
    uint result = pcgHash(seed);

    seed++;

    return float(result) * INV_UINT_MAX;
}

uint nextUint(inout uint seed, uint max) {
    uint result = pcgHash(seed);

    seed++;

    return result % max;
}

vec2 randomVec2(inout uint seed) {
    return vec2(randomFloat(seed), randomFloat(seed));
}

vec3 randomVec3(inout uint seed) {
    return vec3(randomFloat(seed), randomFloat(seed), randomFloat(seed));
}

/**
 * @brief Generates a random 2D blue noise value using a texture array.
 * 
 * The blue noise texture array is expected to be 64x64 pixels, with 64 different textures.
 * Each texture is used to animate the noise over time.
 * 
 * @param pixel The pixel coordinates (not UV) to sample from the blue noise texture.
 * @param frame The current frame index, used to select the appropriate texture slice.
 * @param blue_noise_texture_index The base index of the blue noise texture in the texture array.
 *
 * @return A float2 containing two [0..1] values sampled from the animated blue noise texture.
 *
 * @note Taken from https://github.com/JorenJoestar/DevGames2025/blob/main/source/shaders/devgames_2025/global.h
 *
vec2 animated_blue_noise(uvec2 pixel, uint frame, sampler2DArray textureArray, uint blue_noise_texture_index) {
    // Blue noise texture size is 128x128
    pixel = (pixel % 128);
    // There are 64 blue noise textures
    uint slice = (frame % 64);
    // Read blue noise from texture without filtering.
    // Adding non nearest/point filter is wrong.
    vec2 blue_noise = texture(textureArray[blue_noise_texture_index + slice], pixel).rg;
    
    return blue_noise;
}*/

#endif