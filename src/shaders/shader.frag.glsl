#version 450
#extension GL_EXT_debug_printf : enable

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 inColor;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec3 inPosWorld;
layout(location = 3) in vec4 inNormalWorld;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec4 outPosWorld;
layout(location = 2) out vec4 outNormalWorld;

void main() {
    outColor = texture(texSampler, inTexCoord);
    outPosWorld = vec4(inPosWorld, 0.0);
    outNormalWorld = inNormalWorld;
}