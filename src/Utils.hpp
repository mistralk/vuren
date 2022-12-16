#ifndef UTILS_HPP
#define UTILS_HPP

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

namespace vrb {

const std::vector<const char*> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME
};

std::vector<char> readFile(const std::string& filename);

VkResult CreateDebugUtilsMessengerEXT(vk::Instance instance, 
    const vk::DebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const vk::AllocationCallbacks* pAllocator,
    vk::DebugUtilsMessengerEXT* pDebugMessenger);

void DestroyDebugUtilsMessengerEXT(vk::Instance instance, 
    vk::DebugUtilsMessengerEXT debugMessenger, 
    const vk::AllocationCallbacks* pAllocator);

} // namespace vrb

#endif // UTILS_HPP