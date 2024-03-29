#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "Common.hpp"
#include "GBufferCommon.h"
#include "CommonShaders/HitData.h"

layout(location = 0) rayPayloadInEXT SurfaceHit payload;

layout(push_constant) uniform _PushConstantRay {
    PushConstantRay pcRay;
};

void main() {
    payload = getInitialValues();
}