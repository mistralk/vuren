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
#include <memory>

#include "VulkanContext.hpp"
#include "Common.hpp"
#include "RenderPass.hpp"
#include "Texture.hpp"
#include "Utils.hpp"
#include "Timer.hpp"

namespace vrb {

class OffscreenRenderPass : public RenderPass {
public:
    OffscreenRenderPass(VulkanContext* pContext, vk::CommandPool* pCommandPool, std::shared_ptr<std::unordered_map<std::string, Texture>> pGlobalTextureDict, std::shared_ptr<std::unordered_map<std::string, vk::Buffer>> pGlobalBufferDict) 
        : RenderPass(pContext, pCommandPool, pGlobalTextureDict, pGlobalBufferDict) {
    }

    ~OffscreenRenderPass() {

    }

    OffscreenRenderPass() {}

    void setup() {
        // create the color image
        Texture texture;
        createImage(*m_pContext, texture,
            m_extent.width, m_extent.height, 
            vk::Format::eR32G32B32A32Sfloat, 
            vk::ImageTiling::eOptimal, 
            vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage,
            vk::MemoryPropertyFlagBits::eDeviceLocal);
        createImageView(*m_pContext, texture, vk::Format::eR32G32B32A32Sfloat, vk::ImageAspectFlagBits::eColor);
        createSampler(*m_pContext, texture);
        // transitionImageLayout(*m_pContext, *m_pCommandPool, texture, vk::Format::eR32G32B32A32Sfloat, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);

        // create the depth image
        Texture depthTexture;
        vk::Format depthFormat = findDepthFormat(*m_pContext);
        createImage(*m_pContext, depthTexture, m_extent.width, m_extent.height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);
        createImageView(*m_pContext, depthTexture, depthFormat, vk::ImageAspectFlagBits::eDepth);
        // transitionImageLayout(*m_pContext, *m_pCommandPool, texture, depthFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
        
        // register the created images
        m_pGlobalTextureDict->insert({"OffscreenColor", texture});
        m_pGlobalTextureDict->insert({"OffscreenDepth", depthTexture});

        // create a descriptor set
        std::vector<ResourceBindingInfo> bindings = {
            {"UniformBuffer", vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex},
            {"ModelTexture", vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment}
        };
        createDescriptorSet(bindings);

        // create framebuffers for the attachments
        std::vector<AttachmentInfo> colorAttachments = {
            { .imageView = (*m_pGlobalTextureDict)["OffscreenColor"].descriptorInfo.imageView, 
            .format = vk::Format::eR32G32B32A32Sfloat, 
            .oldLayout = vk::ImageLayout::eUndefined, 
            .newLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
            .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
            .srcAccessMask = vk::AccessFlagBits::eNone,
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite }
        };
        AttachmentInfo depthStencilAttachment = {
            .imageView = (*m_pGlobalTextureDict)["OffscreenDepth"].descriptorInfo.imageView,
            .format = depthFormat, 
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .srcStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests,
            .dstStageMask = vk::PipelineStageFlagBits::eEarlyFragmentTests,
            .srcAccessMask = vk::AccessFlagBits::eNone,
            .dstAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentWrite
        };

        // create a vulkan render pass object
        createVkRenderPass(colorAttachments, depthStencilAttachment);

        createFramebuffer(colorAttachments, depthStencilAttachment);

        // create a graphics pipeline for this render pass
        createRasterPipeline("shaders/shader.vert.spv", "shaders/shader.frag.spv");
    }

    void record(vk::CommandBuffer commandBuffer) {
        std::array<vk::ClearValue, 2> clearValues {};
        clearValues[0].color = vk::ClearColorValue {std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = vk::ClearDepthStencilValue {1.0f, 0};

        vk::RenderPassBeginInfo renderPassInfo { 
            .renderPass = m_renderPass,
            .framebuffer = m_framebuffer,
            .renderArea {
                .offset = {0, 0},
                .extent = m_extent },
            .clearValueCount = static_cast<uint32_t>(clearValues.size()),
            .pClearValues = clearValues.data()
        };
        
        commandBuffer.beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_rasterPipeline.getPipeline());

        vk::Viewport viewport { 
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(m_extent.width),
            .height = static_cast<float>(m_extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        commandBuffer.setViewport(0, 1, &viewport);

        vk::Rect2D scissor { 
            .offset = {0, 0},
            .extent = m_extent 
        };
        commandBuffer.setScissor(0, 1, &scissor);

        vk::Buffer vertexBuffers[] = {(*m_pGlobalBufferDict)["VertexBuffer"]};
        vk::DeviceSize offsets[] = {0};
        commandBuffer.bindVertexBuffers(0, 1, vertexBuffers, offsets);
        commandBuffer.bindIndexBuffer((*m_pGlobalBufferDict)["IndexBuffer"], 0, vk::IndexType::eUint32);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
        commandBuffer.drawIndexed(static_cast<uint32_t>(m_pIndices->size()), 1, 0, 0, 0);

        commandBuffer.endRenderPass();
    }

    void setIndexList(std::vector<uint32_t>* pIndices) {
        m_pIndices = pIndices;
    }

private:
    vk::Format m_colorAttachmentFormat {vk::Format::eR32G32B32A32Sfloat};

    std::vector<uint32_t>* m_pIndices;

}; // class OffscreenRenderPass

class FinalRenderPass : public RenderPass {
public:
    FinalRenderPass(VulkanContext* pContext, vk::CommandPool* pCommandPool, std::shared_ptr<std::unordered_map<std::string, Texture>> pGlobalTextureDict, std::shared_ptr<std::unordered_map<std::string, vk::Buffer>> pGlobalBufferDict) 
        : RenderPass(pContext, pCommandPool, pGlobalTextureDict, pGlobalBufferDict) {
    }

    ~FinalRenderPass() {

    }

    FinalRenderPass(){}

    void setSwapChainImagePointers(std::shared_ptr<std::vector<vk::Framebuffer>> swapChainFramebuffers, std::shared_ptr<std::vector<vk::Image>> swapChainColorImages, std::shared_ptr<std::vector<vk::ImageView>> swapChainColorImageViews) {
        m_swapChainFramebuffers = swapChainFramebuffers;
        m_swapChainColorImages = swapChainColorImages;
        m_swapChainColorImageViews = swapChainColorImageViews;
    }

    void setup() {
        // create the depth image
        Texture depthTexture;
        vk::Format depthFormat = findDepthFormat(*m_pContext);
        createImage(*m_pContext, depthTexture, m_extent.width, m_extent.height, depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);
        createImageView(*m_pContext, depthTexture, depthFormat, vk::ImageAspectFlagBits::eDepth);

        // register the created images
        // (*m_pGlobalTextureDict)["FinalColor"] = ?; // global dict is not required for swap chain images
        m_pGlobalTextureDict->insert({"FinalDepth", depthTexture});

        // create a descriptor set
        std::vector<ResourceBindingInfo> bindings = {
            {"OffscreenColor", vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment}
        };
        createDescriptorSet(bindings);

        // create the renderpass
        std::vector<AttachmentInfo> colorAttachments; 
        AttachmentInfo colorAttachment { 
            .imageView = VK_NULL_HANDLE, 
            .format = vk::Format::eB8G8R8A8Srgb, 
            .oldLayout = vk::ImageLayout::eUndefined, 
            .newLayout = vk::ImageLayout::ePresentSrcKHR,
            .srcStageMask = vk::PipelineStageFlagBits::eBottomOfPipe,
            .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput,
            .srcAccessMask = vk::AccessFlagBits::eMemoryRead,
            .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead
        };
        colorAttachments.push_back(colorAttachment);
        AttachmentInfo depthStencilAttachment = {
            .imageView = (*m_pGlobalTextureDict)["FinalDepth"].descriptorInfo.imageView,
            .format = depthFormat, 
            .oldLayout = vk::ImageLayout::eUndefined,
            .newLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .srcStageMask = vk::PipelineStageFlagBits::eNone,
            .dstStageMask = vk::PipelineStageFlagBits::eNone,
            .srcAccessMask = vk::AccessFlagBits::eNone,
            .dstAccessMask = vk::AccessFlagBits::eNone
        };
        createVkRenderPass(colorAttachments, depthStencilAttachment);

        // create swap chain framebuffers
        createSwapChainFrameBuffers(depthTexture);

        // create a graphics pipeline for this render pass
        createRasterPipeline("shaders/final.vert.spv", "shaders/final.frag.spv", true);
    }

    void record(vk::CommandBuffer commandBuffer) {
        std::array<vk::ClearValue, 2> clearValues {};
        clearValues[0].color = vk::ClearColorValue {std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}};
        clearValues[1].depthStencil = vk::ClearDepthStencilValue {1.0f, 0};

        vk::RenderPassBeginInfo renderPassInfo { 
            .renderPass = m_renderPass,
            .framebuffer = (*m_swapChainFramebuffers)[m_swapChainImageIndex],
            .renderArea { 
                .offset = {0, 0},
                .extent = m_extent },
            .clearValueCount = static_cast<uint32_t>(clearValues.size()),
            .pClearValues = clearValues.data()
        };
        
        commandBuffer.beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_rasterPipeline.getPipeline());

        vk::Viewport viewport { 
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(m_extent.width),
            .height = static_cast<float>(m_extent.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        commandBuffer.setViewport(0, 1, &viewport);

        vk::Rect2D scissor { 
            .offset = {0, 0},
            .extent = m_extent 
        };
        commandBuffer.setScissor(0, 1, &scissor);

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, 1, &m_descriptorSet, 0, nullptr);
        
        commandBuffer.draw(3, 1, 0, 0);

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

        commandBuffer.endRenderPass();
    }

    void updateSwapChainImageIndex(uint32_t newIndex) {
        m_swapChainImageIndex = newIndex;
    }

    void createSwapChainFrameBuffers(Texture& swapChainDepthImage) {
        m_swapChainFramebuffers->resize(m_swapChainColorImageViews->size());

        for (size_t i = 0; i < m_swapChainFramebuffers->size(); ++i) {
            // Color attachment differs for every swap chain image,
            // but the same depth image can be used by all of them
            // because only a single subpass is running at the same time due to out semaphores
            std::array<vk::ImageView, 2> attachments = {
                (*m_swapChainColorImageViews)[i],
                swapChainDepthImage.descriptorInfo.imageView
            };

            vk::FramebufferCreateInfo framebufferInfo { 
                .renderPass = m_renderPass,
                .attachmentCount = static_cast<uint32_t>(attachments.size()),
                .pAttachments = attachments.data(),
                .width = m_extent.width,
                .height = m_extent.height,
                .layers = 1
            };

            if (m_pContext->m_device.createFramebuffer(&framebufferInfo, nullptr, &(*m_swapChainFramebuffers)[i]) != vk::Result::eSuccess) {
                throw std::runtime_error("failed to create swapchain framebuffer!");
            }
        }
    }

private:
    uint32_t m_swapChainImageIndex;

    std::shared_ptr<std::vector<vk::Framebuffer>> m_swapChainFramebuffers;
    std::shared_ptr<std::vector<vk::Image>> m_swapChainColorImages;
    std::shared_ptr<std::vector<vk::ImageView>> m_swapChainColorImageViews;

}; // class FinalRenderPass

class Application {
private:
    GLFWwindow* m_pWindow;

    VulkanContext m_vkContext;

    // swap chain
    vk::SwapchainKHR m_swapChain;
    vk::Format m_swapChainImageFormat;
    vk::Extent2D m_swapChainExtent;
    uint32_t m_imageCount;
    std::shared_ptr<std::vector<vk::Framebuffer>> m_swapChainFramebuffers;
    std::shared_ptr<std::vector<vk::Image>> m_swapChainColorImages;
    std::shared_ptr<std::vector<vk::ImageView>> m_swapChainColorImageViews;

    vk::CommandPool m_commandPool;
    std::vector<vk::CommandBuffer> m_commandBuffers;

    vk::Semaphore m_imageAvailableSemaphore;
    vk::Semaphore m_renderFinishedSemaphore;
    vk::Fence m_inFlightFence;


    // scene description
    std::vector<Vertex> m_vertices;
    vk::Buffer m_vertexBuffer;
    vk::DeviceMemory m_vertexBufferMemory;

    std::vector<uint32_t> m_indices;
    vk::Buffer m_indexBuffer;
    vk::DeviceMemory m_indexBufferMemory;

    vk::Buffer m_uniformBuffer;
    vk::DeviceMemory m_uniformBufferMemory;
    void* m_uniformBufferMapped;

    Texture m_modelTexture;


    // for the offscreen pass
    OffscreenRenderPass offscreenRenderPass;

    bool m_framebufferResized = false;

    // for the final pass and gui
    // this pass is directly presented into swap chain framebuffers

    FinalRenderPass finalRenderPass;

    vk::DescriptorPool m_imguiDescriptorPool; // additional descriptor pool for imgui

    std::shared_ptr<std::unordered_map<std::string, Texture>> m_pGlobalTextureDict {nullptr};
    std::shared_ptr<std::unordered_map<std::string, vk::Buffer>> m_pGlobalBufferDict {nullptr};

public:
    Application() 
        {
        //   m_pGlobalTextureDict(std::make_shared<std::unordered_map<std::string, Texture>>()),
        //   m_pGlobalBufferDict(std::make_shared<std::unordered_map<std::string, vk::Buffer>>()),
        //   m_swapChainFramebuffers(std::make_shared<std::vector<vk::Framebuffer>>()),
        //   m_swapChainColorImages(std::make_shared<std::vector<vk::Image>>()),
        //   m_swapChainColorImageViews(std::make_shared<std::vector<vk::ImageView>>()),
        // offscreenRenderPass(&m_vkContext, &m_commandPool, m_pGlobalTextureDict, m_pGlobalBufferDict),
        //   finalRenderPass(&m_vkContext, &m_commandPool, m_pGlobalTextureDict, m_pGlobalBufferDict)
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
        
        ImGui_ImplVulkan_Init(&initInfo, finalRenderPass.getRenderPass());

        ImGui::StyleColorsClassic();

        // execute a GPU command to upload imgui font textures
        vk::CommandBuffer commandBuffer = beginSingleTimeCommands(m_vkContext, m_commandPool);
        ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
        endSingleTimeCommands(m_vkContext, m_commandPool, commandBuffer);
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


        m_pGlobalTextureDict = std::make_shared<std::unordered_map<std::string, Texture>>();
        m_pGlobalBufferDict = std::make_shared<std::unordered_map<std::string, vk::Buffer>>();
        m_swapChainFramebuffers = std::make_shared<std::vector<vk::Framebuffer>>();
        m_swapChainColorImages = std::make_shared<std::vector<vk::Image>>();
        m_swapChainColorImageViews = std::make_shared<std::vector<vk::ImageView>>();

        OffscreenRenderPass o(&m_vkContext, &m_commandPool, m_pGlobalTextureDict, m_pGlobalBufferDict);
        FinalRenderPass f(&m_vkContext, &m_commandPool, m_pGlobalTextureDict, m_pGlobalBufferDict);
        offscreenRenderPass = o;
        finalRenderPass = f;

        createSwapChain();
        createSwapChainImageViews();
        createCommandPool();

        createModelTextureImage();
        createModelTextureImageView();
        createModelTextureSampler();
        m_pGlobalTextureDict->insert({"ModelTexture", m_modelTexture});
        loadObjModel(m_vertices, m_indices);
        offscreenRenderPass.setIndexList(&m_indices);

        createVertexBuffer();
        m_pGlobalBufferDict->insert({"VertexBuffer", m_vertexBuffer});
        createIndexBuffer();
        m_pGlobalBufferDict->insert({"IndexBuffer", m_indexBuffer});
        createUniformBuffer();
        m_pGlobalBufferDict->insert({"UniformBuffer", m_uniformBuffer});

        offscreenRenderPass.setExtent(m_swapChainExtent);
        offscreenRenderPass.setup();

        finalRenderPass.setSwapChainImagePointers(m_swapChainFramebuffers, m_swapChainColorImages, m_swapChainColorImageViews);
        finalRenderPass.setExtent(m_swapChainExtent);
        finalRenderPass.setup();

        createCommandBuffers();
        createSyncObjects();
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
        ImGui_ImplVulkan_Shutdown();
        m_vkContext.m_device.destroyDescriptorPool(m_imguiDescriptorPool, nullptr);
        offscreenRenderPass.cleanup();
        finalRenderPass.cleanup();

        // cleanup offscreen render pass data

        m_vkContext.m_device.destroyBuffer(m_uniformBuffer, nullptr);
        m_vkContext.m_device.freeMemory(m_uniformBufferMemory, nullptr);
        m_vkContext.m_device.destroyBuffer(m_vertexBuffer, nullptr);
        m_vkContext.m_device.freeMemory(m_vertexBufferMemory, nullptr);
        m_vkContext.m_device.destroyBuffer(m_indexBuffer, nullptr);
        m_vkContext.m_device.freeMemory(m_indexBufferMemory, nullptr);

        for (auto buffer : *m_pGlobalBufferDict) {
            // std::cout << buffer.first << std::endl;
            // m_pGlobalBufferDict->erase(buffer.first);
        }

        for (auto texture : *m_pGlobalTextureDict) {
            destroyTexture(m_vkContext, texture.second);
            // std::cout << texture.first << std::endl;
            // m_pGlobalTextureDict->erase(texture.first);
        }

        cleanupSwapChain();

        m_vkContext.m_device.destroySemaphore(m_imageAvailableSemaphore, nullptr);
        m_vkContext.m_device.destroySemaphore(m_renderFinishedSemaphore, nullptr);
        m_vkContext.m_device.destroyFence(m_inFlightFence, nullptr);

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
        m_swapChainColorImages->resize(m_imageCount);
        if (m_vkContext.m_device.getSwapchainImagesKHR(m_swapChain, &m_imageCount, m_swapChainColorImages->data()) != vk::Result::eSuccess) {
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
            if (availablePresentMode == vk::PresentModeKHR::eImmediate) {
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

    void createSwapChainImageViews() {
        m_swapChainColorImageViews->resize(m_swapChainColorImages->size());

        for (size_t i = 0; i < m_swapChainColorImageViews->size(); ++i) {
            vk::ImageViewCreateInfo viewInfo { 
                .image = (*m_swapChainColorImages)[i],
                .viewType = vk::ImageViewType::e2D,
                .format = m_swapChainImageFormat,
                .subresourceRange = { 
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1 } 
            };

            if (m_vkContext.m_device.createImageView(&viewInfo, nullptr, &(*m_swapChainColorImageViews)[i]) != vk::Result::eSuccess) {
                throw std::runtime_error("failed to create swap chain image view!");
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

    void createCommandBuffers() {
        m_commandBuffers.resize(1);

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

        updateUniformBuffer();
        offscreenRenderPass.record(commandBuffer);

        vk::ImageMemoryBarrier barrier { 
            .oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
            .newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = (*m_pGlobalTextureDict)["OffscreenColor"].image,
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

        finalRenderPass.record(commandBuffer);

        try { 
            commandBuffer.end();
        } catch (vk::SystemError err) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void createSyncObjects() {
        vk::SemaphoreCreateInfo semaphoreInfo{};
        vk::FenceCreateInfo fenceInfo{ 
            .flags = vk::FenceCreateFlagBits::eSignaled 
        };

        if (m_vkContext.m_device.createSemaphore(&semaphoreInfo, nullptr, &m_imageAvailableSemaphore) != vk::Result::eSuccess ||
            m_vkContext.m_device.createSemaphore(&semaphoreInfo, nullptr, &m_renderFinishedSemaphore) != vk::Result::eSuccess ||
            m_vkContext.m_device.createFence(&fenceInfo, nullptr, &m_inFlightFence) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }

    void drawFrame() {
        vk::Result result;

        // at the start of the frame, we want to wait until the previous frame has finished
        do {
            result = m_vkContext.m_device.waitForFences(1, &m_inFlightFence, VK_TRUE, UINT64_MAX);
        } while(result == vk::Result::eTimeout);

        uint32_t imageIndex;
        // "...to be signaled when the presentation engine is finished using the image. 
        // That's the point in time where we can start drawing to it."
        result = m_vkContext.m_device.acquireNextImageKHR(m_swapChain, UINT64_MAX, m_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
        if (result == vk::Result::eErrorOutOfDateKHR) {
            recreateSwapChain();
            return;
        } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }
        
        finalRenderPass.updateSwapChainImageIndex(imageIndex);

        // only reset the fence if we are submitting work
        if (m_vkContext.m_device.resetFences(1, &m_inFlightFence) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to reset fence!");
        }

        // currently only one command buffer is used.
        m_commandBuffers[0].reset();
        recordCommandBuffer(m_commandBuffers[0], imageIndex);

        vk::Semaphore waitSemaphores[] = {m_imageAvailableSemaphore};
        vk::PipelineStageFlags waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
        vk::Semaphore signalSemaphores[] = {m_renderFinishedSemaphore};

        vk::SubmitInfo submitInfo { 
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = waitSemaphores,
            .pWaitDstStageMask = waitStages,
            .commandBufferCount = 1,
            .pCommandBuffers = &m_commandBuffers[0],
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = signalSemaphores
        };
        
        if (m_vkContext.m_graphicsQueue.submit(1, &submitInfo, m_inFlightFence) != vk::Result::eSuccess) {
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
    }

    void cleanupSwapChain() {        
        for (size_t i = 0; i < m_swapChainFramebuffers->size(); ++i) {
            m_vkContext.m_device.destroyFramebuffer((*m_swapChainFramebuffers)[i], nullptr);
        }

        for (size_t i = 0; i < m_swapChainColorImageViews->size(); ++i) {
            m_vkContext.m_device.destroyImageView((*m_swapChainColorImageViews)[i], nullptr);
        }

        m_vkContext.m_device.destroySwapchainKHR(m_swapChain, nullptr);
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
        // cleanupOffscreenResources();

        createSwapChain();
        createSwapChainImageViews();
        // createSwapChainFrameBuffers();

        // createOffscreenRender();
        // updateFinalDescriptorSets();
    }

    void createModelTextureImage() {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load("textures/viking_room.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        vk::DeviceSize imageSize = texWidth * texHeight * 4;

        if (!pixels) {
            throw std::runtime_error("failed to load texture image!");
        }

        vk::Buffer stagingBuffer;
        vk::DeviceMemory stagingBufferMemory;
        createBuffer(m_vkContext, imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

        void* data;
        data = m_vkContext.m_device.mapMemory(stagingBufferMemory, 0, imageSize);
            memcpy(data, pixels, static_cast<size_t>(imageSize));
        m_vkContext.m_device.unmapMemory(stagingBufferMemory);

        stbi_image_free(pixels);

        createImage(m_vkContext, m_modelTexture, texWidth, texHeight, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
    
        // Transition the texture image to ImageLayout::eTransferDstOptimal
        // The image was create with the ImageLayout::eUndefined layout
        // Because we don't care about its contents before performing the copy operation
        transitionImageLayout(m_vkContext, m_commandPool, m_modelTexture, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        // Execute the staging buffer to image copy operation
        copyBufferToImage(m_vkContext, m_commandPool, stagingBuffer, m_modelTexture.image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
        
        // To be able to start sampling from the texture image in the shader,
        // need one last transition to prepare it for shader access
        transitionImageLayout(m_vkContext, m_commandPool, m_modelTexture, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
    
        m_vkContext.m_device.destroyBuffer(stagingBuffer, nullptr);
        m_vkContext.m_device.freeMemory(stagingBufferMemory, nullptr);
    }

    void createModelTextureImageView() {
        createImageView(m_vkContext, m_modelTexture, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
    }

    void createModelTextureSampler() {
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

        if (m_vkContext.m_device.createSampler(&samplerInfo, nullptr, &m_modelTexture.descriptorInfo.sampler) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create texture sampler!");
        }
    }

    void createVertexBuffer() {
        vk::DeviceSize bufferSize = sizeof(m_vertices[0]) * m_vertices.size();
        
        vk::Buffer stagingBuffer;
        vk::DeviceMemory stagingBufferMemory;
        createBuffer(m_vkContext, bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

        void* data;
        data = m_vkContext.m_device.mapMemory(stagingBufferMemory, 0, bufferSize);
            memcpy(data, m_vertices.data(), (size_t)bufferSize);
        m_vkContext.m_device.unmapMemory(stagingBufferMemory);

        createBuffer(m_vkContext, bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, m_vertexBuffer, m_vertexBufferMemory);

        copyBuffer(m_vkContext, m_commandPool, stagingBuffer, m_vertexBuffer, bufferSize);

        m_vkContext.m_device.destroyBuffer(stagingBuffer, nullptr);
        m_vkContext.m_device.freeMemory(stagingBufferMemory, nullptr);
    }

    void createIndexBuffer() {
        vk::DeviceSize bufferSize = sizeof(m_indices[0]) * m_indices.size();
        
        vk::Buffer stagingBuffer;
        vk::DeviceMemory stagingBufferMemory;
        createBuffer(m_vkContext, bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, stagingBuffer, stagingBufferMemory);

        void* data;
        data = m_vkContext.m_device.mapMemory(stagingBufferMemory, 0, bufferSize);
            memcpy(data, m_indices.data(), (size_t)bufferSize);
        m_vkContext.m_device.unmapMemory(stagingBufferMemory);

        createBuffer(m_vkContext, bufferSize, vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal, m_indexBuffer, m_indexBufferMemory);

        copyBuffer(m_vkContext, m_commandPool, stagingBuffer, m_indexBuffer, bufferSize);

        m_vkContext.m_device.destroyBuffer(stagingBuffer, nullptr);
        m_vkContext.m_device.freeMemory(stagingBufferMemory, nullptr);
    }

    void createUniformBuffer() {
        vk::DeviceSize bufferSize = sizeof(UniformBufferObject);

        createBuffer(m_vkContext, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent, m_uniformBuffer, m_uniformBufferMemory);

        m_uniformBufferMapped = m_vkContext.m_device.mapMemory(m_uniformBufferMemory, 0, bufferSize);
    }

    void updateUniformBuffer() {
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

        memcpy(m_uniformBufferMapped, &ubo, sizeof(ubo));
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

