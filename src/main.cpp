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
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
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
#include "SwapChain.hpp"
#include "VulkanContext.hpp"
#include "RenderPasses/AmbientOcclusionPass/AmbientOcclusionPass.hpp"
#include "RenderPasses/GBufferPass/RayTracedGBufferPass.hpp"
#include "RenderPasses/GBufferPass/RasterGBufferPass.hpp"
#include "RenderPasses/AccumulationPass/AccumulationPass.hpp"
#include "RenderPasses/PathTracingPass/PathTracingPass.hpp"

namespace vuren {

class Application {
public:
    Application() {}

    void run() {
        initGlfw();
        initApplication();
        initScene();
        initRenderGraph();
        initImGui();
        mainLoop();
        cleanup();
    }

    void initGlfw() {
        glfwInit();

        // tell glfw to not create an OpenGL context
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        m_pWindow = glfwCreateWindow(kWidth, kHeight, "vuren", nullptr, nullptr);
        glfwSetWindowUserPointer(m_pWindow, this);
        glfwSetFramebufferSizeCallback(m_pWindow, framebufferResizeCallback);
        glfwSetMouseButtonCallback(m_pWindow, mouseCallback);
        glfwSetKeyCallback(m_pWindow, keyCallback);
    }

    void initApplication() {
        // init vulkan instance
        m_vkContext.init("test", m_pWindow);

        // init resource manager and scene object
        m_pResourceManager = std::make_shared<ResourceManager>(&m_vkContext);
        m_pScene           = std::make_shared<Scene>();

        // init swap chain
        m_pSwapChain = std::make_shared<SwapChain>(&m_vkContext, m_pWindow);
        m_pSwapChain->createSwapChain();
        m_pSwapChain->createSwapChainImageViews();
        m_finalRenderPass.setSwapChain(m_pSwapChain);

        // init command pool
        createCommandPool();
        createCommandBuffers();
        createSyncObjects();
        m_pResourceManager->setCommandPool(m_commandPool);
        m_pResourceManager->setExtent(m_pSwapChain->getExtent());
    }

    void initScene() {
        // scene camera and object description
        m_pResourceManager->createUniformBuffer<CameraData>("CameraBuffer");
        m_pScene->getCamera().setExtent(m_pSwapChain->getExtent());
        m_pScene->getCamera().setMappedCameraBuffer(m_pResourceManager->getMappedBuffer("CameraBuffer"));
        m_pScene->getCamera().init();

        auto texture1 = m_pResourceManager->createModelTexture("Bunny", "assets/textures/texture.jpg");
        m_pScene->addTexture(texture1);
        m_pResourceManager->loadObjModel("Bunny", "assets/models/bunny.obj", m_pScene, 0);
        m_pResourceManager->loadObjModel("GreenBunny", "assets/models/bunny.obj", m_pScene, 1);

        auto texture2 = m_pResourceManager->createModelTexture("VikingRoom", "assets/textures/viking_room.png");
        m_pScene->addTexture(texture2);

        // m_pResourceManager->loadObjModel("Room", "assets/models/viking_room.obj", m_pScene);

        Material material1 = {
            .diffuse = vec3(0.8f, 0.3f, 0.3f),
            .textureId = 0
        };
        Material material2 = {
            .diffuse = vec3(0.0f, 1.0f, 0.0f),
            .textureId = 1
        };
        m_pScene->addMaterial(material1);
        m_pScene->addMaterial(material2);
        m_pResourceManager->createMaterialBuffer(m_pScene);

        m_pResourceManager->createObjectDeviceInfoBuffer(m_pScene);

        createRandomInstances(0, 9);
        createRandomInstances(1, 1);
    }

    void initRenderGraph() {
        // an example application using vuren's render passes: hybrid ambient occlusion
        //
        //     +--------------------+
        //     |     Rasterized     |
        //     |    G-Buffer Pass   |
        //     +-----+--------+-----+
        // World Pos |        | World Normal
        //     +-----v--------v-----+
        //     |  Ambient Occlusion |
        //     |        Pass        |
        //     +---------+----------+
        //               | AO Output
        //     +---------v----------+
        //     |      Temporal      |
        //     |  Accumulation Pass |
        //     +---------+----------+
        //               | Accumulated Frames
        //     +---------v----------+
        //     | Final Presentation |
        //     |    (Swap Chain)    |
        //     +--------------------+

        // rasterized g-buffer pass
        m_rasterGBufferPass.init(&m_vkContext, m_commandPool, m_pResourceManager, m_pScene);
        m_rasterGBufferPass.setup();

        // ray traced ambient occlusion pass
        // input textures: world position, world normal (from g-buffer pass)
        // m_aoPass.init(&m_vkContext, m_commandPool, m_pResourceManager, m_pScene);
        // m_aoPass.connectTextureWorldPos("RasterWorldPos");
        // m_aoPass.connectTextureWorldNormal("RasterWorldNormal");
        // m_aoPass.setup();

        m_pathTracingPass.init(&m_vkContext, m_commandPool, m_pResourceManager, m_pScene);
        m_pathTracingPass.connectTextureWorldPos("RasterWorldPos");
        m_pathTracingPass.connectTextureWorldNormal("RasterWorldNormal");
        m_pathTracingPass.setup();

        // temporal accumulation pass
        // input textures: the current frame's rendered result
        m_accumPass.init(&m_vkContext, m_commandPool, m_pResourceManager, m_pScene);
        m_accumPass.connectTextureCurrentFrame("PtOutput");
        m_accumPass.setup();

        // final rendering pass (and swap chain)
        // which texture will be displayed on the screen is selected at runtime (by the gui)
        m_finalRenderPass.init(&m_vkContext, m_commandPool, m_pResourceManager, m_pScene);
        m_finalRenderPass.setup();

        // set to display the output texture of the last render pass by default
        m_vkContext.kCurrentItem = m_vkContext.kOffscreenOutputTextureNames.size() - 1;
    }

    void mainLoop() {
        Timer timer;

        while (!glfwWindowShouldClose(m_pWindow)) {
            float deltaTime = static_cast<float>(timer.elapsed());
            glfwPollEvents();

            updateGUI(deltaTime);
            manipulateCamera();
            // m_aoPass.updateUniformBuffer();
            m_pathTracingPass.updateUniformBuffer();
            m_accumPass.updateUniformBuffer();

            drawFrame();
        }

        m_vkContext.m_device.waitIdle();
    }

    void recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex) {
        vk::CommandBufferBeginInfo beginInfo{ .flags            = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
                                              .pInheritanceInfo = nullptr };

        if (commandBuffer.begin(&beginInfo) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        m_rasterGBufferPass.record(commandBuffer);
        m_rasterGBufferPass.outputTextureBarrier(commandBuffer);

        // AO pass output texture
        // m_aoPass.record(commandBuffer);
        // auto aoTexture = m_pResourceManager->getTexture("AoOutput");
        // transitionImageLayout(commandBuffer, aoTexture, vk::ImageLayout::eGeneral,
        //                       vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits::eRayTracingShaderKHR,
        //                       vk::PipelineStageFlagBits::eFragmentShader);

        
        m_pathTracingPass.record(commandBuffer);
        auto rtTexture = m_pResourceManager->getTexture("PtOutput");
        transitionImageLayout(commandBuffer, rtTexture, vk::ImageLayout::eGeneral,
                              vk::ImageLayout::eShaderReadOnlyOptimal, vk::PipelineStageFlagBits::eRayTracingShaderKHR,
                              vk::PipelineStageFlagBits::eFragmentShader);



        m_accumPass.record(commandBuffer);
        m_accumPass.outputTextureBarrier(commandBuffer);

        // this texture will be read from final fullscreen triangle shader
        m_finalRenderPass.record(commandBuffer);

        // for raster attachments, we don't need to transition to the original layout(eColorAttachmentOptimal)
        // explicitly. because we defined oldLayout = eUndefined(which means "don't care") for raster render pass
        // initialization. in contrast, ray traced output texutre layout need to be recovered explicitly.
        transitionImageLayout(commandBuffer, rtTexture, vk::ImageLayout::eShaderReadOnlyOptimal,
                              vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits::eFragmentShader,
                              vk::PipelineStageFlagBits::eBottomOfPipe);
        // to fix: general and scalable image flushing/invalidation strategy for ray tracing passes

        try {
            commandBuffer.end();
        } catch (vk::SystemError err) {
            throw std::runtime_error("failed to record command buffer!");
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
        result = m_vkContext.m_device.acquireNextImageKHR(m_pSwapChain->getVkSwapChain(), UINT64_MAX,
                                                          m_imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
        if (result == vk::Result::eErrorOutOfDateKHR) {
            m_pSwapChain->recreateSwapChain();
            return;
        } else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }

        m_pSwapChain->updateImageIndex(imageIndex);

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

        if (m_vkContext.m_graphicsQueue.submit(1, &submitInfo, m_inFlightFence) != vk::Result::eSuccess) {
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        vk::SwapchainKHR swapChains[]{ m_pSwapChain->getVkSwapChain() };
        vk::PresentInfoKHR presentInfo{ .waitSemaphoreCount = 1,
                                        .pWaitSemaphores    = signalSemaphores,
                                        .swapchainCount     = 1,
                                        .pSwapchains        = swapChains,
                                        .pImageIndices      = &imageIndex,
                                        .pResults           = nullptr };

        result = m_vkContext.m_presentQueue.presentKHR(&presentInfo);

        if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || m_framebufferResized) {
            m_framebufferResized = false;
            m_pSwapChain->recreateSwapChain();
        } else if (result != vk::Result::eSuccess) {
            throw std::runtime_error("failed to present swap chain image!");
        }
    }

    void updateGUI(float deltaTime) {
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

        // m_aoPass.updateGui();
        m_pathTracingPass.updateGui();
        m_accumPass.updateGui();

        ImGui::End();
    }

    void cleanup() {
        ImGui_ImplVulkan_Shutdown();
        m_vkContext.m_device.destroyDescriptorPool(m_imguiDescriptorPool, nullptr);

        m_rasterGBufferPass.cleanup();
        // m_aoPass.cleanup();
        m_pathTracingPass.cleanup();
        m_accumPass.cleanup();
        m_finalRenderPass.cleanup();

        m_pResourceManager->destroyManagedTextures();
        m_pResourceManager->destroyManagedBuffers();

        m_pSwapChain->cleanupSwapChain();

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
        std::uniform_real_distribution<float> uniformDist(0.0f, 1.0f);
        std::uniform_real_distribution<float> uniformDistPos(-2.0f, 2.0f);
        for (uint32_t i = 0; i < instanceCount; ++i) {
            ObjectInstance instance;

            auto pos   = glm::translate(glm::identity<glm::mat4>(),
                                        glm::vec3(uniformDistPos(rng), uniformDistPos(rng), uniformDistPos(rng)));
            auto scale = glm::scale(glm::identity<glm::mat4>(), glm::vec3(1.0f, 1.0f, 1.0f));
            auto rot_z = glm::rotate(glm::identity<glm::mat4>(), uniformDist(rng) * glm::radians(360.0f),
                                     glm::vec3(0.0f, 0.0f, 1.0f));
            auto rot_y = glm::rotate(glm::identity<glm::mat4>(), uniformDist(rng) * glm::radians(360.0f),
                                     glm::vec3(0.0f, 1.0f, 0.0f));
            auto rot_x = glm::rotate(glm::identity<glm::mat4>(), uniformDist(rng) * glm::radians(360.0f),
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
                                               .MinImageCount  = m_pSwapChain->getImageCount(),
                                               .ImageCount     = m_pSwapChain->getImageCount(),
                                               .MSAASamples    = VK_SAMPLE_COUNT_1_BIT };

        ImGui_ImplVulkan_Init(&initInfo, m_finalRenderPass.getRenderPass());

        ImGui::StyleColorsClassic();

        // execute a GPU command to upload imgui font textures
        vk::CommandBuffer commandBuffer = beginSingleTimeCommands(m_vkContext, m_commandPool);
        ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
        endSingleTimeCommands(m_vkContext, m_commandPool, commandBuffer);
        ImGui_ImplVulkan_DestroyFontUploadObjects();
    }

    static void framebufferResizeCallback(GLFWwindow *pWindow, int width, int height) {
        auto app                  = reinterpret_cast<Application *>(glfwGetWindowUserPointer(pWindow));
        app->m_framebufferResized = true;
    }

    static void keyCallback(GLFWwindow *pWindow, int key, int scancode, int action, int mods) {
        auto app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(pWindow));
    }

    static void mouseCallback(GLFWwindow *pWindow, int button, int action, int mods) {
        auto app = reinterpret_cast<Application *>(glfwGetWindowUserPointer(pWindow));

        static bool pressed = false;

        if (!pressed) {
            if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
                pressed = true;
                double xpos, ypos;
                glfwGetCursorPos(pWindow, &xpos, &ypos);
                app->m_pScene->getCamera().pressLeftMouse(xpos, ypos);
            }
        } else {
            if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
                pressed = false;
                double xpos, ypos;
                glfwGetCursorPos(pWindow, &xpos, &ypos);
                app->m_pScene->getCamera().releaseLeftMouse(xpos, ypos);
            }
        }
    }

    void manipulateCamera() {
        // currently camera is not optimzed (so many duplicated ops)

        // dolly
        if (glfwGetKey(m_pWindow, GLFW_KEY_W))
            m_pScene->getCamera().forward(0.003f);
        if (glfwGetKey(m_pWindow, GLFW_KEY_S))
            m_pScene->getCamera().forward(-0.003f);
        if (glfwGetKey(m_pWindow, GLFW_KEY_A))
            m_pScene->getCamera().dolly(-0.003f);
        if (glfwGetKey(m_pWindow, GLFW_KEY_D))
            m_pScene->getCamera().dolly(0.003f);

        // pan
        if (m_pScene->getCamera().isLeftMousePressed()) {
            double xpos, ypos;
            glfwGetCursorPos(m_pWindow, &xpos, &ypos);
            m_pScene->getCamera().rotate(static_cast<float>(xpos), static_cast<float>(ypos));
        }

        m_pScene->getCamera().updateCamera();
    }

private:
    GLFWwindow *m_pWindow;

    VulkanContext m_vkContext;
    std::shared_ptr<SwapChain> m_pSwapChain{ nullptr };

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
    AccumulationPass m_accumPass;
    PathTracingPass m_pathTracingPass;

    // for the final pass and gui
    // this pass is directly presented into swap chain framebuffers
    FinalRenderPass m_finalRenderPass;
    vk::DescriptorPool m_imguiDescriptorPool; // additional descriptor pool for imgui
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
