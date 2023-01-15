#ifndef SHADER_COMMON_H
#define SHADER_COMMON_H

struct SurfaceHit {
    vec3 worldPos;
    vec3 worldNormal;
    vec2 texCoord;
};

struct HitPayload {
    vec3 hitValue;
};

#endif // SHADER_COMMON_H