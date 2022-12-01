#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <set>
#include <cstring>
#include <optional>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <fstream>
#include <array>
#include <chrono>

const uint32_t kWidth = 800;
const uint32_t kHeight = 600;
static std::string kAppName = "vrb";
static std::string kEngineName = "No Engine";
const int kMaxFramesInFlight = 2;

const std::vector<const char*> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef NDEBUG
    const bool kEnableValidationLayers = false;
#else
    const bool kEnableValidationLayers = true;
#endif

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription() {
        vk::VertexInputBindingDescription bindingDescription{ .binding = 0,
                                                              .stride = sizeof(Vertex),
                                                              .inputRate = vk::VertexInputRate::eVertex};

        return bindingDescription;
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}},

    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}}
};

const std::vector<uint32_t> indices = {
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4
};

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

VkResult CreateDebugUtilsMessengerEXT(vk::Instance instance, 
    const vk::DebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const vk::AllocationCallbacks* pAllocator,
    vk::DebugUtilsMessengerEXT* pDebugMessenger) {

        auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(static_cast<VkInstance>(instance), "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            return func(instance,
                (VkDebugUtilsMessengerCreateInfoEXT*)pCreateInfo, 
                (VkAllocationCallbacks*)pAllocator, 
                (VkDebugUtilsMessengerEXT*)pDebugMessenger);
        }
        else {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }
    }

void DestroyDebugUtilsMessengerEXT(vk::Instance instance, vk::DebugUtilsMessengerEXT debugMessenger, const vk::AllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(static_cast<VkInstance>(instance), "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, (VkAllocationCallbacks*)pAllocator);
    }
}

class VRBApplication {
private:
    GLFWwindow* window_;
    
    vk::Instance instance_;
    vk::DebugUtilsMessengerEXT debugMessenger_;
    vk::PhysicalDevice physicalDevice_;
    vk::Device device_;
    vk::Queue graphicsQueue_;
    vk::SurfaceKHR surface_;
    vk::Queue presentQueue_;
    
    vk::SwapchainKHR swapChain_;
    std::vector<vk::Image> swapChainImages_;
    vk::Format swapChainImageFormat_;
    vk::Extent2D swapChainExtent_;
    std::vector<vk::ImageView> swapChainImageViews_;

    vk::RenderPass renderPass_;
    vk::DescriptorSetLayout descriptorSetLayout_;
    vk::PipelineLayout pipelineLayout_;
    vk::Pipeline graphicsPipeline_;

    std::vector<vk::Framebuffer> swapChainFramebuffers_;

    vk::CommandPool commandPool_;
    std::vector<vk::CommandBuffer> commandBuffers_;

    std::vector<vk::Semaphore> imageAvailableSemaphores_;
    std::vector<vk::Semaphore> renderFinishedSemaphores_;
    std::vector<vk::Fence> inFlightFences_;

    vk::Buffer vertexBuffer_;
    vk::DeviceMemory vertexBufferMemory_;
    vk::Buffer indexBuffer_;
    vk::DeviceMemory indexBufferMemory_;

    std::vector<vk::Buffer> uniformBuffers_;
    std::vector<vk::DeviceMemory> uniformBuffersMemory_;
    std::vector<void*> uniformBuffersMapped_;

    vk::DescriptorPool descriptorPool_;
    std::vector<vk::DescriptorSet> descriptorSets_;

    vk::Image textureImage_;
    vk::DeviceMemory textureImageMemory_;

    vk::Image depthImage_;
    vk::DeviceMemory depthImageMemory_;
    vk::ImageView depthImageView_;

    uint32_t currentFrame_ = 0;
    bool framebufferResized_ = false;

public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    void initWindow() {
        glfwInit();

        // tell glfw to not create an OpenGL context
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        
        // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window_ = glfwCreateWindow(kWidth, kHeight, kAppName.c_str(), nullptr, nullptr);
        glfwSetWindowUserPointer(window_, this);
        glfwSetFramebufferSizeCallback(window_, framebufferResizeCallback);
    }

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<VRBApplication*>(glfwGetWindowUserPointer(window));
        app->framebufferResized_ = true;
    }

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createCommandPool();
        createTextureImage();
        createDepthResources();
        createFramebuffers();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
        createSyncObjects();
    }

    void createInstance() {
        if (kEnableValidationLayers && !checkValidationLayerSupport()) {
            throw std::runtime_error("validation layers requested, but not available!");
        }

        vk::ApplicationInfo applicationInfo{ .pApplicationName      = kAppName.c_str(),
                                             .applicationVersion    = 1,
                                             .pEngineName           = kEngineName.c_str(), 
                                             .engineVersion         = 1,
                                             .apiVersion            = VK_API_VERSION_1_1 };

        vk::InstanceCreateInfo instanceCreateInfo{ .pApplicationInfo = &applicationInfo };

        vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

        if (kEnableValidationLayers) {
            instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
            instanceCreateInfo.ppEnabledLayerNames = kValidationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            instanceCreateInfo.pNext = (vk::DebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
        }
        else {
            instanceCreateInfo.enabledLayerCount = 0;
            instanceCreateInfo.pNext = nullptr;
        }

        // "Vulkan is a platform agnostic API, which means that you need an extension
        //  to interface with the window system. GLFW has a handy built-in function
        //  that returns the extension(s) it needs to do ..."

        auto extensions = getRequiredExtensions();
        instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        instanceCreateInfo.ppEnabledExtensionNames = extensions.data();

        std::vector<vk::ExtensionProperties> avaliableExtensions =  vk::enumerateInstanceExtensionProperties(nullptr);
        std::cout << "available extensions:\n";
        for (const auto& extension : avaliableExtensions) {
            std::cout << '\t' << extension.extensionName << '\n';
        }

        instance_ = vk::createInstance(instanceCreateInfo, nullptr);
    }

    void createSurface() {
        VkSurfaceKHR surface;

        if (glfwCreateWindowSurface(static_cast<VkInstance>(instance_), window_, nullptr, &surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface!");
        }

        surface_ = vk::SurfaceKHR(surface);
    }

    void createLogicalDevice() {
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);

        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
        std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};
        float queuePriority = 1.0f;

        for (uint32_t queueFamily : uniqueQueueFamilies) {
            vk::DeviceQueueCreateInfo queueCreateInfo { .queueFamilyIndex = queueFamily,
                                                        .queueCount = 1,
                                                        .pQueuePriorities = &queuePriority };
            queueCreateInfos.push_back(queueCreateInfo);
        }
        
        // The currently available drivers will only allow you to create a small number of queues 
        // for each queue family and you don't really need more than one. That's because 
        // you can create all of the command buffers on multiple threads and then 
        // submit them all at once on the main thread with a single low-overhead call."

        vk::PhysicalDeviceFeatures deviceFeatures{};

        vk::DeviceCreateInfo createInfo{ .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
                                         .pQueueCreateInfos = queueCreateInfos.data(),
                                         .enabledExtensionCount = static_cast<uint32_t>(kDeviceExtensions.size()),
                                         .ppEnabledExtensionNames = kDeviceExtensions.data(),
                                         .pEnabledFeatures = &deviceFeatures };

        if (kEnableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
            createInfo.ppEnabledLayerNames = kValidationLayers.data();
        }
        else {
            createInfo.enabledLayerCount = 0;
        }

        if (physicalDevice_.createDevice(&createInfo, nullptr, &device_) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create logical device!");
        }

        graphicsQueue_ = device_.getQueue(indices.graphicsFamily.value(), 0);
        presentQueue_ = device_.getQueue(indices.presentFamily.value(), 0);

    }

    void mainLoop() {
        while (!glfwWindowShouldClose(window_)) {
            glfwPollEvents();
            drawFrame();
        }

        device_.waitIdle();
    }

    void cleanup() {
        cleanupSwapChain();

        device_.destroyImage(textureImage_, nullptr);
        device_.freeMemory(textureImageMemory_, nullptr);

        for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
            device_.destroyBuffer(uniformBuffers_[i], nullptr);
            device_.freeMemory(uniformBuffersMemory_[i], nullptr);
        }

        device_.destroyDescriptorPool(descriptorPool_, nullptr);
        device_.destroyDescriptorSetLayout(descriptorSetLayout_, nullptr);

        device_.destroyBuffer(vertexBuffer_, nullptr);
        device_.freeMemory(vertexBufferMemory_, nullptr);
        device_.destroyBuffer(indexBuffer_, nullptr);
        device_.freeMemory(indexBufferMemory_, nullptr);

        device_.destroyPipeline(graphicsPipeline_, nullptr);
        device_.destroyPipelineLayout(pipelineLayout_, nullptr);

        device_.destroyRenderPass(renderPass_, nullptr);

        for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
            device_.destroySemaphore(imageAvailableSemaphores_[i], nullptr);
            device_.destroySemaphore(renderFinishedSemaphores_[i], nullptr);
            device_.destroyFence(inFlightFences_[i], nullptr);
        }

        device_.destroyCommandPool(commandPool_, nullptr);

        device_.destroy(nullptr);

        if (kEnableValidationLayers) {
            DestroyDebugUtilsMessengerEXT(instance_, debugMessenger_, nullptr);
        }

        instance_.destroySurfaceKHR(surface_, nullptr);
        instance_.destroy(nullptr);

        glfwDestroyWindow(window_);
        glfwTerminate();
    }

    bool checkValidationLayerSupport() {
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

    std::vector<const char*> getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (kEnableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallBack(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,    // verbose < info < warning < error
        VkDebugUtilsMessageTypeFlagsEXT meessageType,              // general, validation, performance
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, // {message, object handles, object count}
        void* pUserData) {

        // if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
        return VK_FALSE;
    }

    void populateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo) {
        createInfo = { 
            .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | 
                               vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                               vk::DebugUtilsMessageSeverityFlagBitsEXT::eError,
            .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | 
                           vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation | 
                           vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
            .pfnUserCallback = debugCallBack };
    }

    void setupDebugMessenger() {
        if (!kEnableValidationLayers) return;

        vk::DebugUtilsMessengerCreateInfoEXT createInfo{};
        populateDebugMessengerCreateInfo(createInfo);
        
        if (CreateDebugUtilsMessengerEXT(instance_, &createInfo, nullptr, &debugMessenger_) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug messenger!");
        }
    }

    void pickPhysicalDevice() {
        std::vector<vk::PhysicalDevice> devices = instance_.enumeratePhysicalDevices();
        if (devices.empty()) {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        for (const auto& device : devices) {
            if (isDeviceSuitable(device)) {
                physicalDevice_ = device;
                break;
            }
        }

        if (!physicalDevice_) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
        
    }

    bool isDeviceSuitable(vk::PhysicalDevice device) {

        // vk::PhysicalDeviceProperties deviceProperties = device.getProperties();
        // vk::PhysicalDeviceFeatures deviceFeatures = device.getFeatures();
        // bool suitable = deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu &&
        //                 deviceFeatures.geometryShader;

        QueueFamilyIndices indices = findQueueFamilies(device);
        bool extensionsSupported = checkDeviceExtensionSupport(device);
        bool swapChainAdequate = false;
        if (extensionsSupported) {
            SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
            swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
        }
        return indices.isComplete() && extensionsSupported && swapChainAdequate;
    }

    bool checkDeviceExtensionSupport(vk::PhysicalDevice device) {
        std::vector<vk::ExtensionProperties> availableExtensions = device.enumerateDeviceExtensionProperties(nullptr);
        std::set<std::string> requiredExtensions(kDeviceExtensions.begin(), kDeviceExtensions.end());

        for (const auto& extension : availableExtensions) {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device) {
        QueueFamilyIndices indices;

        std::vector<vk::QueueFamilyProperties> queueFamilies = device.getQueueFamilyProperties();

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            
            if (device.getSurfaceSupportKHR(i, surface_)) {
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

    struct SwapChainSupportDetails {
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice_);

        vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        vk::SwapchainCreateInfoKHR createInfo { .surface = surface_,
                                                .minImageCount = imageCount,
                                                .imageFormat = surfaceFormat.format,
                                                .imageColorSpace = surfaceFormat.colorSpace,
                                                .imageExtent = extent,
                                                .imageArrayLayers = 1,
                                                .imageUsage = vk::ImageUsageFlagBits::eColorAttachment };
        
        QueueFamilyIndices indices = findQueueFamilies(physicalDevice_);
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else {
            createInfo.imageSharingMode = vk::SharingMode::eExclusive;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices = nullptr;
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (device_.createSwapchainKHR(&createInfo, nullptr, &swapChain_) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create swap chain!");
        }

        if (device_.getSwapchainImagesKHR(swapChain_, &imageCount, nullptr) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to get swap chain imageCount!");
        }
        swapChainImages_.resize(imageCount);
        if (device_.getSwapchainImagesKHR(swapChain_, &imageCount, swapChainImages_.data()) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to get swap chain!");
        }
        swapChainImageFormat_ = surfaceFormat.format;
        swapChainExtent_ = extent;
    }

    SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device) {
        SwapChainSupportDetails details;

        details.capabilities = device.getSurfaceCapabilitiesKHR(surface_);
        details.formats = device.getSurfaceFormatsKHR(surface_);
        details.presentModes = device.getSurfacePresentModesKHR(surface_);

        return details;
    }

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
                availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }

    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
                return availablePresentMode;
            }
        }
        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        }
        else {
            int width, height;
            glfwGetFramebufferSize(window_, &width, &height);

            vk::Extent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
            return actualExtent;
        }
    }

    void createImageViews() {
        swapChainImageViews_.resize(swapChainImages_.size());

        for (size_t i = 0; i < swapChainImages_.size(); ++i) {
            swapChainImageViews_[i] = createImageView(swapChainImages_[i], swapChainImageFormat_, vk::ImageAspectFlagBits::eColor);
        }
    }

    void createRenderPass() {
        vk::AttachmentDescription colorAttachment { .format = swapChainImageFormat_, 
                                                    .samples = vk::SampleCountFlagBits::e1,
                                                    .loadOp = vk::AttachmentLoadOp::eClear,
                                                    .storeOp = vk::AttachmentStoreOp::eStore,
                                                    .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                                                    .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                                                    .initialLayout = vk::ImageLayout::eUndefined,
                                                    .finalLayout = vk::ImageLayout::ePresentSrcKHR };
        
        vk::AttachmentReference colorAttachmentRef { .attachment = 0,
                                                     .layout = vk::ImageLayout::eColorAttachmentOptimal };
        
        vk::AttachmentDescription depthAttachment { .format = findDepthFormat(),
                                                    .samples = vk::SampleCountFlagBits::e1,
                                                    .loadOp = vk::AttachmentLoadOp::eClear,
                                                    .storeOp = vk::AttachmentStoreOp::eDontCare,
                                                    .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
                                                    .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
                                                    .initialLayout = vk::ImageLayout::eUndefined,
                                                    .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal };

        vk::AttachmentReference depthAttachmentRef { .attachment = 1,
                                                     .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal };

        vk::SubpassDescription subpass = { .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
                                           .colorAttachmentCount = 1,
                                           .pColorAttachments = &colorAttachmentRef,
                                           .pDepthStencilAttachment = &depthAttachmentRef };
        
        vk::SubpassDependency dependency { .srcSubpass = VK_SUBPASS_EXTERNAL,
                                           .dstSubpass = 0,
                                           .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
                                           .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
                                           .srcAccessMask = vk::AccessFlagBits::eNone,
                                           .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite };
        
        std::array<vk::AttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
        vk::RenderPassCreateInfo renderPassInfo { .attachmentCount = static_cast<uint32_t>(attachments.size()),
                                                  .pAttachments = attachments.data(),
                                                  .subpassCount = 1,
                                                  .pSubpasses = &subpass,
                                                  .dependencyCount = 1,
                                                  .pDependencies = &dependency };
        
        if (device_.createRenderPass(&renderPassInfo, nullptr, &renderPass_) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create render pass!");
        }

    }

    void createDescriptorSetLayout() {
        vk::DescriptorSetLayoutBinding uboLayoutBinding { .binding = 0,
                                                          .descriptorType = vk::DescriptorType::eUniformBuffer,
                                                          .descriptorCount = 1,
                                                          .stageFlags = vk::ShaderStageFlagBits::eVertex,
                                                          .pImmutableSamplers = nullptr };
        
        vk::DescriptorSetLayoutCreateInfo layoutInfo { .bindingCount = 1,
                                                       .pBindings = &uboLayoutBinding };
        
        if (device_.createDescriptorSetLayout(&layoutInfo, nullptr, &descriptorSetLayout_) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    void createGraphicsPipeline() {
        auto vertShaderCode = readFile("shaders/shader.vert.spv");
        auto fragShaderCode = readFile("shaders/shader.frag.spv");

        vk::ShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        vk::ShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo { .stage = vk::ShaderStageFlagBits::eVertex,
                                                                .module = vertShaderModule,
                                                                .pName = "main" };

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo { .stage = vk::ShaderStageFlagBits::eFragment,
                                                                .module = fragShaderModule,
                                                                .pName = "main" };
        
        vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
        
        // Fixed function: Vertex input
        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescription = Vertex::getAttributeDescriptions();
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo { .vertexBindingDescriptionCount = 1,
                                                                 .pVertexBindingDescriptions = &bindingDescription,
                                                                 .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size()),
                                                                 .pVertexAttributeDescriptions = attributeDescription.data() };
        
        // Fixed function: Input assembly
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly { .topology = vk::PrimitiveTopology::eTriangleList,
                                                                 .primitiveRestartEnable = VK_FALSE };
        
        // Fixed function: Viewport and scissor        
        std::vector<vk::DynamicState> dynamicStates = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };

        vk::PipelineDynamicStateCreateInfo dynamicState { .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                                                          .pDynamicStates = dynamicStates.data() };

        vk::PipelineViewportStateCreateInfo viewportState { .viewportCount = 1,
                                                            .scissorCount = 1 };

        // Fixed function: Rasterizer
        vk::PipelineRasterizationStateCreateInfo rasterizer { .depthClampEnable = VK_FALSE,
                                                              .rasterizerDiscardEnable = VK_FALSE,
                                                              .polygonMode = vk::PolygonMode::eFill,
                                                              .cullMode = vk::CullModeFlagBits::eBack,
                                                              .frontFace = vk::FrontFace::eCounterClockwise,
                                                              .depthBiasEnable = VK_FALSE,
                                                              .depthBiasConstantFactor = 0.0f,
                                                              .depthBiasClamp = 0.0f,
                                                              .depthBiasSlopeFactor = 0.0f,
                                                              .lineWidth = 1.0f };
        
        // Fixed function: Multisampling
        vk::PipelineMultisampleStateCreateInfo multisampling { .rasterizationSamples = vk::SampleCountFlagBits::e1,
                                                               .sampleShadingEnable = VK_FALSE,
                                                               .minSampleShading = 1.0f,
                                                               .pSampleMask = nullptr,
                                                               .alphaToCoverageEnable = VK_FALSE,
                                                               .alphaToOneEnable = VK_FALSE };

        // Fixed function: Depth and stencil testing
        vk::PipelineDepthStencilStateCreateInfo depthStencil { .depthTestEnable = VK_TRUE,
                                                               .depthWriteEnable = VK_TRUE,
                                                               .depthCompareOp = vk::CompareOp::eLess,
                                                               .depthBoundsTestEnable = VK_FALSE,
                                                               .stencilTestEnable = VK_FALSE,
                                                               .front = {},
                                                               .back = {},
                                                               .minDepthBounds = 0.0f,
                                                               .maxDepthBounds = 1.0f };

        // Fixed function: Color blending
        vk::PipelineColorBlendAttachmentState colorBlendAttachment { .blendEnable = VK_FALSE,
                                                                     .srcColorBlendFactor = vk::BlendFactor::eOne,
                                                                     .dstColorBlendFactor = vk::BlendFactor::eZero,
                                                                     .colorBlendOp = vk::BlendOp::eAdd,
                                                                     .srcAlphaBlendFactor = vk::BlendFactor::eOne,
                                                                     .dstAlphaBlendFactor = vk::BlendFactor::eZero,
                                                                     .alphaBlendOp = vk::BlendOp::eAdd,
                                                                     .colorWriteMask = vk::ColorComponentFlagBits::eR 
                                                                                       | vk::ColorComponentFlagBits::eG 
                                                                                       | vk::ColorComponentFlagBits::eB 
                                                                                       | vk::ColorComponentFlagBits::eA };

        vk::PipelineColorBlendStateCreateInfo colorBlending { .logicOpEnable = VK_FALSE,
                                                              .logicOp = vk::LogicOp::eCopy,
                                                              .attachmentCount = 1,
                                                              .pAttachments = &colorBlendAttachment };
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo { .setLayoutCount = 1,
                                                          .pSetLayouts = &descriptorSetLayout_,
                                                          .pushConstantRangeCount = 0,
                                                          .pPushConstantRanges = nullptr };

        if (device_.createPipelineLayout(&pipelineLayoutInfo, nullptr, &pipelineLayout_) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        vk::GraphicsPipelineCreateInfo pipelineInfo { .stageCount = 2,
                                                      .pStages = shaderStages,
                                                      .pVertexInputState = &vertexInputInfo,
                                                      .pInputAssemblyState = &inputAssembly,
                                                      .pViewportState = &viewportState,
                                                      .pRasterizationState = &rasterizer,
                                                      .pMultisampleState = &multisampling,
                                                      .pDepthStencilState = &depthStencil,
                                                      .pColorBlendState = &colorBlending,
                                                      .pDynamicState = &dynamicState,
                                                      .layout = pipelineLayout_,
                                                      .renderPass = renderPass_,
                                                      .subpass = 0,
                                                      .basePipelineHandle = VK_NULL_HANDLE,
                                                      .basePipelineIndex = -1 };

        if (device_.createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline_) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        device_.destroyShaderModule(fragShaderModule, nullptr);
        device_.destroyShaderModule(vertShaderModule, nullptr);
    }

    vk::ShaderModule createShaderModule(const std::vector<char>& code) {
        vk::ShaderModuleCreateInfo createInfo { .codeSize = code.size(),
                                                .pCode = reinterpret_cast<const uint32_t*>(code.data()) };
        
        vk::ShaderModule shaderModule;
        if (device_.createShaderModule(&createInfo, nullptr, &shaderModule) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

    void createFramebuffers() {
        swapChainFramebuffers_.resize(swapChainImageViews_.size());

        for (size_t i = 0; i < swapChainImageViews_.size(); ++i) {
            // Color attachment differs for every swap chain image,
            // but the same depth image can be used by all of them
            // because only a single subpass is running at the same time due to out semaphores
            std::array<vk::ImageView, 2> attachments = {
                swapChainImageViews_[i],
                depthImageView_
            };

            vk::FramebufferCreateInfo framebufferInfo { .renderPass = renderPass_,
                                                        .attachmentCount = static_cast<uint32_t>(attachments.size()),
                                                        .pAttachments = attachments.data(),
                                                        .width = swapChainExtent_.width,
                                                        .height = swapChainExtent_.height,
                                                        .layers = 1 };

            if (device_.createFramebuffer(&framebufferInfo, nullptr, &swapChainFramebuffers_[i]) != vk::Result::eSuccess) {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice_);

        vk::CommandPoolCreateInfo poolInfo { .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                             .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value() };
        
        if (device_.createCommandPool(&poolInfo, nullptr, &commandPool_) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void createTextureImage() {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load("textures/texture.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        vk::DeviceSize imageSize = texWidth * texHeight * 4;

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        vk::Buffer stagingBuffer;
        vk::DeviceMemory stagingBufferMemory;
        createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

        void* data;
        data = device_.mapMemory(stagingBufferMemory, 0, imageSize);
            memcpy(data, pixels, static_cast<size_t>(imageSize));
        device_.unmapMemory(stagingBufferMemory);

        stbi_image_free(pixels);

        createImage(texWidth, texHeight, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, textureImage_, textureImageMemory_);
    
        // Transition the texture image to ImageLayout::eTransferDstOptimal
        // The image was create with the ImageLayout::eUndefined layout
        // Because we don't care about its contents before performing the copy operation
        transitionImageLayout(textureImage_, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        // Execute the staging buffer to image copy operation
        copyBufferToImage(stagingBuffer, textureImage_, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        
        // To be able to start sampling from the texture image in the shader,
        // need one last transition to prepare it for shader access
        transitionImageLayout(textureImage_, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
    
        device_.destroyBuffer(stagingBuffer, nullptr);
        device_.freeMemory(stagingBufferMemory, nullptr);
    }

    void createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Image& image, vk::DeviceMemory& imageMemory) {
        vk::ImageCreateInfo imageInfo { .imageType = vk::ImageType::e2D, 
                                        .format = format,
                                        .extent = { .width = width,
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
                                        .initialLayout = vk::ImageLayout::eUndefined };
        
        if (device_.createImage(&imageInfo, nullptr, &image) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create image!");
        }

        vk::MemoryRequirements memRequirements;
        device_.getImageMemoryRequirements(image, &memRequirements);

        vk::MemoryAllocateInfo allocInfo { .allocationSize = memRequirements.size,
                                           .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties) };
        
        if (device_.allocateMemory(&allocInfo, nullptr, &imageMemory) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        device_.bindImageMemory(image, imageMemory, 0);
    }

    vk::ImageView createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags) {
        vk::ImageViewCreateInfo viewInfo { .image = image,
                                           .viewType = vk::ImageViewType::e2D,
                                           .format = format,
                                           .subresourceRange = { .aspectMask = aspectFlags,
                                                                 .baseMipLevel = 0,
                                                                 .levelCount = 1,
                                                                 .baseArrayLayer = 0,
                                                                 .layerCount = 1 } };

        vk::ImageView imageView;
        if (device_.createImageView(&viewInfo, nullptr, &imageView) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create image view!");
        }

        return imageView;
    }

    void transitionImageLayout(vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
        
        vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

        vk::ImageMemoryBarrier barrier { .oldLayout = oldLayout,
                                         .newLayout = newLayout,
                                         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                         .image = image,
                                         .subresourceRange = { .baseMipLevel = 0,
                                                               .levelCount = 1,
                                                               .baseArrayLayer = 0,
                                                               .layerCount = 1 } };

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

        // Transfer writes that don't need to wait on anything
        if (oldLayout == vk::ImageLayout::eUndefined && 
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
        
        else {
            throw std::invalid_argument("unsupported layout transition!");
        }
        
        commandBuffer.pipelineBarrier(sourceStage, destinationStage, 
                                      {},
                                      0, nullptr, 
                                      0, nullptr,
                                      1, &barrier);

        endSingleTimeCommands(commandBuffer);
    }
    
    void createDepthResources() {
        vk::Format depthFormat = findDepthFormat();
        createImage(swapChainExtent_.width, swapChainExtent_.height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, depthImage_, depthImageMemory_);
        depthImageView_ = createImageView(depthImage_, depthFormat, vk::ImageAspectFlagBits::eDepth);

        transitionImageLayout(depthImage_, depthFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
    }

    vk::Format findDepthFormat() {
        return findSupportedFormat(
            {vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
            vk::ImageTiling::eOptimal,
            vk::FormatFeatureFlagBits::eDepthStencilAttachment
        );
    }

    vk::Format findSupportedFormat(const std::vector<vk::Format>& candidates, vk::ImageTiling tiling, vk::FormatFeatureFlags features) {
        for (vk::Format format : candidates) {
            vk::FormatProperties props;
            physicalDevice_.getFormatProperties(format, &props);

            if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
                return format;
            } else if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format!");
    }

    bool hasStencilComponent(vk::Format format) {
        return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
    }

    void createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Buffer& buffer, vk::DeviceMemory& bufferMemory) {
        vk::BufferCreateInfo bufferInfo { .size = size,
                                          .usage = usage,
                                          .sharingMode = vk::SharingMode::eExclusive };
        
        if (device_.createBuffer(&bufferInfo, nullptr, &buffer) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create vertex buffer!");
        }

        vk::MemoryRequirements memRequirements;
        device_.getBufferMemoryRequirements(buffer, &memRequirements);

        vk::MemoryAllocateInfo allocInfo{ .allocationSize = memRequirements.size,
                                          .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties) };
        
        if (device_.allocateMemory(&allocInfo, nullptr, &bufferMemory) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to allocate vertex buffer memory!");
        }

        device_.bindBufferMemory(buffer, bufferMemory, 0);
    }

    void createVertexBuffer() {
        vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
        
        vk::Buffer stagingBuffer;
        vk::DeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

        void* data;
        data = device_.mapMemory(stagingBufferMemory, 0, bufferSize);
            memcpy(data, vertices.data(), (size_t)bufferSize);
        device_.unmapMemory(stagingBufferMemory);

        createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, vertexBuffer_, vertexBufferMemory_);

        copyBuffer(stagingBuffer, vertexBuffer_, bufferSize);

        device_.destroyBuffer(stagingBuffer, nullptr);
        device_.freeMemory(stagingBufferMemory, nullptr);
    }

    void createIndexBuffer() {
        vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();
        
        vk::Buffer stagingBuffer;
        vk::DeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

        void* data;
        data = device_.mapMemory(stagingBufferMemory, 0, bufferSize);
            memcpy(data, indices.data(), (size_t)bufferSize);
        device_.unmapMemory(stagingBufferMemory);

        createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, indexBuffer_, indexBufferMemory_);

        copyBuffer(stagingBuffer, indexBuffer_, bufferSize);

        device_.destroyBuffer(stagingBuffer, nullptr);
        device_.freeMemory(stagingBufferMemory, nullptr);
    }

    void createUniformBuffers() {
        vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

        uniformBuffers_.resize(kMaxFramesInFlight);
        uniformBuffersMemory_.resize(kMaxFramesInFlight);
        uniformBuffersMapped_.resize(kMaxFramesInFlight);

        for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
            createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, uniformBuffers_[i], uniformBuffersMemory_[i]);

            uniformBuffersMapped_[i] = device_.mapMemory(uniformBuffersMemory_[i], 0, bufferSize);
        }
    }

    void copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size) {
        vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

        vk::BufferCopy copyRegion { .srcOffset = 0,
                                    .dstOffset = 0,
                                    .size = size };
        commandBuffer.copyBuffer(srcBuffer, dstBuffer, 1, &copyRegion);

        endSingleTimeCommands(commandBuffer);
    }

    void copyBufferToImage(vk::Buffer buffer, vk::Image image, uint32_t width, uint32_t height) {
        vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

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

        endSingleTimeCommands(commandBuffer);
    }

    vk::CommandBuffer beginSingleTimeCommands() {
        vk::CommandBufferAllocateInfo allocInfo { .commandPool = commandPool_,
                                                  .level = vk::CommandBufferLevel::ePrimary,
                                                  .commandBufferCount = 1 };

        vk::CommandBuffer commandBuffer;
        if (device_.allocateCommandBuffers(&allocInfo, &commandBuffer) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to allocate command buffer!");
        }

        vk::CommandBufferBeginInfo beginInfo { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
        if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to begin command buffer!");
        }

        return commandBuffer;
    }

    void endSingleTimeCommands(vk::CommandBuffer commandBuffer) {
        commandBuffer.end();

        vk::SubmitInfo submitInfo { .commandBufferCount = 1,
                                    .pCommandBuffers = &commandBuffer };
        if (graphicsQueue_.submit(1, &submitInfo, VK_NULL_HANDLE) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to submit command buffer to graphics queue!");
        }
        graphicsQueue_.waitIdle();

        device_.freeCommandBuffers(commandPool_, 1, &commandBuffer);
    }

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
        vk::PhysicalDeviceMemoryProperties memProperties;
        physicalDevice_.getMemoryProperties(&memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties ) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    void createDescriptorPool() {
        vk::DescriptorPoolSize poolSize { .descriptorCount = static_cast<uint32_t>(kMaxFramesInFlight) };
        vk::DescriptorPoolCreateInfo poolInfo { .maxSets = static_cast<uint32_t>(kMaxFramesInFlight),
                                                .poolSizeCount = 1,
                                                .pPoolSizes = &poolSize };
        
        if (device_.createDescriptorPool(&poolInfo, nullptr, &descriptorPool_) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void createDescriptorSets() {
        std::vector<vk::DescriptorSetLayout> layouts(kMaxFramesInFlight, descriptorSetLayout_);
        vk::DescriptorSetAllocateInfo allocInfo { .descriptorPool = descriptorPool_,
                                                  .descriptorSetCount = static_cast<uint32_t>(kMaxFramesInFlight),
                                                  .pSetLayouts = layouts.data() };
        
        descriptorSets_.resize(kMaxFramesInFlight);

        if (device_.allocateDescriptorSets(&allocInfo, descriptorSets_.data()) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to allocate descriptor sets!");
        }

        for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
            vk::DescriptorBufferInfo bufferInfo { .buffer = uniformBuffers_[i],
                                                  .offset = 0,
                                                  .range = sizeof(UniformBufferObject) };
            
            vk::WriteDescriptorSet descriptorWrite { .dstSet = descriptorSets_[i],
                                                     .dstBinding = 0,
                                                     .dstArrayElement = 0,
                                                     .descriptorCount = 1,
                                                     .descriptorType = vk::DescriptorType::eUniformBuffer,
                                                     .pImageInfo = nullptr,
                                                     .pBufferInfo = &bufferInfo,
                                                     .pTexelBufferView = nullptr };
            
            device_.updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
        }
    }

    void createCommandBuffers() {
        commandBuffers_.resize(kMaxFramesInFlight);

        vk::CommandBufferAllocateInfo allocInfo { .commandPool = commandPool_,
                                                  .level = vk::CommandBufferLevel::ePrimary,
                                                  .commandBufferCount = (uint32_t) commandBuffers_.size() };
        
        if (device_.allocateCommandBuffers(&allocInfo, commandBuffers_.data()) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex) {
        vk::CommandBufferBeginInfo beginInfo { .pInheritanceInfo = nullptr };
        
        if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        std::array<vk::ClearValue, 2> clearValues {};
        clearValues[0].color = vk::ClearColorValue {std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = vk::ClearDepthStencilValue {1.0f, 0};

        vk::RenderPassBeginInfo renderPassInfo { .renderPass = renderPass_,
                                                 .framebuffer = swapChainFramebuffers_[imageIndex],
                                                 .renderArea { .offset = {0, 0},
                                                               .extent = swapChainExtent_},
                                                 .clearValueCount = static_cast<uint32_t>(clearValues.size()),
                                                 .pClearValues = clearValues.data() };
        
        commandBuffer.beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline_);

        vk::Viewport viewport { .x = 0.0f,
                                .y = 0.0f,
                                .width = static_cast<float>(swapChainExtent_.width),
                                .height = static_cast<float>(swapChainExtent_.height),
                                .minDepth = 0.0f,
                                .maxDepth = 1.0f };
        commandBuffer.setViewport(0, 1, &viewport);

        vk::Rect2D scissor { .offset = {0, 0},
                             .extent = swapChainExtent_ };
        commandBuffer.setScissor(0, 1, &scissor);

        vk::Buffer vertexBuffers[] = {vertexBuffer_};
        vk::DeviceSize offsets[] = {0};
        commandBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);
        commandBuffer.bindIndexBuffer(indexBuffer_, 0, vk::IndexType::eUint32);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout_, 0, 1, &descriptorSets_[currentFrame_], 0, nullptr);
        commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
        
        commandBuffer.endRenderPass();

        try { 
            commandBuffer.end();
        } catch (vk::SystemError err) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void createSyncObjects() {
        imageAvailableSemaphores_.resize(kMaxFramesInFlight);
        renderFinishedSemaphores_.resize(kMaxFramesInFlight);
        inFlightFences_.resize(kMaxFramesInFlight);

        vk::SemaphoreCreateInfo semaphoreInfo{};
        vk::FenceCreateInfo fenceInfo{ .flags = vk::FenceCreateFlagBits::eSignaled };

        for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
            if (device_.createSemaphore(&semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]) != vk::Result::eSuccess ||
                device_.createSemaphore(&semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]) != vk::Result::eSuccess ||
                device_.createFence(&fenceInfo, nullptr, &inFlightFences_[i]) != vk::Result::eSuccess) {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }

    void drawFrame() {
        vk::Result result;

        // "At the start of the frame, we want to wait until the previous frame has finished"
        do {
            result = device_.waitForFences(1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);
        } while(result == vk::Result::eTimeout);
        assert(result == vk::Result::eSuccess);

        uint32_t imageIndex;
        // "...to be signaled when the presentation engine is finished using the image. 
        // That's the point in time where we can start drawing to it."
        result = device_.acquireNextImageKHR(swapChain_, UINT64_MAX, imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &imageIndex);
        if (result == vk::Result::eErrorOutOfDateKHR) {
            recreateSwapChain();
            return;
        } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        // only reset the fence if we are submitting work
        if (device_.resetFences(1, &inFlightFences_[currentFrame_]) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to reset fence!");
        }

        commandBuffers_[currentFrame_].reset();
        recordCommandBuffer(commandBuffers_[currentFrame_], imageIndex);

        updateUniformBuffer(currentFrame_);

        vk::Semaphore waitSemaphores[] = {imageAvailableSemaphores_[currentFrame_]};
        vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
        vk::Semaphore signalSemaphores[] = {renderFinishedSemaphores_[currentFrame_]};

        vk::SubmitInfo submitInfo{ .waitSemaphoreCount = 1,
                                   .pWaitSemaphores = waitSemaphores,
                                   .pWaitDstStageMask = waitStages,
                                   .commandBufferCount = 1,
                                   .pCommandBuffers = &commandBuffers_[currentFrame_],
                                   .signalSemaphoreCount = 1,
                                   .pSignalSemaphores = signalSemaphores };
        
        if (graphicsQueue_.submit(1, &submitInfo, inFlightFences_[currentFrame_]) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        vk::SwapchainKHR swapChains[] {swapChain_};
        vk::PresentInfoKHR presentInfo { .waitSemaphoreCount = 1,
                                         .pWaitSemaphores = signalSemaphores,
                                         .swapchainCount = 1,
                                         .pSwapchains = swapChains,
                                         .pImageIndices = &imageIndex,
                                         .pResults = nullptr };
        
        result = presentQueue_.presentKHR(&presentInfo);
    
        if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized_) {
            framebufferResized_ = false;
            recreateSwapChain();
        } else if (result != vk::Result::eSuccess) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        currentFrame_ = (currentFrame_ + 1) % kMaxFramesInFlight;
    }

    void updateUniformBuffer(uint32_t currentImage) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent_.width / (float)swapChainExtent_.height, 0.1f, 10.0f);

        // GLM's Y coordinate of the clip coordinates is inverted
        // To compensate this, flip the sign on the scaling factor of the Y axis in the proj matrix.
        ubo.proj[1][1] *= -1;

        memcpy(uniformBuffersMapped_[currentImage], &ubo, sizeof(ubo));

    }

    void cleanupSwapChain() {
        device_.destroyImageView(depthImageView_, nullptr);
        device_.destroyImage(depthImage_, nullptr);
        device_.freeMemory(depthImageMemory_, nullptr);

        for (size_t i = 0; i < swapChainFramebuffers_.size(); ++i) {
            device_.destroyFramebuffer(swapChainFramebuffers_[i], nullptr);
        }

        for (size_t i = 0; i < swapChainImageViews_.size(); ++i) {
            device_.destroyImageView(swapChainImageViews_[i], nullptr);
        }

        device_.destroySwapchainKHR(swapChain_, nullptr);
    }

    void recreateSwapChain() {
        int width = 0, height = 0;
        glfwGetFramebufferSize(window_, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(window_, &width, &height);
            glfwWaitEvents();
        }

        device_.waitIdle();

        cleanupSwapChain();

        createSwapChain();
        createImageViews();
        createDepthResources();
        createFramebuffers();
    }
};

int main() {
    VRBApplication app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}