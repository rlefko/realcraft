# tests/CMakeLists.txt
# RealCraft unit tests

include(GoogleTest)

# Placeholder test to verify test infrastructure works
add_executable(realcraft_tests
    placeholder_test.cpp
)

# Link test dependencies (includes GTest, GLM, Bullet, FastNoise2)
realcraft_link_test_dependencies(realcraft_tests)

realcraft_configure_target(realcraft_tests)

# Discover tests for CTest
gtest_discover_tests(realcraft_tests)

# Platform unit tests (no display required - can run in CI)
add_executable(realcraft_platform_tests
    unit/platform/timer_test.cpp
    unit/platform/file_io_test.cpp
    unit/platform/key_codes_test.cpp
    unit/platform/input_mapper_test.cpp
)

target_link_libraries(realcraft_platform_tests
    PRIVATE
        realcraft::platform
        GTest::gtest
        GTest::gtest_main
        nlohmann_json::nlohmann_json
)

target_include_directories(realcraft_platform_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

realcraft_configure_target(realcraft_platform_tests)
gtest_discover_tests(realcraft_platform_tests)

# World unit tests
add_executable(realcraft_world_tests
    unit/world/biome_test.cpp
    unit/world/block_test.cpp
    unit/world/cave_test.cpp
    unit/world/chunk_border_test.cpp
    unit/world/chunk_data_test.cpp
    unit/world/chunk_test.cpp
    unit/world/climate_test.cpp
    unit/world/erosion_test.cpp
    unit/world/ore_test.cpp
    unit/world/serialization_test.cpp
    unit/world/structure_test.cpp
    unit/world/terrain_generator_test.cpp
    unit/world/tree_test.cpp
    unit/world/types_test.cpp
    unit/world/vegetation_test.cpp
)

target_link_libraries(realcraft_world_tests
    PRIVATE
        realcraft::world
        realcraft::core
        GTest::gtest
        GTest::gtest_main
        glm::glm
)

target_include_directories(realcraft_world_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

realcraft_configure_target(realcraft_world_tests)
gtest_discover_tests(realcraft_world_tests)

# Rendering unit tests (no GPU required for basic tests)
add_executable(realcraft_rendering_tests
    unit/rendering/camera_test.cpp
    unit/rendering/frustum_test.cpp
    unit/rendering/lighting_test.cpp
    unit/rendering/mesh_vertex_test.cpp
)

target_link_libraries(realcraft_rendering_tests
    PRIVATE
        realcraft::rendering
        realcraft::world
        realcraft::core
        GTest::gtest
        GTest::gtest_main
        glm::glm
)

target_include_directories(realcraft_rendering_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

realcraft_configure_target(realcraft_rendering_tests)
gtest_discover_tests(realcraft_rendering_tests)

# Physics unit tests
add_executable(realcraft_physics_tests
    unit/physics/types_test.cpp
    unit/physics/colliders_test.cpp
    unit/physics/ray_caster_test.cpp
    unit/physics/chunk_collider_test.cpp
    unit/physics/physics_world_test.cpp
)

target_link_libraries(realcraft_physics_tests
    PRIVATE
        realcraft::physics
        realcraft::world
        realcraft::core
        GTest::gtest
        GTest::gtest_main
        glm::glm
        ${BULLET_LIBRARIES}
)

target_include_directories(realcraft_physics_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${BULLET_INCLUDE_DIRS}
)

realcraft_configure_target(realcraft_physics_tests)
gtest_discover_tests(realcraft_physics_tests)
