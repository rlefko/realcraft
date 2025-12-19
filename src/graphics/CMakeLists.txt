# src/graphics/CMakeLists.txt
# Graphics Abstraction Layer library

# Platform-independent sources
set(GRAPHICS_SOURCES
    device.cpp
    shader_compiler.cpp
)

# Create static library
add_library(realcraft_graphics STATIC ${GRAPHICS_SOURCES})

# Add platform-specific backend sources
if(REALCRAFT_PLATFORM_MACOS)
    target_sources(realcraft_graphics PRIVATE
        metal/metal_device.mm
        metal/metal_command_buffer.mm
        metal/metal_buffer.mm
        metal/metal_texture.mm
        metal/metal_sampler.mm
        metal/metal_shader.mm
        metal/metal_pipeline.mm
        metal/metal_render_pass.mm
    )
    # Enable Objective-C++ for Metal sources
    set_source_files_properties(
        metal/metal_device.mm
        metal/metal_command_buffer.mm
        metal/metal_buffer.mm
        metal/metal_texture.mm
        metal/metal_sampler.mm
        metal/metal_shader.mm
        metal/metal_pipeline.mm
        metal/metal_render_pass.mm
        PROPERTIES COMPILE_FLAGS "-x objective-c++"
    )
elseif(REALCRAFT_PLATFORM_LINUX OR REALCRAFT_PLATFORM_WINDOWS)
    target_sources(realcraft_graphics PRIVATE
        vulkan/vulkan_device.cpp
    )
endif()

# Include directories
target_include_directories(realcraft_graphics
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Public dependencies (users of this library will need these)
target_link_libraries(realcraft_graphics
    PUBLIC
        glm::glm
    PRIVATE
        spdlog::spdlog
        glslang::glslang
        glslang::glslang-default-resource-limits
        glslang::SPIRV
        SPIRV-Tools-static
        spirv-cross-core
        spirv-cross-glsl
        spirv-cross-msl
        spirv-cross-reflect
)

# Platform-specific frameworks
if(REALCRAFT_PLATFORM_MACOS)
    target_link_libraries(realcraft_graphics
        PRIVATE
            ${METAL_FRAMEWORK}
            ${METALKIT_FRAMEWORK}
            ${QUARTZCORE_FRAMEWORK}
            ${FOUNDATION_FRAMEWORK}
    )
elseif(REALCRAFT_PLATFORM_LINUX)
    target_link_libraries(realcraft_graphics
        PRIVATE
            Vulkan::Vulkan
    )
endif()

# Apply compiler settings
realcraft_configure_target(realcraft_graphics)

# Alias for cleaner CMake usage
add_library(realcraft::graphics ALIAS realcraft_graphics)
