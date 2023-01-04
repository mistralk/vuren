#ifndef COMMON_HPP
#define COMMON_HPP

#define VULKAN_HPP_NO_CONSTRUCTORS
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
    glm::vec3 color;
    glm::vec2 texCoord;

    static vk::VertexInputBindingDescription getBindingDescription() {
        vk::VertexInputBindingDescription bindingDescription{ 
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = vk::VertexInputRate::eVertex
        };

        return bindingDescription;
    }

    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = vk::Format::eR32G32Sfloat;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }

    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

struct UniformBufferObject {
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
    // glm::vec3 pos;
    // glm::vec3 rot;
    // float scale;

    glm::mat4x4 transform;

    uint32_t objectId;

    static vk::VertexInputBindingDescription getBindingDescription() {
        vk::VertexInputBindingDescription bindingDescription{ 
            .binding = 1,
            .stride = sizeof(ObjectInstance),
            .inputRate = vk::VertexInputRate::eInstance
        };

        return bindingDescription;
    }

    static std::array<vk::VertexInputAttributeDescription, 5> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 5> attributeDescriptions{};
        attributeDescriptions[0].binding = 1;
        attributeDescriptions[0].location = 3;
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[0].offset = 0;

        attributeDescriptions[1].binding = 1;
        attributeDescriptions[1].location = 4;
        attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset = 4 * sizeof(float);

        attributeDescriptions[2].binding = 1;
        attributeDescriptions[2].location = 5;
        attributeDescriptions[2].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[2].offset = 8 * sizeof(float);

        attributeDescriptions[3].binding = 1;
        attributeDescriptions[3].location = 6;
        attributeDescriptions[3].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[3].offset = 12 * sizeof(float);

        // attributeDescriptions[0].binding = 1;
        // attributeDescriptions[0].location = 3;
        // attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
        // attributeDescriptions[0].offset = offsetof(ObjectInstance, pos);

        // attributeDescriptions[1].binding = 1;
        // attributeDescriptions[1].location = 4;
        // attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
        // attributeDescriptions[1].offset = offsetof(ObjectInstance, rot);

        // attributeDescriptions[2].binding = 1;
        // attributeDescriptions[2].location = 5;
        // attributeDescriptions[2].format = vk::Format::eR32Sfloat;
        // attributeDescriptions[2].offset = offsetof(ObjectInstance, scale);

        attributeDescriptions[4].binding = 1;
        attributeDescriptions[4].location = 7;
        attributeDescriptions[4].format = vk::Format::eR32Sint;
        attributeDescriptions[4].offset = 16 * sizeof(float); // offsetof(ObjectInstance, objectId);

        return attributeDescriptions;
    }
};

} // namespace vrb

namespace std {
template<> struct hash<vrb::Vertex> {
    size_t operator()(vrb::Vertex const& vertex) const {
        return ((hash<glm::vec3>()(vertex.pos) ^
                (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex.texCoord) << 1);
    }
};
} // namespace std

#endif // COMMON_HPP