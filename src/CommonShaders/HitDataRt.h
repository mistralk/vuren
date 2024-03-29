#ifndef RT_HIT_COMMON_H
#define RT_HIT_COMMON_H

#include "HitData.h"

struct IntersectionAttribute {
    vec2 barycentrics;
};

layout(buffer_reference, scalar) buffer Vertices {
    Vertex v[];
};

layout(buffer_reference, scalar) buffer Indices {
    ivec3 i[];
};

SurfaceHit getHitData(SceneObjectDevice objInfo, IntersectionAttribute attribs, Material material) {
    SurfaceHit hit = getInitialValues();

    Vertices vertices = Vertices(objInfo.vertexAddress);
    Indices indices = Indices(objInfo.indexAddress);

    ivec3 ind = indices.i[gl_PrimitiveID];
    Vertex v0 = vertices.v[ind.x];
    Vertex v1 = vertices.v[ind.y];
    Vertex v2 = vertices.v[ind.z];

    // barycentric coordinate in the intersected triangle
    vec3 barycentrics = vec3(1.0 - attribs.barycentrics.x - attribs.barycentrics.y, attribs.barycentrics.x, attribs.barycentrics.y);

    // local space position (= interpolated vertex position)
    vec3 pos = (v0.pos * barycentrics.x) + (v1.pos * barycentrics.y) + (v2.pos * barycentrics.z);

    // world position
    hit.worldPos = vec4((gl_ObjectToWorldEXT * vec4(pos, 1.0)), 1.0);

    // local shading normal
    vec3 normal = (v0.normal * barycentrics.x) + (v1.normal * barycentrics.y) + (v2.normal * barycentrics.z);

    // world shading normal
    // this is equivalent to instanceInvTransposeWorld * normal.
    //   WorldToObject = inverse(world)
    //   WorldToObject^T * v = v * WorldToObject
    hit.worldNormal = normalize(vec3(normal * gl_WorldToObjectEXT));

    hit.diffuse = material.diffuse;

    return hit;
}

#endif // RT_HIT_COMMON_H