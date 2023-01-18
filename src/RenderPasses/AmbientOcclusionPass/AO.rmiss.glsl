#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "Common.hpp"
#include "AoCommon.h"

layout(location = 0) rayPayloadInEXT SurfaceHit payload;

void main() {
    payload.color = 1.0;
}