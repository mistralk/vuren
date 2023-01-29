target_sources(vuren PRIVATE
    Common.hpp
    VulkanContext.hpp
    VulkanContext.cpp
    SwapChain.hpp
    SwapChain.cpp
    Utils.hpp
    Utils.cpp
    ResourceManager.hpp
    ResourceManager.cpp
    Camera.hpp
    Camera.cpp
    Scene.hpp
    Scene.cpp
    RenderPass.hpp
    RenderPass.cpp
    Timer.hpp
    Timer.cpp
    main.cpp
)

file(GLOB_RECURSE RENDER_PASS_SOURCES "RenderPasses/*.cpp" "RenderPasses/*.hpp" "RenderPasses/*.h" "CommonShaders/*.h")
target_sources(vuren PRIVATE ${RENDER_PASS_SOURCES})