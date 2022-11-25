#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

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
    glm::vec2 pos;
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
        attributeDescriptions[0].format = vk::Format::eR32G32Sfloat;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        return attributeDescriptions;
    }
};

const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

const std::vector<uint32_t> indices = {
    0, 1, 2, 2, 3, 0
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
        createGraphicsPipeline();
        createFramebuffers();
        createCommandPool();
        createVertexBuffer();
        createIndexBuffer();
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
            vk::ImageViewCreateInfo createInfo { .image = swapChainImages_[i],
                                                 .viewType = vk::ImageViewType::e2D,
                                                 .format = swapChainImageFormat_,
                                                 .components { .r = vk::ComponentSwizzle::eIdentity, 
                                                               .g = vk::ComponentSwizzle::eIdentity, 
                                                               .b = vk::ComponentSwizzle::eIdentity, 
                                                               .a = vk::ComponentSwizzle::eIdentity },
                                                 .subresourceRange { .aspectMask = vk::ImageAspectFlagBits::eColor,
                                                                     .baseMipLevel = 0,
                                                                     .levelCount = 1,
                                                                     .baseArrayLayer = 0,
                                                                     .layerCount = 1 } };
        
            if (device_.createImageView(&createInfo, nullptr, &swapChainImageViews_[i]) != vk::Result::eSuccess) {
                throw std::runtime_error("failed to create image views!");
            }
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
        
        vk::SubpassDescription subpass = { .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
                                           .colorAttachmentCount = 1,
                                           .pColorAttachments = &colorAttachmentRef };
        
        vk::SubpassDependency dependency { .srcSubpass = VK_SUBPASS_EXTERNAL,
                                           .dstSubpass = 0,
                                           .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                           .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                           .srcAccessMask = vk::AccessFlagBits::eNone,
                                           .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite };
        
        vk::RenderPassCreateInfo renderPassInfo { .attachmentCount = 1,
                                                  .pAttachments = &colorAttachment,
                                                  .subpassCount = 1,
                                                  .pSubpasses = &subpass,
                                                  .dependencyCount = 1,
                                                  .pDependencies = &dependency };
        
        if (device_.createRenderPass(&renderPassInfo, nullptr, &renderPass_) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create render pass!");
        }

    }

    void createGraphicsPipeline() {
        auto vertShaderCode = readFile("src/shaders/vert.spv");
        auto fragShaderCode = readFile("src/shaders/frag.spv");

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
                                                              .frontFace = vk::FrontFace::eClockwise,
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

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo { .setLayoutCount = 0,
                                                          .pSetLayouts = nullptr,
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
                                                      .pDepthStencilState = nullptr,
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
            vk::ImageView attachments[] = {
                swapChainImageViews_[i]
            };

            vk::FramebufferCreateInfo framebufferInfo { .renderPass = renderPass_,
                                                        .attachmentCount = 1,
                                                        .pAttachments = attachments,
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

    void copyBuffer(vk::Buffer srcBuffer, vk::Buffer dstBuffer, vk::DeviceSize size) {
        vk::CommandBufferAllocateInfo allocInfo { .commandPool = commandPool_,
                                                  .level = vk::CommandBufferLevel::ePrimary,
                                                  .commandBufferCount = 1 };
        
        vk::CommandBuffer commandBuffer;
        device_.allocateCommandBuffers(&allocInfo, &commandBuffer);

        vk::CommandBufferBeginInfo beginInfo { .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
        commandBuffer.begin(&beginInfo);

        vk::BufferCopy copyRegion { .srcOffset = 0,
                                    .dstOffset = 0,
                                    .size = size };
        commandBuffer.copyBuffer(srcBuffer, dstBuffer, 1, &copyRegion);

        commandBuffer.end();

        vk::SubmitInfo submitInfo { .commandBufferCount = 1,
                                    .pCommandBuffers = &commandBuffer };
        graphicsQueue_.submit(1, &submitInfo, VK_NULL_HANDLE);
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

        vk::ClearValue clearColor = { std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f} };
        vk::RenderPassBeginInfo renderPassInfo { .renderPass = renderPass_,
                                                 .framebuffer = swapChainFramebuffers_[imageIndex],
                                                 .renderArea { .offset = {0, 0},
                                                               .extent = swapChainExtent_},
                                                 .clearValueCount = 1,
                                                 .pClearValues = &clearColor };
        
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
        // "At the start of the frame, we want to wait until the previous frame has finished"
        device_.waitForFences(1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        // "...to be signaled when the presentation engine is finished using the image. 
        // That's the point in time where we can start drawing to it."
        vk::Result result = device_.acquireNextImageKHR(swapChain_, UINT64_MAX, imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &imageIndex);
        if (result == vk::Result::eErrorOutOfDateKHR) {
            recreateSwapChain();
            return;
        } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        // only reset the fence if we are submitting work
        device_.resetFences(1, &inFlightFences_[currentFrame_]);

        commandBuffers_[currentFrame_].reset();
        recordCommandBuffer(commandBuffers_[currentFrame_], imageIndex);

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

    void cleanupSwapChain() {
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