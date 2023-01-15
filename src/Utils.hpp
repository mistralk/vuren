#ifndef UTILS_HPP
#define UTILS_HPP

#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>

#include "Common.hpp"
#include <vector>

namespace vuren {

std::vector<char> readFile(const std::string &filename);

VkResult CreateDebugUtilsMessengerEXT(vk::Instance instance, const vk::DebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                      const vk::AllocationCallbacks *pAllocator,
                                      vk::DebugUtilsMessengerEXT *pDebugMessenger);

void DestroyDebugUtilsMessengerEXT(vk::Instance instance, vk::DebugUtilsMessengerEXT debugMessenger,
                                   const vk::AllocationCallbacks *pAllocator);

} // namespace vuren

#endif // UTILS_HPP