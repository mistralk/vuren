#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_scalar_block_layout : enable

#include "Common.hpp"
#include "CommonShaders/HitDataRt.h"

layout(set = 0, binding = 5) uniform sampler2D[] texSamplers;
layout(set = 0, binding = 6) buffer _SceneObjectDevice {
    SceneObjectDevice data[];
} objDevice;
layout(set = 0, binding = 7) buffer _Material {
    Material data[];
} materials;

layout(location = 0) rayPayloadInEXT SurfaceHit payload;
hitAttributeEXT IntersectionAttribute attribs;

void main() {
    SceneObjectDevice objInfo = objDevice.data[gl_InstanceCustomIndexEXT];
    Material material = materials.data[objInfo.materialId];
    payload = getHitData(objInfo, attribs, material);
}