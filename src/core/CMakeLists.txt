# src/core/CMakeLists.txt
# Engine Core library

set(CORE_SOURCES
    logger.cpp
    memory.cpp
    config.cpp
    game_loop.cpp
    engine.cpp
)

# Create static library
add_library(realcraft_core STATIC ${CORE_SOURCES})

# Link dependencies
target_link_libraries(realcraft_core
    PUBLIC
        realcraft::platform
        realcraft::graphics
    PRIVATE
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)

# Include directories
target_include_directories(realcraft_core
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Apply compiler settings
realcraft_configure_target(realcraft_core)

# Alias for cleaner CMake usage
add_library(realcraft::core ALIAS realcraft_core)
