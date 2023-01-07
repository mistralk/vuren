#version 450
#extension GL_EXT_debug_printf : enable

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 3) in mat4 instanceTransform;
// layout(location = 3) in vec4 col0
// layout(location = 4) in vec4 col1
// layout(location = 5) in vec4 col2
// layout(location = 6) in vec4 col3
layout(location = 7) in int instanceId;

layout(location = 0) out vec3 outColor;
layout(location = 1) out vec2 outTexCoord;
layout(location = 2) out vec3 outPosWorld;
layout(location = 3) out vec3 outNormal;

void main() {
	vec4 pos = instanceTransform * vec4(inPosition, 1.0); // see what's happen at pos.w

	// if (instanceTransform[0][3]+ instanceTransform[1][3]+instanceTransform[2][3]+instanceTransform[3][3] == 4.0)
	//  	pos.x *= 2.0f;
	// 현재 3행의 모든 원소들이 기대대로 (0,0,0,1)이 아니라 (1,1,1,1)이 들어가고 있다

	// OpenGL uses post-multiplication (vector-on-the-right) with column-major matrix memory layout.
	gl_Position = ubo.proj * ubo.view * ubo.model * pos;

	outColor = inNormal;
    outTexCoord = inTexCoord;
	outPosWorld = pos.xyz;
	outNormal = inNormal;
}