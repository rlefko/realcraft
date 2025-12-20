// RealCraft Rendering System
// procedural_texture.hpp - Procedural texture generation for placeholders

#pragma once

#include <cstdint>
#include <vector>

namespace realcraft::rendering {

// Pattern types for procedural textures
enum class TexturePattern {
    Solid,
    Checkerboard,
    Noise,
    Gradient,
    Brick,
    Grid,
};

// Generates procedural textures
class ProceduralTextureGenerator {
public:
    // Generate RGBA texture data
    // Returns vector of size width * height * 4
    [[nodiscard]] static std::vector<uint8_t> generate(uint32_t width, uint32_t height, uint32_t color_primary,
                                                       uint32_t color_secondary, TexturePattern pattern,
                                                       uint32_t seed = 0);

    // Convenience methods for common block textures
    [[nodiscard]] static std::vector<uint8_t> generate_stone(uint32_t size, uint32_t seed = 0);
    [[nodiscard]] static std::vector<uint8_t> generate_dirt(uint32_t size, uint32_t seed = 0);
    [[nodiscard]] static std::vector<uint8_t> generate_grass_top(uint32_t size, uint32_t seed = 0);
    [[nodiscard]] static std::vector<uint8_t> generate_grass_side(uint32_t size, uint32_t seed = 0);
    [[nodiscard]] static std::vector<uint8_t> generate_sand(uint32_t size, uint32_t seed = 0);
    [[nodiscard]] static std::vector<uint8_t> generate_water(uint32_t size, uint32_t seed = 0);

    // Pack RGBA into uint32
    static constexpr uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        return (static_cast<uint32_t>(r)) | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(b) << 16) |
               (static_cast<uint32_t>(a) << 24);
    }

private:
    // Simple hash for noise generation
    static uint32_t hash(uint32_t x, uint32_t y, uint32_t seed);

    // Noise value 0-255
    static uint8_t noise_value(uint32_t x, uint32_t y, uint32_t seed);

    // Blend two colors
    static void blend_colors(uint32_t c1, uint32_t c2, float t, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a);
};

}  // namespace realcraft::rendering
