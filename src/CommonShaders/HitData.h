#ifndef HIT_DATA_H
#define HIT_DATA_H

struct SurfaceHit {
    vec4 worldPos;
    vec3 worldNormal;
    vec3 color; // will be removed
    vec2 texCoord;

    // material info
    vec3 diffuse;
    vec3 textureId;
};

SurfaceHit getInitialValues() {
    SurfaceHit hit;
    hit.worldPos = vec4(0.0, 0.0, 0.0, 0.0);
    hit.worldNormal = vec3(0.0, 0.0, 0.0);
    hit.texCoord = vec2(0.0, 0.0);
    hit.color = vec3(0.0, 0.0, 0.0);
    hit.diffuse = vec3(0.0, 0.0, 0.0);
    return hit;
}

#endif // HIT_DATA_H