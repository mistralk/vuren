#include "VulkanContext.hpp"
#include "Utils.hpp"
#include <set>

namespace vrb {

VulkanContext::VulkanContext() {
}

VulkanContext::~VulkanContext() {
}

void VulkanContext::init(std::string appName, GLFWwindow* pWindow) {
    m_appName = appName;
    m_pWindow = pWindow;
    
    createInstance();
    setupDebugMessenger();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
}

void VulkanContext::cleanup() {
    m_device.destroy(nullptr);

    if (kEnableValidationLayers) {
        DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    }

    m_instance.destroySurfaceKHR(m_surface, nullptr);
    m_instance.destroy(nullptr);
}

void VulkanContext::createInstance() {
    #if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
        vk::DynamicLoader dl;
        PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr");
        VULKAN_HPP_DEFAULT_DISPATCHER.init(vkGetInstanceProcAddr);
    #endif

    if (kEnableValidationLayers && !checkValidationLayerSupport()) {
        throw std::runtime_error("validation layers requested, but not available!");
    }

    assert(!m_appName.empty());

    vk::ApplicationInfo applicationInfo {
        .pApplicationName      = m_appName.c_str(),
        .applicationVersion    = 1,
        .pEngineName           = nullptr, 
        .engineVersion         = 1,
        .apiVersion            = VK_API_VERSION_1_2
    };

    vk::InstanceCreateInfo instanceCreateInfo {
        .pApplicationInfo = &applicationInfo
    };

    vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

    if (kEnableValidationLayers) {
        instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
        instanceCreateInfo.ppEnabledLayerNames = kValidationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        instanceCreateInfo.pNext = (vk::DebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;

        vk::ValidationFeatureEnableEXT enabledValidationFeatures[] {
            vk::ValidationFeatureEnableEXT::eDebugPrintf
        };
        vk::ValidationFeaturesEXT validationFeatures {
            .pNext = instanceCreateInfo.pNext,
            .enabledValidationFeatureCount = std::size(enabledValidationFeatures),
            .pEnabledValidationFeatures = enabledValidationFeatures
        };
        instanceCreateInfo.pNext = &validationFeatures;

        auto extensions = getRequiredExtensions();
        instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        instanceCreateInfo.ppEnabledExtensionNames = extensions.data();
        vk::StructureChain<vk::InstanceCreateInfo, vk::ValidationFeaturesEXT, vk::DebugUtilsMessengerCreateInfoEXT> chain = {instanceCreateInfo, validationFeatures, debugCreateInfo};
        m_instance = vk::createInstance(chain.get<vk::InstanceCreateInfo>()), nullptr;


        #if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
            VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance);
        #endif
        return;
    }
    else {
        instanceCreateInfo.enabledLayerCount = 0;
        instanceCreateInfo.pNext = nullptr;

        auto extensions = getRequiredExtensions();
        instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        instanceCreateInfo.ppEnabledExtensionNames = extensions.data();
        m_instance = vk::createInstance(instanceCreateInfo, nullptr);


        #if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
            VULKAN_HPP_DEFAULT_DISPATCHER.init(m_instance);
        #endif
    }

    // "Vulkan is a platform agnostic API, which means that you need an extension
    //  to interface with the window system. GLFW has a handy built-in function
    //  that returns the extension(s) it needs to do ..."

    // auto extensions = getRequiredExtensions();
    // instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    // instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

    std::vector<vk::ExtensionProperties> avaliableExtensions =  vk::enumerateInstanceExtensionProperties(nullptr);
    std::cout << "available extensions:\n";
    for (const auto& extension : avaliableExtensions) {
        std::cout << '\t' << extension.extensionName << '\n';
    }

    // m_instance = vk::createInstance(instanceCreateInfo, nullptr);
}

void VulkanContext::setupDebugMessenger() {
    if (!kEnableValidationLayers) return;

    vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);
    
    if (CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("failed to set up debug messenger!");
    }
}

void VulkanContext::createSurface() {
    VkSurfaceKHR surface;

    if (glfwCreateWindowSurface(static_cast<VkInstance>(m_instance), m_pWindow, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }

    m_surface = vk::SurfaceKHR(surface);
}

void VulkanContext::pickPhysicalDevice() {
    std::vector<vk::PhysicalDevice> devices = m_instance.enumeratePhysicalDevices();
    if (devices.empty()) {
        throw std::runtime_error("failed to find GPUs with Vulkan support!");
    }

    for (const auto& device : devices) {
        if (isDeviceSuitable(device)) {
            m_physicalDevice = device;
            break;
        }
    }

    // if (!m_physicalDevice) {
    //     throw std::runtime_error("failed to find a suitable GPU!");
    // }
}

void VulkanContext::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};
    float queuePriority = 1.0f;

    for (uint32_t queueFamily : uniqueQueueFamilies) {
        vk::DeviceQueueCreateInfo queueCreateInfo { 
            .queueFamilyIndex = queueFamily,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };
        queueCreateInfos.push_back(queueCreateInfo);
    }
    
    // The currently available drivers will only allow you to create a small number of queues 
    // for each queue family and you don't really need more than one. That's because 
    // you can create all of the command buffers on multiple threads and then 
    // submit them all at once on the main thread with a single low-overhead call."

    vk::PhysicalDeviceFeatures deviceFeatures {
        .samplerAnisotropy = VK_TRUE
    };

    vk::PhysicalDeviceAccelerationStructureFeaturesKHR accelFeature {
        .accelerationStructure = VK_TRUE
    };
    vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature {
        .rayTracingPipeline = VK_TRUE
    };
    vk::PhysicalDeviceBufferDeviceAddressFeaturesEXT bufferAddressFeature {
        .bufferDeviceAddress = VK_TRUE,
        .bufferDeviceAddressCaptureReplay = VK_TRUE
    };
    vk::PhysicalDeviceDescriptorIndexingFeaturesEXT descriptorIndexingFeature {
        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
    };

    vk::DeviceCreateInfo createInfo { 
        // .pNext = &accelFeature,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(kDeviceExtensions.size()),
        .ppEnabledExtensionNames = kDeviceExtensions.data(),
        .pEnabledFeatures = &deviceFeatures 
    };

    vk::StructureChain<vk::DeviceCreateInfo, 
                    vk::PhysicalDeviceAccelerationStructureFeaturesKHR, 
                    vk::PhysicalDeviceRayTracingPipelineFeaturesKHR,
                    vk::PhysicalDeviceBufferDeviceAddressFeaturesEXT,
                    vk::PhysicalDeviceDescriptorIndexingFeaturesEXT> chain = {createInfo, accelFeature, rtPipelineFeature, bufferAddressFeature, descriptorIndexingFeature};

    if (kEnableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
        createInfo.ppEnabledLayerNames = kValidationLayers.data();
    }
    else {
        createInfo.enabledLayerCount = 0;
    }

    m_device = m_physicalDevice.createDevice(chain.get<vk::DeviceCreateInfo>(), nullptr);

    #if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
        VULKAN_HPP_DEFAULT_DISPATCHER.init(m_device);
    #endif

    m_graphicsQueue = m_device.getQueue(indices.graphicsFamily.value(), 0);
    m_presentQueue = m_device.getQueue(indices.presentFamily.value(), 0);
}


bool VulkanContext::isDeviceSuitable(vk::PhysicalDevice device) {
    QueueFamilyIndices indices = findQueueFamilies(device);
    bool extensionsSupported = checkDeviceExtensionSupport(device);
    bool swapChainAdequate = false;

    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    vk::PhysicalDeviceFeatures supportedFeatures;
    device.getFeatures(&supportedFeatures);

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

SwapChainSupportDetails VulkanContext::querySwapChainSupport(vk::PhysicalDevice device) {
    SwapChainSupportDetails details;

    details.capabilities = device.getSurfaceCapabilitiesKHR(m_surface);
    details.formats = device.getSurfaceFormatsKHR(m_surface);
    details.presentModes = device.getSurfacePresentModesKHR(m_surface);

    return details;
}

QueueFamilyIndices VulkanContext::findQueueFamilies(vk::PhysicalDevice device) {
    QueueFamilyIndices indices;

    std::vector<vk::QueueFamilyProperties> queueFamilies = device.getQueueFamilyProperties();

    int i = 0;
    for (const auto& queueFamily : queueFamilies) {
        
        if (device.getSurfaceSupportKHR(i, m_surface)) {
            indices.presentFamily = i;
        }

        if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics) {
            indices.graphicsFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

bool VulkanContext::checkValidationLayerSupport() {
    std::vector<vk::LayerProperties> availableLayers = vk::enumerateInstanceLayerProperties();
    
    int cnt = 0;
    for (const char* layerName : kValidationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (std::strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) {
            return false;
        }
    }

    return true;
}

std::vector<const char*> VulkanContext::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (kEnableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

bool VulkanContext::checkDeviceExtensionSupport(vk::PhysicalDevice device) {
    std::vector<vk::ExtensionProperties> availableExtensions = device.enumerateDeviceExtensionProperties(nullptr);
    std::set<std::string> requiredExtensions(kDeviceExtensions.begin(), kDeviceExtensions.end());

    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::debugCallBack(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,    // verbose < info < warning < error
    VkDebugUtilsMessageTypeFlagsEXT meessageType,              // general, validation, performance
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, // {message, object handles, object count}
    void* pUserData) {

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

void VulkanContext::populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = { 
        .pNext = VK_NULL_HANDLE,
        .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | 
                            vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
        .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | 
                        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | 
                        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
        .pfnUserCallback = debugCallBack
    };
}

} // namespace vrb