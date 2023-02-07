#ifndef PT_COMMON_H
#define PT_COMMON_H

#include "Common.hpp"

#ifdef __cplusplus
// #include <glm/glm.hpp>
namespace vuren {

// using vec2 = glm::vec2;
// using vec3 = glm::vec3;
// using vec4 = glm::vec4;
// using mat4 = glm::mat4x4;
// using uint = unsigned int;
#endif

struct FrameData {
    uint frameCount;
};

#ifdef __cplusplus
} // namespace vuren
#endif

#endif // PT_COMMON_H