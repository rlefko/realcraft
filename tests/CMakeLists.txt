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
