#ifndef COMMON_HPP
#define COMMON_HPP

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

namespace vrb {

const uint32_t kWidth = 800;
const uint32_t kHeight = 600;
static std::string kAppName = "vrb";
// const int kMaxFramesInFlight = 1;

struct Buffer;

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 texCoord;

    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions{};

        // position
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        // normal
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset = offsetof(Vertex, normal);

        // uv coordinate
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = vk::Format::eR32G32Sfloat;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }

    bool operator==(const Vertex& other) const {
        return pos == other.pos && normal == other.normal && texCoord == other.texCoord;
    }
};

struct Camera {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

struct SceneObject {
    uint32_t vertexBufferSize {0};
    uint32_t indexBufferSize {0};
    Buffer* vertexBuffer;
    Buffer* indexBuffer;
};

struct ObjectInstance {
    glm::mat4x4 world;
    glm::mat4x4 invTransposeWorld;
    uint32_t objectId;

    static std::array<vk::VertexInputAttributeDescription, 9> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 9> attributeDescriptions{};

        // world matrix 4x4
        for (uint32_t i = 0; i < 4; ++i) {
            attributeDescriptions[i].format = vk::Format::eR32G32B32A32Sfloat;
            attributeDescriptions[i].offset = i * 4 * sizeof(float);
        }

        // transpose of inverse of world matrix 4x4
        for (uint32_t i = 4; i < 8; ++i) {
            attributeDescriptions[i].format = vk::Format::eR32G32B32A32Sfloat;
            attributeDescriptions[i].offset = i * 4 * sizeof(float);
        }

        // object id
        attributeDescriptions[8].format = vk::Format::eR32Sint;
        attributeDescriptions[8].offset = 32 * sizeof(float); // offsetof(ObjectInstance, objectId);

        return attributeDescriptions;
    }
};

} // namespace vrb

namespace std {
template<> struct hash<vrb::Vertex> {
    size_t operator()(vrb::Vertex const& vertex) const {
        return ((hash<glm::vec3>()(vertex.pos) ^
                (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex.texCoord) << 1);
    }
};
} // namespace std

#endif // COMMON_HPP