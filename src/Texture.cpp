#include "Texture.hpp"

namespace vrb {

void destroyTexture(const VulkanContext& context, Texture& texture) {
    context.m_device.destroySampler(texture.descriptorInfo.sampler, nullptr);
    context.m_device.destroyImage(texture.image, nullptr);
    context.m_device.destroyImageView(texture.descriptorInfo.imageView, nullptr);
    context.m_device.freeMemory(texture.memory, nullptr);
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
    vk::CommandBufferAllocateInfo allocInfo { .commandPool = commandPool,
                                                .level = vk::CommandBufferLevel::ePrimary,
                                                .commandBufferCount = 1 };

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

    vk::SubmitInfo submitInfo { .commandBufferCount = 1,
                                .pCommandBuffers = &commandBuffer };

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

void createBuffer(const VulkanContext& context, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& bufferMemory) {
    vk::BufferCreateInfo bufferInfo { .size = size,
                                        .usage = usage,
                                        .sharingMode = vk::SharingMode::eExclusive };
    
    if (context.m_device.createBuffer(&bufferInfo, nullptr, &buffer) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to create a buffer!");
    }

    vk::MemoryRequirements memRequirements;
    context.m_device.getBufferMemoryRequirements(buffer, &memRequirements);

    vk::MemoryAllocateInfo allocInfo{ .allocationSize = memRequirements.size,
                                        .memoryTypeIndex = findMemoryType(context, memRequirements.memoryTypeBits, properties) };
    
    if (context.m_device.allocateMemory(&allocInfo, nullptr, &bufferMemory) != vk::Result::eSuccess) {
        throw std::runtime_error("failed to allocate a buffer memory!");
    }

    context.m_device.bindBufferMemory(buffer, bufferMemory, 0);
}

void copyBuffer(const VulkanContext& context, vk::CommandPool& commandPool, vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size) {
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands(context, commandPool);

    vk::BufferCopy copyRegion { .srcOffset = 0,
                                .dstOffset = 0,
                                .size = size };
    commandBuffer.copyBuffer(srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(context, commandPool, commandBuffer);
}

void copyBufferToImage(const VulkanContext& context, vk::CommandPool& commandPool, vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height) {
    vk::CommandBuffer commandBuffer = beginSingleTimeCommands(context, commandPool);

    vk::BufferImageCopy region { .bufferOffset = 0,
                                    .bufferRowLength = 0,
                                    .bufferImageHeight = 0,
                                    .imageSubresource = { .aspectMask = vk::ImageAspectFlagBits::eColor,
                                                        .mipLevel = 0,
                                                        .baseArrayLayer = 0,
                                                        .layerCount = 1 },
                                    .imageOffset = { 0, 0, 0 },
                                    .imageExtent = { width, height, 1 } };
    
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

    // For transitioning to the offscreen image
    if (oldLayout == vk::ImageLayout::eUndefined &&
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

} // namespace vrb