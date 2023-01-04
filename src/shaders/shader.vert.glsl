#version 450
#extension GL_EXT_debug_printf : enable

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;

layout(location = 3) in mat4 instanceTransform;
// layout(location = 3) in vec3 instancePos;
// layout(location = 4) in vec3 instanceRot;
// layout(location = 5) in float instanceScale;
layout(location = 7) in int instanceId;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;

void main() {
	// vec4 pos = vec4(inPosition + vec3(instanceTransform[3].xyz), 1.0);
    // gl_Position = ubo.proj * ubo.view * pos;
	/*
	이렇게 할 시, 4번째 컬럼은 트랜슬레이트 컴포넌트들로서 제대로 처리됨
	즉 GPU 메모리 레이아웃은 컬럼 메이저 오더
	하지만 GLSL에서의 실제 계산 순서 자체는 우리가 손으로 풀 때처럼 M.v로 처리됨

	OpenGL uses post-multiplication (vector-on-the-right) with column-major matrix memory layout.
	*/

	vec4 pos = instanceTransform * vec4(inPosition, 1.0); // see what's happen at pos.w
	gl_Position = ubo.proj * ubo.view * ubo.model * vec4(pos.xyz, 1.0);

	fragColor = inColor;
    fragTexCoord = inTexCoord;
}