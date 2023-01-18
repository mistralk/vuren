#ifndef HIT_DATA_H
#define HIT_DATA_H

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

#endif // HIT_DATA_H