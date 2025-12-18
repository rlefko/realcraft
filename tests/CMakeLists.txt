# tests/CMakeLists.txt
# RealCraft unit tests

include(GoogleTest)

# Placeholder test to verify test infrastructure works
add_executable(realcraft_tests
    placeholder_test.cpp
)

target_link_libraries(realcraft_tests
    PRIVATE
        GTest::gtest
        GTest::gtest_main
        glm::glm
)

realcraft_configure_target(realcraft_tests)

# Discover tests for CTest
gtest_discover_tests(realcraft_tests)
