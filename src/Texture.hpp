#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#include "VulkanContext.hpp"

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>


namespace vrb {

struct Texture {
    vk::Image image {VK_NULL_HANDLE};
    vk::DeviceMemory memory {VK_NULL_HANDLE};
    vk::DescriptorImageInfo descriptorInfo {VK_NULL_HANDLE};
};

// struct Buffer {
//     vk::Buffer buffer {VK_NULL_HANDLE};
//     vk::DeviceMemory memory {VK_NULL_HANDLE};
//     vk::DescriptorBufferInfo descriptorInfo {VK_NULL_HANDLE};
// };


void destroyTexture(const VulkanContext& context, Texture& texture);

bool hasStencilComponent(vk::Format format);

vk::Format findSupportedFormat(const VulkanContext& context, const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features);

vk::Format findDepthFormat(const VulkanContext& context);

uint32_t findMemoryType(const VulkanContext& context, uint32_t typeFilter, vk::MemoryPropertyFlags properties);

vk::CommandBuffer beginSingleTimeCommands(const VulkanContext& context, vk::CommandPool& commandPool);

void endSingleTimeCommands(const VulkanContext& context, vk::CommandPool& commandPool, vk::CommandBuffer commandBuffer);

void createImage(const VulkanContext& context, Texture& texture, uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties);

void createImageView(const VulkanContext& context, Texture& texture, vk::Format format, vk::ImageAspectFlags aspectFlags);

void createSampler(const VulkanContext& context, Texture& texture);

void createBuffer(const VulkanContext& context, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& bufferMemory);

void copyBuffer(const VulkanContext& context, vk::CommandPool& commandPool, vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size);

void copyBufferToImage(const VulkanContext& context, vk::CommandPool& commandPool, vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height);

void transitionImageLayout(const VulkanContext& context, vk::CommandPool& commandPool, Texture& texture, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);

} // namespace vrb

#endif // TEXTURE_HPP