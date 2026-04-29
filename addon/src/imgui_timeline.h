
#pragma once
#include <imgui.h>
#include <imgui_internal.h>
#include <vector>
#include <string>
#include <cstdint>
#include <limits>
#include <algorithm>

namespace ImGuiX {

struct TimelineItem {
    int         id = -1;
    double      t0 = 0.0;
    double      t1 = 0.0;
    ImU32       color = IM_COL32(90, 160, 250, 255);
    std::string label;
    bool        selected = false;
};





struct TimelineTrack {
    std::string               name;
    float                     height = 26.f;
    float                     min_height = 20.f;
    float                     max_height = 100.f;
    bool                      solo = false;
    bool                      mute = false;
    bool                      selected = false; // Track selection state
    std::vector<TimelineItem> items;
    
    // Default constructor
    TimelineTrack() = default;
    
#if defined(__APPLE__) && defined(__OBJC__) && defined(IMGUI_TIMELINE_METAL_COMPATIBILITY)
    // Constructor for Metal demo compatibility (only on Apple platforms with Metal support)
    TimelineTrack(const std::string& n, float h, float min_h, float max_h, bool s, bool m, const std::vector<TimelineItem>& i)
        : name(n), height(h), min_height(min_h), max_height(max_h), solo(s), mute(m), selected(false), items(i) {}
#endif
};

struct TimelineConfig {
    double min_time = 0.0;
    double max_time = 60.0;
    double view_min = 0.0;
    double view_max = 10.0;
    float  label_width = 180.f;
    float  ruler_height = 26.f;
    float  timeline_width = 800.f;  // Fixed timeline width (doesn't stretch with parent)
    float  grid_thickness = 1.f;
    float  item_rounding = 0.f; // 0 = squared corners, no rounding
    float  min_track_height = 18.f;  // Minimum track height (default -1 from 19.f)
    bool   allow_zoom = true;
    bool   allow_pan  = true;
};

struct TimelineEdit {
    bool   changed = false;
    bool   moved = false;
    bool   resized = false;
    bool   selection_changed = false;
    bool   track_selection_changed = false; // Track selection changed
    int    item_id = -1;
    int    selected_track_index = -1; // Index of selected track
    double new_t0 = 0.0;
    double new_t1 = 0.0;

};

class Timeline {
public:
    explicit Timeline(const TimelineConfig& cfg = {});
    void Frame(const char* label, std::vector<TimelineTrack>& tracks, TimelineEdit* out_edit = nullptr);
    void   SetView(double vmin, double vmax);
    void   SetRange(double tmin, double tmax);
    void   SetCurrentTime(double t);
    void   TogglePlayPause();
    bool   IsPlaying() const { return st_.is_playing; }
    void   SetPlayVelocity(double velocity); // Added: Set playback speed (1x, 2x, 5x)
    double GetPlayVelocity() const { return st_.play_velocity; } // Added: Get current velocity
    double GetCurrentTime() const { return st_.current_time; }
    double GetViewMin() const { return st_.view_min; }
    double GetViewMax() const { return st_.view_max; }
    float  TimeToPixels(double t, float x0, float x1) const;
    double PixelsToTime(float x, float x0, float x1) const;
    void   ShowDeleteTrackModal(const std::string& track_name, int track_index);

private:
    struct State {
        double view_min = 0.0;
        double view_max = 10.0;
        double current_time = 0.0;
        int    ui_start_time = 0;
        int    ui_end_time = 50;
        bool   is_panning = false;
        ImVec2 pan_anchor_px = {0,0};
        double pan_anchor_t  = 0.0;
        bool   is_playing = false;
        double play_start_time = 0.0;
        double play_velocity = 1.0; // Added: 1x, 2x, 5x speed multiplier
    } st_;

    TimelineConfig cfg_{};

    enum class DragMode { None, Move, ResizeL, ResizeR };
    DragMode drag_mode_ = DragMode::None;
    int      drag_item_track_idx_ = -1;
    int      drag_item_idx_ = -1;
    double   drag_item_t0_ = 0.0, drag_item_t1_ = 0.0;
    
    // Track selection state
    int selected_track_index_ = -1;

    // Track height resize drag state (member to avoid static-local hazards)
    int   height_resize_track_ = -1;
    float height_resize_initial_ = 0.0f;
    float height_resize_mouse_y_ = 0.0f;
    
    // Modal state
    bool show_delete_modal_ = false;
    std::string track_to_delete_name_;
    int track_to_delete_index_ = -1;
    
    // Scrollbar state
    float scroll_offset_ = 0.0f;
    float scroll_handle_height_ = 0.0f;
    bool is_scrolling_ = false;

    void   drawRuler(ImDrawList* dl, const ImRect& r, float bottom_boundary = -1.0f);
    void   handleZoomPan(const ImRect& timelineRect);
    void   enforceViewBounds();
    
    void   drawTrack(ImDrawList* dl, TimelineTrack& track, const ImRect& area, 
                     float x0, float x1, float y, float time_to_pixel_factor, double view_min, 
                     bool area_hovered, int& hovered_track, int& hovered_item, int track_index,
                     std::vector<TimelineTrack>& tracks, TimelineEdit* out_edit);
    void   renderDeleteTrackModal(std::vector<TimelineTrack>& tracks);
    std::string generateUniqueTrackName(const std::vector<TimelineTrack>& tracks);
    void   updateScrollbar(const std::vector<TimelineTrack>& tracks, const ImRect& area);
    void   drawScrollbar(ImDrawList* dl, const ImRect& area, const std::vector<TimelineTrack>& tracks);
};

} // namespace ImGuiX


