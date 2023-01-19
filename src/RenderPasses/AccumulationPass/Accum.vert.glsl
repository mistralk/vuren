#version 460

layout(location = 0) out vec2 texCoord;

vec2 positions[3] = vec2[](
    vec2(0, 0),
    vec2(2, 0),
    vec2(0, 2)
);

void main() {
    texCoord = positions[gl_VertexIndex];
    gl_Position = vec4(texCoord * 2.0f - 1.0f, 0.0f, 1.0f); // (-1, -1), (3, -1), (-1, 3)
}