# src/gameplay/CMakeLists.txt
# Gameplay subsystem library

set(GAMEPLAY_SOURCES
    block_interaction.cpp
)

# Create static library
add_library(realcraft_gameplay STATIC ${GAMEPLAY_SOURCES})

# Link dependencies
target_link_libraries(realcraft_gameplay
    PUBLIC
        realcraft::physics
        realcraft::world
        realcraft::core
    PRIVATE
        spdlog::spdlog
        glm::glm
)

# Include directories
target_include_directories(realcraft_gameplay
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Apply compiler settings
realcraft_configure_target(realcraft_gameplay)

# Alias for cleaner CMake usage
add_library(realcraft::gameplay ALIAS realcraft_gameplay)
