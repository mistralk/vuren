struct HitPayload {
    vec3 hitValue;
};

struct IntersectionAttribute {
    vec2 barycentrics;
};

struct SurfaceHit {
    vec3 worldPos;
    vec3 shadingNormal;
    vec2 texCoord;
};

// SurfaceHit getHitData(IntersectionAttribute attribs) {
//     SurfaceHit hit;

//     hit.worldPos = vec3(0.0, 0.0, 0.0);
//     hit.shadingNormal = vec3(0.0, 0.0, 0.0);
//     hit.texCoord = vec2(0.0, 0.0);

//     return hit;
// }