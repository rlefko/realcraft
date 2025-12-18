// RealCraft - Placeholder Test
// Verifies that the test infrastructure is working

#include <glm/glm.hpp>
#include <gtest/gtest.h>

// Bullet Physics
#include <btBulletDynamicsCommon.h>

// FastNoise2
#include <FastNoise/FastNoise.h>

#include <cmath>
#include <vector>

// =============================================================================
// Build System Tests
// =============================================================================

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

// =============================================================================
// Bullet Physics Tests
// =============================================================================

TEST(BulletPhysicsTest, WorldCreation) {
    // Test that we can create a physics world
    btDefaultCollisionConfiguration collision_config;
    btCollisionDispatcher dispatcher(&collision_config);
    btDbvtBroadphase broadphase;
    btSequentialImpulseConstraintSolver solver;

    btDiscreteDynamicsWorld world(&dispatcher, &broadphase, &solver, &collision_config);

    // Should be able to create world without crashing
    EXPECT_TRUE(true);
}

TEST(BulletPhysicsTest, GravitySetting) {
    // Test gravity configuration
    btDefaultCollisionConfiguration collision_config;
    btCollisionDispatcher dispatcher(&collision_config);
    btDbvtBroadphase broadphase;
    btSequentialImpulseConstraintSolver solver;

    btDiscreteDynamicsWorld world(&dispatcher, &broadphase, &solver, &collision_config);

    // Set and verify gravity
    world.setGravity(btVector3(0, -9.81f, 0));
    btVector3 gravity = world.getGravity();

    EXPECT_FLOAT_EQ(gravity.getX(), 0.0f);
    EXPECT_NEAR(gravity.getY(), -9.81f, 0.001f);
    EXPECT_FLOAT_EQ(gravity.getZ(), 0.0f);
}

TEST(BulletPhysicsTest, RigidBodyCreation) {
    // Test creating a simple rigid body
    btDefaultCollisionConfiguration collision_config;
    btCollisionDispatcher dispatcher(&collision_config);
    btDbvtBroadphase broadphase;
    btSequentialImpulseConstraintSolver solver;

    btDiscreteDynamicsWorld world(&dispatcher, &broadphase, &solver, &collision_config);
    world.setGravity(btVector3(0, -9.81f, 0));

    // Create a box shape
    btBoxShape* box_shape = new btBoxShape(btVector3(1.0f, 1.0f, 1.0f));

    // Create motion state
    btDefaultMotionState* motion_state = new btDefaultMotionState(
        btTransform(btQuaternion(0, 0, 0, 1), btVector3(0, 10, 0)));

    // Calculate inertia
    btScalar mass = 1.0f;
    btVector3 inertia(0, 0, 0);
    box_shape->calculateLocalInertia(mass, inertia);

    // Create rigid body
    btRigidBody::btRigidBodyConstructionInfo rb_info(mass, motion_state, box_shape, inertia);
    btRigidBody* body = new btRigidBody(rb_info);

    // Add to world
    world.addRigidBody(body);

    EXPECT_EQ(world.getNumCollisionObjects(), 1);

    // Cleanup
    world.removeRigidBody(body);
    delete body;
    delete motion_state;
    delete box_shape;
}

// =============================================================================
// FastNoise2 Tests
// =============================================================================

TEST(FastNoise2Test, SimplexNoiseCreation) {
    // Test that we can create a simplex noise generator
    auto fn_simplex = FastNoise::New<FastNoise::Simplex>();
    EXPECT_NE(fn_simplex, nullptr);
}

TEST(FastNoise2Test, NoiseGeneration) {
    // Test generating noise values
    auto fn_simplex = FastNoise::New<FastNoise::Simplex>();

    // Generate some noise values
    float noise1 = fn_simplex->GenSingle2D(0.0f, 0.0f, 1337);
    float noise2 = fn_simplex->GenSingle2D(1.0f, 1.0f, 1337);
    float noise3 = fn_simplex->GenSingle2D(0.0f, 0.0f, 1337);  // Same as noise1

    // Noise values should be in valid range (approximately -1 to 1)
    EXPECT_GE(noise1, -1.5f);
    EXPECT_LE(noise1, 1.5f);
    EXPECT_GE(noise2, -1.5f);
    EXPECT_LE(noise2, 1.5f);

    // Same inputs should produce same output (deterministic)
    EXPECT_FLOAT_EQ(noise1, noise3);

    // Different inputs should (usually) produce different output
    EXPECT_NE(noise1, noise2);
}

TEST(FastNoise2Test, FractalNoiseCreation) {
    // Test creating fractal noise with octaves
    auto fn_simplex = FastNoise::New<FastNoise::Simplex>();
    auto fn_fractal = FastNoise::New<FastNoise::FractalFBm>();

    fn_fractal->SetSource(fn_simplex);
    fn_fractal->SetOctaveCount(4);

    float noise = fn_fractal->GenSingle2D(0.5f, 0.5f, 1337);

    // Fractal noise can exceed [-1, 1] range
    EXPECT_GE(noise, -2.0f);
    EXPECT_LE(noise, 2.0f);
}

TEST(FastNoise2Test, NoiseArray) {
    // Test generating an array of noise values
    auto fn_simplex = FastNoise::New<FastNoise::Simplex>();

    const int size = 16;
    std::vector<float> noise_output(size * size);

    fn_simplex->GenUniformGrid2D(
        noise_output.data(),
        0.0f, 0.0f,     // offset x, y
        size, size,     // count x, y
        0.1f, 0.1f,     // step size x, y (frequency)
        1337            // seed
    );

    // Verify we got valid output
    bool has_variation = false;
    float first_value = noise_output[0];
    for (float value : noise_output) {
        EXPECT_GE(value, -1.5f);
        EXPECT_LE(value, 1.5f);
        if (std::abs(value - first_value) > 0.001f) {
            has_variation = true;
        }
    }

    // Noise should have some variation
    EXPECT_TRUE(has_variation);
}
