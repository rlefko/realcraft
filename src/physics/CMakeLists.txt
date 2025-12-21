# src/physics/CMakeLists.txt
# Physics subsystem library

set(PHYSICS_SOURCES
    colliders.cpp
    chunk_collider.cpp
    convex_hull_generator.cpp
    debris_system.cpp
    fluid_simulation.cpp
    fragmentation.cpp
    impact_damage.cpp
    physics_world.cpp
    player_controller.cpp
    ray_caster.cpp
    rigid_body.cpp
    structural_integrity.cpp
    support_graph.cpp
)

# Create static library
add_library(realcraft_physics STATIC ${PHYSICS_SOURCES})

# Link dependencies
target_link_libraries(realcraft_physics
    PUBLIC
        realcraft::world
        realcraft::core
    PRIVATE
        spdlog::spdlog
        glm::glm
        ${BULLET_LIBRARIES}
)

# Include directories
target_include_directories(realcraft_physics
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${BULLET_INCLUDE_DIRS}
)

# Apply compiler settings
realcraft_configure_target(realcraft_physics)

# Alias for cleaner CMake usage
add_library(realcraft::physics ALIAS realcraft_physics)
