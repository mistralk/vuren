#ifndef SHADER_COMMON_H
#define SHADER_COMMON_H

struct SurfaceHit {
    vec3 worldPos;
    vec3 worldNormal;
    vec2 texCoord;
};

SurfaceHit getInitialValues() {
    SurfaceHit hit;
    hit.worldPos = vec3(0.0, 0.0, 0.0);
    hit.worldNormal = vec3(0.0, 0.0, 0.0);
    hit.texCoord = vec2(0.0, 0.0);
    return hit;
}

// struct HitPayload {
//     vec3 hitValue;
// };

#endif // SHADER_COMMON_H