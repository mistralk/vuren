#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

struct hitPayload {
    vec3 hitValue;
};

layout(location = 0) rayPayloadInEXT hitPayload payload;

void main() {
    payload.hitValue = vec3(0.0, 1.0, 0.0);
}
