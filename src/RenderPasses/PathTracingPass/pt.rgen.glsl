#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : enable

#include "Common.hpp"
#include "PtCommon.h"
#include "CommonShaders/Random.h"
#include "CommonShaders/HitData.h"

layout(location = 0) rayPayloadEXT SurfaceHit payload;

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;
layout(set = 0, binding = 1) uniform _FrameData {
	FrameData frameData;
};

// g-buffers
layout(set = 0, binding = 2) uniform sampler2D worldPos;
layout(set = 0, binding = 3) uniform sampler2D worldNormal;

// output texture
layout(set = 0, binding = 4, rgba32f) uniform image2D outputColor;

void main() {
    const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
    const vec2 inUV = pixelCenter / vec2(gl_LaunchSizeEXT.xy);
    // vec2 ndc = inUV * 2.0 - 1.0;

    uint rngState = initRNG(gl_LaunchIDEXT.xy, gl_LaunchSizeEXT.xy, frameData.frameCount);
    uint rayFlags = gl_RayFlagsOpaqueEXT;
    float tMin = 0.00001; // bias to avoid self-intersection
    float tMax = 10000.0; 

    vec3 radiance = vec3(0.0);
    vec3 throughput = vec3(1.0);
    int max_depth = 4;

    // depth = 0: black image

    // get primary hit(depth = 1) info from g-buffers
    vec4 pos = texture(worldPos, inUV);
    vec3 normal = texture(worldNormal, inUV).xyz;
    
    vec2 uv = vec2(rand(rngState), rand(rngState));
    vec3 worldDir = getCosHemisphereSample(uv, normal);

    // max_depth = 0: black image
    // max_depth = 1: we can see area light only
    // max_depth = 2: we can see "one scatter" result
    for (int depth = 1; depth <= max_depth; ++depth) {
        if (depth > 1) {
            // trace a scatter ray using path sampling info computed at the iteration right before (or from g-buffer)
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

            // chit 또는 miss shader가 실행되어 페이로드가 알아서 업데이트된다
            // chit으로부터 셰이딩 데이터(hit position, normal)를 업데이트
            pos = payload.worldPos;
            normal = payload.worldNormal;
            // 라이트소스 위에 힛포인트가 올라간 경우에도 알아서 처리될것임

            vec3 brdfValue = payload.diffuse * INV_PI;
            throughput *= brdfValue; // brdf / pdf 
        }
        
        // ray miss: background radiance was already calculated in the miss shader
        if (pos.w == 0.0)
            break;

        // lighting
        // for (int lightIdx = 0; lightIdx <);
        //     radiance += throughput * sceneLights[lightIdx].Le(worldDir)

        vec3 lightpos = vec3(2,2,2);
        vec3 ldir = lightpos - pos.xyz;
        vec3 L = normalize(ldir);
        float ldist = length(ldir);
        float intensity = 15.0 / (ldist * ldist);
        float NdotL = clamp(dot(normal, L), 0.0, 1.0);

        radiance += vec3(intensity * throughput * NdotL);

        // shading the pixel
        // radiance = throughput;

        // sample the next path segment
        uv = vec2(rand(rngState), rand(rngState));
        worldDir = getCosHemisphereSample(uv, normal);
    
    }

    imageStore(outputColor, ivec2(gl_LaunchIDEXT.xy), vec4(radiance, 0));
}