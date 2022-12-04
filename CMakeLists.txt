cmake_minimum_required(VERSION 3.16)

project(vulkan_render_base)
add_executable(vulkan_render_base)

find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

set_target_properties(vulkan_render_base PROPERTIES CMAKE_CXX_STANDARD 20)
set_target_properties(vulkan_render_base PROPERTIES CMAKE_CXX_STANDARD_REQUIRED ON)
set_target_properties(vulkan_render_base PROPERTIES CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(src)

target_include_directories(vulkan_render_base PRIVATE ext/stb)

target_link_libraries(vulkan_render_base PRIVATE
    vulkan
    glfw
)

# compile shaders
# reference: https://github.com/zeux/niagara/blob/master/CMakeLists.txt
set(GLSLANG_VALIDATOR "glslangValidator")
file(GLOB_RECURSE GLSL_SOURCE_FILES "${PROJECT_SOURCE_DIR}/src/shaders/*.glsl")

foreach(GLSL_SOURCE_FILE ${GLSL_SOURCE_FILES})
    # message(STATUS "Load shader: ${GLSL_SOURCE_FILE}")
    get_filename_component(STEM ${GLSL_SOURCE_FILE} NAME_WLE)
    set(SPIRV_OUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/${STEM}.spv")
    
    add_custom_command(
        COMMENT "Compiling shader"
        OUTPUT ${SPIRV_OUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders/"
        COMMAND ${GLSLANG_VALIDATOR} -V ${GLSL_SOURCE_FILE} -o ${SPIRV_OUT}
        DEPENDS ${GLSL_SOURCE_FILE}
    )

    list(APPEND SPIRV_BINARY_FILES ${SPIRV_OUT})
endforeach()

add_custom_target(shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(vulkan_render_base shaders)

# copy texture assets
file(COPY ${CMAKE_CURRENT_LIST_DIR}/textures DESTINATION ${CMAKE_CURRENT_BINARY_DIR})