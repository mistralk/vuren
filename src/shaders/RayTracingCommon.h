#ifndef RAY_TRACING_COMMON_H
#define RAY_TRACING_COMMON_H

#include "ShaderCommon.h"

struct IntersectionAttribute {
    vec2 barycentrics;
};

layout(buffer_reference, scalar) buffer Vertices {
    Vertex v[];
};

layout(buffer_reference, scalar) buffer Indices {
    ivec3 i[];
};

SurfaceHit getHitData(SceneObjectDevice objInfo, IntersectionAttribute attribs) {
    SurfaceHit hit;
    hit.worldPos = vec3(0.0, 0.0, 0.0);
    hit.worldNormal = vec3(0.0, 0.0, 0.0);
    hit.texCoord = vec2(0.0, 0.0);

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
    hit.worldPos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));

    // local shading normal
    vec3 normal = (v0.normal * barycentrics.x) + (v1.normal * barycentrics.y) + (v2.normal * barycentrics.z);

    // world shading normal
    // this is equivalent to instanceInvTransposeWorld * normal.
    //   WorldToObject = inverse(world)
    //   WorldToObject^T * v = v * WorldToObject
    hit.worldNormal = normalize(vec3(normal * gl_WorldToObjectEXT));

    return hit;
}

#endif // RAY_TRACING_COMMON_H