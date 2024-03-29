#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_nonuniform_qualifier : enable

#include "CommonShaders/HitData.h"

// input from descriptor set
layout(binding = 1) uniform sampler2D[] texSamplers;

// input from vertex shader
layout(location = 0) in SurfaceHit inHitData;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outPosWorld;
layout(location = 2) out vec4 outNormalWorld;

void main() {
    uint texId = 0;
    outColor = texture(texSamplers[nonuniformEXT(texId)], inHitData.texCoord);
    outPosWorld = inHitData.worldPos;
    outNormalWorld = vec4(inHitData.worldNormal, 1.0);
}