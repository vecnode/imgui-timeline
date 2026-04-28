// Older macOS 12.2
#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>

#include <limits>
#include "imgui.h"
#include "backends/imgui_impl_osx.h"
#include "backends/imgui_impl_metal.h"

#include "../../addon/src/imgui_timeline.h"

@interface AppDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@end

@implementation AppDelegate
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender { return YES; }

- (void)windowWillClose:(NSNotification *)notification {
    [NSApp terminate:nil];
}
@end

static void ProcessFallbackInput(NSWindow* window, NSEvent* event) {
    // Basic input handling when OSX backend is not available
    switch (event.type) {
        case NSEventTypeKeyDown:
            if (event.keyCode == 53) { // Escape key
                [window close];
            }
            break;
        case NSEventTypeLeftMouseDown:
        case NSEventTypeRightMouseDown:
        case NSEventTypeOtherMouseDown:
            // Basic mouse handling
            break;
        default:
            break;
    }
}

static void BuildTimelineCanvas(ImGuiX::Timeline& timeline, std::vector<ImGuiX::TimelineTrack>& tracks) {
    using namespace ImGuiX;
    
    // Timeline Canvas Window - Floating with title bar for future buttons
    ImGui::SetNextWindowPos(ImVec2(50, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(800, 400), ImGuiCond_FirstUseEver);
    
    ImGuiWindowFlags canvas_flags = ImGuiWindowFlags_NoBringToFrontOnFocus | 
                                   ImGuiWindowFlags_NoCollapse;
    
    ImGui::Begin("imgui_timeline_canvas", nullptr, canvas_flags);
    
    // Top bar with title and space for future buttons
    ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyleColorVec4(ImGuiCol_TitleBg));
    ImGui::Text("Timeline Canvas");
    ImGui::PopStyleColor();
    ImGui::Separator();
    
    // Space for future buttons (you can add them here)
    // ImGui::Button("Play"); ImGui::SameLine();
    // ImGui::Button("Pause"); ImGui::SameLine();
    // ImGui::Button("Stop");
    
    ImGui::Separator();
    
    // Timeline takes full available height and width
    TimelineEdit edit{};
    timeline.Frame("##timeline", tracks, &edit);
    
    // Handle timeline editing
    if (edit.changed) {
        if (edit.moved || edit.resized) {
            ImGui::Text("Edited item %d (moved:%d resized:%d)", edit.item_id, (int)edit.moved, (int)edit.resized);
            ImGui::Text("New span: [%.3f, %.3f]", edit.new_t0, edit.new_t1);
        }
    }
    
    ImGui::End();
}

static void BuildTimelineControls(ImGuiX::Timeline& timeline, std::vector<ImGuiX::TimelineTrack>& tracks, bool metal_backend_available, bool osx_backend_available) {
    using namespace ImGuiX;
    
    // Controls Window - Floating, separate from canvas
    ImGui::SetNextWindowPos(ImVec2(900, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
    
    ImGuiWindowFlags controls_flags = ImGuiWindowFlags_NoResize | 
                                     ImGuiWindowFlags_NoCollapse |
                                     ImGuiWindowFlags_AlwaysAutoResize;
    
    ImGui::Begin("imgui_timeline_controls", nullptr, controls_flags);
    
    ImGui::Text("Timeline Controls");
    ImGui::Separator();
    
    // Backend status display
    ImGui::Text("Backend Status:");
    ImGui::Text("  Metal: %s", metal_backend_available ? "✓ Active" : "✗ Failed");
    ImGui::Text("  OSX Input: %s", osx_backend_available ? "✓ Active" : "✗ Failed");
    ImGui::Separator();
    
    // Current time display
    ImGui::Text("Current Time: %.3fs", timeline.GetCurrentTime());
    ImGui::Text("View: [%.3f, %.3f]", timeline.GetViewMin(), timeline.GetViewMax());
    
    // Play/Pause status
    ImGui::Text("Status: %s", timeline.IsPlaying() ? "Playing" : "Paused");
    ImGui::Text("Press SPACE to play/pause");
    
    // Velocity Controls - 1x, 2x, 5x speed
    ImGui::Separator();
    ImGui::Text("Playback Velocity:");
    if (ImGui::Button("1x")) {
        timeline.SetPlayVelocity(1.0);
    }
    ImGui::SameLine();
    if (ImGui::Button("2x")) {
        timeline.SetPlayVelocity(2.0);
    }
    ImGui::SameLine();
    if (ImGui::Button("5x")) {
        timeline.SetPlayVelocity(5.0);
    }
    ImGui::Text("Current: %.1fx", timeline.GetPlayVelocity());
    
    ImGui::Separator();
    
    // Zoom controls
    if (ImGui::Button("Zoom In")) {
        double center = (timeline.GetViewMin() + timeline.GetViewMax()) * 0.5;
        double span = (timeline.GetViewMax() - timeline.GetViewMin()) * 0.8;
        timeline.SetView(center - span * 0.5, center + span * 0.5);
    }
    ImGui::SameLine();
    if (ImGui::Button("Zoom Out")) {
        double center = (timeline.GetViewMin() + timeline.GetViewMax()) * 0.5;
        double span = (timeline.GetViewMax() - timeline.GetViewMin()) * 1.25;
        timeline.SetView(center - span * 0.5, center + span * 0.5);
    }
    
    // Reset view
    if (ImGui::Button("Reset View")) {
        timeline.SetView(0.0, 20.0);
    }
    
    ImGui::SameLine();
    
    // Frame button - fit all media items to view
    if (ImGui::Button("Frame")) {
        // Find the earliest and latest times from all tracks
        double min_time = std::numeric_limits<double>::max();
        double max_time = std::numeric_limits<double>::lowest();
        
        for (const auto& track : tracks) {
            for (const auto& item : track.items) {
                min_time = std::min(min_time, item.t0);
                max_time = std::max(max_time, item.t1);
            }
        }
        
        // If we have media items, frame them with some padding
        if (min_time != std::numeric_limits<double>::max() && max_time != std::numeric_limits<double>::lowest()) {
            double padding = (max_time - min_time) * 0.1; // 10% padding
            timeline.SetView(min_time - padding, max_time + padding);
        } else {
            // No media items, reset to default view
            timeline.SetView(0.0, 20.0);
        }
    }
    
    ImGui::Separator();
    
    // Track info
    ImGui::Text("Tracks: %zu", tracks.size());
    for (size_t i = 0; i < tracks.size(); ++i) {
        ImGui::Text("%s: %zu items", tracks[i].name.c_str(), tracks[i].items.size());
    }
    
    ImGui::End();
}

int main(int argc, const char** argv) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        AppDelegate* delegate = [AppDelegate new];
        [NSApp setDelegate:delegate];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        // Simple Menu
        NSMenu* menubar = [[NSMenu alloc] init];
        NSMenuItem* appMenuItem = [[NSMenuItem alloc] init];
        [menubar addItem:appMenuItem];
        [NSApp setMainMenu:menubar];
        NSMenu* appMenu = [[NSMenu alloc] init];
        NSString* quitTitle = [@"Quit " stringByAppendingString:[[NSProcessInfo processInfo] processName]];
        NSMenuItem* quitMenuItem = [[NSMenuItem alloc] initWithTitle:quitTitle action:@selector(terminate:) keyEquivalent:@"q"];
        [appMenu addItem:quitMenuItem];
        [appMenuItem setSubmenu:appMenu];

        // Window
        NSRect frame = NSMakeRect(100, 100, 1280, 720);
        NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                       styleMask:(NSWindowStyleMaskTitled |
                                                                  NSWindowStyleMaskClosable |
                                                                  NSWindowStyleMaskMiniaturizable |
                                                                  NSWindowStyleMaskResizable)
                                                         backing:NSBackingStoreBuffered defer:NO];
        [window setTitle:@"ImGui Timeline Demo (Metal)"];
        [window setDelegate:delegate];
        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];

        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        MTKView* mtkView = [[MTKView alloc] initWithFrame:frame device:device];
        mtkView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
        mtkView.depthStencilPixelFormat = MTLPixelFormatDepth32Float_Stencil8;
        mtkView.clearColor = MTLClearColorMake(0.10, 0.10, 0.12, 1.0);
        mtkView.paused = YES;
        mtkView.enableSetNeedsDisplay = YES;
        [window setContentView:mtkView];

        id<MTLCommandQueue> commandQueue = [device newCommandQueue];

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsDark();
        
        // Reduce font size by 2px
        ImGui::GetIO().FontGlobalScale = 0.85f;

        // Adaptive backend initialization
        bool osx_backend_available = false;
        bool metal_backend_available = false;
        
        // Try to initialize Metal backend first
        metal_backend_available = ImGui_ImplMetal_Init(device);
        if (!metal_backend_available) {
            NSLog(@"Warning: Metal backend initialization failed");
        }
        
        // Try to initialize OSX backend for input handling
        osx_backend_available = ImGui_ImplOSX_Init(mtkView);
        if (!osx_backend_available) {
            NSLog(@"Warning: OSX backend initialization failed, using minimal input handling");
        }
        
        // Load custom font
        ImFont* font = io.Fonts->AddFontFromFileTTF("media/DejaVuSans.ttf", 14.0f);
        if (font) {
            io.FontDefault = font;
        }

        using namespace ImGuiX;
        TimelineConfig cfg; cfg.min_time = 0.0; cfg.max_time = 120.0; cfg.view_min = 0.0; cfg.view_max = 20.0; cfg.label_width = 250.f; cfg.ruler_height = 28.f; cfg.timeline_width = 0.0f; // 0 = use initial parent width
        Timeline timeline(cfg);
        std::vector<TimelineTrack> tracks;
        tracks.push_back({"Video", 28.f, 20.f, 100.f, false, false, { {1,  0.0,  8.5, IM_COL32(255,120,120,255), "Intro", false}, {2, 10.0, 22.0, IM_COL32(255,180,120,255), "Scene A", false} }});
        tracks.push_back({"Audio", 26.f, 20.f, 100.f, false, false, { {3,  5.0, 18.0, IM_COL32(120,200,255,255), "Music", false} }});
        tracks.push_back({"Effects", 26.f, 20.f, 100.f, false, false, { {4, 12.0, 15.0, IM_COL32(140,220,140,255), "Particles", false}, {5, 16.0, 19.0, IM_COL32(200,120,220,255), "Glow", false} }});
        tracks.push_back({"sdskjdk", 26.f, 20.f, 100.f, false, false, { {6, 2.0, 2.0, IM_COL32(255,200,100,255), "Fade In", false}, {7, 8.0, 8.0, IM_COL32(100,200,255,255), "Fade Out", false}, {8, 15.0, 15.0, IM_COL32(200,100,255,255), "Peak", false} }});
        tracks.push_back({"qwertyuiop", 26.f, 20.f, 100.f, false, false, { {9, 3.0, 3.0, IM_COL32(255,150,150,255), "Effect A", false}, {10, 9.0, 9.0, IM_COL32(150,255,150,255), "Effect B", false} }});
        tracks.push_back({"zxcvbnmasd", 26.f, 20.f, 100.f, false, false, { {11, 4.0, 4.0, IM_COL32(150,150,255,255), "Control A", false}, {12, 11.0, 11.0, IM_COL32(255,255,150,255), "Control B", false} }});

        while (true) {
            @autoreleasepool {
                // Adaptive input handling
                if (osx_backend_available) {
                    // Use OSX backend for input
                    for (;;) {
                        NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                            untilDate:[NSDate dateWithTimeIntervalSinceNow:0.0]
                                                               inMode:NSDefaultRunLoopMode
                                                              dequeue:YES];
                        if (!event) break;
                        [NSApp sendEvent:event];
                    }
                    ImGui_ImplOSX_NewFrame(mtkView);
                } else {
                    // Minimal input handling fallback
                    for (;;) {
                        NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                            untilDate:[NSDate dateWithTimeIntervalSinceNow:0.0]
                                                               inMode:NSDefaultRunLoopMode
                                                              dequeue:YES];
                        if (!event) break;
                        [NSApp sendEvent:event];
                        
                        // Process input with fallback handler
                        ProcessFallbackInput(window, event);
                    }
                }
                
                // Metal rendering frame
                MTLRenderPassDescriptor* rpDesc = mtkView.currentRenderPassDescriptor;
                if (rpDesc && metal_backend_available) {
                    ImGui_ImplMetal_NewFrame(rpDesc);
                }
                ImGui::NewFrame();

                    BuildTimelineCanvas(timeline, tracks);
                BuildTimelineControls(timeline, tracks, metal_backend_available, osx_backend_available);
                


                ImGui::Render();
                @autoreleasepool {
                    if (metal_backend_available) {
                        MTLRenderPassDescriptor* rpDesc = mtkView.currentRenderPassDescriptor;
                        id<CAMetalDrawable> drawable = [mtkView currentDrawable];
                        if (rpDesc && drawable) {
                            id<MTLCommandBuffer> cmd = [commandQueue commandBuffer];
                            id<MTLRenderCommandEncoder> enc = [cmd renderCommandEncoderWithDescriptor:rpDesc];
                            ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), cmd, enc);
                            [enc endEncoding];
                            [cmd presentDrawable:drawable];
                            [cmd commit];
                        }
                        [mtkView setNeedsDisplay:YES];
                    } else {
                        // Fallback: just update the view without Metal rendering
                        [mtkView setNeedsDisplay:YES];
                    }
                }
            }
        }

        // Adaptive shutdown
        if (metal_backend_available) {
            ImGui_ImplMetal_Shutdown();
        }
        if (osx_backend_available) {
            ImGui_ImplOSX_Shutdown();
        }
        ImGui::DestroyContext();
        return 0;
    }
}
