// RealCraft Platform Abstraction Layer
// window_macos.mm - macOS-specific Metal layer setup

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

extern "C" void* realcraft_setup_metal_layer(void* native_window) {
    if (native_window == nullptr) {
        return nullptr;
    }

    @autoreleasepool {
        NSWindow* window = (__bridge NSWindow*)native_window;
        NSView* contentView = [window contentView];

        if (contentView == nil) {
            return nullptr;
        }

        // Create Metal layer
        CAMetalLayer* metalLayer = [CAMetalLayer layer];

        // Get the default Metal device
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (device == nil) {
            NSLog(@"RealCraft: Failed to create Metal device");
            return nullptr;
        }

        metalLayer.device = device;
        metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        metalLayer.framebufferOnly = YES;

        // Set the layer to scale with Retina displays
        metalLayer.contentsScale = [window backingScaleFactor];

        // Set the layer size to match the view
        CGSize viewSize = contentView.bounds.size;
        metalLayer.drawableSize = CGSizeMake(
            viewSize.width * metalLayer.contentsScale,
            viewSize.height * metalLayer.contentsScale
        );

        // Enable the layer to be used as the view's backing layer
        [contentView setWantsLayer:YES];
        [contentView setLayer:metalLayer];

        // Make the layer resize with the view
        metalLayer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
        metalLayer.needsDisplayOnBoundsChange = YES;

        NSLog(@"RealCraft: Metal layer created with device: %@", [device name]);

        // Return the Metal layer pointer (bridged to void*)
        return (__bridge void*)metalLayer;
    }
}
