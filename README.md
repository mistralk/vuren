# vulkan_render_base

A prototype-oriented Vulkan rendering engine. A lot of parts has inspired by several tutorials and examples. I started this project with following the great [Vulkan Tutorial](https://vulkan-tutorial.com/), but now it has a slightly different code structure.

Currently it has only been tested on Ubuntu 22.04.

## Prerequisites

- vulkan
- glslangValidator
- glfw3
- glm

## Features

- [x] Vulkan context initialization
- [x] Render pass abstraction
- [x] GUI and basic statistics
- [ ] Render skeleton for both rasterization and ray tracing
- [ ] Reference unbiased path tracer

## Good references and resources

Vulkan tutorials and samples

- [Vulkan Tutorial](https://vulkan-tutorial.com/)
- [Vulkan Ray Tracing Tutorials by NVIDIA](https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR)
- [Vulkan Samples by Khronos Group](https://github.com/KhronosGroup/Vulkan-Samples)

CMake structure and shader compilation

- [https://github.com/zeux/niagara/blob/master/CMakeLists.txt](https://github.com/zeux/niagara/blob/master/CMakeLists.txt)
- [https://github.com/KhronosGroup/Vulkan-Tools/blob/master/cube/CMakeLists.txt](https://github.com/KhronosGroup/Vulkan-Tools/blob/master/cube/CMakeLists.txt)