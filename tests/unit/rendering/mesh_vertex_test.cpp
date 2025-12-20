// RealCraft Rendering Tests
// mesh_vertex_test.cpp - Unit tests for VoxelVertex struct

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <realcraft/rendering/mesh_vertex.hpp>

namespace realcraft::rendering::test {

TEST(VoxelVertexTest, SizeIs36Bytes) {
    // VoxelVertex should be exactly 36 bytes for GPU alignment (vec4 position)
    EXPECT_EQ(sizeof(VoxelVertex), 36u);
}

TEST(VoxelVertexTest, PositionOffset) {
    // Position should be at offset 0 (16 bytes for vec4)
    EXPECT_EQ(offsetof(VoxelVertex, position), 0u);
}

TEST(VoxelVertexTest, NormalOffset) {
    // Normal should follow position (16 bytes)
    EXPECT_EQ(offsetof(VoxelVertex, normal), 16u);
}

TEST(VoxelVertexTest, AmbientOcclusionOffset) {
    // AO should follow normal (19 bytes)
    EXPECT_EQ(offsetof(VoxelVertex, ao), 19u);
}

TEST(VoxelVertexTest, UVOffset) {
    // UV should follow ao (20 bytes)
    EXPECT_EQ(offsetof(VoxelVertex, uv), 20u);
}

TEST(VoxelVertexTest, TextureIndexOffset) {
    // Texture index should follow uv (28 bytes)
    EXPECT_EQ(offsetof(VoxelVertex, texture_index), 28u);
}

TEST(VoxelVertexTest, LightSkyOffset) {
    // Light sky should follow texture_index (30 bytes)
    EXPECT_EQ(offsetof(VoxelVertex, light_sky), 30u);
}

TEST(VoxelVertexTest, LightBlockOffset) {
    // Light block should follow light_sky (31 bytes)
    EXPECT_EQ(offsetof(VoxelVertex, light_block), 31u);
}

TEST(VoxelVertexTest, ColorOffset) {
    // Color should follow light_block (32 bytes)
    EXPECT_EQ(offsetof(VoxelVertex, color), 32u);
}

TEST(VoxelVertexTest, DefaultConstruction) {
    VoxelVertex vertex{};

    // All fields should be zero-initialized
    EXPECT_FLOAT_EQ(vertex.position[0], 0.0f);
    EXPECT_FLOAT_EQ(vertex.position[1], 0.0f);
    EXPECT_FLOAT_EQ(vertex.position[2], 0.0f);
    EXPECT_FLOAT_EQ(vertex.position[3], 0.0f);
    EXPECT_EQ(vertex.normal[0], 0);
    EXPECT_EQ(vertex.normal[1], 0);
    EXPECT_EQ(vertex.normal[2], 0);
    EXPECT_EQ(vertex.ao, 0);
    EXPECT_FLOAT_EQ(vertex.uv[0], 0.0f);
    EXPECT_FLOAT_EQ(vertex.uv[1], 0.0f);
    EXPECT_EQ(vertex.texture_index, 0);
    EXPECT_EQ(vertex.light_sky, 0);
    EXPECT_EQ(vertex.light_block, 0);
}

TEST(VoxelVertexTest, SetPosition) {
    VoxelVertex vertex{};
    vertex.position[0] = 1.5f;
    vertex.position[1] = 64.0f;
    vertex.position[2] = -32.25f;
    vertex.position[3] = 1.0f;

    EXPECT_FLOAT_EQ(vertex.position[0], 1.5f);
    EXPECT_FLOAT_EQ(vertex.position[1], 64.0f);
    EXPECT_FLOAT_EQ(vertex.position[2], -32.25f);
    EXPECT_FLOAT_EQ(vertex.position[3], 1.0f);
}

TEST(VoxelVertexTest, SetNormal) {
    VoxelVertex vertex{};

    // Normal pointing up (+Y)
    vertex.normal[0] = 0;
    vertex.normal[1] = 127;
    vertex.normal[2] = 0;

    EXPECT_EQ(vertex.normal[0], 0);
    EXPECT_EQ(vertex.normal[1], 127);
    EXPECT_EQ(vertex.normal[2], 0);
}

TEST(VoxelVertexTest, NormalCanBeNegative) {
    VoxelVertex vertex{};

    // Normal pointing down (-Y)
    vertex.normal[0] = 0;
    vertex.normal[1] = -127;
    vertex.normal[2] = 0;

    EXPECT_EQ(vertex.normal[0], 0);
    EXPECT_EQ(vertex.normal[1], -127);
    EXPECT_EQ(vertex.normal[2], 0);
}

TEST(VoxelVertexTest, SetAO) {
    VoxelVertex vertex{};

    // Full AO (no occlusion)
    vertex.ao = 255;
    EXPECT_EQ(vertex.ao, 255);

    // Half AO
    vertex.ao = 128;
    EXPECT_EQ(vertex.ao, 128);

    // No AO (full occlusion)
    vertex.ao = 0;
    EXPECT_EQ(vertex.ao, 0);
}

TEST(VoxelVertexTest, SetUV) {
    VoxelVertex vertex{};
    vertex.uv[0] = 0.5f;
    vertex.uv[1] = 0.75f;

    EXPECT_FLOAT_EQ(vertex.uv[0], 0.5f);
    EXPECT_FLOAT_EQ(vertex.uv[1], 0.75f);
}

TEST(VoxelVertexTest, SetTextureIndex) {
    VoxelVertex vertex{};

    vertex.texture_index = 42;
    EXPECT_EQ(vertex.texture_index, 42);

    // Max value
    vertex.texture_index = 65535;
    EXPECT_EQ(vertex.texture_index, 65535);
}

TEST(VoxelVertexTest, SetLightLevels) {
    VoxelVertex vertex{};

    vertex.light_sky = 15;
    vertex.light_block = 8;

    EXPECT_EQ(vertex.light_sky, 15);
    EXPECT_EQ(vertex.light_block, 8);
}

TEST(VoxelVertexTest, SetColor) {
    VoxelVertex vertex{};

    vertex.color[0] = 255;  // R
    vertex.color[1] = 128;  // G
    vertex.color[2] = 64;   // B
    vertex.color[3] = 200;  // A

    EXPECT_EQ(vertex.color[0], 255);
    EXPECT_EQ(vertex.color[1], 128);
    EXPECT_EQ(vertex.color[2], 64);
    EXPECT_EQ(vertex.color[3], 200);
}

TEST(VoxelVertexTest, IsPOD) {
    // VoxelVertex should be POD for efficient GPU transfer
    EXPECT_TRUE(std::is_trivially_copyable_v<VoxelVertex>);
    EXPECT_TRUE(std::is_standard_layout_v<VoxelVertex>);
}

TEST(VoxelVertexTest, ArrayOfVertices) {
    // Create array of vertices for mesh
    std::vector<VoxelVertex> vertices(1000);

    // Should be contiguous for GPU upload
    for (size_t i = 0; i < vertices.size() - 1; i++) {
        ptrdiff_t diff = reinterpret_cast<char*>(&vertices[i + 1]) - reinterpret_cast<char*>(&vertices[i]);
        EXPECT_EQ(diff, 36);  // 36 bytes per vertex
    }
}

TEST(VoxelVertexTest, MemoryCopy) {
    VoxelVertex src{};
    src.position[0] = 1.0f;
    src.position[1] = 2.0f;
    src.position[2] = 3.0f;
    src.position[3] = 1.0f;
    src.normal[0] = 0;
    src.normal[1] = 127;
    src.normal[2] = 0;
    src.ao = 255;
    src.uv[0] = 0.5f;
    src.uv[1] = 0.5f;
    src.texture_index = 5;
    src.light_sky = 15;
    src.light_block = 0;
    src.color[0] = 255;
    src.color[1] = 255;
    src.color[2] = 255;
    src.color[3] = 255;

    VoxelVertex dst{};
    std::memcpy(&dst, &src, sizeof(VoxelVertex));

    EXPECT_FLOAT_EQ(dst.position[0], src.position[0]);
    EXPECT_FLOAT_EQ(dst.position[1], src.position[1]);
    EXPECT_FLOAT_EQ(dst.position[2], src.position[2]);
    EXPECT_FLOAT_EQ(dst.position[3], src.position[3]);
    EXPECT_EQ(dst.normal[0], src.normal[0]);
    EXPECT_EQ(dst.normal[1], src.normal[1]);
    EXPECT_EQ(dst.normal[2], src.normal[2]);
    EXPECT_EQ(dst.ao, src.ao);
    EXPECT_FLOAT_EQ(dst.uv[0], src.uv[0]);
    EXPECT_FLOAT_EQ(dst.uv[1], src.uv[1]);
    EXPECT_EQ(dst.texture_index, src.texture_index);
    EXPECT_EQ(dst.light_sky, src.light_sky);
    EXPECT_EQ(dst.light_block, src.light_block);
}

TEST(VoxelVertexTest, PackNormal) {
    int8_t packed[3];

    // Test +Y normal
    VoxelVertex::pack_normal(0.0f, 1.0f, 0.0f, packed);
    EXPECT_EQ(packed[0], 0);
    EXPECT_EQ(packed[1], 127);
    EXPECT_EQ(packed[2], 0);

    // Test -X normal
    VoxelVertex::pack_normal(-1.0f, 0.0f, 0.0f, packed);
    EXPECT_EQ(packed[0], -127);
    EXPECT_EQ(packed[1], 0);
    EXPECT_EQ(packed[2], 0);
}

TEST(VoxelVertexTest, GetAttributes) {
    auto attrs = VoxelVertex::get_attributes();
    EXPECT_EQ(attrs.size(), 5u);

    // Check first attribute (position at offset 0)
    EXPECT_EQ(attrs[0].location, 0u);
    EXPECT_EQ(attrs[0].offset, 0u);
}

TEST(VoxelVertexTest, GetBinding) {
    auto binding = VoxelVertex::get_binding();
    EXPECT_EQ(binding.stride, 36u);
    EXPECT_EQ(binding.per_instance, false);
}

}  // namespace realcraft::rendering::test
