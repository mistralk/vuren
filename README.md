# vulkan_render_base

A prototype-oriented Vulkan rendering project template. Its underlying code structure is based on [Vulkan Tutorial](https://vulkan-tutorial.com/). Also a lot of parts has inspired by several other great tutorials and examples.

While I could start implementing rendering algorithms on some convenient open-source frameworks, wrappers and helpers(like NVIDIA Falcor, NVIDIA nvvk, Microsoft MiniEngine, …), I wound up writing another Vulkan rendering engine from scratch. Because I am still not familiar with real-time rendering architecture based on modern explicit graphics API, so I wanted to understand the internal structure of them by “reinventing the wheel”.

Currently it only works on Ubuntu 22.04.

## Prerequisites

- vulkan
- glfw3
- glm

## Features

- [x] Vulkan context initialization
- [] Loading OBJ data
- [] GUI and basic statistics

## Good references and resources

Vulkan tutorials and samples

- [Vulkan Tutorial](https://vulkan-tutorial.com/)
- [NVIDIA Vulkan Ray Tracing Tutorials](https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR)

CMake structure and shader compilation

- [https://github.com/zeux/niagara/blob/master/CMakeLists.txt](https://github.com/zeux/niagara/blob/master/CMakeLists.txt)
- [https://github.com/KhronosGroup/Vulkan-Tools/blob/master/cube/CMakeLists.txt](https://github.com/KhronosGroup/Vulkan-Tools/blob/master/cube/CMakeLists.txt)