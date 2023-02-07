#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "Common.hpp"

#include "CommonShaders/HitData.h"

// camera data
layout(binding = 0) uniform _Camera {
	CameraData camera;
};

// per-vertex data
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

// per-instance data
layout(location = 3) in mat4 instanceWorld;
 // layout(location = 3) in vec4 col0
 // layout(location = 4) in vec4 col1
 // layout(location = 5) in vec4 col2
 // layout(location = 6) in vec4 col3

layout(location = 7) in mat4 instanceInvTransposeWorld;
 // layout(location = 7) in vec4 col0
 // layout(location = 8) in vec4 col1
 // layout(location = 9) in vec4 col2
 // layout(location = 10) in vec4 col3

layout(location = 11) in int instanceId;

// output
layout(location = 0) out SurfaceHit outHitData;

void main() {
	vec4 worldPos = instanceWorld * vec4(inPosition, 1.0);

	// OpenGL uses post-multiplication (vector-on-the-right) with column-major matrix memory layout.
	// gl_Position = camera.proj * worldPos;
	gl_Position = camera.proj * camera.view * worldPos;

	outHitData.worldPos = worldPos;
	outHitData.worldNormal = normalize((instanceInvTransposeWorld * vec4(inNormal, 0.0)).xyz);
    outHitData.texCoord = inTexCoord;
}