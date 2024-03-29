#ifndef RESOURCE_MANAGER_HPP
#define RESOURCE_MANAGER_HPP

#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

#include "Common.hpp"
#include "Scene.hpp"
#include "VulkanContext.hpp"

namespace vuren {

bool hasStencilComponent(vk::Format format);

vk::Format findSupportedFormat(const VulkanContext &context, const std::vector<vk::Format> &candidates,
                               vk::ImageTiling tiling, vk::FormatFeatureFlags features);

vk::Format findDepthFormat(const VulkanContext &context);

uint32_t findMemoryType(const VulkanContext &context, uint32_t typeFilter, vk::MemoryPropertyFlags properties);

vk::CommandBuffer beginSingleTimeCommands(const VulkanContext &context, vk::CommandPool &commandPool);

void endSingleTimeCommands(const VulkanContext &context, vk::CommandPool &commandPool, vk::CommandBuffer commandBuffer);

void copyBuffer(const VulkanContext &context, vk::CommandPool &commandPool, vk::Buffer srcBuffer, vk::Buffer dstBuffer,
                vk::DeviceSize size);

void copyBufferToImage(const VulkanContext &context, vk::CommandPool &commandPool, vk::Buffer buffer, vk::Image image,
                       uint32_t width, uint32_t height);

void transitionImageLayout(vk::CommandBuffer commandBuffer, std::shared_ptr<Texture> pTexture, vk::ImageLayout oldLayout,
                           vk::ImageLayout newLayout,
                           vk::PipelineStageFlags srcStage = vk::PipelineStageFlagBits::eAllCommands,
                           vk::PipelineStageFlags dstStage = vk::PipelineStageFlagBits::eAllCommands);

void transitionImageLayout(const VulkanContext &context, vk::CommandPool &commandPool, std::shared_ptr<Texture> pTexture,
                           vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                           vk::PipelineStageFlags srcStage = vk::PipelineStageFlagBits::eAllCommands,
                           vk::PipelineStageFlags dstStage = vk::PipelineStageFlagBits::eAllCommands);

void transitionImageLayout(const VulkanContext &context, vk::CommandPool &commandPool, std::shared_ptr<Texture> pTexture,
                           vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

class ResourceManager {
public:
    ResourceManager(VulkanContext *pContext);
    ~ResourceManager();

    void connectTextures(const std::string &srcTexture, const std::string &dstTexture);

    void createTextureRGBA32Sfloat(const std::string &name);
    void createDepthTexture(const std::string &name);
    std::shared_ptr<Texture> createModelTexture(const std::string &name, const std::string &filename);
    void createModelTextureSampler(std::shared_ptr<Texture> pTexture);

    void loadObjModel(const std::string &name, const std::string &filename, std::shared_ptr<Scene> pScene, uint32_t materialId);
    void createObjectDeviceInfoBuffer(std::shared_ptr<Scene> pScene) {
        createBufferByHostData<SceneObjectDevice>(pScene->getObjectsDevice(), vk::BufferUsageFlagBits::eStorageBuffer,
                                                  vk::MemoryPropertyFlagBits::eDeviceLocal, "SceneObjectDeviceInfo");
    }
    void createMaterialBuffer(std::shared_ptr<Scene> pScene){
        createBufferByHostData<Material>(pScene->getMaterials(), vk::BufferUsageFlagBits::eStorageBuffer,
                                                  vk::MemoryPropertyFlagBits::eDeviceLocal, "MaterialBuffer");
    }
    void createAs(vk::AccelerationStructureCreateInfoKHR createInfo, AccelerationStructure &as);
    void destroyAs(AccelerationStructure &as);

    std::shared_ptr<Texture> createTexture(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling,
                          vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties);
    void createImageView(std::shared_ptr<Texture> pTexture, vk::Format format, vk::ImageAspectFlags aspectFlags);
    void createSampler(std::shared_ptr<Texture> pTexture);

    Buffer createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties);
    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties,
                      vk::Buffer &buffer, vk::DeviceMemory &memory);
    void destroyTexture(Texture& texture);
    void destroyBuffer(Buffer& buffer);

    Buffer createVertexBuffer(const std::string &name, const std::vector<Vertex> &vertices) {
        vk::BufferUsageFlags bufferUsage =
            vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst |
            vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
        vk::MemoryPropertyFlags memoryProperty = vk::MemoryPropertyFlagBits::eDeviceLocal;
        Buffer buffer = createBufferByHostData<Vertex>(vertices, bufferUsage, memoryProperty, name);
        return buffer;
    }

    Buffer createIndexBuffer(const std::string &name, const std::vector<uint32_t> &indices) {
        vk::BufferUsageFlags bufferUsage =
            vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR |
            vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst |
            vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress;
        vk::MemoryPropertyFlags memoryProperty = vk::MemoryPropertyFlagBits::eDeviceLocal;
        Buffer buffer = createBufferByHostData<uint32_t>(indices, bufferUsage, memoryProperty, name);
        return buffer;
    }

    template <typename DataType> void createUniformBuffer(const std::string &name) {
        vk::DeviceSize bufferSize = sizeof(DataType);

        Buffer uniformBuffer =
            createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer,
                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void *mapped;
        mapped = m_pContext->m_device.mapMemory(uniformBuffer.memory, 0, bufferSize);

        m_uniformBufferMappedDict.insert({ name, mapped });
        m_globalBufferDict.insert({ name, std::make_shared<Buffer>(uniformBuffer) });
    }

    // if name is not given, the returned buffer will not be managed by resource manager.
    // in that case, user must manually destroy the buffer after use.
    template <typename DataType>
    Buffer createBufferByHostData(const std::vector<DataType> &hostData, vk::BufferUsageFlags bufferUsage,
                                  vk::MemoryPropertyFlags memoryProperty, const std::string &name = "") {
        vk::DeviceSize bufferSize = sizeof(hostData[0]) * hostData.size();

        Buffer stagingBuffer =
            createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        void *data;
        data = m_pContext->m_device.mapMemory(stagingBuffer.memory, 0, bufferSize);
        memcpy(data, hostData.data(), (size_t) bufferSize);
        m_pContext->m_device.unmapMemory(stagingBuffer.memory);

        Buffer newBuffer;
        newBuffer = createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | bufferUsage, memoryProperty);

        copyBuffer(*m_pContext, m_commandPool, stagingBuffer.descriptorInfo.buffer, newBuffer.descriptorInfo.buffer,
                   bufferSize);

        destroyBuffer(stagingBuffer);

        // the buffer has name and will be managed by resource manager.
        if (!name.empty())
            m_globalBufferDict.insert({ name, std::make_shared<Buffer>(newBuffer) });

        return newBuffer;
    }

    void destroyManagedTextures();
    void destroyManagedBuffers();

    void setExtent(vk::Extent2D extent);
    void setCommandPool(vk::CommandPool commandPool);

    std::shared_ptr<Texture> getTexture(const std::string &name) {
        if (m_globalTextureDict.find(name) == m_globalTextureDict.end())
            throw std::runtime_error("failed to find the texture!");
        return m_globalTextureDict[name];
    }

    std::shared_ptr<Buffer> getBuffer(const std::string &name) {
        if (m_globalBufferDict.find(name) == m_globalBufferDict.end())
            throw std::runtime_error("failed to find the buffer!");
        return m_globalBufferDict[name];
    }

    void *getMappedBuffer(const std::string &name) {
        if (m_uniformBufferMappedDict.find(name) == m_uniformBufferMappedDict.end())
            throw std::runtime_error("failed to find the mapped buffer!");
        return m_uniformBufferMappedDict[name];
    }

    const std::unordered_map<std::string, std::shared_ptr<Texture>> &getTextureDict() { return m_globalTextureDict; }

    void insertBuffer(const std::string &name, Buffer buffer) { 
        auto pBuffer = std::make_shared<Buffer>(buffer);
        m_globalBufferDict.insert({ name, pBuffer });
    }

    void insertTexture(const std::string &name, Texture texture) {
        auto pTexture = std::make_shared<Texture>(texture);
        m_globalTextureDict.insert({ name, pTexture });
    }

    vk::Extent2D getExtent() {
        return m_extent;
    }

private:
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_globalTextureDict;
    std::unordered_map<std::string, std::shared_ptr<Buffer>> m_globalBufferDict;
    std::unordered_map<std::string, void *> m_uniformBufferMappedDict;

    VulkanContext *m_pContext{ nullptr };
    vk::CommandPool m_commandPool{ VK_NULL_HANDLE };
    vk::Extent2D m_extent;

}; // class ResourceManager

} // namespace vuren

#endif // RESOURCE_MANAGER_HPP