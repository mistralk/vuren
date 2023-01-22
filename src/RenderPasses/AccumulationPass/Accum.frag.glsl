#version 460
#extension GL_GOOGLE_include_directive : enable

#include "AccumCommon.h"

layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D currentFrame;
layout(binding = 1, rgba32f) uniform image2D historyBuffer;
layout(binding = 2) uniform _AccumData {
	AccumData accumData;
};

void main() {
    vec4 current = texture(currentFrame, texCoord);
    vec4 history = imageLoad(historyBuffer, ivec2(gl_FragCoord.xy));
    vec4 accum = current + history * accumData.frameCount;
    
    outColor = accum / (accumData.frameCount + 1);
    imageStore(historyBuffer, ivec2(gl_FragCoord.xy), outColor);
}