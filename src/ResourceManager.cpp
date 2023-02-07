#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>

#include "ResourceManager.hpp"

namespace vuren {

ResourceManager::ResourceManager(VulkanContext *pContext) : m_pContext(pContext) {}

ResourceManager::~ResourceManager() {}

// make inputTextureKey point to previous pass' output texture
void ResourceManager::connectTextures(const std::string &srcTexture, const std::string &dstTexture) {
    if (m_globalTextureDict.find(srcTexture) == m_globalTextureDict.end()) {
        throw std::runtime_error("connectTexture: cannot find the src texture!");
    }
    if (m_globalTextureDict.find(dstTexture) != m_globalTextureDict.end()) {
        throw std::runtime_error("connectTexture: the destination texture is already pointing a texture!");
    }
    m_globalTextureDict.insert({ dstTexture, m_globalTextureDict[srcTexture] });
}

void ResourceManager::createTextureRGBA32Sfloat(const std::string &name) {
    if (m_globalTextureDict.find(name) != m_globalTextureDict.end()) {
        return;
    }

    std::shared_ptr<Texture> pTexture = createTexture(
        m_extent.width, m_extent.height, vk::Format::eR32G32B32A32Sfloat, vk::ImageTiling::eOptimal,
        vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage,
        vk::MemoryPropertyFlagBits::eDeviceLocal);
    createImageView(pTexture, vk::Format::eR32G32B32A32Sfloat, vk::ImageAspectFlagBits::eColor);
    createSampler(pTexture);
    // transitionImageLayout(*m_pContext, m_commandPool, texture, vk::Format::eR32G32B32A32Sfloat,
    // vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);

    pTexture->name = name;

    m_globalTextureDict.insert({ name, pTexture });
}

void ResourceManager::createDepthTexture(const std::string &name) {
    if (m_globalTextureDict.find(name) != m_globalTextureDict.end())
        throw std::runtime_error("same depth key already exists in texture dictionary!");

    vk::Format depthFormat = findDepthFormat(*m_pContext);
    std::shared_ptr<Texture> pDepthTexture =
        createTexture(m_extent.width, m_extent.height, depthFormat, vk::ImageTiling::eOptimal,
                      vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);
    createImageView(pDepthTexture, depthFormat, vk::ImageAspectFlagBits::eDepth);
    // transitionImageLayout(*m_pContext, m_commandPool, texture, depthFormat,
    // vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);

    pDepthTexture->name = name;

    m_globalTextureDict.insert({ name, pDepthTexture });
}

std::shared_ptr<Texture> ResourceManager::createModelTexture(const std::string &name, const std::string &filename) {
    int texWidth, texHeight, texChannels;
    stbi_uc *pixels          = stbi_load(filename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    vk::DeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    Buffer stagingBuffer =
        createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc,
                     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

    void *data;
    data = m_pContext->m_device.mapMemory(stagingBuffer.memory, 0, imageSize);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    m_pContext->m_device.unmapMemory(stagingBuffer.memory);

    stbi_image_free(pixels);

    std::shared_ptr<Texture> pModelTexture =
        createTexture(texWidth, texHeight, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
                      vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                      vk::MemoryPropertyFlagBits::eDeviceLocal);

    // Transition the texture image to ImageLayout::eTransferDstOptimal
    // The image was create with the ImageLayout::eUndefined layout
    // Because we don't care about its contents before performing the copy operation
    transitionImageLayout(*m_pContext, m_commandPool, pModelTexture, vk::Format::eR8G8B8A8Srgb,
                          vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

    // Execute the staging buffer to image copy operation
    copyBufferToImage(*m_pContext, m_commandPool, stagingBuffer.descriptorInfo.buffer, pModelTexture->image,
                      static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

    // To be able to start sampling from the texture image in the shader,
    // need one last transition to prepare it for shader access
    transitionImageLayout(*m_pContext, m_commandPool, pModelTexture, vk::Format::eR8G8B8A8Srgb,
                          vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    destroyBuffer(stagingBuffer);

    createImageView(pModelTexture, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
    createModelTextureSampler(pModelTexture);

    pModelTexture->name = name;

    m_globalTextureDict.insert({ name, pModelTexture });

    return pModelTexture;
}

void ResourceManager::createModelTextureSampler(std::shared_ptr<Texture> pTexture) {
    vk::PhysicalDeviceProperties properties{};
    m_pContext->m_physicalDevice.getProperties(&properties);

    vk::SamplerCreateInfo samplerInfo{ .magFilter               = vk::Filter::eLinear,
                                       .minFilter               = vk::Filter::eLinear,
                                       .mipmapMode              = vk::SamplerMipmapMode::eLinear,
                                       .addressModeU            = vk::SamplerAddressMode::eRepeat,
                                       .addressModeV            = vk::SamplerAddressMode::eRepeat,
                                       .addressModeW            = vk::SamplerAddressMode::eRepeat,
                                       .mipLodBias              = 0.0f,
                                       .anisotropyEnable        = VK_TRUE,
                                       .maxAnisotropy           = properties.limits.maxSamplerAnisotropy,
                                       .compareEnable           = VK_FALSE,
                                       .compareOp               = vk::CompareOp::eAlways,
                                       .minLod                  = 0.0f,
                                       .maxLod                  = 0.0f,
                                       .borderColor             = vk::BorderColor::eIntOpaqueBlack,
                                       .unnormalizedCoordinates = VK_FALSE };

    if (m_pContext->m_device.createSampler(&samplerInfo, nullptr, &pTexture->descriptorInfo.sampler) !=
        vk::Result::eSuccess) {
        throw std::runtime_error("failed to create texture sampler!");
    }
}

void ResourceManager::destroyManagedTextures() {
    for (auto texture: m_globalTextureDict) {
        destroyTexture(*texture.second);
    }
}

void ResourceManager::destroyManagedBuffers() {
    for (auto buffer: m_globalBufferDict) {
        destroyBuffer(*buffer.second);
    }
}

void ResourceManager::destroyTexture(Texture& texture) {
    if (texture.descriptorInfo.sampler) m_pContext->m_device.destroySampler(texture.descriptorInfo.sampler, nullptr);
    if (texture.image) m_pContext->m_device.destroyImage(texture.image, nullptr);
    if (texture.descriptorInfo.imageView) m_pContext->m_device.destroyImageView(texture.descriptorInfo.imageView, nullptr);
    if (texture.memory) m_pContext->m_device.freeMemory(texture.memory, nullptr);

    texture.descriptorInfo.sampler = VK_NULL_HANDLE;
    texture.image = VK_NULL_HANDLE;
    texture.descriptorInfo.imageView = VK_NULL_HANDLE;
    texture.memory = VK_NULL_HANDLE;
}

void ResourceManager::destroyBuffer(Buffer& buffer) {
    if (buffer.descriptorInfo.buffer) m_pContext->m_device.destroyBuffer(buffer.descriptorInfo.buffer, nullptr);
    if (buffer.memory) m_pContext->m_device.freeMemory(buffer.memory, nullptr);

    buffer.descriptorInfo.buffer = VK_NULL_HANDLE;
    buffer.memory = VK_NULL_HANDLE;
}

void ResourceManager::setExtent(vk::Extent2D extent) { m_extent = extent; }

void ResourceManager::setCommandPool(vk::CommandPool commandPool) { m_commandPool = commandPool; }

void ResourceManager::loadObjModel(const std::string &name, const std::string &filename,
                                   std::shared_ptr<Scene> pScene, uint32_t materialId) {
    std::string vertexBufferKey = std::string(name + "_vertexBuffer");
    std::string indexBufferKey  = std::string(name + "_indexBuffer");
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filename.c_str())) {
        throw std::runtime_error(warn + err);
    }

    // model loading and vertex deduplication function is based off of Vulkan Tutorial's code.
    // https://vulkan-tutorial.com/Loading_models#page_Loading-vertices-and-indices
    std::unordered_map<Vertex, uint32_t> uniqueVertices{};

    for (const auto &shape: shapes) {
        for (const auto &index: shape.mesh.indices) {
            Vertex vertex{};

            vertex.pos = { attrib.vertices[3 * index.vertex_index + 0], attrib.vertices[3 * index.vertex_index + 1],
                           attrib.vertices[3 * index.vertex_index + 2] };

            vertex.normal = { attrib.normals[3 * index.normal_index + 0], attrib.normals[3 * index.normal_index + 1],
                              attrib.normals[3 * index.normal_index + 2] };

            vertex.texCoord = { attrib.texcoords[2 * index.texcoord_index + 0],
                                1.0f - attrib.texcoords[2 * index.texcoord_index + 1] };

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
            }

            indices.push_back(uniqueVertices[vertex]);
        }
    }

    createVertexBuffer(vertexBufferKey, vertices);
    createIndexBuffer(indexBufferKey, indices);

    SceneObject object = { .vertexBufferSize = static_cast<uint32_t>(vertices.size()),
                           .indexBufferSize  = static_cast<uint32_t>(indices.size()),
                           .pVertexBuffer    = m_globalBufferDict[vertexBufferKey],
                           .pIndexBuffer     = m_globalBufferDict[indexBufferKey],
                           .materialId       = materialId };

    // using this address information, shaders can access these buffers by indexing.
    SceneObjectDevice objectDeviceInfo = {
        .vertexAddress = m_pContext->getBufferDeviceAddress(m_globalBufferDict[vertexBufferKey]->descriptorInfo.buffer),
        .indexAddress  = m_pContext->getBufferDeviceAddress(m_globalBufferDict[indexBufferKey]->descriptorInfo.buffer),
        .materialId = materialId
    };

    pScene->addObject(object);
    pScene->addObjectDevice(objectDeviceInfo);
}

Buffer ResourceManager::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage,
                                     vk::MemoryPropertyFlags properties) {
    vk::Buffer buffer;
    vk::DeviceMemory memory;
    createBuffer(size, usage, properties, buffer, memory);

    Buffer bufferObject;
    bufferObject.memory                = memory;
    bufferObject.descriptorInfo.buffer = buffer;
    bufferObject.descriptorInfo.offset = 0;
    bufferObject.descriptorInfo.range  = size;

    return bufferObject;
}

void ResourceManager::createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties,
                                   vk::Buffer &buffer, vk::DeviceMemory &memory) {
    vk::BufferCreateInfo bufferInfo{ .size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive };

    if (m_pContext->m_device.createBuffer(&bufferInfo, nullptr, &buffer) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create a buffer!");
    }

    vk::MemoryRequirements memRequirements;
    m_pContext->m_device.getBufferMemoryRequirements(buffer, &memRequirements);

    vk::MemoryAllocateInfo allocInfo{ .allocationSize = memRequirements.size,
                                      .memoryTypeIndex =
                                          findMemoryType(*m_pContext, memRequirements.memoryTypeBits, properties) };

    if (m_pContext->m_device.allocateMemory(&allocInfo, nullptr, &memory) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to allocate a buffer memory!");
    }

    m_pContext->m_device.bindBufferMemory(buffer, memory, 0);
}

void ResourceManager::createAs(vk::AccelerationStructureCreateInfoKHR createInfo, AccelerationStructure &as) {
    as.buffer         = createBuffer(createInfo.size,
                                     vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR |
                                         vk::BufferUsageFlagBits::eShaderDeviceAddress,
                                     vk::MemoryPropertyFlagBits::eDeviceLocal);
    createInfo.buffer = as.buffer.descriptorInfo.buffer;
    if (m_pContext->m_device.createAccelerationStructureKHR(&createInfo, nullptr, &as.as) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create a acceleration structure!");
    };
}

void ResourceManager::destroyAs(AccelerationStructure &as) {
    m_pContext->m_device.destroyBuffer(as.buffer.descriptorInfo.buffer, nullptr);
    m_pContext->m_device.freeMemory(as.buffer.memory, nullptr);
    m_pContext->m_device.destroyAccelerationStructureKHR(as.as);
}

std::shared_ptr<Texture> ResourceManager::createTexture(uint32_t width, uint32_t height, vk::Format format,
                                                        vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                                                        vk::MemoryPropertyFlags properties) {
    auto texture = std::make_shared<Texture>();

    vk::ImageCreateInfo imageInfo{ .imageType             = vk::ImageType::e2D,
                                   .format                = format,
                                   .extent                = { .width = width, .height = height, .depth = 1 },
                                   .mipLevels             = 1,
                                   .arrayLayers           = 1,
                                   .samples               = vk::SampleCountFlagBits::e1,
                                   .tiling                = tiling,
                                   .usage                 = usage,
                                   .sharingMode           = vk::SharingMode::eExclusive,
                                   .queueFamilyIndexCount = {},
                                   .pQueueFamilyIndices   = {},
                                   .initialLayout         = vk::ImageLayout::eUndefined };

    if (m_pContext->m_device.createImage(&imageInfo, nullptr, &texture->image) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create image!");
    }

    vk::MemoryRequirements memRequirements;
    m_pContext->m_device.getImageMemoryRequirements(texture->image, &memRequirements);

    vk::MemoryAllocateInfo allocInfo{ .allocationSize = memRequirements.size,
                                      .memoryTypeIndex =
                                          findMemoryType(*m_pContext, memRequirements.memoryTypeBits, properties) };

    if (m_pContext->m_device.allocateMemory(&allocInfo, nullptr, &texture->memory) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    m_pContext->m_device.bindImageMemory(texture->image, texture->memory, 0);

    return texture;
}

void ResourceManager::createImageView(std::shared_ptr<Texture> pTexture, vk::Format format,
                                      vk::ImageAspectFlags aspectFlags) {
    vk::ImageViewCreateInfo viewInfo{ .image            = pTexture->image,
                                      .viewType         = vk::ImageViewType::e2D,
                                      .format           = format,
                                      .subresourceRange = { .aspectMask     = aspectFlags,
                                                            .baseMipLevel   = 0,
                                                            .levelCount     = 1,
                                                            .baseArrayLayer = 0,
                                                            .layerCount     = 1 } };

    if (m_pContext->m_device.createImageView(&viewInfo, nullptr, &pTexture->descriptorInfo.imageView) !=
        vk::Result::eSuccess) {
        throw std::runtime_error("failed to create image view!");
    }
}

void ResourceManager::createSampler(std::shared_ptr<Texture> pTexture) {
    vk::SamplerCreateInfo samplerInfo{
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

    if (m_pContext->m_device.createSampler(&samplerInfo, nullptr, &pTexture->descriptorInfo.sampler) !=
        vk::Result::eSuccess) {
        throw std::runtime_error("failed to create sampler!");
    }
}

//// class ResourceManager member functions

bool hasStencilComponent(vk::Format format) {
    return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}

vk::Format findSupportedFormat(const VulkanContext &context, const std::vector<vk::Format> &candidates,
                               vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
    for (vk::Format format: candidates) {
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

vk::Format findDepthFormat(const VulkanContext &context) {
    return findSupportedFormat(context,
                               { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint },
                               vk::ImageTiling::eOptimal, vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

uint32_t findMemoryType(const VulkanContext &context, uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
    vk::PhysicalDeviceMemoryProperties memProperties;
    context.m_physicalDevice.getMemoryProperties(&memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

vk::CommandBuffer beginSingleTimeCommands(const VulkanContext &context, vk::CommandPool &commandPool) {
    vk::CommandBufferAllocateInfo allocInfo{ .commandPool        = commandPool,
                                             .level              = vk::CommandBufferLevel::ePrimary,
                                             .commandBufferCount = 1 };

    vk::CommandBuffer commandBuffer;
    if (context.m_device.allocateCommandBuffers(&allocInfo, &commandBuffer) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to allocate command buffer!");
    }

    vk::CommandBufferBeginInfo beginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
    if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to begin command buffer!");
    }

    return commandBuffer;
}

void endSingleTimeCommands(const VulkanContext &context, vk::CommandPool &commandPool,
                           vk::CommandBuffer commandBuffer) {
    commandBuffer.end();

    vk::SubmitInfo submitInfo{ .commandBufferCount = 1, .pCommandBuffers = &commandBuffer };

    if (context.m_graphicsQueue.submit(1, &submitInfo, VK_NULL_HANDLE) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to submit command buffer to graphics queue!");
    }
    context.m_graphicsQueue.waitIdle();

    context.m_device.freeCommandBuffers(commandPool, 1, &commandBuffer);
}

void copyBuffer(const VulkanContext &context, vk::CommandPool &commandPool, vk::Buffer srcBuffer, vk::Buffer dstBuffer,
                vk::DeviceSize size) {
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands(context, commandPool);

    vk::BufferCopy copyRegion{ .srcOffset = 0, .dstOffset = 0, .size = size };
    commandBuffer.copyBuffer(srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(context, commandPool, commandBuffer);
}

void copyBufferToImage(const VulkanContext &context, vk::CommandPool &commandPool, vk::Buffer buffer, vk::Image image,
                       uint32_t width, uint32_t height) {
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands(context, commandPool);

    vk::BufferImageCopy region{ .bufferOffset      = 0,
                                .bufferRowLength   = 0,
                                .bufferImageHeight = 0,
                                .imageSubresource  = { .aspectMask     = vk::ImageAspectFlagBits::eColor,
                                                       .mipLevel       = 0,
                                                       .baseArrayLayer = 0,
                                                       .layerCount     = 1 },
                                .imageOffset       = { 0, 0, 0 },
                                .imageExtent       = { width, height, 1 } };

    commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, 1, &region);

    endSingleTimeCommands(context, commandPool, commandBuffer);
}

void transitionImageLayout(vk::CommandBuffer commandBuffer, std::shared_ptr<Texture> pTexture,
                           vk::ImageLayout oldLayout, vk::ImageLayout newLayout, vk::PipelineStageFlags srcStage,
                           vk::PipelineStageFlags dstStage) {
    vk::ImageMemoryBarrier barrier;

    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = pTexture->image;

    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    if (newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
    } else {
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    }

    switch (oldLayout) {
        case vk::ImageLayout::eUndefined:
            barrier.srcAccessMask = vk::AccessFlagBits::eNone;
            break;
        case vk::ImageLayout::eGeneral:
            barrier.srcAccessMask = vk::AccessFlagBits::eNone;
            break;
        case vk::ImageLayout::eColorAttachmentOptimal:
            barrier.srcAccessMask =
                vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
            break;
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            barrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
            break;
        case vk::ImageLayout::eTransferDstOptimal:
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            break;
        default:
            throw std::invalid_argument("unsupported layout transition!");
    }

    switch (newLayout) {
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
            break;
        case vk::ImageLayout::eGeneral:
            barrier.dstAccessMask = vk::AccessFlagBits::eNone;
            break;
        case vk::ImageLayout::eColorAttachmentOptimal:
            barrier.dstAccessMask =
                vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;
            break;
        case vk::ImageLayout::eTransferDstOptimal:
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
            break;
        case vk::ImageLayout::eDepthAttachmentOptimal:
        case vk::ImageLayout::eDepthStencilAttachmentOptimal:
            barrier.dstAccessMask =
                vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            break;
        default:
            throw std::invalid_argument("unsupported layout transition!");
    }

    pTexture->descriptorInfo.imageLayout = newLayout;

    commandBuffer.pipelineBarrier(srcStage, dstStage, {}, 0, nullptr, 0, nullptr, 1, &barrier);
}

void transitionImageLayout(const VulkanContext &context, vk::CommandPool &commandPool,
                           std::shared_ptr<Texture> pTexture, vk::ImageLayout oldLayout, vk::ImageLayout newLayout,
                           vk::PipelineStageFlags srcStage, vk::PipelineStageFlags dstStage) {
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands(context, commandPool);
    transitionImageLayout(commandBuffer, pTexture, oldLayout, newLayout, srcStage, dstStage);
    endSingleTimeCommands(context, commandPool, commandBuffer);
}

void transitionImageLayout(const VulkanContext &context, vk::CommandPool &commandPool,
                           std::shared_ptr<Texture> pTexture, vk::Format format, vk::ImageLayout oldLayout,
                           vk::ImageLayout newLayout) {
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands(context, commandPool);

    // transition

    vk::ImageMemoryBarrier barrier{ .oldLayout           = oldLayout,
                                    .newLayout           = newLayout,
                                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                    .image               = pTexture->image,
                                    .subresourceRange    = {
                                           .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1 } };

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
    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eGeneral) {

        barrier.srcAccessMask = vk::AccessFlagBits::eNone;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eShaderRead;

        sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eRayTracingShaderKHR;
    }

    // For transitioning to the offscreen image
    else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eColorAttachmentOptimal) {

        barrier.srcAccessMask = vk::AccessFlagBits::eNone;
        barrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

        sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    }

    // Transfer writes that don't need to wait on anything
    else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {

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
    else if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        barrier.srcAccessMask = vk::AccessFlagBits::eNone;
        barrier.dstAccessMask =
            vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        // The depth buffer reading happens in the eEarlyFragmentTests stage
        // and the writing in the eLateFragmentTests stage
        destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests;
    }

    else if (oldLayout == vk::ImageLayout::eColorAttachmentOptimal &&
             newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {

        barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentRead;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage      = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    }

    else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    pTexture->descriptorInfo.imageLayout = newLayout;

    commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, 0, nullptr, 0, nullptr, 1, &barrier);

    // end single time commands
    endSingleTimeCommands(context, commandPool, commandBuffer);
}

} // namespace vuren