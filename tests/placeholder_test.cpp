// RealCraft - Placeholder Test
// Verifies that the test infrastructure is working

#include <gtest/gtest.h>
#include <glm/glm.hpp>

TEST(BuildSystemTest, PlaceholderTest) {
    // This test verifies that the build system and test infrastructure work
    EXPECT_TRUE(true);
}

TEST(BuildSystemTest, GlmIntegration) {
    // Verify GLM is properly linked
    glm::vec3 a(1.0f, 2.0f, 3.0f);
    glm::vec3 b(4.0f, 5.0f, 6.0f);
    glm::vec3 c = a + b;

    EXPECT_FLOAT_EQ(c.x, 5.0f);
    EXPECT_FLOAT_EQ(c.y, 7.0f);
    EXPECT_FLOAT_EQ(c.z, 9.0f);
}

TEST(BuildSystemTest, PlatformMacros) {
    // At least one platform macro should be defined
    int platform_count = 0;
#if defined(REALCRAFT_PLATFORM_MACOS)
    platform_count++;
#endif
#if defined(REALCRAFT_PLATFORM_WINDOWS)
    platform_count++;
#endif
#if defined(REALCRAFT_PLATFORM_LINUX)
    platform_count++;
#endif
    EXPECT_EQ(platform_count, 1) << "Exactly one platform should be defined";
}
