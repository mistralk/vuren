#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_vulkan.h>
#include <imgui/imgui.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <set>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

#include "Common.hpp"
#include "RenderPass.hpp"
#include "ResourceManager.hpp"
#include "Scene.hpp"
#include "Timer.hpp"
#include "Utils.hpp"
#include "VulkanContext.hpp"
#include "RenderPasses/AmbientOcclusionPass/AmbientOcclusionPass.hpp"
#include "RenderPasses/GBufferPass/RayTracedGBufferPass.hpp"
#include "RenderPasses/GBufferPass/RasterGBufferPass.hpp"

namespace vuren {

class FinalRenderPass : public RasterRenderPass {
public:
    FinalRenderPass() {}

    ~FinalRenderPass() {}

    void init(VulkanContext *pContext, vk::CommandPool commandPool, std::shared_ptr<ResourceManager> pResourceManager,
              std::shared_ptr<Scene> pScene) override {
        RasterRenderPass::init(pContext, commandPool, pResourceManager, pScene);
    }

    void setSwapChainImagePointers(std::shared_ptr<std::vector<vk::Framebuffer>> swapChainFramebuffers,
                                   std::shared_ptr<std::vector<vk::Image>> swapChainColorImages,
                                   std::shared_ptr<std::vector<vk::ImageView>> swapChainColorImageViews) {
        m_swapChainFramebuffers    = swapChainFramebuffers;
        m_swapChainColorImages     = swapChainColorImages;
        m_swapChainColorImageViews = swapChainColorImageViews;
    }

    void define() override {
        // create the attachment images
        // global dict is not required for swap chain images
        m_pResourceManager->createDepthTexture("FinalDepth");

        // create a descriptor set
        std::vector<ResourceBindingInfo> bindings = {
            { m_pContext->kOffscreenOutputTextureNames[m_pContext->kCurrentItem],
              vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment, 1 }
        };
        createDescriptorSet(bindings);

        // create the renderpass
        std::vector<AttachmentInfo> colorAttachments;
        AttachmentInfo colorAttachment{ .imageView     = VK_NULL_HANDLE,
                                        .format        = vk::Format::eB8G8R8A8Srgb,
                                        .oldLayout     = vk::ImageLayout::eUndefined,
                                        .newLayout     = vk::ImageLayout::ePresentSrcKHR,
                                        .srcStageMask  = vk::PipelineStageFlagBits::eBottomOfPipe,
                                        .dstStageMask  = vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                        .srcAccessMask = vk::AccessFlagBits::eMemoryRead,
                                        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite |
                                                         vk::AccessFlagBits::eColorAttachmentRead };
        colorAttachments.push_back(colorAttachment);
        AttachmentInfo depthStencilAttachment = {
            .imageView     = m_pResourceManager->getTexture("FinalDepth")->descriptorInfo.imageView,
            .format        = findDepthFormat(*m_pContext),
            .oldLayout     = vk::ImageLayout::eUndefined,
            .newLayout     = vk::ImageLayout::eDepthStencilAttachmentOptimal,
            .srcStageMask  = vk::PipelineStageFlagBits::eNone,
            .dstStageMask  = vk::PipelineStageFlagBits::eNone,
            .srcAccessMask = vk::AccessFlagBits::eNone,
            .dstAccessMask = vk::AccessFlagBits::eNone
        };
        createVkRenderPass(colorAttachments, depthStencilAttachment);

        // create swap chain framebuffers
        createSwapChainFrameBuffers(*m_pResourceManager->getTexture("FinalDepth"));

        // create a graphics pipeline for this render pass
        setupRasterPipeline("shaders/CommonShaders/Final.vert.spv", "shaders/CommonShaders/Final.frag.spv", true);
    }

    void record(vk::CommandBuffer commandBuffer) override {
        std::array<vk::ClearValue, 2> clearValues{};
        clearValues[0].color        = vk::ClearColorValue{ std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f } };
        clearValues[1].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };

        vk::RenderPassBeginInfo renderPassInfo{ .renderPass  = m_renderPass,
                                                .framebuffer = (*m_swapChainFramebuffers)[m_swapChainImageIndex],
                                                .renderArea{ .offset = { 0, 0 }, .extent = m_extent },
                                                .clearValueCount = static_cast<uint32_t>(clearValues.size()),
                                                .pClearValues    = clearValues.data() };

        commandBuffer.beginRenderPass(&renderPassInfo, vk::SubpassContents::eInline);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_pipeline);

        vk::Viewport viewport{ .x        = 0.0f,
                               .y        = 0.0f,
                               .width    = static_cast<float>(m_extent.width),
                               .height   = static_cast<float>(m_extent.height),
                               .minDepth = 0.0f,
                               .maxDepth = 1.0f };
        commandBuffer.setViewport(0, 1, &viewport);

        vk::Rect2D scissor{ .offset = { 0, 0 }, .extent = m_extent };
        commandBuffer.setScissor(0, 1, &scissor);

        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipelineLayout, 0, 1, &m_descriptorSet, 0,
                                         nullptr);

        commandBuffer.draw(3, 1, 0, 0);

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

        commandBuffer.endRenderPass();
    }

    void updateSwapChainImageIndex(uint32_t newIndex) { m_swapChainImageIndex = newIndex; }

    void createSwapChainFrameBuffers(Texture swapChainDepthImage) {
        m_swapChainFramebuffers->resize(m_swapChainColorImageViews->size());

        for (size_t i = 0; i < m_swapChainFramebuffers->size(); ++i) {
            // Color attachment differs for every swap chain image,
            // but the same depth image can be used by all of them
            // because only a single subpass is running at the same time due to out semaphores
            std::array<vk::ImageView, 2> attachments = { (*m_swapChainColorImageViews)[i],
                                                         swapChainDepthImage.descriptorInfo.imageView };

            vk::FramebufferCreateInfo framebufferInfo{ .renderPass      = m_renderPass,
                                                       .attachmentCount = static_cast<uint32_t>(attachments.size()),
                                                       .pAttachments    = attachments.data(),
                                                       .width           = m_extent.width,
                                                       .height          = m_extent.height,
                                                       .layers          = 1 };

            if (m_pContext->m_device.createFramebuffer(&framebufferInfo, nullptr, &(*m_swapChainFramebuffers)[i]) !=
                vk::Result::eSuccess) {
                throw std::runtime_error("failed to create swapchain framebuffer!");
            }
        }

        m_colorAttachmentCount = 1;
    }

    // required when changing resolution
    void updateDescriptorSets() {
        std::vector<ResourceBindingInfo> bindings = {
            { m_pContext->kOffscreenOutputTextureNames[m_pContext->kCurrentItem],
              vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment }
        };

        std::vector<vk::WriteDescriptorSet> descriptorWrites;

        for (size_t i = 0; i < bindings.size(); ++i) {
            vk::DescriptorImageInfo imageInfo;
            vk::DescriptorBufferInfo bufferInfo;

            vk::DescriptorBufferInfo *pBufferInfo = nullptr;
            vk::DescriptorImageInfo *pImageInfo   = nullptr;

            switch (bindings[i].descriptorType) {
                case vk::DescriptorType::eCombinedImageSampler:
                    imageInfo.sampler     = m_pResourceManager->getTexture(bindings[i].name)->descriptorInfo.sampler;
                    imageInfo.imageView   = m_pResourceManager->getTexture(bindings[i].name)->descriptorInfo.imageView;
                    imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
                    pImageInfo            = &imageInfo;
                    break;

                case vk::DescriptorType::eUniformBuffer:
                    bufferInfo.buffer = m_pResourceManager->getBuffer(bindings[i].name)->descriptorInfo.buffer;
                    bufferInfo.offset = 0;
                    bufferInfo.range  = m_pResourceManager->getBuffer(bindings[i].name)->descriptorInfo.range;
                    pBufferInfo       = &bufferInfo;
                    break;

                default:
                    throw std::runtime_error("unsupported descriptor type!");
            }

            vk::WriteDescriptorSet bufferWrite = { .dstSet           = m_descriptorSet,
                                                   .dstBinding       = static_cast<uint32_t>(i),
                                                   .dstArrayElement  = 0,
                                                   .descriptorCount  = 1,
                                                   .descriptorType   = bindings[i].descriptorType,
                                                   .pImageInfo       = pImageInfo,
                                                   .pBufferInfo      = pBufferInfo,
                                                   .pTexelBufferView = nullptr };

            descriptorWrites.push_back(bufferWrite);
        }

        m_pContext->m_device.updateDescriptorSets(static_cast<uint32_t>(descriptorWrites.size()),
                                                  descriptorWrites.data(), 0, nullptr);
    }

private:
    uint32_t m_swapChainImageIndex;

    std::shared_ptr<std::vector<vk::Framebuffer>> m_swapChainFramebuffers;
    std::shared_ptr<std::vector<vk::Image>> m_swapChainColorImages;
    std::shared_ptr<std::vector<vk::ImageView>> m_swapChainColorImageViews;

}; // class FinalRenderPass

class Application {
private:
    GLFWwindow *m_pWindow;

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

    bool m_framebufferResized = false;

    std::shared_ptr<ResourceManager> m_pResourceManager{ nullptr };

    // scene description
    std::shared_ptr<Scene> m_pScene;

    RasterGBufferPass m_rasterGBufferPass;
    RayTracedGBufferPass m_rtGBufferPass;
    AmbientOcclusionPass m_aoPass;

    // for the final pass and gui
    // this pass is directly presented into swap chain framebuffers
    FinalRenderPass m_finalRenderPass;
    vk::DescriptorPool m_imguiDescriptorPool; // additional descriptor pool for imgui

public:
    Application() {}

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
        vk::DescriptorPoolSize poolSizes[] = { { vk::DescriptorType::eSampler, 1000 },
                                               { vk::DescriptorType::eCombinedImageSampler, 1000 },
                                               { vk::DescriptorType::eSampledImage, 1000 },
                                               { vk::DescriptorType::eStorageImage, 1000 },
                                               { vk::DescriptorType::eUniformTexelBuffer, 1000 },
                                               { vk::DescriptorType::eStorageTexelBuffer, 1000 },
                                               { vk::DescriptorType::eUniformBuffer, 1000 },
                                               { vk::DescriptorType::eStorageBuffer, 1000 },
                                               { vk::DescriptorType::eUniformBufferDynamic, 1000 },
                                               { vk::DescriptorType::eStorageBufferDynamic, 1000 },
                                               { vk::DescriptorType::eInputAttachment, 1000 } };

        vk::DescriptorPoolCreateInfo poolCreateInfo = { .flags   = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                                                        .maxSets = 1000,
                                                        .poolSizeCount = std::size(poolSizes),
                                                        .pPoolSizes    = poolSizes };

        if (m_vkContext.m_device.createDescriptorPool(&poolCreateInfo, nullptr, &m_imguiDescriptorPool) !=
            vk::Result::eSuccess) {
            throw std::runtime_error("failed to create descriptor pool for imgui!");
        }

        // initialize imgui
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForVulkan(m_pWindow, true);

        ImGui_ImplVulkan_InitInfo initInfo = { .Instance       = m_vkContext.m_instance,
                                               .PhysicalDevice = m_vkContext.m_physicalDevice,
                                               .Device         = m_vkContext.m_device,
                                               .Queue          = m_vkContext.m_graphicsQueue,
                                               .DescriptorPool = m_imguiDescriptorPool,
                                               .MinImageCount  = m_imageCount,
                                               .ImageCount     = m_imageCount,
                                               .MSAASamples    = VK_SAMPLE_COUNT_1_BIT };

        ImGui_ImplVulkan_Init(&initInfo, m_finalRenderPass.getRenderPass());

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
        auto imguiIO      = ImGui::GetIO();
        imguiIO.DeltaTime = deltaTime;

        ImGui::Begin("Render Configuration");
        ImGui::Text("Statistics");
        ImGui::Text(" %.2f FPS (%.2f ms)", imguiIO.Framerate, imguiIO.DeltaTime);

        if (ImGui::BeginCombo("Output", m_vkContext.kOffscreenOutputTextureNames[m_vkContext.kCurrentItem].c_str(),
                              0)) {
            for (int i = 0; i < m_vkContext.kOffscreenOutputTextureNames.size(); ++i) {
                const bool isSelected = (m_vkContext.kCurrentItem == i);
                if (ImGui::Selectable(m_vkContext.kOffscreenOutputTextureNames[i].c_str(), isSelected)) {
                    m_vkContext.kCurrentItem = i;
                    m_vkContext.kDirty       = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        m_aoPass.updateGui();

        ImGui::End();
    }

    void initWindow() {
        glfwInit();

        // tell glfw to not create an OpenGL context
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        m_pWindow = glfwCreateWindow(kWidth, kHeight, "vuren", nullptr, nullptr);
        glfwSetWindowUserPointer(m_pWindow, this);
        glfwSetFramebufferSizeCallback(m_pWindow, framebufferResizeCallback);
    }

    static void framebufferResizeCallback(GLFWwindow *window, int width, int height) {
        auto app                  = reinterpret_cast<Application *>(glfwGetWindowUserPointer(window));
        app->m_framebufferResized = true;
    }

    void initVulkan() {
        m_vkContext.init("test", m_pWindow);

        m_pResourceManager = std::make_shared<ResourceManager>(&m_vkContext);
        m_pScene           = std::make_shared<Scene>();

        m_swapChainFramebuffers    = std::make_shared<std::vector<vk::Framebuffer>>();
        m_swapChainColorImages     = std::make_shared<std::vector<vk::Image>>();
        m_swapChainColorImageViews = std::make_shared<std::vector<vk::ImageView>>();

        m_rasterGBufferPass.init(&m_vkContext, m_commandPool, m_pResourceManager, m_pScene);
        m_finalRenderPass.init(&m_vkContext, m_commandPool, m_pResourceManager, m_pScene);

        createSwapChain();
        createSwapChainImageViews();
        m_pResourceManager->setExtent(m_swapChainExtent);
        m_rasterGBufferPass.setExtent(m_swapChainExtent);
        m_finalRenderPass.setExtent(m_swapChainExtent);
        createCommandPool();
        m_pResourceManager->setCommandPool(m_commandPool);

        m_pResourceManager->createUniformBuffer<Camera>("CameraBuffer");
        auto texture = m_pResourceManager->createModelTexture("VikingRoom", "assets/textures/viking_room.png");
        m_pScene->addTexture(texture);
        auto texture1 = m_pResourceManager->createModelTexture("Bunny", "assets/textures/texture.jpg");
        m_pScene->addTexture(texture1);

        // m_pResourceManager->loadObjModel("Room", "assets/models/viking_room.obj", m_pScene);
        m_pResourceManager->loadObjModel("Bunny", "assets/models/bunny.obj", m_pScene);
        m_pResourceManager->createObjectDeviceInfoBuffer(m_pScene);

        // createRandomInstances(0, 10);
        createRandomInstances(0, 10);

        m_rasterGBufferPass.setup();

        // m_rtGBufferPass.setExtent(m_swapChainExtent);
        // m_rtGBufferPass.init(&m_vkContext, m_commandPool, m_pResourceManager, m_pScene);
        // m_rtGBufferPass.setup();
        m_aoPass.setExtent(m_swapChainExtent);
        m_aoPass.init(&m_vkContext, m_commandPool, m_pResourceManager, m_pScene);
        m_aoPass.setup();

        m_finalRenderPass.setSwapChainImagePointers(m_swapChainFramebuffers, m_swapChainColorImages,
                                                    m_swapChainColorImageViews);
        m_finalRenderPass.setup();

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

        m_rasterGBufferPass.cleanup();
        // m_rtGBufferPass.cleanup();
        m_aoPass.cleanup();
        m_finalRenderPass.cleanup();

        m_pResourceManager->destroyManagedTextures();
        m_pResourceManager->destroyManagedBuffers();

        cleanupSwapChain();

        m_vkContext.m_device.destroySemaphore(m_imageAvailableSemaphore, nullptr);
        m_vkContext.m_device.destroySemaphore(m_renderFinishedSemaphore, nullptr);
        m_vkContext.m_device.destroyFence(m_inFlightFence, nullptr);

        m_vkContext.m_device.destroyCommandPool(m_commandPool, nullptr);

        m_vkContext.cleanup();

        glfwDestroyWindow(m_pWindow);
        glfwTerminate();
    }

    void createRandomInstances(uint32_t objId, uint32_t instanceCount) {
        std::vector<ObjectInstance> instances;

        m_pScene->setInstanceCount(objId, instanceCount);

        static std::default_random_engine rng((unsigned) time(nullptr));
        std::uniform_real_distribution<float> uniformDist(0.0, 1.0);
        std::uniform_real_distribution<float> uniformDistPos(-1.0, 1.0);
        for (uint32_t i = 0; i < instanceCount; ++i) {
            ObjectInstance instance;

            auto pos   = glm::translate(glm::identity<glm::mat4>(),
                                        glm::vec3(uniformDistPos(rng), uniformDistPos(rng), uniformDistPos(rng)));
            auto scale = glm::scale(glm::identity<glm::mat4>(), glm::vec3(0.5f, 0.5f, 0.5f));
            auto rot_z = glm::rotate(glm::identity<glm::mat4>(), uniformDist(rng) * glm::radians(90.0f),
                                     glm::vec3(0.0f, 0.0f, 1.0f));
            auto rot_y = glm::rotate(glm::identity<glm::mat4>(), uniformDist(rng) * glm::radians(90.0f),
                                     glm::vec3(0.0f, 1.0f, 0.0f));
            auto rot_x = glm::rotate(glm::identity<glm::mat4>(), uniformDist(rng) * glm::radians(90.0f),
                                     glm::vec3(1.0f, 0.0f, 0.0f));

            // glsl and glm: uses column-major order matrices of column vectors
            instance.world             = pos * rot_z * rot_y * rot_x * scale;
            instance.invTransposeWorld = glm::transpose(glm::inverse(instance.world));
            instance.objectId          = objId;

            instances.push_back(instance);
        }

        vk::DeviceSize bufferSize = instances.size() * sizeof(ObjectInstance);

        // staging buffer
        Buffer stagingBuffer = m_pResourceManager->createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc,
                                                                vk::MemoryPropertyFlagBits::eHostVisible |
                                                                    vk::MemoryPropertyFlagBits::eHostCoherent);

        void *data;
        data = m_vkContext.m_device.mapMemory(stagingBuffer.memory, 0, bufferSize);
        memcpy(data, instances.data(), (size_t) bufferSize);
        m_vkContext.m_device.unmapMemory(stagingBuffer.memory);

        // instance buffer
        Buffer instanceBuffer = m_pResourceManager->createBuffer(
            bufferSize,
            vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer |
                vk::BufferUsageFlagBits::eShaderDeviceAddress |
                vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR,
            vk::MemoryPropertyFlagBits::eDeviceLocal);

        // copy to staging buffer
        copyBuffer(m_vkContext, m_commandPool, stagingBuffer.descriptorInfo.buffer,
                   instanceBuffer.descriptorInfo.buffer, bufferSize);

        m_pResourceManager->destroyBuffer(stagingBuffer);

        m_pResourceManager->insertBuffer("InstanceBuffer" + std::to_string(objId), instanceBuffer);

        m_pScene->addInstances(instances);
    }

    void createSwapChain() {
        SwapChainSupportDetails swapChainSupport = m_vkContext.querySwapChainSupport(m_vkContext.m_physicalDevice);

        vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
        vk::PresentModeKHR presentMode     = chooseSwapPresentMode(swapChainSupport.presentModes);
        vk::Extent2D extent                = chooseSwapExtent(swapChainSupport.capabilities);

        m_imageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount > 0 &&
            m_imageCount > swapChainSupport.capabilities.maxImageCount) {
            m_imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        vk::SwapchainCreateInfoKHR createInfo{ .surface          = m_vkContext.m_surface,
                                               .minImageCount    = m_imageCount,
                                               .imageFormat      = surfaceFormat.format,
                                               .imageColorSpace  = surfaceFormat.colorSpace,
                                               .imageExtent      = extent,
                                               .imageArrayLayers = 1,
                                               .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment };

        QueueFamilyIndices indices    = m_vkContext.findQueueFamilies(m_vkContext.m_physicalDevice);
        uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.imageSharingMode      = vk::SharingMode::eConcurrent;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices   = queueFamilyIndices;
        } else {
            createInfo.imageSharingMode      = vk::SharingMode::eExclusive;
            createInfo.queueFamilyIndexCount = 0;
            createInfo.pQueueFamilyIndices   = nullptr;
        }

        createInfo.preTransform   = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
        createInfo.presentMode    = presentMode;
        createInfo.clipped        = VK_TRUE;
        createInfo.oldSwapchain   = VK_NULL_HANDLE;

        if (m_vkContext.m_device.createSwapchainKHR(&createInfo, nullptr, &m_swapChain) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create swap chain!");
        }

        if (m_vkContext.m_device.getSwapchainImagesKHR(m_swapChain, &m_imageCount, nullptr) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to get swap chain imageCount!");
        }
        m_swapChainColorImages->resize(m_imageCount);
        if (m_vkContext.m_device.getSwapchainImagesKHR(m_swapChain, &m_imageCount, m_swapChainColorImages->data()) !=
            vk::Result::eSuccess) {
            throw std::runtime_error("failed to get swap chain!");
        }
        m_swapChainImageFormat = surfaceFormat.format;
        m_swapChainExtent      = extent;
    }

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
        for (const auto &availableFormat: availableFormats) {
            if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
                availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }

    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes) {
        for (const auto &availablePresentMode: availablePresentModes) {
            if (availablePresentMode == vk::PresentModeKHR::eImmediate) {
                return availablePresentMode;
            }
        }
        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            glfwGetFramebufferSize(m_pWindow, &width, &height);

            vk::Extent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

            actualExtent.width =
                std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height =
                std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
            return actualExtent;
        }
    }

    void createSwapChainImageViews() {
        m_swapChainColorImageViews->resize(m_swapChainColorImages->size());

        for (size_t i = 0; i < m_swapChainColorImageViews->size(); ++i) {
            vk::ImageViewCreateInfo viewInfo{ .image            = (*m_swapChainColorImages)[i],
                                              .viewType         = vk::ImageViewType::e2D,
                                              .format           = m_swapChainImageFormat,
                                              .subresourceRange = { .aspectMask     = vk::ImageAspectFlagBits::eColor,
                                                                    .baseMipLevel   = 0,
                                                                    .levelCount     = 1,
                                                                    .baseArrayLayer = 0,
                                                                    .layerCount     = 1 } };

            if (m_vkContext.m_device.createImageView(&viewInfo, nullptr, &(*m_swapChainColorImageViews)[i]) !=
                vk::Result::eSuccess) {
                throw std::runtime_error("failed to create swap chain image view!");
            }
        }
    }

    void createCommandPool() {
        QueueFamilyIndices queueFamilyIndices = m_vkContext.findQueueFamilies(m_vkContext.m_physicalDevice);

        vk::CommandPoolCreateInfo poolInfo{ .flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                            .queueFamilyIndex = queueFamilyIndices.graphicsFamily.value() };

        if (m_vkContext.m_device.createCommandPool(&poolInfo, nullptr, &m_commandPool) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void createCommandBuffers() {
        m_commandBuffers.resize(1);

        vk::CommandBufferAllocateInfo allocInfo{ .commandPool        = m_commandPool,
                                                 .level              = vk::CommandBufferLevel::ePrimary,
                                                 .commandBufferCount = (uint32_t) m_commandBuffers.size() };

        if (m_vkContext.m_device.allocateCommandBuffers(&allocInfo, m_commandBuffers.data()) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex) {
        vk::CommandBufferBeginInfo beginInfo{ .flags            = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
                                              .pInheritanceInfo = nullptr };

        if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        // these are not device command. I think they have to be moved out from this command buffer recording..
        updateCameraBuffer("CameraBuffer"); 
        m_aoPass.updateAoDataUniformBuffer();

        // if (kOffscreenOutputTextureNames[kCurrentItem] == "RayTracedPosWorld" ||
        //     kOffscreenOutputTextureNames[kCurrentItem] == "RayTracedNormalWorld")
        //     m_rtGBufferPass.record(commandBuffer);
        // else
        //     m_rasterGBufferPass.record(commandBuffer);

        m_rasterGBufferPass.record(commandBuffer);

        // 결국 descriptor info는 cpu 메모리에 정의된 구조체에 불과
        // 레코딩 단계에서 로드하더라도, 런타임에 동적으로 바뀐 디스크립터 인포가 해당 커맨드 실행 시점에서 적용되었다는 보장이 없음 (그렇게 하려면 CPU 펜스가 필요함)
        // 아예 명시적인 분기로 만들던가 아니면 렌더그래프를 "컴파일" 하던가

        auto colorTexture = m_pResourceManager->getTexture("RasterColor");
        transitionImageLayout(commandBuffer, colorTexture, vk::ImageLayout::eColorAttachmentOptimal,
                              vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits::eAllGraphics,
                              vk::PipelineStageFlagBits::eAllGraphics);

        auto posTexture = m_pResourceManager->getTexture("RasterPosWorld");
        transitionImageLayout(commandBuffer, posTexture, vk::ImageLayout::eColorAttachmentOptimal,
                              vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits::eAllGraphics,
                              vk::PipelineStageFlagBits::eAllGraphics);

        auto normalTexture = m_pResourceManager->getTexture("RasterNormalWorld");
        transitionImageLayout(commandBuffer, normalTexture, vk::ImageLayout::eColorAttachmentOptimal,
                              vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits::eAllGraphics,
                              vk::PipelineStageFlagBits::eAllGraphics);

        // AO pass output texture
        m_aoPass.record(commandBuffer);
        
        auto aoTexture = m_pResourceManager->getTexture("AOOutput");
        transitionImageLayout(commandBuffer, aoTexture, vk::ImageLayout::eGeneral,
                              vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits::eRayTracingShaderKHR,
                              vk::PipelineStageFlagBits::eFragmentShader);

        // 간단한 룰
        // 1. output texture의 newLayout은 무조건 ShaderReadOnly
        // 2. output texture의 oldLayout은 rt는 무조건 General, raster면 ColorAttachment 또는 DepthAttachment (혹은, 그냥 eUndefined)

        // this texture will be read from final fullscreen triangle shader
        m_finalRenderPass.record(commandBuffer);

        // for raster attachments, we don't need to transition to the original layout(eColorAttachmentOptimal)
        // explicitly. because we defined oldLayout = eUndefined(which means "don't care") for raster render pass initialization.
        // in contrast, ray traced output texutre layout need to be recovered explicitly.
        transitionImageLayout(commandBuffer, aoTexture, vk::ImageLayout::eShaderReadOnlyOptimal,
                              vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits::eFragmentShader,
                              vk::PipelineStageFlagBits::eBottomOfPipe);
        
        try {
            commandBuffer.end();
        } catch (vk::SystemError err) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void createSyncObjects() {
        vk::SemaphoreCreateInfo semaphoreInfo{};
        vk::FenceCreateInfo fenceInfo{ .flags = vk::FenceCreateFlagBits::eSignaled };

        if (m_vkContext.m_device.createSemaphore(&semaphoreInfo, nullptr, &m_imageAvailableSemaphore) !=
                vk::Result::eSuccess ||
            m_vkContext.m_device.createSemaphore(&semaphoreInfo, nullptr, &m_renderFinishedSemaphore) !=
                vk::Result::eSuccess ||
            m_vkContext.m_device.createFence(&fenceInfo, nullptr, &m_inFlightFence) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }

    void drawFrame() {
        vk::Result result;

        // at the start of the frame, we want to wait until the previous frame has finished
        do {
            result = m_vkContext.m_device.waitForFences(1, &m_inFlightFence, VK_TRUE, UINT64_MAX);
        } while (result == vk::Result::eTimeout);

        // change the descriptor sets w.r.t. updated gui (e.g., output buffer)
        if (m_vkContext.kDirty) {
            m_finalRenderPass.updateDescriptorSets();
            m_vkContext.kDirty = false;
        }

        uint32_t imageIndex;
        // "...to be signaled when the presentation engine is finished using the image.
        // That's the point in time where we can start drawing to it."
        result = m_vkContext.m_device.acquireNextImageKHR(m_swapChain, UINT64_MAX, m_imageAvailableSemaphore,
                                                          VK_NULL_HANDLE, &imageIndex);
        if (result == vk::Result::eErrorOutOfDateKHR) {
            recreateSwapChain();
            return;
        } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        m_finalRenderPass.updateSwapChainImageIndex(imageIndex);

        // only reset the fence if we are submitting work
        if (m_vkContext.m_device.resetFences(1, &m_inFlightFence) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to reset fence!");
        }

        // currently only one command buffer is used.
        m_commandBuffers[0].reset();
        recordCommandBuffer(m_commandBuffers[0], imageIndex);

        vk::Semaphore waitSemaphores[]      = { m_imageAvailableSemaphore };
        vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
        vk::Semaphore signalSemaphores[]    = { m_renderFinishedSemaphore };

        vk::SubmitInfo submitInfo{ .waitSemaphoreCount   = 1,
                                   .pWaitSemaphores      = waitSemaphores,
                                   .pWaitDstStageMask    = waitStages,
                                   .commandBufferCount   = 1,
                                   .pCommandBuffers      = &m_commandBuffers[0],
                                   .signalSemaphoreCount = 1,
                                   .pSignalSemaphores    = signalSemaphores };

        // m_vkContext.m_graphicsQueue.waitIdle();

        if (m_vkContext.m_graphicsQueue.submit(1, &submitInfo, m_inFlightFence) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        vk::SwapchainKHR swapChains[]{ m_swapChain };
        vk::PresentInfoKHR presentInfo{ .waitSemaphoreCount = 1,
                                        .pWaitSemaphores    = signalSemaphores,
                                        .swapchainCount     = 1,
                                        .pSwapchains        = swapChains,
                                        .pImageIndices      = &imageIndex,
                                        .pResults           = nullptr };

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

    void updateCameraBuffer(std::string name) {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time       = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        Camera ubo{};
        ubo.view = glm::lookAt(
            glm::vec3(3.0 * glm::cos(time * glm::radians(90.0f)), 3.0 * glm::sin(time * glm::radians(90.0f)), 2.0f),
            glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), m_swapChainExtent.width / (float) m_swapChainExtent.height,
                                    0.1f, 10.0f);

        // GLM's Y coordinate of the clip coordinates is inverted
        // To compensate this, flip the sign on the scaling factor of the Y axis in the proj matrix.
        ubo.proj[1][1] *= -1;

        // for camera ray generation
        ubo.invView = glm::inverse(ubo.view);
        ubo.invProj = glm::inverse(ubo.proj);

        memcpy(m_pResourceManager->getMappedBuffer(name), &ubo, sizeof(ubo));
    }
};

} // namespace vuren

int main() {
    vuren::Application app;

    try {
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
