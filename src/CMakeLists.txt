target_sources(vuren PRIVATE
    Common.hpp
    VulkanContext.hpp
    VulkanContext.cpp
    Utils.hpp
    Utils.cpp
    ResourceManager.hpp
    ResourceManager.cpp
    Scene.hpp
    Scene.cpp
    RenderPass.hpp
    RenderPass.cpp
    Timer.hpp
    Timer.cpp
    main.cpp
)

file(GLOB_RECURSE RENDER_PASS_SOURCES "RenderPasses/*.cpp" "RenderPasses/*.hpp")
target_sources(vuren PRIVATE ${RENDER_PASS_SOURCES})