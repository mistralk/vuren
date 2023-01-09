#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>

#include "ResourceManager.hpp"

namespace vrb {

ResourceManager::ResourceManager(VulkanContext* pContext) 
    : m_pContext(pContext) {

}

ResourceManager::~ResourceManager() {

}

Texture ResourceManager::getTexture(const std::string& name) {
    if (m_globalTextureDict.find(name) == m_globalTextureDict.end())
        throw std::runtime_error("failed to find the texture!");
    return m_globalTextureDict[name];
}

Buffer ResourceManager::getBuffer(const std::string& name) {
    if (m_globalBufferDict.find(name) == m_globalBufferDict.end())
        throw std::runtime_error("failed to find the buffer!");
    return m_globalBufferDict[name];
}

void* ResourceManager::getMappedBuffer(const std::string& name) {
    if (m_uniformBufferMappedDict.find(name) == m_uniformBufferMappedDict.end())
        throw std::runtime_error("failed to find the mapped buffer!");
    return m_uniformBufferMappedDict[name];
}

void ResourceManager::createTextureRGBA32Sfloat(const std::string& name) {
    Texture texture;
    createImage(*m_pContext, texture,
        m_extent.width, m_extent.height, 
        vk::Format::eR32G32B32A32Sfloat, 
        vk::ImageTiling::eOptimal, 
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    createImageView(*m_pContext, texture, vk::Format::eR32G32B32A32Sfloat, vk::ImageAspectFlagBits::eColor);
    createSampler(*m_pContext, texture);
    // transitionImageLayout(*m_pContext, m_commandPool, texture, vk::Format::eR32G32B32A32Sfloat, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);

    if (m_globalTextureDict.find(name) != m_globalTextureDict.end())
        throw std::runtime_error("same key already exists in texture dictionary!");
    
    m_globalTextureDict.insert({name, texture});
}

void ResourceManager::createDepthTexture(const std::string& name) {
    Texture depthTexture;
    vk::Format depthFormat = findDepthFormat(*m_pContext);
    createImage(*m_pContext, depthTexture, m_extent.width, m_extent.height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);
    createImageView(*m_pContext, depthTexture, depthFormat, vk::ImageAspectFlagBits::eDepth);
    // transitionImageLayout(*m_pContext, m_commandPool, texture, depthFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);

    if (m_globalTextureDict.find(name) != m_globalTextureDict.end())
        throw std::runtime_error("same depth key already exists in texture dictionary!");

    m_globalTextureDict.insert({name, depthTexture});
}

void ResourceManager::createModelTexture(const std::string& name, const std::string& filename) {
    Texture modelTexture;

    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    vk::DeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    createBuffer(*m_pContext, imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

    void* data;
    data = m_pContext->m_device.mapMemory(stagingBufferMemory, 0, imageSize);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
    m_pContext->m_device.unmapMemory(stagingBufferMemory);

    stbi_image_free(pixels);

    createImage(*m_pContext, modelTexture, texWidth, texHeight, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);

    // Transition the texture image to ImageLayout::eTransferDstOptimal
    // The image was create with the ImageLayout::eUndefined layout
    // Because we don't care about its contents before performing the copy operation
    transitionImageLayout(*m_pContext, m_commandPool, modelTexture, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

    // Execute the staging buffer to image copy operation
    copyBufferToImage(*m_pContext, m_commandPool, stagingBuffer, modelTexture.image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    
    // To be able to start sampling from the texture image in the shader,
    // need one last transition to prepare it for shader access
    transitionImageLayout(*m_pContext, m_commandPool, modelTexture, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    m_pContext->m_device.destroyBuffer(stagingBuffer, nullptr);
    m_pContext->m_device.freeMemory(stagingBufferMemory, nullptr);

    createImageView(*m_pContext, modelTexture, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
    createModelTextureSampler(modelTexture);

    m_globalTextureDict.insert({name, modelTexture});
}

void ResourceManager::createModelTextureSampler(Texture& texture) {
    vk::PhysicalDeviceProperties properties{};
    m_pContext->m_physicalDevice.getProperties(&properties);

    vk::SamplerCreateInfo samplerInfo {
        .magFilter = vk::Filter::eLinear,
        .minFilter = vk::Filter::eLinear,
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .addressModeU = vk::SamplerAddressMode::eRepeat,
        .addressModeV = vk::SamplerAddressMode::eRepeat,
        .addressModeW = vk::SamplerAddressMode::eRepeat,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_TRUE,
        .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
        .compareEnable = VK_FALSE,
        .compareOp = vk::CompareOp::eAlways,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = vk::BorderColor::eIntOpaqueBlack,
        .unnormalizedCoordinates = VK_FALSE
    };

    if (m_pContext->m_device.createSampler(&samplerInfo, nullptr, &texture.descriptorInfo.sampler) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create texture sampler!");
    }
}

void ResourceManager::createVertexBuffer(const std::string& name, const std::vector<Vertex>& vertices) {
    Buffer buffer;

    vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
    
    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    createBuffer(*m_pContext, bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

    void* data;
    data = m_pContext->m_device.mapMemory(stagingBufferMemory, 0, bufferSize);
        memcpy(data, vertices.data(), (size_t)bufferSize);
    m_pContext->m_device.unmapMemory(stagingBufferMemory);

    vk::BufferUsageFlags rayTracingFlags = vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eStorageBuffer;

    vk::Buffer vertexBuffer;
    vk::DeviceMemory vertexBufferMemory;
    createBuffer(*m_pContext, bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | rayTracingFlags, vk::MemoryPropertyFlagBits::eDeviceLocal, vertexBuffer, vertexBufferMemory);

    copyBuffer(*m_pContext, m_commandPool, stagingBuffer, vertexBuffer, bufferSize);

    m_pContext->m_device.destroyBuffer(stagingBuffer, nullptr);
    m_pContext->m_device.freeMemory(stagingBufferMemory, nullptr);

    buffer.descriptorInfo.buffer = vertexBuffer;
    buffer.memory = vertexBufferMemory;

    m_globalBufferDict.insert({name, buffer});
}

void ResourceManager::createIndexBuffer(const std::string& name, const std::vector<uint32_t>& indices) {
    Buffer buffer;

    vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();
    
    vk::Buffer stagingBuffer;
    vk::DeviceMemory stagingBufferMemory;
    createBuffer(*m_pContext, bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

    void* data;
    data = m_pContext->m_device.mapMemory(stagingBufferMemory, 0, bufferSize);
        memcpy(data, indices.data(), (size_t)bufferSize);
    m_pContext->m_device.unmapMemory(stagingBufferMemory);

    vk::BufferUsageFlags rayTracingFlags = vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eStorageBuffer;

    vk::Buffer indexBuffer;
    vk::DeviceMemory indexBufferMemory;
    createBuffer(*m_pContext, bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | rayTracingFlags, vk::MemoryPropertyFlagBits::eDeviceLocal, indexBuffer, indexBufferMemory);

    copyBuffer(*m_pContext, m_commandPool, stagingBuffer, indexBuffer, bufferSize);

    m_pContext->m_device.destroyBuffer(stagingBuffer, nullptr);
    m_pContext->m_device.freeMemory(stagingBufferMemory, nullptr);

    buffer.descriptorInfo.buffer = indexBuffer;
    buffer.memory = indexBufferMemory;

    m_globalBufferDict.insert({name, buffer});
}

void ResourceManager::createUniformBuffer(const std::string& name) {
    Buffer buffer;
    vk::DeviceSize bufferSize = sizeof(Camera);

    vk::Buffer uniformBuffer;
    vk::DeviceMemory uniformBufferMemory;
    createBuffer(*m_pContext, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, uniformBuffer, uniformBufferMemory);

    void* mapped;
    mapped = m_pContext->m_device.mapMemory(uniformBufferMemory, 0, bufferSize);
    
    buffer.descriptorInfo.buffer = uniformBuffer;
    buffer.memory = uniformBufferMemory;

    m_uniformBufferMappedDict.insert({name, mapped});
    m_globalBufferDict.insert({name, buffer});
}

void ResourceManager::destroyTextures() {
    for (auto texture : m_globalTextureDict) {
        destroyTexture(*m_pContext, texture.second);
    }
}

void ResourceManager::destroyBuffers() {
    for (auto buffer : m_globalBufferDict) {
        if (m_uniformBufferMappedDict.find(buffer.first) != m_uniformBufferMappedDict.end()) {
            m_uniformBufferMappedDict.erase(buffer.first);
        }
        destroyBuffer(*m_pContext, buffer.second);
    }
}

void ResourceManager::setExtent(vk::Extent2D extent) {
    m_extent = extent;
}

void ResourceManager::setCommandPool(vk::CommandPool commandPool) {
    m_commandPool = commandPool;
}

SceneObject ResourceManager::loadObjModel(const std::string& name, const std::string& filename) {
    std::string vertexBufferKey = std::string(name + "_vertexBuffer");
    std::string indexBufferKey = std::string(name + "_indexBuffer");
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str())) {
        throw std::runtime_error(warn + err);
    }

    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.pos = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            vertex.normal = {
                attrib.normals[3 * index.normal_index + 0],
                attrib.normals[3 * index.normal_index + 1],
                attrib.normals[3 * index.normal_index + 2]
            };

            vertex.texCoord = {
                attrib.texcoords[2 * index.texcoord_index + 0],
                1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
            };

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(uniqueVertices[vertex]);
        }
    }

    createVertexBuffer(vertexBufferKey, vertices);
    createIndexBuffer(indexBufferKey, indices);

    SceneObject object = {
        .vertexBufferSize = static_cast<uint32_t>(vertices.size()),
        .indexBufferSize = static_cast<uint32_t>(indices.size()),
        .vertexBuffer = &m_globalBufferDict[vertexBufferKey],
        .indexBuffer = &m_globalBufferDict[indexBufferKey]
    };

    return object;
}

//// class ResourceManager member functions

void destroyTexture(const VulkanContext& context, Texture& texture) {
    context.m_device.destroySampler(texture.descriptorInfo.sampler, nullptr);
    context.m_device.destroyImage(texture.image, nullptr);
    context.m_device.destroyImageView(texture.descriptorInfo.imageView, nullptr);
    context.m_device.freeMemory(texture.memory, nullptr);
}

void destroyBuffer(const VulkanContext& context, Buffer& buffer) {
    context.m_device.destroyBuffer(buffer.descriptorInfo.buffer, nullptr);
    context.m_device.freeMemory(buffer.memory, nullptr);
}

bool hasStencilComponent(vk::Format format) {
    return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}

vk::Format findSupportedFormat(const VulkanContext& context, const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
    for (vk::Format format : candidates) {
        vk::FormatProperties props;
        context.m_physicalDevice.getFormatProperties(format, &props);

        if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

vk::Format findDepthFormat(const VulkanContext& context) {
    return findSupportedFormat(context,
        {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
        vk::ImageTiling::eOptimal,
        vk::FormatFeatureFlagBits::eDepthStencilAttachment
    );
}

uint32_t findMemoryType(const VulkanContext& context, uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
    vk::PhysicalDeviceMemoryProperties memProperties;
    context.m_physicalDevice.getMemoryProperties(&memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties ) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

vk::CommandBuffer beginSingleTimeCommands(const VulkanContext& context, vk::CommandPool& commandPool) {
    vk::CommandBufferAllocateInfo allocInfo { 
        .commandPool = commandPool,
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1
    };

    vk::CommandBuffer commandBuffer;
    if (context.m_device.allocateCommandBuffers(&allocInfo, &commandBuffer) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to allocate command buffer!");
    }

    vk::CommandBufferBeginInfo beginInfo { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
    if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to begin command buffer!");
    }

    return commandBuffer;
}

void endSingleTimeCommands(const VulkanContext& context, vk::CommandPool& commandPool, vk::CommandBuffer commandBuffer) {
    commandBuffer.end();

    vk::SubmitInfo submitInfo { 
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer
    };

    if (context.m_graphicsQueue.submit(1, &submitInfo, VK_NULL_HANDLE) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to submit command buffer to graphics queue!");
    }
    context.m_graphicsQueue.waitIdle();

    context.m_device.freeCommandBuffers(commandPool, 1, &commandBuffer);
}


void createImage(const VulkanContext& context, Texture& texture, uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties) {
    vk::ImageCreateInfo imageInfo { 
        .imageType = vk::ImageType::e2D, 
        .format = format,
        .extent = { 
            .width = width,
            .height = height,
            .depth = 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive,
        .queueFamilyIndexCount = {},
        .pQueueFamilyIndices = {},
        .initialLayout = vk::ImageLayout::eUndefined
    };
    
    if (context.m_device.createImage(&imageInfo, nullptr, &texture.image) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create image!");
    }

    vk::MemoryRequirements memRequirements;
    context.m_device.getImageMemoryRequirements(texture.image, &memRequirements);

    vk::MemoryAllocateInfo allocInfo { 
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(context, memRequirements.memoryTypeBits, properties) 
    };
    
    if (context.m_device.allocateMemory(&allocInfo, nullptr, &texture.memory) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    context.m_device.bindImageMemory(texture.image, texture.memory, 0);
}

void createImageView(const VulkanContext& context, Texture& texture, vk::Format format, vk::ImageAspectFlags aspectFlags) {
    vk::ImageViewCreateInfo viewInfo { 
        .image = texture.image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = { 
            .aspectMask = aspectFlags,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1 } 
    };

    if (context.m_device.createImageView(&viewInfo, nullptr, &texture.descriptorInfo.imageView) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create image view!");
    }
}

void createSampler(const VulkanContext& context, Texture& texture) {
    vk::SamplerCreateInfo samplerInfo {
        // .magFilter = vk::Filter::eLinear,
        // .minFilter = vk::Filter::eLinear,
        // .mipmapMode = vk::SamplerMipmapMode::eLinear,
        // .addressModeU = vk::SamplerAddressMode::eRepeat,
        // .addressModeV = vk::SamplerAddressMode::eRepeat,
        // .addressModeW = vk::SamplerAddressMode::eRepeat,
        // .mipLodBias = 0.0f,
        // .anisotropyEnable = VK_FALSE,
        // .compareEnable = VK_FALSE,
        // .compareOp = vk::CompareOp::eAlways,
        // .minLod = 0.0f,
        // .maxLod = 0.0f,
        // .borderColor = vk::BorderColor::eIntOpaqueBlack,
        // .unnormalizedCoordinates = VK_FALSE
    };

    if (context.m_device.createSampler(&samplerInfo, nullptr, &texture.descriptorInfo.sampler) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create sampler!");
    }
}

Buffer createBuffer(const VulkanContext& context, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties) {
    vk::Buffer buffer;
    vk::DeviceMemory memory;
    createBuffer(context, size, usage, properties, buffer, memory);

    Buffer bufferObject;
    bufferObject.memory = memory;
    bufferObject.descriptorInfo.buffer = buffer;

    return bufferObject;
}

void createBuffer(const VulkanContext& context, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& memory) {
    vk::BufferCreateInfo bufferInfo { 
        .size = size,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive
    };

    if (context.m_device.createBuffer(&bufferInfo, nullptr, &buffer) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create a buffer!");
    }

    vk::MemoryRequirements memRequirements;
    context.m_device.getBufferMemoryRequirements(buffer, &memRequirements);

    vk::MemoryAllocateInfo allocInfo{ 
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(context, memRequirements.memoryTypeBits, properties)
    };
    
    if (context.m_device.allocateMemory(&allocInfo, nullptr, &memory) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to allocate a buffer memory!");
    }

    context.m_device.bindBufferMemory(buffer, memory, 0);
}

void copyBuffer(const VulkanContext& context, vk::CommandPool& commandPool, vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size) {
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands(context, commandPool);

    vk::BufferCopy copyRegion { 
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    commandBuffer.copyBuffer(srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(context, commandPool, commandBuffer);
}

void copyBufferToImage(const VulkanContext& context, vk::CommandPool& commandPool, vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height) {
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands(context, commandPool);

    vk::BufferImageCopy region { 
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource = { .aspectMask = vk::ImageAspectFlagBits::eColor,
                            .mipLevel = 0,
                            .baseArrayLayer = 0,
                            .layerCount = 1 },
        .imageOffset = { 0, 0, 0 },
        .imageExtent = { width, height, 1 }
    };

    commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, 1, &region);

    endSingleTimeCommands(context, commandPool, commandBuffer);
}

void transitionImageLayout(const VulkanContext& context, vk::CommandPool& commandPool, Texture& texture, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands(context, commandPool);

    // transition

    vk::ImageMemoryBarrier barrier { 
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = texture.image,
        .subresourceRange = { 
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1 } 
    };

    if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;

        if (hasStencilComponent(format)) {
            barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
        }
    } else {
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    }

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    // storage image
    if (oldLayout == vk::ImageLayout::eUndefined &&
        newLayout == vk::ImageLayout::eGeneral) {

        barrier.srcAccessMask = vk::AccessFlagBits::eNone;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;
        
        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eRayTracingShaderKHR;
    }

    // For transitioning to the offscreen image
    else if (oldLayout == vk::ImageLayout::eUndefined &&
        newLayout == vk::ImageLayout::eColorAttachmentOptimal) {

        barrier.srcAccessMask = vk::AccessFlagBits::eNone;
        barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
        
        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    }

    // Transfer writes that don't need to wait on anything
    else if (oldLayout == vk::ImageLayout::eUndefined && 
        newLayout == vk::ImageLayout::eTransferDstOptimal) {
            
        barrier.srcAccessMask = vk::AccessFlagBits::eNone;
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        // Since the writes don't have to wait on anything,
        // specify the earliest possible pipelie stage for the pre-barrier ops.
        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        // a pseudo-stage where transfers happen
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    }
    
    // Shader reads should wait on transfer writes
    else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && 
                newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {

        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        // Specify shader reading access in the fragment shader pipeline stage.
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    }

    // For transitioning to the depth image
    else if (oldLayout == vk::ImageLayout::eUndefined &&
                newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eNone;
        barrier.dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        // The depth buffer reading happens in the eEarlyFragmentTests stage
        // and the writing in the eLateFragmentTests stage
        destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
    }

    else if (oldLayout == vk::ImageLayout::eColorAttachmentOptimal && 
                newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {

        barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    }
    
    else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    texture.descriptorInfo.imageLayout = newLayout;
    
    commandBuffer.pipelineBarrier(sourceStage, destinationStage, 
                                    {},
                                    0, nullptr, 
                                    0, nullptr,
                                    1, &barrier);

    // end single time commands
    endSingleTimeCommands(context, commandPool, commandBuffer);
}

void createAs(const VulkanContext& context, vk::AccelerationStructureCreateInfoKHR createInfo, AccelerationStructure& as) {
    as.buffer = createBuffer(context, createInfo.size, vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress, vk::MemoryPropertyFlagBits::eDeviceLocal);
    createInfo.buffer = as.buffer.descriptorInfo.buffer;
    if (context.m_device.createAccelerationStructureKHR(&createInfo, nullptr, &as.as)  != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create a acceleration structure!");
    };
}

void destroyAs(const VulkanContext& context, AccelerationStructure& as) {
    context.m_device.destroyBuffer(as.buffer.descriptorInfo.buffer, nullptr);
    context.m_device.freeMemory(as.buffer.memory, nullptr);
    context.m_device.destroyAccelerationStructureKHR(as.as);
}

} // namespace vrb