// RealCraft Rendering System
// hud_renderer.cpp - HUD and UI rendering implementation

#include <algorithm>
#include <cmath>
#include <cstring>
#include <realcraft/core/logger.hpp>
#include <realcraft/graphics/command_buffer.hpp>
#include <realcraft/graphics/shader_compiler.hpp>
#include <realcraft/rendering/hud_renderer.hpp>
#include <realcraft/rendering/texture_atlas.hpp>
#include <sstream>

namespace realcraft::rendering {

// ============================================================================
// Shader Source Code
// ============================================================================

static const char* UI_VERTEX_SHADER = R"(
#version 450

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec4 frag_color;

layout(push_constant) uniform UIPushConstants {
    vec2 screen_size;
    float use_texture;
    float texture_layer;
} push;

void main() {
    // Convert screen-space (0,0 = top-left) to NDC (-1,-1 = bottom-left)
    vec2 ndc = (in_position / push.screen_size) * 2.0 - 1.0;
    ndc.y = -ndc.y;  // Flip Y for screen coords

    gl_Position = vec4(ndc, 0.0, 1.0);
    frag_uv = in_uv;
    frag_color = in_color;
}
)";

static const char* UI_FRAGMENT_SHADER = R"(
#version 450

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec4 frag_color;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform sampler2D ui_texture;

layout(push_constant) uniform UIPushConstants {
    vec2 screen_size;
    float use_texture;
    float texture_layer;
} push;

void main() {
    if (push.use_texture > 0.5) {
        vec4 tex_color = texture(ui_texture, frag_uv);
        out_color = tex_color * frag_color;
    } else {
        out_color = frag_color;
    }
}
)";

// ============================================================================
// Bitmap Font Data
// ============================================================================

// Simple 8x16 bitmap font for ASCII 32-126 (95 characters)
// Each character is 8 pixels wide, 16 pixels tall
// Font texture is 256x256 (32 chars per row, 16 rows)

// clang-format off
static const uint8_t FONT_BITMAP[] = {
    // Space (32) - empty
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // ! (33)
    0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00,
    // " (34)
    0x00, 0x00, 0x6C, 0x6C, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // # (35)
    0x00, 0x00, 0x6C, 0x6C, 0xFE, 0x6C, 0x6C, 0x6C, 0xFE, 0x6C, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // $ (36)
    0x00, 0x18, 0x7C, 0xD6, 0xD0, 0xD0, 0x7C, 0x16, 0x16, 0xD6, 0x7C, 0x18, 0x00, 0x00, 0x00, 0x00,
    // % (37)
    0x00, 0x00, 0x00, 0xC6, 0xCC, 0x18, 0x30, 0x60, 0xCC, 0xC6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // & (38)
    0x00, 0x00, 0x38, 0x6C, 0x6C, 0x38, 0x76, 0xDC, 0xCC, 0xCC, 0x76, 0x00, 0x00, 0x00, 0x00, 0x00,
    // ' (39)
    0x00, 0x00, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // ( (40)
    0x00, 0x00, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // ) (41)
    0x00, 0x00, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00,
    // * (42)
    0x00, 0x00, 0x00, 0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // + (43)
    0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // , (44)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00,
    // - (45)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // . (46)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00,
    // / (47)
    0x00, 0x00, 0x06, 0x0C, 0x0C, 0x18, 0x18, 0x30, 0x30, 0x60, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00,
    // 0 (48)
    0x00, 0x00, 0x7C, 0xC6, 0xCE, 0xDE, 0xF6, 0xE6, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // 1 (49)
    0x00, 0x00, 0x18, 0x38, 0x78, 0x18, 0x18, 0x18, 0x18, 0x18, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00,
    // 2 (50)
    0x00, 0x00, 0x7C, 0xC6, 0x06, 0x0C, 0x18, 0x30, 0x60, 0xC0, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00,
    // 3 (51)
    0x00, 0x00, 0x7C, 0xC6, 0x06, 0x06, 0x3C, 0x06, 0x06, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // 4 (52)
    0x00, 0x00, 0x0C, 0x1C, 0x3C, 0x6C, 0xCC, 0xFE, 0x0C, 0x0C, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00,
    // 5 (53)
    0x00, 0x00, 0xFE, 0xC0, 0xC0, 0xFC, 0x06, 0x06, 0x06, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // 6 (54)
    0x00, 0x00, 0x38, 0x60, 0xC0, 0xFC, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // 7 (55)
    0x00, 0x00, 0xFE, 0xC6, 0x06, 0x0C, 0x18, 0x30, 0x30, 0x30, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00,
    // 8 (56)
    0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0x7C, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // 9 (57)
    0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0x7E, 0x06, 0x06, 0x0C, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00,
    // : (58)
    0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // ; (59)
    0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x18, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00,
    // < (60)
    0x00, 0x00, 0x00, 0x0C, 0x18, 0x30, 0x60, 0x30, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // = (61)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // > (62)
    0x00, 0x00, 0x00, 0x30, 0x18, 0x0C, 0x06, 0x0C, 0x18, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // ? (63)
    0x00, 0x00, 0x7C, 0xC6, 0x06, 0x0C, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00,
    // @ (64)
    0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xDE, 0xDE, 0xDE, 0xC0, 0xC0, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // A (65)
    0x00, 0x00, 0x10, 0x38, 0x6C, 0xC6, 0xC6, 0xFE, 0xC6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00, 0x00,
    // B (66)
    0x00, 0x00, 0xFC, 0x66, 0x66, 0x66, 0x7C, 0x66, 0x66, 0x66, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00,
    // C (67)
    0x00, 0x00, 0x3C, 0x66, 0xC2, 0xC0, 0xC0, 0xC0, 0xC2, 0x66, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // D (68)
    0x00, 0x00, 0xF8, 0x6C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x6C, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00,
    // E (69)
    0x00, 0x00, 0xFE, 0x66, 0x62, 0x68, 0x78, 0x68, 0x62, 0x66, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00,
    // F (70)
    0x00, 0x00, 0xFE, 0x66, 0x62, 0x68, 0x78, 0x68, 0x60, 0x60, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00,
    // G (71)
    0x00, 0x00, 0x3C, 0x66, 0xC2, 0xC0, 0xC0, 0xDE, 0xC6, 0x66, 0x3A, 0x00, 0x00, 0x00, 0x00, 0x00,
    // H (72)
    0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xFE, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00, 0x00,
    // I (73)
    0x00, 0x00, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // J (74)
    0x00, 0x00, 0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0xCC, 0xCC, 0x78, 0x00, 0x00, 0x00, 0x00, 0x00,
    // K (75)
    0x00, 0x00, 0xE6, 0x66, 0x6C, 0x6C, 0x78, 0x6C, 0x6C, 0x66, 0xE6, 0x00, 0x00, 0x00, 0x00, 0x00,
    // L (76)
    0x00, 0x00, 0xF0, 0x60, 0x60, 0x60, 0x60, 0x60, 0x62, 0x66, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00,
    // M (77)
    0x00, 0x00, 0xC6, 0xEE, 0xFE, 0xFE, 0xD6, 0xC6, 0xC6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00, 0x00,
    // N (78)
    0x00, 0x00, 0xC6, 0xE6, 0xF6, 0xFE, 0xDE, 0xCE, 0xC6, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00, 0x00,
    // O (79)
    0x00, 0x00, 0x38, 0x6C, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x6C, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00,
    // P (80)
    0x00, 0x00, 0xFC, 0x66, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x60, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00,
    // Q (81)
    0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xD6, 0xDE, 0x7C, 0x0E, 0x00, 0x00, 0x00, 0x00,
    // R (82)
    0x00, 0x00, 0xFC, 0x66, 0x66, 0x66, 0x7C, 0x6C, 0x66, 0x66, 0xE6, 0x00, 0x00, 0x00, 0x00, 0x00,
    // S (83)
    0x00, 0x00, 0x7C, 0xC6, 0xC6, 0x60, 0x38, 0x0C, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // T (84)
    0x00, 0x00, 0x7E, 0x7E, 0x5A, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // U (85)
    0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // V (86)
    0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0xC6, 0x6C, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    // W (87)
    0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0xD6, 0xD6, 0xFE, 0x6C, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // X (88)
    0x00, 0x00, 0xC6, 0xC6, 0x6C, 0x38, 0x38, 0x38, 0x6C, 0xC6, 0xC6, 0x00, 0x00, 0x00, 0x00, 0x00,
    // Y (89)
    0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // Z (90)
    0x00, 0x00, 0xFE, 0xC6, 0x8C, 0x18, 0x30, 0x60, 0xC2, 0xC6, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00,
    // [ (91)
    0x00, 0x00, 0x3C, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // \ (92)
    0x00, 0x00, 0x60, 0x30, 0x30, 0x18, 0x18, 0x0C, 0x0C, 0x06, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
    // ] (93)
    0x00, 0x00, 0x3C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // ^ (94)
    0x00, 0x10, 0x38, 0x6C, 0xC6, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // _ (95)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00,
    // ` (96)
    0x00, 0x30, 0x18, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // a (97)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x0C, 0x7C, 0xCC, 0xCC, 0x76, 0x00, 0x00, 0x00, 0x00, 0x00,
    // b (98)
    0x00, 0x00, 0xE0, 0x60, 0x60, 0x78, 0x6C, 0x66, 0x66, 0x66, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // c (99)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0xC6, 0xC0, 0xC0, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // d (100)
    0x00, 0x00, 0x1C, 0x0C, 0x0C, 0x3C, 0x6C, 0xCC, 0xCC, 0xCC, 0x76, 0x00, 0x00, 0x00, 0x00, 0x00,
    // e (101)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0xC6, 0xFE, 0xC0, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // f (102)
    0x00, 0x00, 0x38, 0x6C, 0x64, 0x60, 0xF0, 0x60, 0x60, 0x60, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00,
    // g (103)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xCC, 0xCC, 0xCC, 0x7C, 0x0C, 0xCC, 0x78, 0x00, 0x00, 0x00,
    // h (104)
    0x00, 0x00, 0xE0, 0x60, 0x60, 0x6C, 0x76, 0x66, 0x66, 0x66, 0xE6, 0x00, 0x00, 0x00, 0x00, 0x00,
    // i (105)
    0x00, 0x00, 0x18, 0x18, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // j (106)
    0x00, 0x00, 0x06, 0x06, 0x00, 0x0E, 0x06, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3C, 0x00, 0x00, 0x00,
    // k (107)
    0x00, 0x00, 0xE0, 0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0xE6, 0x00, 0x00, 0x00, 0x00, 0x00,
    // l (108)
    0x00, 0x00, 0x38, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // m (109)
    0x00, 0x00, 0x00, 0x00, 0x00, 0xEC, 0xFE, 0xD6, 0xD6, 0xD6, 0xD6, 0x00, 0x00, 0x00, 0x00, 0x00,
    // n (110)
    0x00, 0x00, 0x00, 0x00, 0x00, 0xDC, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00,
    // o (111)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0xC6, 0xC6, 0xC6, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // p (112)
    0x00, 0x00, 0x00, 0x00, 0x00, 0xDC, 0x66, 0x66, 0x66, 0x7C, 0x60, 0x60, 0xF0, 0x00, 0x00, 0x00,
    // q (113)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x76, 0xCC, 0xCC, 0xCC, 0x7C, 0x0C, 0x0C, 0x1E, 0x00, 0x00, 0x00,
    // r (114)
    0x00, 0x00, 0x00, 0x00, 0x00, 0xDC, 0x76, 0x66, 0x60, 0x60, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00,
    // s (115)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0xC6, 0x70, 0x1C, 0xC6, 0x7C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // t (116)
    0x00, 0x00, 0x10, 0x30, 0x30, 0xFC, 0x30, 0x30, 0x30, 0x36, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // u (117)
    0x00, 0x00, 0x00, 0x00, 0x00, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x76, 0x00, 0x00, 0x00, 0x00, 0x00,
    // v (118)
    0x00, 0x00, 0x00, 0x00, 0x00, 0xC6, 0xC6, 0xC6, 0x6C, 0x38, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    // w (119)
    0x00, 0x00, 0x00, 0x00, 0x00, 0xC6, 0xC6, 0xD6, 0xD6, 0xFE, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00,
    // x (120)
    0x00, 0x00, 0x00, 0x00, 0x00, 0xC6, 0x6C, 0x38, 0x38, 0x6C, 0xC6, 0x00, 0x00, 0x00, 0x00, 0x00,
    // y (121)
    0x00, 0x00, 0x00, 0x00, 0x00, 0xC6, 0xC6, 0xC6, 0xC6, 0x7E, 0x06, 0x0C, 0xF8, 0x00, 0x00, 0x00,
    // z (122)
    0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0xCC, 0x18, 0x30, 0x66, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00,
    // { (123)
    0x00, 0x00, 0x0E, 0x18, 0x18, 0x18, 0x70, 0x18, 0x18, 0x18, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00,
    // | (124)
    0x00, 0x00, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00,
    // } (125)
    0x00, 0x00, 0x70, 0x18, 0x18, 0x18, 0x0E, 0x18, 0x18, 0x18, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00,
    // ~ (126)
    0x00, 0x00, 0x76, 0xDC, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
// clang-format on

static constexpr size_t FONT_CHAR_COUNT = 95;  // ASCII 32-126

// ============================================================================
// HUDRenderer Implementation
// ============================================================================

HUDRenderer::HUDRenderer() {
    vertices_.reserve(MAX_UI_VERTICES);
}

HUDRenderer::~HUDRenderer() {
    shutdown();
}

bool HUDRenderer::initialize(graphics::GraphicsDevice* device, TextureAtlas* block_atlas, const HUDConfig& config) {
    if (device_ != nullptr) {
        REALCRAFT_LOG_WARN(core::log_category::GRAPHICS, "HUDRenderer already initialized");
        return false;
    }

    device_ = device;
    block_atlas_ = block_atlas;
    config_ = config;

    if (!create_shaders()) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create HUD shaders");
        shutdown();
        return false;
    }

    if (!create_pipeline()) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create HUD pipeline");
        shutdown();
        return false;
    }

    if (!create_font_texture()) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create font texture");
        shutdown();
        return false;
    }

    if (!create_vertex_buffer()) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create HUD vertex buffer");
        shutdown();
        return false;
    }

    REALCRAFT_LOG_INFO(core::log_category::GRAPHICS, "HUDRenderer initialized successfully");
    return true;
}

void HUDRenderer::shutdown() {
    ui_vertex_buffer_.reset();
    font_sampler_.reset();
    font_texture_.reset();
    ui_pipeline_.reset();
    ui_fragment_shader_.reset();
    ui_vertex_shader_.reset();
    device_ = nullptr;
    block_atlas_ = nullptr;
}

void HUDRenderer::update(const HUDData& data) {
    data_ = data;
}

void HUDRenderer::render(graphics::CommandBuffer* cmd, uint32_t screen_width, uint32_t screen_height) {
    if (!device_ || !ui_pipeline_) {
        return;
    }

    // Apply UI scale
    float scale = config_.ui_scale;
    uint32_t scaled_width = static_cast<uint32_t>(screen_width / scale);
    uint32_t scaled_height = static_cast<uint32_t>(screen_height / scale);

    // Render each HUD element
    render_crosshair(scaled_width, scaled_height);
    render_hotbar(scaled_width, scaled_height);
    render_health_hunger(scaled_width, scaled_height);

    if (config_.debug_overlay_visible) {
        render_debug_overlay(scaled_width, scaled_height);
    }

    // Flush all batched vertices
    if (!vertices_.empty()) {
        // Update vertex buffer
        void* mapped = ui_vertex_buffer_->map();
        if (mapped) {
            size_t copy_size = std::min(vertices_.size() * sizeof(UIVertex), MAX_UI_VERTICES * sizeof(UIVertex));
            std::memcpy(mapped, vertices_.data(), copy_size);
            ui_vertex_buffer_->unmap();
        }

        // Bind pipeline
        cmd->bind_pipeline(ui_pipeline_.get());

        // Push constants with actual screen size
        UIPushConstants push;
        push.screen_size = glm::vec2(scaled_width, scaled_height);
        push.use_texture = 0.0f;
        push.texture_layer = 0.0f;

        // Draw non-textured elements first
        cmd->bind_vertex_buffer(0, ui_vertex_buffer_.get());

        // For simplicity, we'll render everything with the font texture
        // and use vertex colors for solid quads (with a white pixel in the font)
        push.use_texture = 1.0f;
        cmd->push_constants(graphics::ShaderStage::Vertex, 0, sizeof(push), &push);
        cmd->push_constants(graphics::ShaderStage::Fragment, 0, sizeof(push), &push);

        cmd->bind_texture(0, font_texture_.get(), font_sampler_.get());

        uint32_t vertex_count = static_cast<uint32_t>(vertices_.size());
        cmd->draw(vertex_count, 1, 0, 0);

        vertices_.clear();
    }
}

// ============================================================================
// Initialization Helpers
// ============================================================================

bool HUDRenderer::create_shaders() {
    graphics::ShaderCompiler compiler;

    // Compile vertex shader
    graphics::ShaderCompileOptions vert_options;
    vert_options.stage = graphics::ShaderStage::Vertex;
    vert_options.entry_point = "main";

    auto vert_result = compiler.compile_glsl(UI_VERTEX_SHADER, vert_options);
    if (!vert_result.success) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to compile HUD vertex shader: {}",
                            vert_result.error_message);
        return false;
    }

    // Compile fragment shader
    graphics::ShaderCompileOptions frag_options;
    frag_options.stage = graphics::ShaderStage::Fragment;
    frag_options.entry_point = "main";

    auto frag_result = compiler.compile_glsl(UI_FRAGMENT_SHADER, frag_options);
    if (!frag_result.success) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to compile HUD fragment shader: {}",
                            frag_result.error_message);
        return false;
    }

    // Create vertex shader
    graphics::ShaderDesc vert_desc;
    vert_desc.stage = graphics::ShaderStage::Vertex;
    vert_desc.bytecode = vert_result.msl_bytecode.empty()
                             ? std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(vert_result.msl_source.data()),
                                                        vert_result.msl_source.size())
                             : std::span<const uint8_t>(vert_result.msl_bytecode);
    vert_desc.entry_point = "main0";
    vert_desc.debug_name = "hud_vertex";

    ui_vertex_shader_ = device_->create_shader(vert_desc);
    if (!ui_vertex_shader_) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create HUD vertex shader");
        return false;
    }

    // Create fragment shader
    graphics::ShaderDesc frag_desc;
    frag_desc.stage = graphics::ShaderStage::Fragment;
    frag_desc.bytecode = frag_result.msl_bytecode.empty()
                             ? std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(frag_result.msl_source.data()),
                                                        frag_result.msl_source.size())
                             : std::span<const uint8_t>(frag_result.msl_bytecode);
    frag_desc.entry_point = "main0";
    frag_desc.debug_name = "hud_fragment";

    ui_fragment_shader_ = device_->create_shader(frag_desc);
    if (!ui_fragment_shader_) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create HUD fragment shader");
        return false;
    }

    return true;
}

bool HUDRenderer::create_pipeline() {
    graphics::PipelineDesc desc;
    desc.vertex_shader = ui_vertex_shader_.get();
    desc.fragment_shader = ui_fragment_shader_.get();

    // Vertex attributes
    desc.vertex_attributes = UIVertex::get_attributes();
    desc.vertex_bindings = {UIVertex::get_binding()};

    desc.topology = graphics::PrimitiveTopology::TriangleList;

    // No face culling for UI
    desc.rasterizer.cull_mode = graphics::CullMode::None;

    // No depth testing for UI
    desc.depth_stencil.depth_test_enable = false;
    desc.depth_stencil.depth_write_enable = false;

    // Alpha blending
    graphics::BlendState blend;
    blend.enable = true;
    blend.src_color = graphics::BlendFactor::SrcAlpha;
    blend.dst_color = graphics::BlendFactor::OneMinusSrcAlpha;
    blend.color_op = graphics::BlendOp::Add;
    blend.src_alpha = graphics::BlendFactor::One;
    blend.dst_alpha = graphics::BlendFactor::OneMinusSrcAlpha;
    blend.alpha_op = graphics::BlendOp::Add;
    desc.color_blend.push_back(blend);

    desc.color_formats = {graphics::TextureFormat::BGRA8Unorm};
    desc.depth_format = graphics::TextureFormat::Depth32Float;
    desc.debug_name = "hud_pipeline";

    ui_pipeline_ = device_->create_pipeline(desc);
    return ui_pipeline_ != nullptr;
}

bool HUDRenderer::create_font_texture() {
    // Create 256x256 RGBA texture for font atlas
    std::vector<uint8_t> texture_data(FONT_TEXTURE_SIZE * FONT_TEXTURE_SIZE * 4, 0);

    // Fill font texture from bitmap data
    for (size_t char_idx = 0; char_idx < FONT_CHAR_COUNT; ++char_idx) {
        size_t char_col = char_idx % static_cast<size_t>(FONT_CHARS_PER_ROW);
        size_t char_row = char_idx / static_cast<size_t>(FONT_CHARS_PER_ROW);

        size_t base_x = char_col * FONT_CHAR_WIDTH;
        size_t base_y = char_row * FONT_CHAR_HEIGHT;

        // Copy each row of the character
        for (size_t y = 0; y < static_cast<size_t>(FONT_CHAR_HEIGHT); ++y) {
            uint8_t row_bits = FONT_BITMAP[char_idx * FONT_CHAR_HEIGHT + y];

            for (size_t x = 0; x < static_cast<size_t>(FONT_CHAR_WIDTH); ++x) {
                // Check if bit is set (MSB first)
                bool pixel_set = (row_bits & (0x80 >> x)) != 0;

                size_t tex_x = base_x + x;
                size_t tex_y = base_y + y;
                size_t pixel_idx = (tex_y * FONT_TEXTURE_SIZE + tex_x) * 4;

                // White pixel if set, transparent if not
                texture_data[pixel_idx + 0] = 255;                  // R
                texture_data[pixel_idx + 1] = 255;                  // G
                texture_data[pixel_idx + 2] = 255;                  // B
                texture_data[pixel_idx + 3] = pixel_set ? 255 : 0;  // A
            }
        }
    }

    // Add solid white pixel at bottom-right corner for solid quads
    // This is used by add_quad() to render opaque colored rectangles
    size_t solid_pixel_idx = (FONT_TEXTURE_SIZE - 1) * FONT_TEXTURE_SIZE + (FONT_TEXTURE_SIZE - 1);
    solid_pixel_idx *= 4;
    texture_data[solid_pixel_idx + 0] = 255;  // R
    texture_data[solid_pixel_idx + 1] = 255;  // G
    texture_data[solid_pixel_idx + 2] = 255;  // B
    texture_data[solid_pixel_idx + 3] = 255;  // A (opaque)

    // Create texture
    graphics::TextureDesc tex_desc;
    tex_desc.type = graphics::TextureType::Texture2D;
    tex_desc.format = graphics::TextureFormat::RGBA8Unorm;
    tex_desc.width = FONT_TEXTURE_SIZE;
    tex_desc.height = FONT_TEXTURE_SIZE;
    tex_desc.depth = 1;
    tex_desc.mip_levels = 1;
    tex_desc.array_layers = 1;
    tex_desc.usage = graphics::TextureUsage::Sampled | graphics::TextureUsage::TransferDst;
    tex_desc.debug_name = "FontAtlas";

    font_texture_ = device_->create_texture(tex_desc);
    if (!font_texture_) {
        return false;
    }

    // Upload via staging buffer
    graphics::BufferDesc staging_desc;
    staging_desc.size = texture_data.size();
    staging_desc.usage = graphics::BufferUsage::TransferSrc;
    staging_desc.host_visible = true;
    staging_desc.debug_name = "FontStagingBuffer";

    auto staging_buffer = device_->create_buffer(staging_desc);
    if (!staging_buffer) {
        REALCRAFT_LOG_ERROR(core::log_category::GRAPHICS, "Failed to create font staging buffer");
        return false;
    }

    staging_buffer->write(texture_data.data(), texture_data.size());

    // Copy to texture
    auto cmd = device_->create_command_buffer();
    cmd->begin();

    graphics::BufferImageCopy region;
    region.buffer_offset = 0;
    region.buffer_row_length = 0;    // 0 = tightly packed (Metal calculates bytes correctly)
    region.buffer_image_height = 0;  // 0 = tightly packed
    region.texture_offset_x = 0;
    region.texture_offset_y = 0;
    region.texture_offset_z = 0;
    region.texture_width = FONT_TEXTURE_SIZE;
    region.texture_height = FONT_TEXTURE_SIZE;
    region.texture_depth = 1;
    region.mip_level = 0;
    region.array_layer = 0;

    cmd->copy_buffer_to_texture(staging_buffer.get(), font_texture_.get(), region);

    cmd->end();
    device_->submit(cmd.get(), true);  // Wait for completion

    // Create sampler (nearest neighbor for pixel-perfect text)
    graphics::SamplerDesc sampler_desc;
    sampler_desc.min_filter = graphics::FilterMode::Nearest;
    sampler_desc.mag_filter = graphics::FilterMode::Nearest;
    sampler_desc.mip_filter = graphics::FilterMode::Nearest;
    sampler_desc.address_u = graphics::AddressMode::ClampToEdge;
    sampler_desc.address_v = graphics::AddressMode::ClampToEdge;
    sampler_desc.debug_name = "FontSampler";

    font_sampler_ = device_->create_sampler(sampler_desc);
    return font_sampler_ != nullptr;
}

bool HUDRenderer::create_vertex_buffer() {
    graphics::BufferDesc desc;
    desc.size = MAX_UI_VERTICES * sizeof(UIVertex);
    desc.usage = graphics::BufferUsage::Vertex;
    desc.host_visible = true;
    desc.debug_name = "HUDVertexBuffer";

    ui_vertex_buffer_ = device_->create_buffer(desc);
    return ui_vertex_buffer_ != nullptr;
}

// ============================================================================
// Rendering Helpers
// ============================================================================

void HUDRenderer::render_crosshair(uint32_t width, uint32_t height) {
    float cx = width / 2.0f;
    float cy = height / 2.0f;
    float size = config_.crosshair_size;
    float thick = config_.crosshair_thickness;
    float gap = config_.crosshair_gap;
    glm::vec4 color = config_.crosshair_color;

    switch (config_.crosshair_style) {
        case CrosshairStyle::Dot: {
            // Simple dot at center
            add_quad(cx - thick / 2, cy - thick / 2, thick, thick, color);
            break;
        }
        case CrosshairStyle::Cross: {
            // Four lines from center with gap
            // Top
            add_quad(cx - thick / 2, cy - size - gap, thick, size, color);
            // Bottom
            add_quad(cx - thick / 2, cy + gap, thick, size, color);
            // Left
            add_quad(cx - size - gap, cy - thick / 2, size, thick, color);
            // Right
            add_quad(cx + gap, cy - thick / 2, size, thick, color);
            break;
        }
        case CrosshairStyle::Plus: {
            // Plus sign (no gap)
            // Vertical bar
            add_quad(cx - thick / 2, cy - size, thick, size * 2, color);
            // Horizontal bar
            add_quad(cx - size, cy - thick / 2, size * 2, thick, color);
            break;
        }
    }
}

void HUDRenderer::render_hotbar(uint32_t width, uint32_t height) {
    float slot_size = config_.hotbar_slot_size;
    float spacing = config_.hotbar_slot_spacing;
    float icon_padding = config_.hotbar_icon_padding;
    float bottom_margin = config_.hotbar_bottom_margin;

    // Total hotbar width
    float total_width = 9 * slot_size + 8 * spacing;
    float start_x = (width - total_width) / 2.0f;
    float start_y = height - bottom_margin - slot_size;

    for (int i = 0; i < 9; ++i) {
        float x = start_x + i * (slot_size + spacing);
        float y = start_y;

        // Slot background
        glm::vec4 bg_color = (i == data_.selected_slot) ? config_.hotbar_selected_color : config_.hotbar_slot_color;
        add_quad(x, y, slot_size, slot_size, bg_color);

        // Item icon (if slot is not empty)
        const auto& slot = data_.hotbar_slots[i];
        if (!slot.empty && block_atlas_) {
            // Get UV rect from texture atlas (unused for now)
            (void)block_atlas_->get_uv_rect(slot.texture_index);

            // We need to draw this with the block atlas texture, but for simplicity
            // we'll skip the item icons for now (would require separate draw call)
            // TODO: Implement item icon rendering with block atlas

            // Draw stack count if > 1
            if (slot.count > 1) {
                std::string count_str = std::to_string(slot.count);
                float text_x = x + slot_size - 4 - get_text_width(count_str, 1.0f);
                float text_y = y + slot_size - 4 - FONT_CHAR_HEIGHT;
                render_text_shadowed(count_str, text_x, text_y, {1.0f, 1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.8f},
                                     1.0f);
            }
        }
    }
}

void HUDRenderer::render_health_hunger(uint32_t width, uint32_t height) {
    float bar_width = config_.bar_width;
    float bar_height = config_.bar_height;
    float bottom_margin = config_.bar_bottom_margin;
    float slot_size = config_.hotbar_slot_size;
    float hotbar_bottom_margin = config_.hotbar_bottom_margin;

    // Position above hotbar
    float total_width = 9 * config_.hotbar_slot_size + 8 * config_.hotbar_slot_spacing;
    float hotbar_x = (width - total_width) / 2.0f;
    float bar_y = height - hotbar_bottom_margin - slot_size - bottom_margin - bar_height;

    // Health bar (left side)
    float health_x = hotbar_x;
    float health_percent = data_.max_health > 0 ? data_.health / data_.max_health : 0.0f;
    health_percent = std::clamp(health_percent, 0.0f, 1.0f);

    // Background
    add_quad(health_x, bar_y, bar_width, bar_height, config_.bar_background_color);
    // Fill
    add_quad(health_x, bar_y, bar_width * health_percent, bar_height, config_.health_bar_color);

    // Hunger bar (right side)
    float hunger_x = hotbar_x + total_width - bar_width;
    float hunger_percent = data_.max_hunger > 0 ? data_.hunger / data_.max_hunger : 0.0f;
    hunger_percent = std::clamp(hunger_percent, 0.0f, 1.0f);

    // Background
    add_quad(hunger_x, bar_y, bar_width, bar_height, config_.bar_background_color);
    // Fill (from right)
    float fill_width = bar_width * hunger_percent;
    add_quad(hunger_x + bar_width - fill_width, bar_y, fill_width, bar_height, config_.hunger_bar_color);
}

void HUDRenderer::render_debug_overlay(uint32_t width, uint32_t height) {
    float margin = config_.debug_margin;
    float line_height = config_.debug_line_height;
    float scale = config_.debug_font_scale;
    glm::vec4 text_color = config_.debug_text_color;
    glm::vec4 shadow_color = config_.debug_shadow_color;

    float y = margin;

    // Title
    render_text_shadowed("RealCraft", margin, y, text_color, shadow_color, scale);
    y += line_height;

    // FPS
    std::ostringstream fps_ss;
    fps_ss.precision(1);
    fps_ss << std::fixed << "FPS: " << data_.fps << " (" << data_.frame_time_ms << "ms)";
    render_text_shadowed(fps_ss.str(), margin, y, text_color, shadow_color, scale);
    y += line_height;

    y += line_height / 2;  // Spacing

    // Position
    std::ostringstream pos_ss;
    pos_ss.precision(2);
    pos_ss << std::fixed << "XYZ: " << data_.player_position.x << " / " << data_.player_position.y << " / "
           << data_.player_position.z;
    render_text_shadowed(pos_ss.str(), margin, y, text_color, shadow_color, scale);
    y += line_height;

    // Chunk
    std::ostringstream chunk_ss;
    chunk_ss << "Chunk: " << data_.chunk_x << ", " << data_.chunk_z;
    render_text_shadowed(chunk_ss.str(), margin, y, text_color, shadow_color, scale);
    y += line_height;

    // Facing direction
    std::ostringstream facing_ss;
    facing_ss.precision(1);
    // Convert yaw to compass direction
    float yaw = std::fmod(data_.camera_yaw + 360.0f, 360.0f);
    const char* direction = "N";
    if (yaw >= 315.0f || yaw < 45.0f) {
        direction = "S";
    } else if (yaw >= 45.0f && yaw < 135.0f) {
        direction = "W";
    } else if (yaw >= 135.0f && yaw < 225.0f) {
        direction = "N";
    } else {
        direction = "E";
    }
    facing_ss << std::fixed << "Facing: " << direction << " (yaw: " << data_.camera_yaw
              << ", pitch: " << data_.camera_pitch << ")";
    render_text_shadowed(facing_ss.str(), margin, y, text_color, shadow_color, scale);
    y += line_height;

    y += line_height / 2;  // Spacing

    // Render stats
    std::ostringstream render_ss;
    render_ss << "Render: " << data_.chunks_rendered << " chunks, " << data_.triangles << " tris, " << data_.draw_calls
              << " draws";
    render_text_shadowed(render_ss.str(), margin, y, text_color, shadow_color, scale);
    y += line_height;

    // Time of day
    std::ostringstream time_ss;
    time_ss.precision(2);
    const char* time_name = "Night";
    if (data_.time_of_day >= 0.25f && data_.time_of_day < 0.75f) {
        time_name = "Day";
    } else if (data_.time_of_day >= 0.2f && data_.time_of_day < 0.25f) {
        time_name = "Dawn";
    } else if (data_.time_of_day >= 0.75f && data_.time_of_day < 0.8f) {
        time_name = "Dusk";
    }
    time_ss << std::fixed << "Time: " << data_.time_of_day << " (" << time_name << ")";
    render_text_shadowed(time_ss.str(), margin, y, text_color, shadow_color, scale);
}

// ============================================================================
// Geometry Helpers
// ============================================================================

void HUDRenderer::add_quad(float x, float y, float w, float h, const glm::vec4& color) {
    // Sample the solid white pixel at bottom-right corner of font texture
    // This pixel is set to fully opaque white in create_font_texture()
    float u = (FONT_TEXTURE_SIZE - 0.5f) / FONT_TEXTURE_SIZE;
    float v = (FONT_TEXTURE_SIZE - 0.5f) / FONT_TEXTURE_SIZE;

    // Two triangles forming a quad
    // Triangle 1: top-left, top-right, bottom-right
    vertices_.push_back({{x, y}, {u, v}, color});
    vertices_.push_back({{x + w, y}, {u, v}, color});
    vertices_.push_back({{x + w, y + h}, {u, v}, color});

    // Triangle 2: top-left, bottom-right, bottom-left
    vertices_.push_back({{x, y}, {u, v}, color});
    vertices_.push_back({{x + w, y + h}, {u, v}, color});
    vertices_.push_back({{x, y + h}, {u, v}, color});
}

void HUDRenderer::add_textured_quad(float x, float y, float w, float h, float u0, float v0, float u1, float v1,
                                    const glm::vec4& color) {
    // Two triangles forming a quad
    // Triangle 1: top-left, top-right, bottom-right
    vertices_.push_back({{x, y}, {u0, v0}, color});
    vertices_.push_back({{x + w, y}, {u1, v0}, color});
    vertices_.push_back({{x + w, y + h}, {u1, v1}, color});

    // Triangle 2: top-left, bottom-right, bottom-left
    vertices_.push_back({{x, y}, {u0, v0}, color});
    vertices_.push_back({{x + w, y + h}, {u1, v1}, color});
    vertices_.push_back({{x, y + h}, {u0, v1}, color});
}

// ============================================================================
// Text Rendering
// ============================================================================

glm::vec4 HUDRenderer::get_char_uv(char c) const {
    // Map ASCII to font atlas position
    int char_idx = static_cast<int>(c) - 32;  // ASCII 32 = space
    if (char_idx < 0 || char_idx >= static_cast<int>(FONT_CHAR_COUNT)) {
        char_idx = 0;  // Default to space for unknown characters
    }

    int char_col = char_idx % FONT_CHARS_PER_ROW;
    int char_row = char_idx / FONT_CHARS_PER_ROW;

    float u0 = static_cast<float>(char_col * FONT_CHAR_WIDTH) / FONT_TEXTURE_SIZE;
    float v0 = static_cast<float>(char_row * FONT_CHAR_HEIGHT) / FONT_TEXTURE_SIZE;
    float u1 = static_cast<float>((char_col + 1) * FONT_CHAR_WIDTH) / FONT_TEXTURE_SIZE;
    float v1 = static_cast<float>((char_row + 1) * FONT_CHAR_HEIGHT) / FONT_TEXTURE_SIZE;

    return {u0, v0, u1, v1};
}

void HUDRenderer::render_text(const std::string& text, float x, float y, const glm::vec4& color, float scale) {
    float char_w = FONT_CHAR_WIDTH * scale;
    float char_h = FONT_CHAR_HEIGHT * scale;
    float cursor_x = x;

    for (char c : text) {
        if (c == '\n') {
            cursor_x = x;
            y += char_h;
            continue;
        }

        glm::vec4 uv = get_char_uv(c);
        add_textured_quad(cursor_x, y, char_w, char_h, uv.x, uv.y, uv.z, uv.w, color);
        cursor_x += char_w;
    }
}

void HUDRenderer::render_text_shadowed(const std::string& text, float x, float y, const glm::vec4& color,
                                       const glm::vec4& shadow_color, float scale) {
    // Shadow offset
    float offset = 1.0f * scale;

    // Render shadow first
    render_text(text, x + offset, y + offset, shadow_color, scale);

    // Render text on top
    render_text(text, x, y, color, scale);
}

float HUDRenderer::get_text_width(const std::string& text, float scale) const {
    float max_width = 0.0f;
    float current_width = 0.0f;
    float char_w = FONT_CHAR_WIDTH * scale;

    for (char c : text) {
        if (c == '\n') {
            max_width = std::max(max_width, current_width);
            current_width = 0.0f;
        } else {
            current_width += char_w;
        }
    }

    return std::max(max_width, current_width);
}

void HUDRenderer::begin_batch() {
    vertices_.clear();
}

void HUDRenderer::flush_batch(graphics::CommandBuffer* cmd, bool use_texture, graphics::Texture* texture) {
    // This is handled in render() for simplicity
}

}  // namespace realcraft::rendering
