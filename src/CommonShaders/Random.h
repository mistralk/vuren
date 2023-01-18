#ifndef RANDOM_H
#define RANDOM_H

const float PI = 3.1415926535897932384626433832795;
const float INV_PI = 1.0 / 3.1415926535897932384626433832795;

// calculate an orthonormal basis vectors where the z-axis is an arbitrary vector n.
void computeOrthonormalBasis(in vec3 n, out vec3 b1, out vec3 b2) {
    b1 = n.x > 0.9f ? vec3(0, 1, 0) : vec3(1, 0, 0);
    b1 -= n * dot(b1, n);
    b1 = normalize(b1);
    b2 = normalize(cross(n, b1));    
}

vec3 getCosHemisphereSample(in vec2 uv, in vec3 normal) {
    vec3 b1, b2;
    computeOrthonormalBasis(normal, b1, b2);

    // refer to PBR 3/E
    float r = sqrt(uv.x);
    float theta = 2.0 * PI * uv.y;
    vec3 v = vec3(r * cos(theta), r * sin(theta), sqrt(max(0.0, 1.0 - uv.x*uv.x)));
    return normalize(b1 * v.x + b2 * v.y + normal * v.z);
}

float getCosHemispherePdf(float cosTheta) {
    return cosTheta * INV_PI;
}

// GPU pseudo-random number generation code is borrowed from 
// "Ray Tracing Gems II - Ch 14.3.4 Random Number Generation". 
// http://www.realtimerendering.com/raytracinggems/rtg2

uint jenkinsHash(uint x) {
    x += x << 10;
    x ^= x >> 6;
    x += x << 3;
    x ^= x >> 11;
    x += x << 15;
    return x;
}

uint initRNG(uvec2 pixel, uvec2 resolution, uint frame) {
    uint rngState = uint(dot(vec2(pixel), vec2(1, resolution.x))) ^ jenkinsHash(frame);
    return jenkinsHash(rngState);
}

float uintToFloat(uint x) {
    return uintBitsToFloat(0x3f800000 | (x >> 9)) - 1.f;
}

uint xorshift(inout uint rngState) {
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return rngState;
}

float rand(inout uint rngState) {
    return uintToFloat(xorshift(rngState));
}

#endif // RANDOM_H