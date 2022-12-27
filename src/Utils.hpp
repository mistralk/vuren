#ifndef UTILS_HPP
#define UTILS_HPP

#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>

namespace vrb {

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