#version 450

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D texSampler;

void main() {
    outColor = texture(texSampler, texCoord);
    //outColor = vec4(1.0, 1.0, 0.0, 1.0);
}