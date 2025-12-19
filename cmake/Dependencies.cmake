# Dependencies.cmake
# Find and configure third-party dependencies via vcpkg

# Math library (GLM) - header-only
find_package(glm CONFIG REQUIRED)
message(STATUS "Found glm")

# Logging (spdlog)
find_package(spdlog CONFIG REQUIRED)
message(STATUS "Found spdlog")

# JSON parsing (nlohmann_json) - for config files
find_package(nlohmann_json CONFIG REQUIRED)
message(STATUS "Found nlohmann_json")

# Compression (zstd) - for chunk serialization
find_package(zstd CONFIG REQUIRED)
message(STATUS "Found zstd")

# Testing (Google Test)
if(REALCRAFT_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
    message(STATUS "Found GTest")
endif()

# Physics (Bullet3)
find_package(Bullet CONFIG REQUIRED)
message(STATUS "Found Bullet3")
message(STATUS "  Bullet include dirs: ${BULLET_INCLUDE_DIRS}")

# Windowing and input (GLFW3)
find_package(glfw3 CONFIG REQUIRED)
message(STATUS "Found GLFW3")

# Shader compilation (glslang) - GLSL to SPIR-V
find_package(glslang CONFIG REQUIRED)
message(STATUS "Found glslang")

# SPIR-V tools (spirv-tools) - SPIR-V optimization and validation
find_package(SPIRV-Tools CONFIG REQUIRED)
message(STATUS "Found SPIRV-Tools")

# SPIR-V cross-compilation (spirv-cross) - SPIR-V to MSL/HLSL
find_package(spirv_cross_core CONFIG REQUIRED)
find_package(spirv_cross_glsl CONFIG REQUIRED)
find_package(spirv_cross_msl CONFIG REQUIRED)
find_package(spirv_cross_reflect CONFIG REQUIRED)
message(STATUS "Found spirv-cross")

# Noise generation (FastNoise2) - added as git submodule
# FastNoise2 is not available in vcpkg, so we include it as a subdirectory
set(FASTNOISE2_NOISETOOL OFF CACHE BOOL "Build Noise Tool" FORCE)
set(FASTNOISE2_TESTS OFF CACHE BOOL "Build FastNoise2 tests" FORCE)
# Pre-configure FastSIMD source for CPM.cmake to use our submodule instead of downloading
# This fixes Windows CI issues where CPM download can fail due to path length or network issues
set(CPM_FastSIMD_SOURCE "${CMAKE_SOURCE_DIR}/external/FastSIMD" CACHE PATH "FastSIMD source directory")
add_subdirectory(${CMAKE_SOURCE_DIR}/external/FastNoise2)
message(STATUS "Found FastNoise2 (submodule)")

# Platform-specific dependencies
if(REALCRAFT_PLATFORM_MACOS)
    # Metal frameworks (system libraries)
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(METALKIT_FRAMEWORK MetalKit REQUIRED)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    message(STATUS "Found Metal frameworks")
elseif(REALCRAFT_PLATFORM_LINUX)
    # Vulkan
    find_package(Vulkan REQUIRED)
    message(STATUS "Found Vulkan")
elseif(REALCRAFT_PLATFORM_WINDOWS)
    # DX12 is available via Windows SDK
    # Vulkan (optional on Windows)
    if(REALCRAFT_GRAPHICS_API STREQUAL "Vulkan")
        find_package(Vulkan REQUIRED)
        message(STATUS "Found Vulkan")
    endif()
endif()

# Helper function to link common dependencies to a target
function(realcraft_link_dependencies target)
    target_link_libraries(${target}
        PRIVATE
            glm::glm
            spdlog::spdlog
            nlohmann_json::nlohmann_json
            ${BULLET_LIBRARIES}
            FastNoise
            glfw
    )

    # Add Bullet include directories
    target_include_directories(${target}
        PRIVATE
            ${BULLET_INCLUDE_DIRS}
    )

    if(REALCRAFT_PLATFORM_MACOS)
        target_link_libraries(${target}
            PRIVATE
                ${METAL_FRAMEWORK}
                ${METALKIT_FRAMEWORK}
                ${QUARTZCORE_FRAMEWORK}
                ${FOUNDATION_FRAMEWORK}
                ${COCOA_FRAMEWORK}
        )
    elseif(REALCRAFT_PLATFORM_LINUX)
        target_link_libraries(${target}
            PRIVATE
                Vulkan::Vulkan
        )
    endif()
endfunction()

# Helper function to link test dependencies
function(realcraft_link_test_dependencies target)
    target_link_libraries(${target}
        PRIVATE
            GTest::gtest
            GTest::gtest_main
            glm::glm
            ${BULLET_LIBRARIES}
            FastNoise
    )

    target_include_directories(${target}
        PRIVATE
            ${BULLET_INCLUDE_DIRS}
    )
endfunction()

# Helper function to link graphics/shader dependencies
function(realcraft_link_graphics_dependencies target)
    target_link_libraries(${target}
        PRIVATE
            glslang::glslang
            glslang::glslang-default-resource-limits
            glslang::SPIRV
            glslang::SPVRemapper
            SPIRV-Tools-static
            spirv-cross-core
            spirv-cross-glsl
            spirv-cross-msl
            spirv-cross-reflect
    )

    if(REALCRAFT_PLATFORM_MACOS)
        target_link_libraries(${target}
            PRIVATE
                ${METAL_FRAMEWORK}
                ${METALKIT_FRAMEWORK}
                ${QUARTZCORE_FRAMEWORK}
                ${FOUNDATION_FRAMEWORK}
                ${COCOA_FRAMEWORK}
        )
    elseif(REALCRAFT_PLATFORM_LINUX)
        target_link_libraries(${target}
            PRIVATE
                Vulkan::Vulkan
        )
    endif()
endfunction()
