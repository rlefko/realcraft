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

# Testing (Google Test)
if(REALCRAFT_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
    message(STATUS "Found GTest")
endif()

# Note: The following dependencies will be enabled in Milestone 0.3
# Physics (Bullet3)
# find_package(Bullet CONFIG REQUIRED)
# message(STATUS "Found Bullet3")

# Noise generation (FastNoise2)
# find_package(FastNoise2 CONFIG REQUIRED)
# message(STATUS "Found FastNoise2")

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
