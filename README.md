# vuren

![teaser](./docs/imgs/2023-01-26-01-54-58.png)

A prototype-oriented Vulkan rendering engine. A lot of parts has inspired by several tutorials and examples. I started this project with following the great [Vulkan Tutorial](https://vulkan-tutorial.com/), but now it has a different architecture and additional features.

Currently it has only been tested on Ubuntu 22.04 and Windows 10 with NVIDIA RTX GPUs.

## Features

- [x] Vulkan API wrapper
- [x] OBJ mesh loading and instanced rendering
- [x] Rendering resource management
- [x] GUI
- [x] Render pass abstraction
- [x] Render pass and shader templates
    - [x] Rasterized G-buffer rendering
    - [x] Ray-traced G-buffer rendering
    - [x] Ray-traced ambient occlusion
    - [x] Temporal accumulation
    - [ ] Reference unbiased path tracer
- [x] Camera manipulation
- [ ] Support for gltf scenes

## Prerequisites

- Graphics card and driver providing hardware ray tracing
- Vulkan SDK 1.2 or later
- CMake 3.20 or later

## Building the project

First, clone the repository:

```bash
git clone --recursive https://github.com/mistralk/vuren.git
```

If you forgot to add `--recursive` option for cloning, run the following command:

```bash
git submodule update --init --recursive
```

### Ubuntu

First of all, you need to install some vulkan-related developer packages and glslangValidator. Also you might need to install X11 development packages `xorg-dev` for GLFW:

```bash
sudo apt install libvulkan-dev vulkan-validationlayers-dev spirv-tools glslang-tools xorg-dev
```

Now you can build the project by run the following commands in the project root directory:

```bash
mkdir build
cd build
cmake ..
make -j $(nproc)
```

Also you can specify the build type(Debug or Release) before running the cmake command.

Finally, to run the application:

```bash
./vuren
```

### Windows (Visual Studio)

In Windows, Visual Studio 2019 or later, CMake 3.20 or later, and [the latest Vulkan SDK](https://vulkan.lunarg.com/sdk/home) are required. Then you can configure and generate the Visual Studio `.sln` file from CMake GUI. 

## Usage example: prototyping ray-traced ambient occlusion in vuren

As a simple example, we can implement ambient occlusion rendering with multiple render passes in vuren. I adapted the high-level idea of the algorithm and some parts of shader from [Wyman’s DirectX Raytracing tutorial](https://cwyman.org/code/dxrTutors/tutors/Tutor5/tutorial05.md.html), which is based on NVIDIA's Falcor framework.

First we need the next three components to implement ambient occlusion:

1.  **G-Buffer Rendering**: Create g-buffers(world position and world normal of primary hit points) through rasterization. Of course, ray-traced g-buffers could be used instead if we want.
2.  **Ambient Occlusion**: Based off of these g-buffers, sample a scatter ray direction per pixel(cosine-weighted hemisphere sampling) and do hardware ray tracing. 
3.  **Temporal Accumulation**: The result image would be noisy because we are casting only 1 ray per pixel. To make a clean one, we weighted-average multiple noisy frames progressively. Although we can just cast more rays per pixel at a time, it could lead to hanging and reset of GPU driver.

Actually I have implemented these rendering passes already, so all we need to do is that define the relationship between resources and render passes in our application code, and then record the command buffer:

```cpp
// rasterized g-buffer pass
m_rasterGBufferPass.init(&m_vkContext, m_commandPool, m_pResourceManager, m_pScene);
m_rasterGBufferPass.setup();

// ray traced ambient occlusion pass
// input textures: world position, world normal (from g-buffer pass)
m_aoPass.init(&m_vkContext, m_commandPool, m_pResourceManager, m_pScene);
m_aoPass.connectTextureWorldPos("RasterWorldPos");
m_aoPass.connectTextureWorldNormal("RasterWorldNormal");
m_aoPass.setup();

// temporal accumulation pass
// input textures: the current frame's rendered result
m_accumPass.init(&m_vkContext, m_commandPool, m_pResourceManager, m_pScene);
m_accumPass.connectTextureCurrentFrame("AoOutput");
m_accumPass.setup();
```

We can consider it as a kind of render graph although it has still a crude and error-prone interface(to be improved).

In this way, we can easily add render passes and can modify relationship between the various render passes in code. For example, switching to the use of raytraced G-buffers instead of rasterization, adding a tone mapping pass at the end of the rendering, or mixing the ambient occlusion result with the results of other render passes to create shadow effects, etc.

## Licenses

External libraries
- [GLFW](https://github.com/glfw/glfw) for window management and device input (the zlib/libpng license)
- [GLM](https://github.com/g-truc/glm) for math in host code (the MIT license)
- [stb](https://github.com/nothings/stb) for image I/O (public domain)
- [Dear ImGui](https://github.com/ocornut/imgui) for GUI (the MIT license)
- [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) for OBJ data loading (the MIT license)

Assets
- [Viking room](https://sketchfab.com/3d-models/viking-room-a49f1b8e4f5c4ecf9e1fe7d81915ad38) model by nigelgoh (CC BY 4.0), tweaked by [Alexander Overvoorde](https://vulkan-tutorial.com/Loading_models)
- [Stanford bunny](http://www.graphics.stanford.edu/data/3Dscanrep/) model by The Stanford 3D Scanning Repository, tweaked by [Morgan McGuire](https://casual-effects.com/data/)