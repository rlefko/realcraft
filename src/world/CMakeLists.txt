# src/world/CMakeLists.txt
# World subsystem library

set(WORLD_SOURCES
    biome.cpp
    block.cpp
    block_registry.cpp
    chunk.cpp
    chunk_data.cpp
    climate.cpp
    origin_shifter.cpp
    serialization.cpp
    terrain_generator.cpp
    world_manager.cpp
)

# Create static library
add_library(realcraft_world STATIC ${WORLD_SOURCES})

# Link dependencies
target_link_libraries(realcraft_world
    PUBLIC
        realcraft::platform
        realcraft::core
    PRIVATE
        spdlog::spdlog
        $<IF:$<TARGET_EXISTS:zstd::libzstd_static>,zstd::libzstd_static,zstd::libzstd_shared>
        glm::glm
        FastNoise
)

# Include directories
target_include_directories(realcraft_world
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Apply compiler settings
realcraft_configure_target(realcraft_world)

# Alias for cleaner CMake usage
add_library(realcraft::world ALIAS realcraft_world)
