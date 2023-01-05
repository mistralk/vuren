#ifndef RESOURCE_MANAGER_HPP
#define RESOURCE_MANAGER_HPP

#include "VulkanContext.hpp"
#include "Common.hpp"
#include "Scene.hpp"

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>


namespace vrb {

struct Texture {
    vk::Image image {VK_NULL_HANDLE};
    vk::DeviceMemory memory {VK_NULL_HANDLE};
    vk::DescriptorImageInfo descriptorInfo {VK_NULL_HANDLE};
};

struct Buffer {
    vk::DeviceMemory memory {VK_NULL_HANDLE};
    vk::DescriptorBufferInfo descriptorInfo {VK_NULL_HANDLE};
};

void destroyTexture(const VulkanContext& context, Texture& texture);

void destroyBuffer(const VulkanContext& context, Buffer& buffer);

bool hasStencilComponent(vk::Format format);

vk::Format findSupportedFormat(const VulkanContext& context, const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);

vk::Format findDepthFormat(const VulkanContext& context);

uint32_t findMemoryType(const VulkanContext& context, uint32_t typeFilter, vk::MemoryPropertyFlags properties);

vk::CommandBuffer beginSingleTimeCommands(const VulkanContext& context, vk::CommandPool& commandPool);

void endSingleTimeCommands(const VulkanContext& context, vk::CommandPool& commandPool, vk::CommandBuffer commandBuffer);

void createImage(const VulkanContext& context, Texture& texture, uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties);

void createImageView(const VulkanContext& context, Texture& texture, vk::Format format, vk::ImageAspectFlags aspectFlags);

void createSampler(const VulkanContext& context, Texture& texture);

void createBuffer(const VulkanContext& context, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& memory);

void copyBuffer(const VulkanContext& context, vk::CommandPool& commandPool, vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);

void copyBufferToImage(const VulkanContext& context, vk::CommandPool& commandPool, vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height);

void transitionImageLayout(const VulkanContext& context, vk::CommandPool& commandPool, Texture& texture, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

class ResourceManager {
public:
    ResourceManager(VulkanContext* pContext);
    ~ResourceManager();

    void createTextureRGBA32Sfloat(const std::string& name);
    void createDepthTexture(const std::string& name);
    void createModelTexture(const std::string& name, const std::string& filename);
    void createModelTextureSampler(Texture& texture);
    void createVertexBuffer(const std::string& name, const std::vector<Vertex>& vertices);
    void createIndexBuffer(const std::string& name, const std::vector<uint32_t>& indices);
    void createUniformBuffer(const std::string& name);

    SceneObject loadObjModel(const std::string& name, const std::string& filename);

    void destroyTextures();
    void destroyBuffers();

    void setExtent(vk::Extent2D extent);
    void setCommandPool(vk::CommandPool* commandPool);

    Texture getTexture(const std::string& name);
    Buffer getBuffer(const std::string& name);
    void* getMappedBuffer(const std::string& name);

    void insertBuffer(const std::string& name, Buffer buffer) {
        m_globalBufferDict.insert({name, buffer});
    }

    void insertTexture(const std::string& name, Texture texture) {
        m_globalTextureDict.insert({name, texture});
    }

private:
    std::unordered_map<std::string, Texture> m_globalTextureDict;
    std::unordered_map<std::string, Buffer> m_globalBufferDict;
    std::unordered_map<std::string, void*> m_uniformBufferMappedDict;

    VulkanContext* m_pContext {nullptr};
    vk::CommandPool* m_pCommandPool {nullptr};
    vk::Extent2D m_extent;

}; // class ResourceManager

} // namespace vrb

#endif // RESOURCE_MANAGER_HPP