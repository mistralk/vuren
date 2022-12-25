#define VULKAN_HPP_NO_CONSTRUCTORS
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>

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
#include <unordered_map>

#include "VulkanContext.hpp"
#include "Utils.hpp"
#include "Timer.hpp"

namespace vrb {

const uint32_t kWidth = 800;
const uint32_t kHeight = 600;
static std::string kAppName = "vrb";
const int kMaxFramesInFlight = 2;

struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    static vk::VertexInputBindingDescription getBindingDescription() {
        vk::VertexInputBindingDescription bindingDescription{ 
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = vk::VertexInputRate::eVertex
        };

        return bindingDescription;
    }

    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions() {
        std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions{};

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset = offsetof(Vertex, color);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = vk::Format::eR32G32Sfloat;
        attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }

    bool operator==(const Vertex& other) const {
        return pos == other.pos && color == other.color && texCoord == other.texCoord;
    }
};

struct UniformBufferObject {
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};
} // namespace vrb

namespace std {
template<> struct hash<vrb::Vertex> {
    size_t operator()(vrb::Vertex const& vertex) const {
        return ((hash<glm::vec3>()(vertex.pos) ^
                (hash<glm::vec3>()(vertex.color) << 1)) >> 1) ^
                (hash<glm::vec2>()(vertex.texCoord) << 1);
    }
};
} // namespace std

namespace vrb {

class Application {
private:
    GLFWwindow* m_pWindow;

    VulkanContext m_vkContext;

    vk::SwapchainKHR m_swapChain;
    std::vector<vk::Image> m_swapChainImages;
    vk::Format m_swapChainImageFormat;
    vk::Extent2D m_swapChainExtent;
    std::vector<vk::ImageView> m_swapChainImageViews;
    uint32_t m_imageCount;
    std::vector<vk::Framebuffer> m_swapChainFramebuffers;

    vk::CommandPool m_commandPool;
    std::vector<vk::CommandBuffer> m_commandBuffers;

    std::vector<vk::Semaphore> m_imageAvailableSemaphores;
    std::vector<vk::Semaphore> m_renderFinishedSemaphores;
    std::vector<vk::Fence> m_inFlightFences;

    vk::Buffer m_vertexBuffer;
    vk::DeviceMemory m_vertexBufferMemory;
    vk::Buffer m_indexBuffer;
    vk::DeviceMemory m_indexBufferMemory;

    std::vector<vk::Buffer> m_uniformBuffers;
    std::vector<vk::DeviceMemory> m_uniformBuffersMemory;
    std::vector<void*> m_uniformBuffersMapped;

    // for model texture
    vk::Image m_textureImage;
    vk::ImageView m_textureImageView;
    vk::DeviceMemory m_textureImageMemory;
    vk::Sampler m_textureSampler;

    // for the offscreen pass
    vk::RenderPass m_offscreenRenderPass {VK_NULL_HANDLE};
    vk::Framebuffer m_offscreenFramebuffer;
    vk::PipelineLayout m_offscreenPipelineLayout;
    vk::Pipeline m_offscreenPipeline;

    vk::DescriptorSetLayout m_offscreenDescriptorSetLayout;
    vk::DescriptorSet m_offscreenDescriptorSets;
    vk::DescriptorPool m_offscreenDescriptorPool;

    vk::Image m_colorImage;
    vk::DeviceMemory m_colorImageMemory;
    vk::ImageView m_colorImageView;
    vk::Sampler m_colorImageSampler;
    vk::DescriptorImageInfo m_colorImageDescriptor;

    vk::Image m_depthImage;
    vk::DeviceMemory m_depthImageMemory;
    vk::ImageView m_depthImageView;

    uint32_t m_currentFrame = 0;
    bool m_framebufferResized = false;

    // for the final pass and gui
    // this pass is directly presented into swap chain framebuffers
    vk::RenderPass m_finalRenderPass {VK_NULL_HANDLE};
    vk::Pipeline m_finalPipeline;
    vk::PipelineLayout m_finalPipelineLayout;
    vk::DescriptorSetLayout m_finalDescriptorSetLayout;
    std::vector<vk::DescriptorSet> m_finalDescriptorSets;
    vk::DescriptorPool m_finalDescriptorPool;
    vk::DescriptorPool m_imguiDescriptorPool; // additional descriptor pool for imgui

    // for obj models
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;

public:
    Application() {
    }

    void run() {
        initWindow();
        initVulkan();
        initImGui();
        mainLoop();
        cleanup();
    }

private:
    void initImGui() {
        // create descriptor pool for imgui
        vk::DescriptorPoolSize poolSizes[] = {
            { vk::DescriptorType::eSampler, 1000 },
            { vk::DescriptorType::eCombinedImageSampler, 1000 },
            { vk::DescriptorType::eSampledImage, 1000 },
            { vk::DescriptorType::eStorageImage, 1000 },
            { vk::DescriptorType::eUniformTexelBuffer, 1000 },
            { vk::DescriptorType::eStorageTexelBuffer, 1000 },
            { vk::DescriptorType::eUniformBuffer, 1000 },
            { vk::DescriptorType::eStorageBuffer, 1000 },
            { vk::DescriptorType::eUniformBufferDynamic, 1000 },
            { vk::DescriptorType::eStorageBufferDynamic, 1000 },
            { vk::DescriptorType::eInputAttachment, 1000 }
        };
        
        vk::DescriptorPoolCreateInfo poolCreateInfo = { .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                                                        .maxSets = 1000,
                                                        .poolSizeCount = std::size(poolSizes),
                                                        .pPoolSizes = poolSizes };
        
        if (m_vkContext.m_device.createDescriptorPool(&poolCreateInfo, nullptr, &m_imguiDescriptorPool) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create descriptor pool for imgui!");
        }

        // initialize imgui
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForVulkan(m_pWindow, true);

        ImGui_ImplVulkan_InitInfo initInfo = { .Instance = m_vkContext.m_instance,
                                               .PhysicalDevice = m_vkContext.m_physicalDevice,
                                               .Device = m_vkContext.m_device,
                                               .Queue = m_vkContext.m_graphicsQueue,
                                               .DescriptorPool = m_imguiDescriptorPool,
                                               .MinImageCount = m_imageCount,
                                               .ImageCount = m_imageCount,
                                               .MSAASamples = VK_SAMPLE_COUNT_1_BIT };
        
        ImGui_ImplVulkan_Init(&initInfo, m_finalRenderPass);

        ImGui::StyleColorsClassic();

        // execute a GPU command to upload imgui font textures
        vk::CommandBuffer commandBuffer = beginSingleTimeCommands();
        ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
        endSingleTimeCommands(commandBuffer);
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    void updateGUI(float deltaTime) {
        // ImGui_ImplVulkan_NewFrame(); // do i need it?
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        auto imguiIO = ImGui::GetIO();
        imguiIO.DeltaTime = deltaTime;

        ImGui::Begin("Render Configuration");
        ImGui::Text("Statistics");
        ImGui::Text(" %.2f FPS (%.2f ms)", imguiIO.Framerate, imguiIO.DeltaTime);
        ImGui::End();
    }

    void initWindow() {
        glfwInit();

        // tell glfw to not create an OpenGL context
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        
        // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        m_pWindow = glfwCreateWindow(kWidth, kHeight, kAppName.c_str(), nullptr, nullptr);
        glfwSetWindowUserPointer(m_pWindow, this);
        glfwSetFramebufferSizeCallback(m_pWindow, framebufferResizeCallback);
    }

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<Application*>(glfwGetWindowUserPointer(window));
        app->m_framebufferResized = true;
    }

    void initVulkan() {
        m_vkContext.init("test", m_pWindow);
        
        createSwapChain();
        createImageViews();
        createCommandPool();

        createOffscreenRender();
        createOffscreenDescriptorSetLayout();
        createGraphicsPipeline();
        createTextureImage();
        createTextureImageView();
        createTextureSampler();
        loadModel();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createOffscreenDescriptorPool();
        createOffscreenDescriptorSets();

        createGuiRender();
        createGuiDescriptorSetLayout();
        createGuiPipeline();
        createGuiDescriptorPool();
        updateGuiDescriptorSets();

        createCommandBuffers();
        createSyncObjects();
    }

    void loadModel() {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "models/viking_room.obj")) {
            throw std::runtime_error(warn + err);
        }

        std::unordered_map<Vertex, uint32_t> uniqueVertices{};

        for (const auto& shape : shapes) {
            for (const auto& index : shape.mesh.indices) {
                Vertex vertex{};

                vertex.pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };

                vertex.color = {1.0f, 1.0f, 1.0f};

                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(m_vertices.size());
                    m_vertices.push_back(vertex);
                }

                m_indices.push_back(uniqueVertices[vertex]);
            }
        }
    }

    void createOffscreenRender() {
        // create the color image        
        createImage(m_swapChainExtent.width, m_swapChainExtent.height, 
            vk::Format::eR32G32B32A32Sfloat, 
            vk::ImageTiling::eOptimal, 
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage,
            vk::MemoryPropertyFlagBits::eDeviceLocal, 
            m_colorImage, m_colorImageMemory);
        m_colorImageView = createImageView(m_colorImage, vk::Format::eR32G32B32A32Sfloat, vk::ImageAspectFlagBits::eColor);
        createSampler();

        transitionImageLayout(m_colorImage, vk::Format::eR32G32B32A32Sfloat, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
        
        // create the depth image
        createDepthResources();

        // create the renderpass for offscreen
        if (!m_offscreenRenderPass) {
            m_offscreenRenderPass = createRenderPass();
        }

        // create the framebuffer for offscreen
        createOffscreenFramebuffer();
    }

    void createGuiRender() {
        // create the renderpass for gui
        if (!m_finalRenderPass) {
            m_finalRenderPass = createFinalRenderPass();
        }

        // create the framebuffer for gui
        createSwapChainFrameBuffers();
    }

    void mainLoop() {
        Timer timer;

        while (!glfwWindowShouldClose(m_pWindow)) {
            float deltaTime = static_cast<float>(timer.elapsed());
            glfwPollEvents();
            
            updateGUI(deltaTime);
            // updateScene(deltaTime)

            drawFrame();
        }

        m_vkContext.m_device.waitIdle();
    }

    void cleanup() {
        // cleanup offscreen data
        m_vkContext.m_device.destroyImageView(m_textureImageView, nullptr);
        m_vkContext.m_device.destroyImage(m_textureImage, nullptr);
        m_vkContext.m_device.freeMemory(m_textureImageMemory, nullptr);
        m_vkContext.m_device.destroySampler(m_textureSampler, nullptr);

        m_vkContext.m_device.destroyImageView(m_colorImageView, nullptr);
        m_vkContext.m_device.destroyImage(m_colorImage, nullptr);
        m_vkContext.m_device.freeMemory(m_colorImageMemory, nullptr);
        m_vkContext.m_device.destroySampler(m_colorImageSampler, nullptr);

        m_vkContext.m_device.destroyImageView(m_depthImageView, nullptr);
        m_vkContext.m_device.destroyImage(m_depthImage, nullptr);
        m_vkContext.m_device.freeMemory(m_depthImageMemory, nullptr);
        m_vkContext.m_device.destroyFramebuffer(m_offscreenFramebuffer, nullptr);

        m_vkContext.m_device.destroyRenderPass(m_offscreenRenderPass, nullptr);

        for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
            m_vkContext.m_device.destroyBuffer(m_uniformBuffers[i], nullptr);
            m_vkContext.m_device.freeMemory(m_uniformBuffersMemory[i], nullptr);
        }

        m_vkContext.m_device.destroyDescriptorPool(m_offscreenDescriptorPool, nullptr);
        m_vkContext.m_device.destroyDescriptorSetLayout(m_offscreenDescriptorSetLayout, nullptr);

        m_vkContext.m_device.destroyPipeline(m_offscreenPipeline, nullptr);
        m_vkContext.m_device.destroyPipelineLayout(m_offscreenPipelineLayout, nullptr);

        // cleanup final render pass data

        ImGui_ImplVulkan_Shutdown();
        cleanupSwapChain();

        m_vkContext.m_device.destroyBuffer(m_vertexBuffer, nullptr);
        m_vkContext.m_device.freeMemory(m_vertexBufferMemory, nullptr);
        m_vkContext.m_device.destroyBuffer(m_indexBuffer, nullptr);
        m_vkContext.m_device.freeMemory(m_indexBufferMemory, nullptr);
        m_vkContext.m_device.destroyRenderPass(m_finalRenderPass, nullptr);

        m_vkContext.m_device.destroyDescriptorPool(m_finalDescriptorPool, nullptr);
        m_vkContext.m_device.destroyDescriptorSetLayout(m_finalDescriptorSetLayout, nullptr);
        m_vkContext.m_device.destroyDescriptorPool(m_imguiDescriptorPool, nullptr);

        m_vkContext.m_device.destroyPipeline(m_finalPipeline, nullptr);
        m_vkContext.m_device.destroyPipelineLayout(m_finalPipelineLayout, nullptr);

        for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
            m_vkContext.m_device.destroySemaphore(m_imageAvailableSemaphores[i], nullptr);
            m_vkContext.m_device.destroySemaphore(m_renderFinishedSemaphores[i], nullptr);
            m_vkContext.m_device.destroyFence(m_inFlightFences[i], nullptr);
        }

        m_vkContext.m_device.destroyCommandPool(m_commandPool, nullptr);

        m_vkContext.cleanup();

        glfwDestroyWindow(m_pWindow);
        glfwTerminate();
    }

    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = m_vkContext.querySwapChainSupport(m_vkContext.m_physicalDevice);

        vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
        vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

        m_imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 && m_imageCount > swapChainSupport.capabilities.maxImageCount) {
            m_imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        vk::SwapchainCreateInfoKHR createInfo { .surface = m_vkContext.m_surface,
                                                .minImageCount = m_imageCount,
                                                .imageFormat = surfaceFormat.format,
                                                .imageColorSpace = surfaceFormat.colorSpace,
                                                .imageExtent = extent,
                                                .imageArrayLayers = 1,
                                                .imageUsage = vk::ImageUsageFlagBits::eColorAttachment };
        
        QueueFamilyIndices indices = m_vkContext.findQueueFamilies(m_vkContext.m_physicalDevice);
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

        if (m_vkContext.m_device.createSwapchainKHR(&createInfo, nullptr, &m_swapChain) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create swap chain!");
        }

        if (m_vkContext.m_device.getSwapchainImagesKHR(m_swapChain, &m_imageCount, nullptr) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to get swap chain imageCount!");
        }
        m_swapChainImages.resize(m_imageCount);
        if (m_vkContext.m_device.getSwapchainImagesKHR(m_swapChain, &m_imageCount, m_swapChainImages.data()) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to get swap chain!");
        }
        m_swapChainImageFormat = surfaceFormat.format;
        m_swapChainExtent = extent;
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
            glfwGetFramebufferSize(m_pWindow, &width, &height);

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
        m_swapChainImageViews.resize(m_swapChainImages.size());

        for (size_t i = 0; i < m_swapChainImages.size(); ++i) {
            m_swapChainImageViews[i] = createImageView(m_swapChainImages[i], m_swapChainImageFormat, vk::ImageAspectFlagBits::eColor);
        }
    }

    vk::RenderPass createRenderPass() {
        vk::RenderPass renderPass;

        vk::AttachmentDescription colorAttachment { 
            .format = vk::Format::eR32G32B32A32Sfloat, 
            .samples = vk::SampleCountFlagBits::e1,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::eColorAttachmentOptimal 
        };
        
        vk::AttachmentReference colorAttachmentRef { 
            .attachment = 0,
            .layout = vk::ImageLayout::eColorAttachmentOptimal 
        };
        
        vk::AttachmentDescription depthAttachment { 
            .format = findDepthFormat(),
            .samples = vk::SampleCountFlagBits::e1,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eDontCare,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal
        };

        vk::AttachmentReference depthAttachmentRef { 
            .attachment = 1,
            .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal
        };

        vk::SubpassDescription subpass = { 
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentRef,
            .pDepthStencilAttachment = &depthAttachmentRef 
        };
        
        vk::SubpassDependency dependency { 
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
            .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
            .srcAccessMask = vk::AccessFlagBits::eNone,
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite
        };
        
        std::array<vk::AttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

        vk::RenderPassCreateInfo renderPassInfo { 
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &dependency 
        };
        
        if (m_vkContext.m_device.createRenderPass(&renderPassInfo, nullptr, &renderPass) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create render pass!");
        }

        return renderPass;
    }

    vk::RenderPass createFinalRenderPass() {
        vk::RenderPass renderPass;

        vk::AttachmentDescription colorAttachment { 
            .format = vk::Format::eB8G8R8A8Srgb, 
            .samples = vk::SampleCountFlagBits::e1,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .finalLayout = vk::ImageLayout::ePresentSrcKHR 
        };
        
        vk::AttachmentReference colorAttachmentRef { 
            .attachment = 0,
            .layout = vk::ImageLayout::eColorAttachmentOptimal 
        };
        
        vk::AttachmentDescription depthAttachment { 
            .format = findDepthFormat(),
            .samples = vk::SampleCountFlagBits::e1,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eDontCare,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal
        };

        vk::AttachmentReference depthAttachmentRef { 
            .attachment = 1,
            .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal
        };

        vk::SubpassDescription subpass = { 
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .colorAttachmentCount = 1,
            .pColorAttachments = &colorAttachmentRef,
            .pDepthStencilAttachment = &depthAttachmentRef 
        };
        
        vk::SubpassDependency dependency { 
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe,
            .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
            .srcAccessMask = vk::AccessFlagBits::eMemoryRead,
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead
        };
        
        std::array<vk::AttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

        vk::RenderPassCreateInfo renderPassInfo { 
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = 1,
            .pDependencies = &dependency 
        };
        
        if (m_vkContext.m_device.createRenderPass(&renderPassInfo, nullptr, &renderPass) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create final render pass!");
        }

        return renderPass;
    }

    void createOffscreenDescriptorSetLayout() {
        vk::DescriptorSetLayoutBinding uboLayoutBinding { 
            .binding = 0,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eVertex,
            .pImmutableSamplers = nullptr
        };

        vk::DescriptorSetLayoutBinding samplerLayoutBinding {
            .binding = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eFragment,
            .pImmutableSamplers = nullptr
        };

        std::array<vk::DescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
        
        vk::DescriptorSetLayoutCreateInfo layoutInfo { 
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data()
        };
        
        if (m_vkContext.m_device.createDescriptorSetLayout(&layoutInfo, nullptr, &m_offscreenDescriptorSetLayout) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create offscreen descriptor set layout!");
        }
    }

    void createGraphicsPipeline() {
        auto vertShaderCode = readFile("shaders/shader.vert.spv");
        auto fragShaderCode = readFile("shaders/shader.frag.spv");

        vk::ShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        vk::ShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo { 
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = vertShaderModule,
            .pName = "main"
        };

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo { 
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = fragShaderModule,
            .pName = "main" 
        };
        
        vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
        
        // Fixed function: Vertex input
        auto bindingDescription = Vertex::getBindingDescription();
        auto attributeDescription = Vertex::getAttributeDescriptions();
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo { 
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &bindingDescription,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size()),
            .pVertexAttributeDescriptions = attributeDescription.data()
        };
        
        // Fixed function: Input assembly
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly { 
            .topology = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = VK_FALSE
        };
        
        // Fixed function: Viewport and scissor        
        std::vector<vk::DynamicState> dynamicStates = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };

        vk::PipelineDynamicStateCreateInfo dynamicState { 
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()
        };

        vk::PipelineViewportStateCreateInfo viewportState {
            .viewportCount = 1,
            .scissorCount = 1
        };

        // Fixed function: Rasterizer
        vk::PipelineRasterizationStateCreateInfo rasterizer { 
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eBack,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable = VK_FALSE,
            .depthBiasConstantFactor = 0.0f,
            .depthBiasClamp = 0.0f,
            .depthBiasSlopeFactor = 0.0f,
            .lineWidth = 1.0f
        };
        
        // Fixed function: Multisampling
        vk::PipelineMultisampleStateCreateInfo multisampling { 
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 1.0f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE
        };

        // Fixed function: Depth and stencil testing
        vk::PipelineDepthStencilStateCreateInfo depthStencil { 
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = vk::CompareOp::eLess,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
            .front = {},
            .back = {},
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 1.0f
        };

        // Fixed function: Color blending
        vk::PipelineColorBlendAttachmentState colorBlendAttachment { 
            .blendEnable = VK_FALSE,
            .srcColorBlendFactor = vk::BlendFactor::eOne,
            .dstColorBlendFactor = vk::BlendFactor::eZero,
            .colorBlendOp = vk::BlendOp::eAdd,
            .srcAlphaBlendFactor = vk::BlendFactor::eOne,
            .dstAlphaBlendFactor = vk::BlendFactor::eZero,
            .alphaBlendOp = vk::BlendOp::eAdd,
            .colorWriteMask = vk::ColorComponentFlagBits::eR 
                            | vk::ColorComponentFlagBits::eG 
                            | vk::ColorComponentFlagBits::eB 
                            | vk::ColorComponentFlagBits::eA
        };

        vk::PipelineColorBlendStateCreateInfo colorBlending { 
            .logicOpEnable = VK_FALSE,
            .logicOp = vk::LogicOp::eCopy,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment
        };
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo { 
            .setLayoutCount = 1,
            .pSetLayouts = &m_offscreenDescriptorSetLayout,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr
        };

        if (m_vkContext.m_device.createPipelineLayout(&pipelineLayoutInfo, nullptr, &m_offscreenPipelineLayout) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        vk::GraphicsPipelineCreateInfo pipelineInfo { 
            .stageCount = 2,
            .pStages = shaderStages,
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depthStencil,
            .pColorBlendState = &colorBlending,
            .pDynamicState = &dynamicState,
            .layout = m_offscreenPipelineLayout,
            .renderPass = m_offscreenRenderPass,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1
        };

        if (m_vkContext.m_device.createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_offscreenPipeline) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        m_vkContext.m_device.destroyShaderModule(fragShaderModule, nullptr);
        m_vkContext.m_device.destroyShaderModule(vertShaderModule, nullptr);
    }

    void createGuiPipeline() {
        auto vertShaderCode = readFile("shaders/final.vert.spv");
        auto fragShaderCode = readFile("shaders/final.frag.spv");

        vk::ShaderModule vertShaderModule = createShaderModule(vertShaderCode);
        vk::ShaderModule fragShaderModule = createShaderModule(fragShaderCode);

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo { 
            .stage = vk::ShaderStageFlagBits::eVertex,
            .module = vertShaderModule,
            .pName = "main"
        };

        vk::PipelineShaderStageCreateInfo fragShaderStageInfo { 
            .stage = vk::ShaderStageFlagBits::eFragment,
            .module = fragShaderModule,
            .pName = "main" 
        };
        
        vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
        
        // Fixed function: Vertex input
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo { 
            .vertexBindingDescriptionCount = 0,
            .pVertexBindingDescriptions = nullptr,
            .vertexAttributeDescriptionCount = 0,
            .pVertexAttributeDescriptions = nullptr
        };
        
        // Fixed function: Input assembly
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly { 
            .topology = vk::PrimitiveTopology::eTriangleList,
            .primitiveRestartEnable = VK_FALSE
        };
        
        // Fixed function: Viewport and scissor        
        std::vector<vk::DynamicState> dynamicStates = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };

        vk::PipelineDynamicStateCreateInfo dynamicState { 
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates = dynamicStates.data()
        };

        vk::PipelineViewportStateCreateInfo viewportState {
            .viewportCount = 1,
            .scissorCount = 1
        };

        // Fixed function: Rasterizer
        vk::PipelineRasterizationStateCreateInfo rasterizer { 
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = vk::PolygonMode::eFill,
            .cullMode = vk::CullModeFlagBits::eNone,
            .frontFace = vk::FrontFace::eCounterClockwise,
            .depthBiasEnable = VK_FALSE,
            .depthBiasConstantFactor = 0.0f,
            .depthBiasClamp = 0.0f,
            .depthBiasSlopeFactor = 0.0f,
            .lineWidth = 1.0f
        };
        
        // Fixed function: Multisampling
        vk::PipelineMultisampleStateCreateInfo multisampling { 
            .rasterizationSamples = vk::SampleCountFlagBits::e1,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 1.0f,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE
        };

        // Fixed function: Depth and stencil testing
        vk::PipelineDepthStencilStateCreateInfo depthStencil { 
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = vk::CompareOp::eLess,
            .depthBoundsTestEnable = VK_FALSE,
            .stencilTestEnable = VK_FALSE,
            .front = {},
            .back = {},
            .minDepthBounds = 0.0f,
            .maxDepthBounds = 1.0f
        };

        // Fixed function: Color blending
        vk::PipelineColorBlendAttachmentState colorBlendAttachment { 
            .blendEnable = VK_FALSE,
            .srcColorBlendFactor = vk::BlendFactor::eOne,
            .dstColorBlendFactor = vk::BlendFactor::eZero,
            .colorBlendOp = vk::BlendOp::eAdd,
            .srcAlphaBlendFactor = vk::BlendFactor::eOne,
            .dstAlphaBlendFactor = vk::BlendFactor::eZero,
            .alphaBlendOp = vk::BlendOp::eAdd,
            .colorWriteMask = vk::ColorComponentFlagBits::eR 
                            | vk::ColorComponentFlagBits::eG 
                            | vk::ColorComponentFlagBits::eB 
                            | vk::ColorComponentFlagBits::eA
        };

        vk::PipelineColorBlendStateCreateInfo colorBlending { 
            .logicOpEnable = VK_FALSE,
            .logicOp = vk::LogicOp::eCopy,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment
        };
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo { 
            .setLayoutCount = 1,
            .pSetLayouts = &m_finalDescriptorSetLayout,
            .pushConstantRangeCount = 0,
            .pPushConstantRanges = nullptr
        };

        if (m_vkContext.m_device.createPipelineLayout(&pipelineLayoutInfo, nullptr, &m_finalPipelineLayout) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create gui pipeline layout!");
        }

        vk::GraphicsPipelineCreateInfo pipelineInfo { 
            .stageCount = 2,
            .pStages = shaderStages,
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depthStencil,
            .pColorBlendState = &colorBlending,
            .pDynamicState = &dynamicState,
            .layout = m_finalPipelineLayout,
            .renderPass = m_finalRenderPass,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1
        };

        if (m_vkContext.m_device.createGraphicsPipelines(VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_finalPipeline) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create gui graphics pipeline!");
        }

        m_vkContext.m_device.destroyShaderModule(fragShaderModule, nullptr);
        m_vkContext.m_device.destroyShaderModule(vertShaderModule, nullptr);
    }


    vk::ShaderModule createShaderModule(const std::vector<char>& code) {
        vk::ShaderModuleCreateInfo createInfo { .codeSize = code.size(),
                                                .pCode = reinterpret_cast<const uint32_t*>(code.data()) };
        
        vk::ShaderModule shaderModule;
        if (m_vkContext.m_device.createShaderModule(&createInfo, nullptr, &shaderModule) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

    void createOffscreenFramebuffer() {
        std::array<vk::ImageView, 2> attachments = {
            m_colorImageView,
            m_depthImageView
        };

        vk::FramebufferCreateInfo framebufferInfo { 
            .renderPass = m_offscreenRenderPass,
            .attachmentCount = static_cast<uint32_t>(attachments.size()),
            .pAttachments = attachments.data(),
            .width = m_swapChainExtent.width,
            .height = m_swapChainExtent.height,
            .layers = 1
        };

        if (m_vkContext.m_device.createFramebuffer(&framebufferInfo, nullptr, &m_offscreenFramebuffer) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create offscreen framebuffer!");
        }
    }

    void createSwapChainFrameBuffers() {
        m_swapChainFramebuffers.resize(m_swapChainImageViews.size());

        for (size_t i = 0; i < m_swapChainImageViews.size(); ++i) {
            // Color attachment differs for every swap chain image,
            // but the same depth image can be used by all of them
            // because only a single subpass is running at the same time due to out semaphores
            std::array<vk::ImageView, 2> attachments = {
                m_swapChainImageViews[i],
                m_depthImageView
            };

            vk::FramebufferCreateInfo framebufferInfo { 
                .renderPass = m_finalRenderPass,
                .attachmentCount = static_cast<uint32_t>(attachments.size()),
                .pAttachments = attachments.data(),
                .width = m_swapChainExtent.width,
                .height = m_swapChainExtent.height,
                .layers = 1
            };

            if (m_vkContext.m_device.createFramebuffer(&framebufferInfo, nullptr, &m_swapChainFramebuffers[i]) != vk::Result::eSuccess) {
                throw std::runtime_error("failed to create swapchain framebuffer!");
            }
        }
    }


    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = m_vkContext.findQueueFamilies(m_vkContext.m_physicalDevice);

        vk::CommandPoolCreateInfo poolInfo { .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                             .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value() };
        
        if (m_vkContext.m_device.createCommandPool(&poolInfo, nullptr, &m_commandPool) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void createTextureImage() {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load("textures/viking_room.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        vk::DeviceSize imageSize = texWidth * texHeight * 4;

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        vk::Buffer stagingBuffer;
        vk::DeviceMemory stagingBufferMemory;
        createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

        void* data;
        data = m_vkContext.m_device.mapMemory(stagingBufferMemory, 0, imageSize);
            memcpy(data, pixels, static_cast<size_t>(imageSize));
        m_vkContext.m_device.unmapMemory(stagingBufferMemory);

        stbi_image_free(pixels);

        createImage(texWidth, texHeight, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal, m_textureImage, m_textureImageMemory);
    
        // Transition the texture image to ImageLayout::eTransferDstOptimal
        // The image was create with the ImageLayout::eUndefined layout
        // Because we don't care about its contents before performing the copy operation
        transitionImageLayout(m_textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        // Execute the staging buffer to image copy operation
        copyBufferToImage(stagingBuffer, m_textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        
        // To be able to start sampling from the texture image in the shader,
        // need one last transition to prepare it for shader access
        transitionImageLayout(m_textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
    
        m_vkContext.m_device.destroyBuffer(stagingBuffer, nullptr);
        m_vkContext.m_device.freeMemory(stagingBufferMemory, nullptr);
    }

    void createTextureImageView() {
        m_textureImageView = createImageView(m_textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
    }

    void createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Image& image, vk::DeviceMemory& imageMemory) {
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
        
        if (m_vkContext.m_device.createImage(&imageInfo, nullptr, &image) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create image!");
        }

        vk::MemoryRequirements memRequirements;
        m_vkContext.m_device.getImageMemoryRequirements(image, &memRequirements);

        vk::MemoryAllocateInfo allocInfo { 
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties) 
        };
        
        if (m_vkContext.m_device.allocateMemory(&allocInfo, nullptr, &imageMemory) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        m_vkContext.m_device.bindImageMemory(image, imageMemory, 0);
    }

    vk::ImageView createImageView(vk::Image image, vk::Format format, vk::ImageAspectFlags aspectFlags) {
        vk::ImageViewCreateInfo viewInfo { 
            .image = image,
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange = { 
                .aspectMask = aspectFlags,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1 } 
        };

        vk::ImageView imageView;
        if (m_vkContext.m_device.createImageView(&viewInfo, nullptr, &imageView) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create image view!");
        }

        return imageView;
    }

    void createTextureSampler() {
        vk::PhysicalDeviceProperties properties{};
        m_vkContext.m_physicalDevice.getProperties(&properties);

        vk::SamplerCreateInfo samplerInfo {
            .magFilter = vk::Filter::eLinear,
            .minFilter = vk::Filter::eLinear,
            .mipmapMode = vk::SamplerMipmapMode::eLinear,
            .addressModeU = vk::SamplerAddressMode::eRepeat,
            .addressModeV = vk::SamplerAddressMode::eRepeat,
            .addressModeW = vk::SamplerAddressMode::eRepeat,
            .mipLodBias = 0.0f,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = properties.limits.maxSamplerAnisotropy,
            .compareEnable = VK_FALSE,
            .compareOp = vk::CompareOp::eAlways,
            .minLod = 0.0f,
            .maxLod = 0.0f,
            .borderColor = vk::BorderColor::eIntOpaqueBlack,
            .unnormalizedCoordinates = VK_FALSE
        };

        if (m_vkContext.m_device.createSampler(&samplerInfo, nullptr, &m_textureSampler) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    void createSampler() {
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

        if (m_vkContext.m_device.createSampler(&samplerInfo, nullptr, &m_colorImageSampler) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create color iamge sampler!");
        }
    }

    void transitionImageLayout(vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
        
        vk::CommandBuffer commandBuffer = beginSingleTimeCommands();

        vk::ImageMemoryBarrier barrier { 
            .oldLayout = oldLayout,
            .newLayout = newLayout,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
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
        createImage(m_swapChainExtent.width, m_swapChainExtent.height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, m_depthImage, m_depthImageMemory);
        m_depthImageView = createImageView(m_depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);

        transitionImageLayout(m_depthImage, depthFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
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
            m_vkContext.m_physicalDevice.getFormatProperties(format, &props);

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
        
        if (m_vkContext.m_device.createBuffer(&bufferInfo, nullptr, &buffer) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create vertex buffer!");
        }

        vk::MemoryRequirements memRequirements;
        m_vkContext.m_device.getBufferMemoryRequirements(buffer, &memRequirements);

        vk::MemoryAllocateInfo allocInfo{ .allocationSize = memRequirements.size,
                                          .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties) };
        
        if (m_vkContext.m_device.allocateMemory(&allocInfo, nullptr, &bufferMemory) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to allocate vertex buffer memory!");
        }

        m_vkContext.m_device.bindBufferMemory(buffer, bufferMemory, 0);
    }

    void createVertexBuffer() {
        vk::DeviceSize bufferSize = sizeof(m_vertices[0]) * m_vertices.size();
        
        vk::Buffer stagingBuffer;
        vk::DeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

        void* data;
        data = m_vkContext.m_device.mapMemory(stagingBufferMemory, 0, bufferSize);
            memcpy(data, m_vertices.data(), (size_t)bufferSize);
        m_vkContext.m_device.unmapMemory(stagingBufferMemory);

        createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, m_vertexBuffer, m_vertexBufferMemory);

        copyBuffer(stagingBuffer, m_vertexBuffer, bufferSize);

        m_vkContext.m_device.destroyBuffer(stagingBuffer, nullptr);
        m_vkContext.m_device.freeMemory(stagingBufferMemory, nullptr);
    }

    void createIndexBuffer() {
        vk::DeviceSize bufferSize = sizeof(m_indices[0]) * m_indices.size();
        
        vk::Buffer stagingBuffer;
        vk::DeviceMemory stagingBufferMemory;
        createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

        void* data;
        data = m_vkContext.m_device.mapMemory(stagingBufferMemory, 0, bufferSize);
            memcpy(data, m_indices.data(), (size_t)bufferSize);
        m_vkContext.m_device.unmapMemory(stagingBufferMemory);

        createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, m_indexBuffer, m_indexBufferMemory);

        copyBuffer(stagingBuffer, m_indexBuffer, bufferSize);

        m_vkContext.m_device.destroyBuffer(stagingBuffer, nullptr);
        m_vkContext.m_device.freeMemory(stagingBufferMemory, nullptr);
    }

    void createUniformBuffers() {
        vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

        m_uniformBuffers.resize(kMaxFramesInFlight);
        m_uniformBuffersMemory.resize(kMaxFramesInFlight);
        m_uniformBuffersMapped.resize(kMaxFramesInFlight);

        for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
            createBuffer(bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, m_uniformBuffers[i], m_uniformBuffersMemory[i]);

            m_uniformBuffersMapped[i] = m_vkContext.m_device.mapMemory(m_uniformBuffersMemory[i], 0, bufferSize);
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
        vk::CommandBufferAllocateInfo allocInfo { .commandPool = m_commandPool,
                                                  .level = vk::CommandBufferLevel::ePrimary,
                                                  .commandBufferCount = 1 };

        vk::CommandBuffer commandBuffer;
        if (m_vkContext.m_device.allocateCommandBuffers(&allocInfo, &commandBuffer) != vk::Result::eSuccess) {
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

        if (m_vkContext.m_graphicsQueue.submit(1, &submitInfo, VK_NULL_HANDLE) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to submit command buffer to graphics queue!");
        }
        m_vkContext.m_graphicsQueue.waitIdle();

        m_vkContext.m_device.freeCommandBuffers(m_commandPool, 1, &commandBuffer);
    }

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) {
        vk::PhysicalDeviceMemoryProperties memProperties;
        m_vkContext.m_physicalDevice.getMemoryProperties(&memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties ) {
                return i;
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }
    
    void createOffscreenDescriptorPool() {
        std::array<vk::DescriptorPoolSize, 2> poolSizes{}; 
        
        poolSizes[0].type = vk::DescriptorType::eUniformBuffer;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(kMaxFramesInFlight);
        
        poolSizes[1].type = vk::DescriptorType::eCombinedImageSampler;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(kMaxFramesInFlight);
        
        
        vk::DescriptorPoolCreateInfo poolInfo { 
            .maxSets = static_cast<uint32_t>(kMaxFramesInFlight),
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes = poolSizes.data()
        };
    
        if (m_vkContext.m_device.createDescriptorPool(&poolInfo, nullptr, &m_offscreenDescriptorPool) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create offscreen descriptor pool!");
        }
    }

    void createOffscreenDescriptorSets() {
        vk::DescriptorSetAllocateInfo allocInfo { 
            .descriptorPool = m_offscreenDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &m_offscreenDescriptorSetLayout
        };
        
        if (m_vkContext.m_device.allocateDescriptorSets(&allocInfo, &m_offscreenDescriptorSets) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to allocate offscreen descriptor sets!");
        }

        vk::DescriptorBufferInfo bufferInfo { 
            .buffer = m_uniformBuffers[0],
            .offset = 0,
            .range = sizeof(UniformBufferObject)
        };

        vk::DescriptorImageInfo imageInfo {
            .sampler = m_textureSampler,
            .imageView = m_textureImageView,
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
        };
        
        vk::WriteDescriptorSet uniformBufferWrite = { 
            .dstSet = m_offscreenDescriptorSets,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eUniformBuffer,
            .pImageInfo = nullptr,
            .pBufferInfo = &bufferInfo,
            .pTexelBufferView = nullptr
        };
        vk::WriteDescriptorSet combinedSamplerWrite = { 
            .dstSet = m_offscreenDescriptorSets,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .pImageInfo = &imageInfo
        };

        vk::WriteDescriptorSet descriptorWrites[] = {
            uniformBufferWrite,
            combinedSamplerWrite
        };
        
        m_vkContext.m_device.updateDescriptorSets(static_cast<uint32_t>(std::size(descriptorWrites)), descriptorWrites, 0, nullptr);
    }

    void createGuiDescriptorSetLayout() {
        vk::DescriptorSetLayoutBinding samplerLayoutBinding { 
            .binding = 0,
            .descriptorType = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = 1,
            .stageFlags = vk::ShaderStageFlagBits::eFragment,
            .pImmutableSamplers = nullptr
        };

        std::array<vk::DescriptorSetLayoutBinding, 1> bindings = {samplerLayoutBinding};
        
        vk::DescriptorSetLayoutCreateInfo layoutInfo { 
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data()
        };
        
        if (m_vkContext.m_device.createDescriptorSetLayout(&layoutInfo, nullptr, &m_finalDescriptorSetLayout) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create gui descriptor set layout!");
        }
    }

    void createGuiDescriptorPool() {
        std::array<vk::DescriptorPoolSize, 1> poolSizes{};
        poolSizes[0].type = vk::DescriptorType::eCombinedImageSampler;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(kMaxFramesInFlight);

        vk::DescriptorPoolCreateInfo poolInfo { 
            .maxSets = static_cast<uint32_t>(kMaxFramesInFlight),
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes = poolSizes.data()
        };
    
        if (m_vkContext.m_device.createDescriptorPool(&poolInfo, nullptr, &m_finalDescriptorPool) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create gui descriptor pool!");
        }
    }

    void updateGuiDescriptorSets() {
        if (m_finalDescriptorSets.empty()) {
            std::vector<vk::DescriptorSetLayout> layouts(kMaxFramesInFlight, m_finalDescriptorSetLayout);
            vk::DescriptorSetAllocateInfo allocInfo { 
                .descriptorPool = m_finalDescriptorPool,
                .descriptorSetCount = static_cast<uint32_t>(kMaxFramesInFlight),
                .pSetLayouts = layouts.data()
            };
            
            m_finalDescriptorSets.resize(kMaxFramesInFlight);

            if (m_vkContext.m_device.allocateDescriptorSets(&allocInfo, m_finalDescriptorSets.data()) != vk::Result::eSuccess) {
                throw std::runtime_error("failed to allocate gui descriptor sets!");
            }
        }

        for (size_t i = 0; i < kMaxFramesInFlight; ++i) {        
            m_colorImageDescriptor.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            m_colorImageDescriptor.imageView = m_colorImageView;
            m_colorImageDescriptor.sampler = m_colorImageSampler;

            vk::WriteDescriptorSet descriptorWrite { 
                .dstSet = m_finalDescriptorSets[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo = &m_colorImageDescriptor
            };
            
            m_vkContext.m_device.updateDescriptorSets(1, &descriptorWrite, 0, nullptr);
        }
    }

    void createCommandBuffers() {
        m_commandBuffers.resize(kMaxFramesInFlight);

        vk::CommandBufferAllocateInfo allocInfo { 
            .commandPool = m_commandPool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = (uint32_t) m_commandBuffers.size()
        };
        
        if (m_vkContext.m_device.allocateCommandBuffers(&allocInfo, m_commandBuffers.data()) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex) {
        vk::CommandBufferBeginInfo beginInfo { 
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
            .pInheritanceInfo = nullptr
        };
        
        if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        updateUniformBuffer(m_currentFrame);
        recordOffscreenRenderPass(commandBuffer);

        vk::ImageMemoryBarrier barrier { 
            .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = m_colorImage,
            .subresourceRange = { 
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1 } 
        };
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;
        vk::PipelineStageFlags sourceStage = vk::PipelineStageFlagBits::eAllCommands;
        vk::PipelineStageFlags destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
        commandBuffer.pipelineBarrier(sourceStage, destinationStage, 
                                      {},
                                      0, nullptr, 
                                      0, nullptr,
                                      1, &barrier);

        recordGuiRenderPass(commandBuffer, imageIndex);

        try { 
            commandBuffer.end();
        } catch (vk::SystemError err) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void recordOffscreenRenderPass(vk::CommandBuffer commandBuffer) {
        std::array<vk::ClearValue, 2> clearValues {};
        clearValues[0].color = vk::ClearColorValue {std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = vk::ClearDepthStencilValue {1.0f, 0};

        vk::RenderPassBeginInfo renderPassInfo { 
            .renderPass = m_offscreenRenderPass,
            .framebuffer = m_offscreenFramebuffer,
            .renderArea {
                .offset = {0, 0},
                .extent = m_swapChainExtent },
            .clearValueCount = static_cast<uint32_t>(clearValues.size()),
            .pClearValues = clearValues.data()
        };
        
        commandBuffer.beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_offscreenPipeline);

        vk::Viewport viewport { 
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(m_swapChainExtent.width),
            .height = static_cast<float>(m_swapChainExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        commandBuffer.setViewport(0, 1, &viewport);

        vk::Rect2D scissor { 
            .offset = {0, 0},
            .extent = m_swapChainExtent 
        };
        commandBuffer.setScissor(0, 1, &scissor);

        vk::Buffer vertexBuffers[] = {m_vertexBuffer};
        vk::DeviceSize offsets[] = {0};
        commandBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);
        commandBuffer.bindIndexBuffer(m_indexBuffer, 0, vk::IndexType::eUint32);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_offscreenPipelineLayout, 0, 1, &m_offscreenDescriptorSets, 0, nullptr);
        commandBuffer.drawIndexed(static_cast<uint32_t>(m_indices.size()), 1, 0, 0, 0);

        commandBuffer.endRenderPass();
    }

    void recordGuiRenderPass(vk::CommandBuffer commandBuffer, uint32_t imageIndex) {
        std::array<vk::ClearValue, 2> clearValues {};
        clearValues[0].color = vk::ClearColorValue {std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = vk::ClearDepthStencilValue {1.0f, 0};

        vk::RenderPassBeginInfo renderPassInfo { 
            .renderPass = m_finalRenderPass,
            .framebuffer = m_swapChainFramebuffers[imageIndex],
            .renderArea { 
                .offset = {0, 0},
                .extent = m_swapChainExtent },
            .clearValueCount = static_cast<uint32_t>(clearValues.size()),
            .pClearValues = clearValues.data()
        };
        
        commandBuffer.beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_finalPipeline);

        vk::Viewport viewport { 
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(m_swapChainExtent.width),
            .height = static_cast<float>(m_swapChainExtent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        commandBuffer.setViewport(0, 1, &viewport);

        vk::Rect2D scissor { 
            .offset = {0, 0},
            .extent = m_swapChainExtent 
        };
        commandBuffer.setScissor(0, 1, &scissor);

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_finalPipelineLayout, 0, 1, &m_finalDescriptorSets[m_currentFrame], 0, nullptr);
        
        commandBuffer.draw(3, 1, 0, 0);

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

        commandBuffer.endRenderPass();
    }

    void createSyncObjects() {
        m_imageAvailableSemaphores.resize(kMaxFramesInFlight);
        m_renderFinishedSemaphores.resize(kMaxFramesInFlight);
        m_inFlightFences.resize(kMaxFramesInFlight);

        vk::SemaphoreCreateInfo semaphoreInfo{};
        vk::FenceCreateInfo fenceInfo{ .flags = vk::FenceCreateFlagBits::eSignaled };

        for (size_t i = 0; i < kMaxFramesInFlight; ++i) {
            if (m_vkContext.m_device.createSemaphore(&semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != vk::Result::eSuccess ||
                m_vkContext.m_device.createSemaphore(&semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != vk::Result::eSuccess ||
                m_vkContext.m_device.createFence(&fenceInfo, nullptr, &m_inFlightFences[i]) != vk::Result::eSuccess) {
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }

    void drawFrame() {
        vk::Result result;

        // at the start of the frame, we want to wait until the previous frame has finished
        do {
            result = m_vkContext.m_device.waitForFences(1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX);
        } while(result == vk::Result::eTimeout);

        uint32_t imageIndex;
        // "...to be signaled when the presentation engine is finished using the image. 
        // That's the point in time where we can start drawing to it."
        result = m_vkContext.m_device.acquireNextImageKHR(m_swapChain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex);
        if (result == vk::Result::eErrorOutOfDateKHR) {
            recreateSwapChain();
            return;
        } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        // only reset the fence if we are submitting work
        if (m_vkContext.m_device.resetFences(1, &m_inFlightFences[m_currentFrame]) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to reset fence!");
        }

        m_commandBuffers[m_currentFrame].reset();
        
        recordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex);

        vk::Semaphore waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame]};
        vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
        vk::Semaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame]};

        vk::SubmitInfo submitInfo { 
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = waitSemaphores,
            .pWaitDstStageMask = waitStages,
            .commandBufferCount = 1,
            .pCommandBuffers = &m_commandBuffers[m_currentFrame],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = signalSemaphores
        };
        
        if (m_vkContext.m_graphicsQueue.submit(1, &submitInfo, m_inFlightFences[m_currentFrame]) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        vk::SwapchainKHR swapChains[] {m_swapChain};
        vk::PresentInfoKHR presentInfo { 
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = signalSemaphores,
            .swapchainCount = 1,
            .pSwapchains = swapChains,
            .pImageIndices = &imageIndex,
            .pResults = nullptr 
        };
        
        result = m_vkContext.m_presentQueue.presentKHR(&presentInfo);
    
        if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || m_framebufferResized) {
            m_framebufferResized = false;
            recreateSwapChain();
        } else if (result != vk::Result::eSuccess) {
            throw std::runtime_error("failed to present swap chain image!");
        }

        m_currentFrame = (m_currentFrame + 1) % kMaxFramesInFlight;
    }

    void updateUniformBuffer(uint32_t currentImage) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), m_swapChainExtent.width / (float)m_swapChainExtent.height, 0.1f, 10.0f);

        // GLM's Y coordinate of the clip coordinates is inverted
        // To compensate this, flip the sign on the scaling factor of the Y axis in the proj matrix.
        ubo.proj[1][1] *= -1;

        memcpy(m_uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));

    }

    void cleanupSwapChain() {
        for (size_t i = 0; i < m_swapChainFramebuffers.size(); ++i) {
            m_vkContext.m_device.destroyFramebuffer(m_swapChainFramebuffers[i], nullptr);
        }

        for (size_t i = 0; i < m_swapChainImageViews.size(); ++i) {
            m_vkContext.m_device.destroyImageView(m_swapChainImageViews[i], nullptr);
        }

        m_vkContext.m_device.destroySwapchainKHR(m_swapChain, nullptr);
    }

    void cleanupOffscreenBuffer() {
        m_vkContext.m_device.destroyFramebuffer(m_offscreenFramebuffer, nullptr);

        m_vkContext.m_device.destroyImage(m_colorImage, nullptr);
        m_vkContext.m_device.destroyImageView(m_colorImageView, nullptr);
        m_vkContext.m_device.freeMemory(m_colorImageMemory, nullptr);
        m_vkContext.m_device.destroyImage(m_depthImage, nullptr);
        m_vkContext.m_device.destroyImageView(m_depthImageView, nullptr);
        m_vkContext.m_device.freeMemory(m_depthImageMemory, nullptr);
    }

    void recreateSwapChain() {
        int width = 0, height = 0;
        glfwGetFramebufferSize(m_pWindow, &width, &height);
        while (width == 0 || height == 0) {
            glfwGetFramebufferSize(m_pWindow, &width, &height);
            glfwWaitEvents();
        }

        m_vkContext.m_device.waitIdle();

        cleanupSwapChain();
        cleanupOffscreenBuffer();

        createSwapChain();
        createImageViews();
        createSwapChainFrameBuffers();

        createOffscreenRender();
        updateGuiDescriptorSets();
    }
};

} // namespace vrb

int main() {
    vrb::Application app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

