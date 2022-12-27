#ifndef TEXTURE_HPP
#define TEXTURE_HPP

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

namespace vrb {

struct Texture {
    vk::Image image {VK_NULL_HANDLE};
    vk::DeviceMemory memory {VK_NULL_HANDLE};
    vk::DescriptorImageInfo descriptorInfo {VK_NULL_HANDLE};
};

void destroyTexture(vk::Device device, Texture& texture) {
    device.destroySampler(texture.descriptorInfo.sampler, nullptr);
    device.destroyImage(texture.image, nullptr);
    device.destroyImageView(texture.descriptorInfo.imageView, nullptr);
    device.freeMemory(texture.memory, nullptr);
}

} // namespace vrb

#endif // TEXTURE_HPP