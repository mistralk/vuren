#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "ShaderCommon.glsl"

layout(location = 0) rayPayloadInEXT HitPayload payload;
hitAttributeEXT IntersectionAttribute attribs;

void main() {
    payload.hitValue = vec3(attribs.barycentrics.x, attribs.barycentrics.y, 0.0);
}
