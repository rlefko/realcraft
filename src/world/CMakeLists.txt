# src/world/CMakeLists.txt
# World subsystem library

set(WORLD_SOURCES
    biome.cpp
    block.cpp
    block_registry.cpp
    cave_generator.cpp
    chunk.cpp
    chunk_data.cpp
    climate.cpp
    erosion.cpp
    erosion_cpu.cpp
    erosion_gpu.cpp
    erosion_heightmap.cpp
    ore_generator.cpp
    origin_shifter.cpp
    river_carver.cpp
    serialization.cpp
    structure_generator.cpp
    terrain_generator.cpp
    tree_generator.cpp
    vegetation_generator.cpp
    world_manager.cpp
)

# Create static library
add_library(realcraft_world STATIC ${WORLD_SOURCES})

# Link dependencies
target_link_libraries(realcraft_world
    PUBLIC
        realcraft::platform
        realcraft::core
        realcraft::graphics
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
