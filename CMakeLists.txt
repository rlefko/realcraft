cmake_minimum_required(VERSION 3.28)

# Project definition
project(RealCraft
    VERSION 0.1.0
    DESCRIPTION "Realistic voxel sandbox game engine"
    LANGUAGES CXX
)

# Prevent in-source builds
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed. Please create a build directory.")
endif()

# Include custom CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Platform detection and compiler settings
include(PlatformDetection)
include(CompilerSettings)
include(BuildOptions)

# Set C++ standard globally
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for tooling (clangd, etc.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type default for single-config generators
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
        "Debug" "Release" "RelWithDebInfo" "MinSizeRel")
endif()

# Dependencies (vcpkg handles finding packages)
include(Dependencies)

# Add source subdirectory
add_subdirectory(src)

# Tests
if(REALCRAFT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Configuration summary
message(STATUS "")
message(STATUS "===== RealCraft Configuration =====")
message(STATUS "  Version:       ${PROJECT_VERSION}")
message(STATUS "  Platform:      ${REALCRAFT_PLATFORM}")
message(STATUS "  Architecture:  ${REALCRAFT_ARCH}")
message(STATUS "  Graphics API:  ${REALCRAFT_GRAPHICS_API}")
message(STATUS "  Build Type:    ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:  ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler:      ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "===================================")
message(STATUS "")
