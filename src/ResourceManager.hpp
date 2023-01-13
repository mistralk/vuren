#ifndef RESOURCE_MANAGER_HPP
#define RESOURCE_MANAGER_HPP

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#include "VulkanContext.hpp"
#include "Common.hpp"
#include "Scene.hpp"



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

struct AccelerationStructure {
    vk::AccelerationStructureKHR as;
    Buffer buffer;
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

Buffer createBuffer(const VulkanContext& context, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);

void createBuffer(const VulkanContext& context, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& memory);

void copyBuffer(const VulkanContext& context, vk::CommandPool& commandPool, vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);

void copyBufferToImage(const VulkanContext& context, vk::CommandPool& commandPool, vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height);

void transitionImageLayout(vk::CommandBuffer commandBuffer, Texture& texture, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::PipelineStageFlags srcStage = vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlags dstStage = vk::PipelineStageFlagBits::eAllCommands);

void transitionImageLayout(const VulkanContext& context, vk::CommandPool& commandPool, Texture& texture, vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::PipelineStageFlags srcStage = vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlags dstStage = vk::PipelineStageFlagBits::eAllCommands);

void transitionImageLayout(const VulkanContext& context, vk::CommandPool& commandPool, Texture& texture, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

void createAs(const VulkanContext& context, vk::AccelerationStructureCreateInfoKHR createInfo, AccelerationStructure& as);

void destroyAs(const VulkanContext& context, AccelerationStructure& as);


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

    template <typename DataType>
    Buffer createBufferByHostData(const std::vector<DataType>& hostData, vk::BufferUsageFlags bufferUsage, vk::MemoryPropertyFlags memoryProperty) {
        vk::DeviceSize bufferSize = sizeof(hostData[0]) * hostData.size();
    
        vk::Buffer stagingBuffer;
        vk::DeviceMemory stagingBufferMemory;
        createBuffer(*m_pContext, bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

        void* data;
        data = m_pContext->m_device.mapMemory(stagingBufferMemory, 0, bufferSize);
            memcpy(data, hostData.data(), (size_t)bufferSize);
        m_pContext->m_device.unmapMemory(stagingBufferMemory);

        vk::Buffer buffer;
        vk::DeviceMemory memory;
        createBuffer(*m_pContext, bufferSize, vk::BufferUsageFlagBits::eTransferDst | bufferUsage, memoryProperty, buffer, memory);

        copyBuffer(*m_pContext, m_commandPool, stagingBuffer, buffer, bufferSize);

        m_pContext->m_device.destroyBuffer(stagingBuffer, nullptr);
        m_pContext->m_device.freeMemory(stagingBufferMemory, nullptr);

        Buffer bufferObject;
        bufferObject.descriptorInfo.buffer = buffer;
        bufferObject.memory = memory;

        return bufferObject;
    }

    SceneObject loadObjModel(const std::string& name, const std::string& filename);

    void destroyTextures();
    void destroyBuffers();

    void setExtent(vk::Extent2D extent);
    void setCommandPool(vk::CommandPool commandPool);

    Texture getTexture(const std::string& name);
    Buffer getBuffer(const std::string& name);
    void* getMappedBuffer(const std::string& name);
    const std::unordered_map<std::string, Texture>& getTextureDict() {
        return m_globalTextureDict;
    }

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
    vk::CommandPool m_commandPool {VK_NULL_HANDLE};
    vk::Extent2D m_extent;

}; // class ResourceManager

} // namespace vrb

#endif // RESOURCE_MANAGER_HPP