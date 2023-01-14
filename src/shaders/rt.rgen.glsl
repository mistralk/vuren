#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "../Common.hpp"
#include "ShaderCommon.h"

layout(location = 0) rayPayloadEXT HitPayload payload;

layout(set = 0, binding = 0) uniform _Camera {
	Camera camera;
};
layout(set = 0, binding = 4, rgba32f) uniform image2D image;
layout(set = 0, binding = 5) uniform accelerationStructureEXT tlas;

void main() {
    const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
    const vec2 inUV = pixelCenter / vec2(gl_LaunchSizeEXT.xy);
    vec2 ndc = inUV * 2.0 - 1.0;
    
    vec4 origin = camera.invView * vec4(0, 0, 0, 1);
    vec4 target = camera.invProj * vec4(ndc.x, ndc.y, 1, 1);
    vec4 direction = camera.invView * vec4(normalize(target.xyz), 0);

    uint rayFlags = gl_RayFlagsOpaqueEXT;
    float tMin = 0.0;
    float tMax = 10000.0;

    traceRayEXT(tlas,           // as
                rayFlags,       // rayFlags
                0xFF,           // cullMask
                0,              // sbtRecordOffset
                0,              // sbtRecordStride
                0,              // missIndex
                origin.xyz,     // ray origin
                tMin,           // ray min range
                direction.xyz,  // ray direction
                tMax,           // ray max range
                0               // payload location
    );

    imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(payload.hitValue, 1.0));
}