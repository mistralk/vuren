cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(vuren)
add_executable(vuren)

find_package(Vulkan REQUIRED)
target_include_directories(vuren PUBLIC ${Vulkan_INCLUDE_DIR})
target_link_libraries(vuren ${Vulkan_LIBRARY})

if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")    
endif()

add_subdirectory(src)
target_include_directories(vuren PRIVATE src)

# external libraries
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(ext/glfw)
target_include_directories(vuren PRIVATE ext/glfw/include)
target_link_libraries(vuren glfw)

target_include_directories(vuren PRIVATE ext)
target_include_directories(vuren PRIVATE ext/imgui)
target_include_directories(vuren PRIVATE ext/imgui/backends)
target_sources(vuren PRIVATE
    ext/imgui/imgui.cpp
    ext/imgui/imgui_draw.cpp
    ext/imgui/imgui_widgets.cpp
    ext/imgui/imgui_tables.cpp
    ext/imgui/backends/imgui_impl_vulkan.cpp
    ext/imgui/backends/imgui_impl_glfw.cpp
)

set(GLM_TEST_ENABLE OFF CACHE BOOL "" FORCE)
set(GLM_ENABLE_EXPERIMENTAL ON CACHE BOOL "" FORCE)
add_subdirectory(ext/glm)
target_include_directories(vuren PRIVATE ext/glm)

# compile shaders
# reference: https://github.com/zeux/niagara/blob/master/CMakeLists.txt
set(GLSLANG_VALIDATOR "glslangValidator")
find_program(GLSLANG_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin $ENV{VK_SDK_PATH}/Bin)
file(GLOB_RECURSE GLSL_SOURCE_FILES "${PROJECT_SOURCE_DIR}/src/*.glsl")

foreach(GLSL_SOURCE_FILE ${GLSL_SOURCE_FILES})
    message(STATUS "Load shader: ${GLSL_SOURCE_FILE}")
    
    cmake_path(SET GLSL_SOURCE_PATH ${GLSL_SOURCE_FILE})
    cmake_path(GET GLSL_SOURCE_PATH PARENT_PATH SHADER_PARENT)
    file(RELATIVE_PATH PARENT_PATH ${PROJECT_SOURCE_DIR}/src ${SHADER_PARENT})
    make_directory("${CMAKE_CURRENT_BINARY_DIR}/shaders/${PARENT_PATH}")
    get_filename_component(STEM ${GLSL_SOURCE_FILE} NAME_WLE)

    set(SPIRV_OUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/${PARENT_PATH}/${STEM}.spv")
    
    add_custom_command(
        COMMENT "Compiling shader"
        OUTPUT ${SPIRV_OUT}
        COMMAND ${GLSLANG_VALIDATOR} -V ${GLSL_SOURCE_FILE} -I"${PROJECT_SOURCE_DIR}/src" -I"${PROJECT_SOURCE_DIR}/src/shaders" -o ${SPIRV_OUT} --target-env spirv1.5
        DEPENDS ${GLSL_SOURCE_FILE}
    )

    list(APPEND SPIRV_BINARY_FILES ${SPIRV_OUT})
endforeach()

add_custom_target(shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(vuren shaders)

# copy assets
file(COPY ${CMAKE_CURRENT_LIST_DIR}/assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})