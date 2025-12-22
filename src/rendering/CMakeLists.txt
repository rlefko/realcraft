# src/rendering/CMakeLists.txt
# Rendering subsystem library

set(RENDERING_SOURCES
    camera.cpp
    chunk_mesh.cpp
    frustum.cpp
    hud_renderer.cpp
    lighting.cpp
    mesh_generator.cpp
    mesh_manager.cpp
    procedural_texture.cpp
    render_system.cpp
    texture_atlas.cpp
    texture_manager.cpp
)

# Create static library
add_library(realcraft_rendering STATIC ${RENDERING_SOURCES})

# Link dependencies
target_link_libraries(realcraft_rendering
    PUBLIC
        realcraft::core
        realcraft::world
        realcraft::graphics
    PRIVATE
        spdlog::spdlog
        glm::glm
)

# Include directories
target_include_directories(realcraft_rendering
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Apply compiler settings
realcraft_configure_target(realcraft_rendering)

# Alias for cleaner CMake usage
add_library(realcraft::rendering ALIAS realcraft_rendering)
