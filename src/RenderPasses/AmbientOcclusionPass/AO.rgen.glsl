#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_nonuniform_qualifier : enable

#include "Common.hpp"
#include "AoCommon.h"
#include "CommonShaders/Random.h"

layout(location = 0) rayPayloadEXT SurfaceHit payload;

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;
layout(set = 0, binding = 1) uniform _AoData {
	AoData aoData;
};
layout(set = 0, binding = 2) uniform sampler2D worldPos;
layout(set = 0, binding = 3) uniform sampler2D worldNormal;
layout(set = 0, binding = 4, rgba32f) uniform image2D outputColor;

void main() {
    const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
    const vec2 inUV = pixelCenter / vec2(gl_LaunchSizeEXT.xy);
    vec2 ndc = inUV * 2.0 - 1.0;

    uint rayFlags = gl_RayFlagsOpaqueEXT | gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsSkipClosestHitShaderEXT;
    float tMin = 0.00001; // bias to avoid self-intersection
    float tMax = aoData.radius; 

    vec4 pos = texture(worldPos, inUV);
    vec3 normal = texture(worldNormal, inUV).xyz;

    float color = 1.0;

    uint rngState = initRNG(gl_LaunchIDEXT.xy, gl_LaunchSizeEXT.xy, aoData.frameCount);

    if (pos.w != 0.0) {
        payload.color = 0.0;
        vec2 uv = vec2(rand(rngState), rand(rngState));
        vec3 worldDir = getCosHemisphereSample(uv, normal);

        traceRayEXT(tlas,           // as
                    rayFlags,       // rayFlags
                    0xFF,           // cullMask
                    0,              // sbtRecordOffset
                    0,              // sbtRecordStride
                    0,              // missIndex
                    pos.xyz,        // ray origin
                    tMin,           // ray min range
                    worldDir,       // ray direction
                    tMax,           // ray max range
                    0               // payload location
        );

        color = payload.color;
    }

    imageStore(outputColor, ivec2(gl_LaunchIDEXT.xy), vec4(color, color, color, 0));
}