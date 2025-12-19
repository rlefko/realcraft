// RealCraft Graphics Abstraction Layer
// metal_shader.mm - Metal shader implementation

#import <Metal/Metal.h>

#include "metal_impl.hpp"
#include "metal_types.hpp"

#include <spdlog/spdlog.h>

namespace realcraft::graphics {

// ============================================================================
// MetalShader Implementation
// ============================================================================

MetalShader::MetalShader(id<MTLDevice> device, const ShaderDesc& desc)
    : stage_(desc.stage), entry_point_(desc.entry_point) {
    @autoreleasepool {
        NSError* error = nil;

        // The bytecode should be MSL source code (as string) or metallib binary
        if (desc.bytecode.empty()) {
            spdlog::error("MetalShader: Empty bytecode provided");
            return;
        }

        // Try to load as metallib first
        dispatch_data_t data = dispatch_data_create(
            desc.bytecode.data(), desc.bytecode.size(),
            dispatch_get_main_queue(), DISPATCH_DATA_DESTRUCTOR_DEFAULT);

        library_ = [device newLibraryWithData:data error:&error];
        dispatch_release(data);

        if (!library_) {
            // Try as MSL source code
            NSString* source = [[NSString alloc]
                initWithBytes:desc.bytecode.data()
                       length:desc.bytecode.size()
                     encoding:NSUTF8StringEncoding];

            if (source) {
                MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
                options.languageVersion = MTLLanguageVersion3_0;

                library_ = [device newLibraryWithSource:source options:options error:&error];
            }
        }

        if (!library_) {
            spdlog::error("MetalShader: Failed to create library: {}",
                          error ? [[error localizedDescription] UTF8String] : "unknown error");
            return;
        }

        // Get the function
        NSString* func_name = [NSString stringWithUTF8String:entry_point_.c_str()];
        function_ = [library_ newFunctionWithName:func_name];

        if (!function_) {
            spdlog::error("MetalShader: Function '{}' not found in library", entry_point_);
            return;
        }

        if (!desc.debug_name.empty()) {
            function_.label = [NSString stringWithUTF8String:desc.debug_name.c_str()];
        }

        spdlog::debug("MetalShader created: stage={} entry={}",
                      static_cast<int>(stage_), entry_point_);
    }
}

MetalShader::~MetalShader() {
    @autoreleasepool {
        function_ = nil;
        library_ = nil;
    }
}

ShaderStage MetalShader::get_stage() const { return stage_; }
const std::string& MetalShader::get_entry_point() const { return entry_point_; }
const ShaderReflection* MetalShader::get_reflection() const {
    return reflection_ ? &*reflection_ : nullptr;
}
id<MTLFunction> MetalShader::get_mtl_function() const { return function_; }

// ============================================================================
// Factory Function
// ============================================================================

std::unique_ptr<Shader> create_metal_shader(id<MTLDevice> device, const ShaderDesc& desc) {
    return std::make_unique<MetalShader>(device, desc);
}

}  // namespace realcraft::graphics
