#ifndef COMMON_HPP
#define COMMON_HPP

#ifdef __cplusplus

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

namespace vuren {

using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4x4;
using uint = unsigned int;

const uint32_t kWidth  = 800;
const uint32_t kHeight = 600;
// const int kMaxFramesInFlight = 1;

struct Texture {
    std::string name;
    vk::Image image{ VK_NULL_HANDLE };
    vk::DeviceMemory memory{ VK_NULL_HANDLE };
    vk::DescriptorImageInfo descriptorInfo{ VK_NULL_HANDLE };
};

struct Buffer {
    vk::DeviceMemory memory{ VK_NULL_HANDLE };
    vk::DescriptorBufferInfo descriptorInfo{ VK_NULL_HANDLE };
};

struct AccelerationStructure {
    vk::AccelerationStructureKHR as;
    Buffer buffer;
};

struct SceneObject {
    uint vertexBufferSize{ 0 };
    uint indexBufferSize{ 0 };
    std::shared_ptr<Buffer> pVertexBuffer;
    std::shared_ptr<Buffer> pIndexBuffer;
    uint materialId{ 0 };
    uint instanceCount{ 0 };
};

#endif // __cplusplus

struct SceneObjectDevice {
    uint64_t vertexAddress;
    uint64_t indexAddress;
    uint materialId;
};

struct SceneGlobalData {
    uint lightCount;
};

struct Light {
    uint type;
    vec3 pos;
    vec3 intensity;
};

struct Material {
    vec3 diffuse;
    uint textureId;
};

struct Vertex {
    vec3 pos;
    vec3 normal;
    vec2 texCoord;

#ifdef __cplusplus
    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions{};

        // position
        attributeDescriptions[0].binding  = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format   = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[0].offset   = offsetof(Vertex, pos);

        // normal
        attributeDescriptions[1].binding  = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format   = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset   = offsetof(Vertex, normal);

        // uv coordinate
        attributeDescriptions[2].binding  = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format   = vk::Format::eR32G32Sfloat;
        attributeDescriptions[2].offset   = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }

    bool operator==(const Vertex &other) const {
        return pos == other.pos && normal == other.normal && texCoord == other.texCoord;
    }
#endif // __cplusplus
};

struct ObjectInstance {
    mat4 world;
    mat4 invTransposeWorld;
    uint objectId;

#ifdef __cplusplus
    static std::array<vk::VertexInputAttributeDescription, 9> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 9> attributeDescriptions{};

        // world matrix 4x4
        for (uint i = 0; i < 4; ++i) {
            attributeDescriptions[i].format = vk::Format::eR32G32B32A32Sfloat;
            attributeDescriptions[i].offset = i * 4 * sizeof(float);
        }

        // transpose of inverse of world matrix 4x4
        for (uint i = 4; i < 8; ++i) {
            attributeDescriptions[i].format = vk::Format::eR32G32B32A32Sfloat;
            attributeDescriptions[i].offset = i * 4 * sizeof(float);
        }

        // object id
        attributeDescriptions[8].format = vk::Format::eR32Sint;
        attributeDescriptions[8].offset = 32 * sizeof(float); // offsetof(ObjectInstance, objectId);

        return attributeDescriptions;
    }
#endif // __cplusplus
};

struct CameraData {
    mat4 view;
    mat4 proj;
    mat4 invView;
    mat4 invProj;
};

struct PushConstantRay {
    vec4 clearColor;
    vec3 lightPosition;
    float lightIntensity;
    int lightType;
};

#ifdef __cplusplus
} // namespace vuren

namespace std {
template <> struct hash<vuren::Vertex> {
    size_t operator()(vuren::Vertex const &vertex) const {
        return ((hash<glm::vec3>()(vertex.pos) ^ (hash<glm::vec3>()(vertex.normal) << 1)) >> 1) ^
               (hash<glm::vec2>()(vertex.texCoord) << 1);
    }
};
} // namespace std
#endif // __cplusplus

#endif // COMMON_HPP