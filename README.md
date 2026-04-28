# imgui-timeline

imgui-timeline addon with professional track controls, solo/mute, and height adjustment.

### Quick Start (Ubuntu/Linux)
```bash
./build_app.sh
./build/build_OPENGL_Release_ubuntu/bin/imgui_timeline_demo
```

### Quick Start (macOS 12.2+)
```bash
./build_app.sh
./build/build_METAL_Release_macos/bin/imgui_timeline_metal
```

### Manual Build
```bash
# Auto-fetch ImGui (or let the script do this)
git submodule update --init --recursive

# Linux/Ubuntu (OpenGL)
cmake -S . -B build -DBUILD_DEMO=ON -DUSE_BACKEND=OPENGL
cmake --build build -j
./build/bin/imgui_timeline_demo

# macOS (Metal)
cmake -S . -B build -DBUILD_DEMO=ON -DUSE_BACKEND=METAL
cmake --build build -j
./build/bin/imgui_timeline_metal
```

### Integration

```cmake
add_subdirectory(addon)
```
```cpp
#include <imgui_timeline.h>

ImGuiX::Timeline timeline;
std::vector<ImGuiX::TimelineTrack> tracks;
ImGuiX::TimelineEdit edit;
timeline.Frame("##timeline", tracks, &edit);
```

**Note**: `libs/imgui/` is auto-fetched by build script.
