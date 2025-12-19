// RealCraft Graphics Abstraction Layer
// metal_render_pass.mm - Metal render pass and framebuffer implementation

#import <Metal/Metal.h>

#include <realcraft/graphics/render_pass.hpp>
#include <realcraft/graphics/texture.hpp>
#include <realcraft/graphics/device.hpp>

#include "metal_types.hpp"

#include <spdlog/spdlog.h>

namespace realcraft::graphics {

// ============================================================================
// MetalRenderPass Implementation
// ============================================================================

class MetalRenderPass : public RenderPass {
public:
    explicit MetalRenderPass(const RenderPassDesc& desc);
    ~MetalRenderPass() override = default;

    size_t get_color_attachment_count() const override { return color_attachments_.size(); }
    bool has_depth_attachment() const override { return has_depth_; }
    TextureFormat get_color_format(size_t index) const override {
        return index < color_attachments_.size() ? color_attachments_[index].format
                                                  : TextureFormat::Unknown;
    }
    TextureFormat get_depth_format() const override { return depth_format_; }

    const AttachmentDesc& get_color_attachment_desc(size_t index) const {
        return color_attachments_[index];
    }
    const AttachmentDesc& get_depth_attachment_desc() const { return depth_attachment_; }

private:
    std::vector<AttachmentDesc> color_attachments_;
    AttachmentDesc depth_attachment_;
    TextureFormat depth_format_ = TextureFormat::Unknown;
    bool has_depth_ = false;
};

MetalRenderPass::MetalRenderPass(const RenderPassDesc& desc)
    : color_attachments_(desc.color_attachments) {
    if (desc.depth_attachment) {
        depth_attachment_ = *desc.depth_attachment;
        depth_format_ = depth_attachment_.format;
        has_depth_ = true;
    }

    spdlog::debug("MetalRenderPass created: {} color attachments, depth={}",
                  color_attachments_.size(), has_depth_);
}

// ============================================================================
// MetalFramebuffer Implementation
// ============================================================================

class MetalFramebuffer : public Framebuffer {
public:
    MetalFramebuffer(const FramebufferDesc& desc);
    ~MetalFramebuffer() override = default;

    uint32_t get_width() const override { return width_; }
    uint32_t get_height() const override { return height_; }
    size_t get_color_attachment_count() const override { return color_attachments_.size(); }
    Texture* get_color_attachment(size_t index) const override {
        return index < color_attachments_.size() ? color_attachments_[index] : nullptr;
    }
    Texture* get_depth_attachment() const override { return depth_attachment_; }

    RenderPass* get_render_pass() const { return render_pass_; }

private:
    RenderPass* render_pass_ = nullptr;
    std::vector<Texture*> color_attachments_;
    Texture* depth_attachment_ = nullptr;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
};

MetalFramebuffer::MetalFramebuffer(const FramebufferDesc& desc)
    : render_pass_(desc.render_pass)
    , color_attachments_(desc.color_attachments)
    , depth_attachment_(desc.depth_attachment)
    , width_(desc.width)
    , height_(desc.height) {
    spdlog::debug("MetalFramebuffer created: {}x{}, {} color attachments",
                  width_, height_, color_attachments_.size());
}

// ============================================================================
// Factory Functions
// ============================================================================

std::unique_ptr<RenderPass> create_metal_render_pass(const RenderPassDesc& desc) {
    return std::make_unique<MetalRenderPass>(desc);
}

std::unique_ptr<Framebuffer> create_metal_framebuffer(const FramebufferDesc& desc) {
    return std::make_unique<MetalFramebuffer>(desc);
}

}  // namespace realcraft::graphics
