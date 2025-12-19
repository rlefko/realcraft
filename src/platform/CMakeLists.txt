# src/platform/CMakeLists.txt
# Platform Abstraction Layer library

# Collect platform-independent sources
set(PLATFORM_SOURCES
    timer.cpp
    file_io.cpp
    key_codes.cpp
    input_mapper.cpp
    window.cpp
    input.cpp
)

# Create static library
add_library(realcraft_platform STATIC ${PLATFORM_SOURCES})

# Add platform-specific sources
if(REALCRAFT_PLATFORM_MACOS)
    target_sources(realcraft_platform PRIVATE
        macos/window_macos.mm
    )
    # Enable Objective-C++ for Metal integration
    set_source_files_properties(macos/window_macos.mm
        PROPERTIES COMPILE_FLAGS "-x objective-c++"
    )
elseif(REALCRAFT_PLATFORM_WINDOWS)
    # Windows-specific sources (future)
    # target_sources(realcraft_platform PRIVATE
    #     windows/window_windows.cpp
    # )
elseif(REALCRAFT_PLATFORM_LINUX)
    # Linux-specific sources (future)
    # target_sources(realcraft_platform PRIVATE
    #     linux/window_linux.cpp
    # )
endif()

# Public dependencies (users of this library will need these)
target_link_libraries(realcraft_platform
    PUBLIC
        glfw
        glm::glm
    PRIVATE
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)

# Platform-specific frameworks
if(REALCRAFT_PLATFORM_MACOS)
    target_link_libraries(realcraft_platform
        PRIVATE
            ${COCOA_FRAMEWORK}
            ${QUARTZCORE_FRAMEWORK}
            ${METAL_FRAMEWORK}
            ${FOUNDATION_FRAMEWORK}
    )
endif()

# Include directories
target_include_directories(realcraft_platform
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Apply compiler settings
realcraft_configure_target(realcraft_platform)

# Alias for cleaner CMake usage
add_library(realcraft::platform ALIAS realcraft_platform)
