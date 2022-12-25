# vulkan_render_base

A prototype-oriented Vulkan rendering project template. Its underlying code structure is based on [Vulkan Tutorial](https://vulkan-tutorial.com/). Also a lot of parts has inspired by several other great tutorials and examples.

Currently it has only been tested on Ubuntu 22.04.

## Prerequisites

- vulkan
- glslangValidator
- glfw3
- glm

## Features

- [x] Vulkan context initialization
- [x] Multiple render passes
- [x] Loading OBJ data
- [ ] GUI and basic statistics
- [ ] Render skeleton for both rasterization and ray tracing
- [ ] Reference unbiased path tracer
- [ ] Camera manipulation

## Good references and resources

Vulkan tutorials and samples

- [Vulkan Tutorial](https://vulkan-tutorial.com/)
- [NVIDIA Vulkan Ray Tracing Tutorials](https://github.com/nvpro-samples/vk_raytracing_tutorial_KHR)

CMake structure and shader compilation

- [https://github.com/zeux/niagara/blob/master/CMakeLists.txt](https://github.com/zeux/niagara/blob/master/CMakeLists.txt)
- [https://github.com/KhronosGroup/Vulkan-Tools/blob/master/cube/CMakeLists.txt](https://github.com/KhronosGroup/Vulkan-Tools/blob/master/cube/CMakeLists.txt)