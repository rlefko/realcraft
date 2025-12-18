# BuildOptions.cmake
# User-configurable build options for RealCraft

# Testing
option(REALCRAFT_BUILD_TESTS "Build unit tests" ON)

# Debug options
option(REALCRAFT_ENABLE_ASAN "Enable Address Sanitizer in Debug builds" ON)

# Release options
option(REALCRAFT_ENABLE_LTO "Enable Link-Time Optimization in Release builds" ON)

# Precompiled headers (for faster compilation)
option(REALCRAFT_USE_PRECOMPILED_HEADERS "Use precompiled headers" ON)

# Ray tracing options (for Phase 7 - Path Tracing)
option(REALCRAFT_ENABLE_RAYTRACING "Enable ray tracing support" ON)

# Upscaling technologies
if(REALCRAFT_PLATFORM_WINDOWS)
    option(REALCRAFT_ENABLE_DLSS "Enable NVIDIA DLSS support" OFF)
endif()
option(REALCRAFT_ENABLE_FSR "Enable AMD FSR support" OFF)
if(REALCRAFT_PLATFORM_MACOS)
    option(REALCRAFT_ENABLE_METALFX "Enable Apple MetalFX support" ON)
endif()

# Performance/profiling
option(REALCRAFT_ENABLE_PROFILING "Enable profiling instrumentation" OFF)

# Worker threads (0 = auto-detect based on hardware)
set(REALCRAFT_WORKER_THREADS "0" CACHE STRING "Number of worker threads (0 = auto)")

# Print build options summary
message(STATUS "Build Options:")
message(STATUS "  REALCRAFT_BUILD_TESTS:       ${REALCRAFT_BUILD_TESTS}")
message(STATUS "  REALCRAFT_ENABLE_ASAN:       ${REALCRAFT_ENABLE_ASAN}")
message(STATUS "  REALCRAFT_ENABLE_LTO:        ${REALCRAFT_ENABLE_LTO}")
message(STATUS "  REALCRAFT_ENABLE_RAYTRACING: ${REALCRAFT_ENABLE_RAYTRACING}")
