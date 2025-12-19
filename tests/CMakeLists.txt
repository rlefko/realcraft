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
