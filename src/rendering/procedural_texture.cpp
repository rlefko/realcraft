// RealCraft Rendering System
// procedural_texture.cpp - Procedural texture generation

#include <algorithm>
#include <cmath>
#include <realcraft/rendering/procedural_texture.hpp>

namespace realcraft::rendering {

uint32_t ProceduralTextureGenerator::hash(uint32_t x, uint32_t y, uint32_t seed) {
    uint32_t h = seed;
    h ^= x * 374761393u;
    h ^= y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h;
}

uint8_t ProceduralTextureGenerator::noise_value(uint32_t x, uint32_t y, uint32_t seed) {
    return static_cast<uint8_t>(hash(x, y, seed) & 0xFF);
}

void ProceduralTextureGenerator::blend_colors(uint32_t c1, uint32_t c2, float t, uint8_t& r, uint8_t& g, uint8_t& b,
                                              uint8_t& a) {
    uint8_t r1 = c1 & 0xFF;
    uint8_t g1 = (c1 >> 8) & 0xFF;
    uint8_t b1 = (c1 >> 16) & 0xFF;
    uint8_t a1 = (c1 >> 24) & 0xFF;

    uint8_t r2 = c2 & 0xFF;
    uint8_t g2 = (c2 >> 8) & 0xFF;
    uint8_t b2 = (c2 >> 16) & 0xFF;
    uint8_t a2 = (c2 >> 24) & 0xFF;

    r = static_cast<uint8_t>(r1 + (r2 - r1) * t);
    g = static_cast<uint8_t>(g1 + (g2 - g1) * t);
    b = static_cast<uint8_t>(b1 + (b2 - b1) * t);
    a = static_cast<uint8_t>(a1 + (a2 - a1) * t);
}

std::vector<uint8_t> ProceduralTextureGenerator::generate(uint32_t width, uint32_t height, uint32_t color_primary,
                                                          uint32_t color_secondary, TexturePattern pattern,
                                                          uint32_t seed) {
    std::vector<uint8_t> data(static_cast<size_t>(width) * height * 4);

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t idx = (y * width + x) * 4;
            uint8_t r, g, b, a;

            switch (pattern) {
                case TexturePattern::Solid: {
                    r = color_primary & 0xFF;
                    g = (color_primary >> 8) & 0xFF;
                    b = (color_primary >> 16) & 0xFF;
                    a = (color_primary >> 24) & 0xFF;
                    break;
                }

                case TexturePattern::Checkerboard: {
                    bool check = ((x / 4) + (y / 4)) % 2 == 0;
                    uint32_t color = check ? color_primary : color_secondary;
                    r = color & 0xFF;
                    g = (color >> 8) & 0xFF;
                    b = (color >> 16) & 0xFF;
                    a = (color >> 24) & 0xFF;
                    break;
                }

                case TexturePattern::Noise: {
                    float t = noise_value(x, y, seed) / 255.0f;
                    blend_colors(color_primary, color_secondary, t, r, g, b, a);
                    break;
                }

                case TexturePattern::Gradient: {
                    float t = static_cast<float>(y) / static_cast<float>(height);
                    blend_colors(color_primary, color_secondary, t, r, g, b, a);
                    break;
                }

                case TexturePattern::Brick: {
                    uint32_t brick_w = width / 4;
                    uint32_t brick_h = height / 4;
                    bool offset_row = (y / brick_h) % 2 == 1;
                    uint32_t bx = offset_row ? (x + brick_w / 2) % width : x;
                    bool is_mortar = (bx % brick_w < 1) || (y % brick_h < 1);
                    uint32_t color = is_mortar ? color_secondary : color_primary;
                    r = color & 0xFF;
                    g = (color >> 8) & 0xFF;
                    b = (color >> 16) & 0xFF;
                    a = (color >> 24) & 0xFF;
                    break;
                }

                case TexturePattern::Grid: {
                    bool is_line = (x % 4 == 0) || (y % 4 == 0);
                    uint32_t color = is_line ? color_secondary : color_primary;
                    r = color & 0xFF;
                    g = (color >> 8) & 0xFF;
                    b = (color >> 16) & 0xFF;
                    a = (color >> 24) & 0xFF;
                    break;
                }

                default: {
                    r = 255;
                    g = 0;
                    b = 255;
                    a = 255;  // Magenta for unknown
                    break;
                }
            }

            data[idx + 0] = r;
            data[idx + 1] = g;
            data[idx + 2] = b;
            data[idx + 3] = a;
        }
    }

    return data;
}

std::vector<uint8_t> ProceduralTextureGenerator::generate_stone(uint32_t size, uint32_t seed) {
    std::vector<uint8_t> data(static_cast<size_t>(size) * size * 4);

    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            uint32_t idx = (y * size + x) * 4;

            // Base gray with noise variation
            uint8_t noise = noise_value(x, y, seed);
            uint8_t base = 128;
            int variation = (static_cast<int>(noise) - 128) / 4;
            uint8_t gray = static_cast<uint8_t>(std::clamp(base + variation, 0, 255));

            // Occasional darker spots
            if (noise_value(x / 2, y / 2, seed + 1) > 220) {
                gray = static_cast<uint8_t>(gray * 0.85f);
            }

            data[idx + 0] = gray;
            data[idx + 1] = gray;
            data[idx + 2] = gray;
            data[idx + 3] = 255;
        }
    }

    return data;
}

std::vector<uint8_t> ProceduralTextureGenerator::generate_dirt(uint32_t size, uint32_t seed) {
    std::vector<uint8_t> data(static_cast<size_t>(size) * size * 4);

    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            uint32_t idx = (y * size + x) * 4;

            uint8_t noise = noise_value(x, y, seed);
            int variation = (static_cast<int>(noise) - 128) / 6;

            // Brown base
            data[idx + 0] = static_cast<uint8_t>(std::clamp(134 + variation, 0, 255));
            data[idx + 1] = static_cast<uint8_t>(std::clamp(96 + variation, 0, 255));
            data[idx + 2] = static_cast<uint8_t>(std::clamp(67 + variation, 0, 255));
            data[idx + 3] = 255;
        }
    }

    return data;
}

std::vector<uint8_t> ProceduralTextureGenerator::generate_grass_top(uint32_t size, uint32_t seed) {
    std::vector<uint8_t> data(static_cast<size_t>(size) * size * 4);

    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            uint32_t idx = (y * size + x) * 4;

            uint8_t noise = noise_value(x, y, seed);
            int variation = (static_cast<int>(noise) - 128) / 5;

            // Green base
            data[idx + 0] = static_cast<uint8_t>(std::clamp(91 + variation / 2, 0, 255));
            data[idx + 1] = static_cast<uint8_t>(std::clamp(139 + variation, 0, 255));
            data[idx + 2] = static_cast<uint8_t>(std::clamp(55 + variation / 3, 0, 255));
            data[idx + 3] = 255;
        }
    }

    return data;
}

std::vector<uint8_t> ProceduralTextureGenerator::generate_grass_side(uint32_t size, uint32_t seed) {
    std::vector<uint8_t> data(static_cast<size_t>(size) * size * 4);

    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            uint32_t idx = (y * size + x) * 4;

            // Top portion is grass, bottom is dirt
            float grass_ratio = 0.25f;  // Top 25% is grass
            float t = static_cast<float>(y) / static_cast<float>(size);

            uint8_t noise = noise_value(x, y, seed);
            int variation = (static_cast<int>(noise) - 128) / 6;

            if (t < grass_ratio) {
                // Green grass
                data[idx + 0] = static_cast<uint8_t>(std::clamp(91 + variation / 2, 0, 255));
                data[idx + 1] = static_cast<uint8_t>(std::clamp(139 + variation, 0, 255));
                data[idx + 2] = static_cast<uint8_t>(std::clamp(55 + variation / 3, 0, 255));
            } else {
                // Brown dirt
                data[idx + 0] = static_cast<uint8_t>(std::clamp(134 + variation, 0, 255));
                data[idx + 1] = static_cast<uint8_t>(std::clamp(96 + variation, 0, 255));
                data[idx + 2] = static_cast<uint8_t>(std::clamp(67 + variation, 0, 255));
            }
            data[idx + 3] = 255;
        }
    }

    return data;
}

std::vector<uint8_t> ProceduralTextureGenerator::generate_sand(uint32_t size, uint32_t seed) {
    std::vector<uint8_t> data(static_cast<size_t>(size) * size * 4);

    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            uint32_t idx = (y * size + x) * 4;

            uint8_t noise = noise_value(x, y, seed);
            int variation = (static_cast<int>(noise) - 128) / 8;

            // Sandy yellow
            data[idx + 0] = static_cast<uint8_t>(std::clamp(219 + variation, 0, 255));
            data[idx + 1] = static_cast<uint8_t>(std::clamp(199 + variation, 0, 255));
            data[idx + 2] = static_cast<uint8_t>(std::clamp(144 + variation, 0, 255));
            data[idx + 3] = 255;
        }
    }

    return data;
}

std::vector<uint8_t> ProceduralTextureGenerator::generate_water(uint32_t size, uint32_t seed) {
    std::vector<uint8_t> data(static_cast<size_t>(size) * size * 4);

    for (uint32_t y = 0; y < size; y++) {
        for (uint32_t x = 0; x < size; x++) {
            uint32_t idx = (y * size + x) * 4;

            uint8_t noise = noise_value(x, y, seed);
            int variation = (static_cast<int>(noise) - 128) / 10;

            // Blue water
            data[idx + 0] = static_cast<uint8_t>(std::clamp(40 + variation, 0, 255));
            data[idx + 1] = static_cast<uint8_t>(std::clamp(100 + variation, 0, 255));
            data[idx + 2] = static_cast<uint8_t>(std::clamp(200 + variation, 0, 255));
            data[idx + 3] = 180;  // Semi-transparent
        }
    }

    return data;
}

}  // namespace realcraft::rendering
