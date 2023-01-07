#version 450
#extension GL_EXT_debug_printf : enable

// camera data
layout(binding = 0) uniform Camera {
    mat4 model;
    mat4 view;
    mat4 proj;
} camera;

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
layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outPosWorld;
layout(location = 3) out vec4 outNormalWorld;

void main() {
	vec4 pos = instanceWorld * vec4(inPosition, 1.0);

	// OpenGL uses post-multiplication (vector-on-the-right) with column-major matrix memory layout.
	gl_Position = camera.proj * camera.view * pos;

	outColor = inNormal;
    outTexCoord = inTexCoord;
	outPosWorld = pos.xyz;
	outNormalWorld = instanceInvTransposeWorld * vec4(inNormal, 0.0);
}