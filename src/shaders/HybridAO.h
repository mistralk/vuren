#ifndef HYBRID_AO_H
#define HYBRID_AO_H

#ifdef __cplusplus
#include <glm/glm.hpp>
namespace vuren {

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4x4;
using uint = unsigned int;
#endif

struct SurfaceHit {
    float color;
};

struct AoData {
    float radius;
    uint frameCount;
};

#ifdef __cplusplus
} // namespace vuren
#endif

#endif // HYBRID_AO_H