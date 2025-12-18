# PlatformDetection.cmake
# Detects operating system and architecture for RealCraft

# Detect operating system
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(REALCRAFT_PLATFORM "macOS")
    set(REALCRAFT_PLATFORM_MACOS TRUE)
    set(REALCRAFT_GRAPHICS_API "Metal")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(REALCRAFT_PLATFORM "Windows")
    set(REALCRAFT_PLATFORM_WINDOWS TRUE)
    set(REALCRAFT_GRAPHICS_API "DX12" CACHE STRING "Graphics API (DX12 or Vulkan)")
    set_property(CACHE REALCRAFT_GRAPHICS_API PROPERTY STRINGS "DX12" "Vulkan")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(REALCRAFT_PLATFORM "Linux")
    set(REALCRAFT_PLATFORM_LINUX TRUE)
    set(REALCRAFT_GRAPHICS_API "Vulkan")
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

# Detect architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64|ARM64")
    set(REALCRAFT_ARCH "arm64")
    set(REALCRAFT_ARCH_ARM64 TRUE)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|amd64")
    set(REALCRAFT_ARCH "x64")
    set(REALCRAFT_ARCH_X64 TRUE)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i686|i386|x86")
    message(FATAL_ERROR "32-bit x86 is not supported. RealCraft requires a 64-bit system.")
else()
    message(WARNING "Unknown architecture: ${CMAKE_SYSTEM_PROCESSOR}, assuming x64")
    set(REALCRAFT_ARCH "x64")
    set(REALCRAFT_ARCH_X64 TRUE)
endif()

# Apple Silicon detection
if(REALCRAFT_PLATFORM_MACOS AND REALCRAFT_ARCH_ARM64)
    set(REALCRAFT_APPLE_SILICON TRUE)
    message(STATUS "Detected Apple Silicon (arm64)")
endif()

# Provide preprocessor definitions for platform/arch detection in code
add_compile_definitions(
    $<$<BOOL:${REALCRAFT_PLATFORM_MACOS}>:REALCRAFT_PLATFORM_MACOS=1>
    $<$<BOOL:${REALCRAFT_PLATFORM_WINDOWS}>:REALCRAFT_PLATFORM_WINDOWS=1>
    $<$<BOOL:${REALCRAFT_PLATFORM_LINUX}>:REALCRAFT_PLATFORM_LINUX=1>
    $<$<BOOL:${REALCRAFT_ARCH_ARM64}>:REALCRAFT_ARCH_ARM64=1>
    $<$<BOOL:${REALCRAFT_ARCH_X64}>:REALCRAFT_ARCH_X64=1>
)

# Graphics API definitions
if(REALCRAFT_GRAPHICS_API STREQUAL "Metal")
    add_compile_definitions(REALCRAFT_GRAPHICS_METAL=1)
elseif(REALCRAFT_GRAPHICS_API STREQUAL "Vulkan")
    add_compile_definitions(REALCRAFT_GRAPHICS_VULKAN=1)
elseif(REALCRAFT_GRAPHICS_API STREQUAL "DX12")
    add_compile_definitions(REALCRAFT_GRAPHICS_DX12=1)
endif()

message(STATUS "Platform: ${REALCRAFT_PLATFORM} (${REALCRAFT_ARCH})")
message(STATUS "Graphics API: ${REALCRAFT_GRAPHICS_API}")
