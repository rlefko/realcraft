// RealCraft Graphics Abstraction Layer
// graphics.hpp - Main include with forward declarations

#pragma once

// Include all graphics headers
#include "buffer.hpp"
#include "command_buffer.hpp"
#include "device.hpp"
#include "pipeline.hpp"
#include "render_pass.hpp"
#include "sampler.hpp"
#include "shader.hpp"
#include "shader_compiler.hpp"
#include "swap_chain.hpp"
#include "texture.hpp"
#include "types.hpp"

namespace realcraft::graphics {

// ============================================================================
// Forward Declarations (for headers that only need pointers/references)
// ============================================================================

class Buffer;
class CommandBuffer;
class ComputePipeline;
class Framebuffer;
class GraphicsDevice;
class Pipeline;
class RenderPass;
class Sampler;
class Shader;
class ShaderCompiler;
class SwapChain;
class Texture;

}  // namespace realcraft::graphics
