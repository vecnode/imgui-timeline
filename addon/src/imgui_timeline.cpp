
#include "imgui_timeline.h"
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstring>
#include <cstdlib>

namespace ImGuiX {

namespace { constexpr double kEps = 1e-9;
static inline double clampd(double v, double lo, double hi){ return v < lo ? lo : (v > hi ? hi : v); }
static inline float  clampf(float  v, float  lo, float  hi){ return v < lo ? lo : (v > hi ? hi : v); } }

Timeline::Timeline(const TimelineConfig& cfg) : cfg_(cfg) {
    if (cfg_.max_time <= cfg_.min_time) cfg_.max_time = cfg_.min_time + 1.0;
    st_.view_min = std::max(cfg_.min_time, cfg_.view_min);
    st_.view_max = std::min(cfg_.max_time, std::max(cfg_.view_min + kEps, cfg_.view_max));
    st_.current_time = st_.view_min;
}

void Timeline::SetView(double vmin, double vmax){ st_.view_min = vmin; st_.view_max = vmax; enforceViewBounds(); }
void Timeline::SetRange(double tmin, double tmax){ cfg_.min_time = tmin; cfg_.max_time = tmax; enforceViewBounds(); }
void Timeline::SetCurrentTime(double t){ st_.current_time = clampd(t, cfg_.min_time, cfg_.max_time); }

float Timeline::TimeToPixels(double t, float x0, float x1) const {
    double span = st_.view_max - st_.view_min; if (span <= kEps) span = kEps;
    float w = x1 - x0; if (w <= 1e-12f) return x0;
    return x0 + (float)(((t - st_.view_min) / span) * (double)w);
}
double Timeline::PixelsToTime(float x, float x0, float x1) const {
    float w = x1 - x0; if (w <= 1e-12f) return st_.view_min;
    double span = st_.view_max - st_.view_min; return st_.view_min + (double)((x - x0) / w) * span;
}

void Timeline::enforceViewBounds(){
    st_.view_min = clampd(st_.view_min, cfg_.min_time, cfg_.max_time - kEps);
    double span = std::max(st_.view_max - st_.view_min, kEps);
    st_.view_max = clampd(st_.view_min + span, st_.view_min + kEps, cfg_.max_time);
    st_.current_time = clampd(st_.current_time, cfg_.min_time, cfg_.max_time);
}

static void choose_tick_step(double span, float pixel_width, double& major_step, double& minor_step){
    if (span < kEps) { major_step = 1.0; minor_step = 0.2; return; }
    const double target_px_major = 100.0;
    const double pps = pixel_width / span;
    const double ideal_major = target_px_major / std::max(pps, 1e-12);
    const double base[] = {1.0, 2.0, 5.0};
    double pow10 = std::pow(10.0, std::floor(std::log10(std::max(ideal_major, kEps))));
    major_step = base[0]*pow10; double best = std::abs(major_step - ideal_major);
    for(double b : base){ double cand = b*pow10; double diff = std::abs(cand - ideal_major); if (diff < best){ best = diff; major_step = cand; } }
    minor_step = major_step/5.0;
}

void Timeline::drawRuler(ImDrawList* dl, const ImRect& r, float bottom_boundary){
    // Professional grey color scheme for ruler - matches app theme
    const ImU32 grid_col = IM_COL32(100, 100, 100, 180); // Subtle grey grid lines
    const ImU32 text_col = IM_COL32(220, 220, 220, 255); // Light grey text for better contrast
    const ImU32 ruler_bg = IM_COL32(70, 70, 70, 255); // Dark grey ruler background
    const ImU32 ruler_header_bg = IM_COL32(60, 60, 60, 255); // Slightly darker grey for sidebar
    
    // Main ruler background
    dl->AddRectFilled(r.Min, ImVec2(r.Max.x, r.Min.y + cfg_.ruler_height), ruler_bg);
    
    // Subtle header line
    dl->AddLine(ImVec2(r.Min.x, r.Min.y), ImVec2(r.Max.x, r.Min.y), IM_COL32(90, 90, 90, 255), 1.0f);
    
    // Left sidebar background for ruler
    dl->AddRectFilled(r.Min, ImVec2(r.Min.x + cfg_.label_width, r.Min.y + cfg_.ruler_height), ruler_header_bg);

    const double span = std::max(st_.view_max - st_.view_min, kEps);
    double major_step, minor_step; choose_tick_step(span, r.GetWidth()-cfg_.label_width, major_step, minor_step);

    const float x0 = r.Min.x + cfg_.label_width;
    const float x1 = r.Max.x;

    // Pre-compute time-to-pixels conversion factors for maximum performance
    const float pixel_width = x1 - x0;
    const float time_to_pixel_factor = static_cast<float>(pixel_width / span);
    const float view_min_pixels = static_cast<float>(st_.view_min);

    double t_start = std::floor(st_.view_min/minor_step) * minor_step;
    for(double t = t_start; t <= st_.view_max + kEps; t += minor_step){
        // Ultra-fast time-to-pixels conversion
        float x = x0 + (static_cast<float>(t) - view_min_pixels) * time_to_pixel_factor;
        if (x < x0 || x > x1) continue;
        const float h = 6.0f;
        dl->AddLine(ImVec2(x, r.Min.y + cfg_.ruler_height - h), ImVec2(x, r.Min.y + cfg_.ruler_height), grid_col);
    }
    t_start = std::floor(st_.view_min/major_step) * major_step;
    char buf[64];
    for(double t = t_start; t <= st_.view_max + kEps; t += major_step){
        // Ultra-fast time-to-pixels conversion
        float x = x0 + (static_cast<float>(t) - view_min_pixels) * time_to_pixel_factor;
        if (x < x0 || x > x1) continue;
        // Draw major grid lines only up to the bottom boundary (not through the bottom bar)
        float grid_end_y = (bottom_boundary > 0.0f) ? bottom_boundary : r.Max.y;
        dl->AddLine(ImVec2(x, r.Min.y + cfg_.ruler_height), ImVec2(x, grid_end_y), grid_col, cfg_.grid_thickness);
        dl->AddLine(ImVec2(x, r.Min.y), ImVec2(x, r.Min.y + cfg_.ruler_height), grid_col, cfg_.grid_thickness);
        snprintf(buf, sizeof(buf), "%.3f", t);
        // Check if text would overflow the right edge before drawing
        ImVec2 text_size = ImGui::CalcTextSize(buf);
        float text_x = x + 4;
        if (text_x + text_size.x <= r.Max.x) {
            dl->AddText(ImVec2(text_x, r.Min.y + 2), text_col, buf);
        }
    }

    // Ultra-fast cursor position calculation - this is the most critical path
    float px = x0 + (static_cast<float>(st_.current_time) - view_min_pixels) * time_to_pixel_factor;
    
    // Bulletproof fast cursor rendering - single draw call, minimal operations
    const ImU32 cursor_color = ImGui::GetColorU32(ImGuiCol_PlotLinesHovered);
    float cursor_end_y = (bottom_boundary > 0.0f) ? bottom_boundary : r.Max.y;
    dl->AddLine(ImVec2(px, r.Min.y), ImVec2(px, cursor_end_y), cursor_color, 1.0f);
}

void Timeline::TogglePlayPause() {
    if (st_.is_playing) {
        st_.is_playing = false;
    } else {
        st_.is_playing = true;
        st_.play_start_time = ImGui::GetTime();
    }
}

void Timeline::SetPlayVelocity(double velocity) {
    // Ensure velocity is always positive and reasonable
    st_.play_velocity = std::max(0.1, std::min(10.0, velocity));
}

void Timeline::SetTrackHeight(size_t track_index, float height) {
    // This method will be called from the main application
    // The actual track height modification happens in the main app
    (void)track_index;
    (void)height;
}

void Timeline::handleZoomPan(const ImRect& timelineRect){
    ImGuiIO& io = ImGui::GetIO();
    const bool hovered = ImGui::IsItemHovered();
    
    // Use immediate mouse state for zero latency
    const bool any_mouse_down = io.MouseDown[0] || io.MouseDown[1] || io.MouseDown[2];
    const ImVec2 mouse_pos = io.MousePos;

    const float x0 = timelineRect.Min.x + cfg_.label_width;
    const float x1 = timelineRect.Max.x;

    // Bulletproof fast zoom with immediate response
    if (cfg_.allow_zoom && hovered && (io.KeyCtrl || io.KeyAlt) && std::abs(io.MouseWheel) > 1e-6f){
        // Ultra-fast pixel-to-time conversion
        const float pixel_width = x1 - x0;
        const double span = std::max(st_.view_max - st_.view_min, kEps);
        const float time_to_pixel_factor = static_cast<float>(pixel_width / span);
        double center_t = st_.view_min + (mouse_pos.x - x0) / time_to_pixel_factor;
        
        const double scale = (io.MouseWheel > 0 ? 0.9 : 1.1);
        const double new_span = clampd(span * scale, kEps, cfg_.max_time - cfg_.min_time);
        const double alpha = (center_t - st_.view_min) / span;
        st_.view_min = center_t - alpha * new_span;
        st_.view_max = st_.view_min + new_span;
        enforceViewBounds();
    }

    // High-performance pan with immediate response
    if (cfg_.allow_pan && hovered && !io.KeyCtrl && !io.KeyAlt && std::abs(io.MouseWheel) > 1e-6f){
        const double span = std::max(st_.view_max - st_.view_min, kEps);
        const double delta = span * 0.15 * (io.MouseWheel > 0 ? -1.0 : 1.0);
        st_.view_min += delta; st_.view_max += delta; enforceViewBounds();
    }

    // Immediate pan start detection
    if (cfg_.allow_pan && hovered && (io.MouseClicked[1] || io.MouseClicked[2])){
        st_.is_panning = true; 
        st_.pan_anchor_px = mouse_pos; 
        st_.pan_anchor_t = st_.view_min;
    }
    
    // High-performance panning with immediate mouse tracking
    if (st_.is_panning){
        if (!any_mouse_down){ 
            st_.is_panning = false; 
        } else {
            float dx = mouse_pos.x - st_.pan_anchor_px.x;
            const double span = std::max(st_.view_max - st_.view_min, kEps);
            const double dt = - (double)dx * span / std::max(1.0f, (x1 - x0));
            const double old_span = span;
            st_.view_min = st_.pan_anchor_t + dt;
            st_.view_max = st_.view_min + old_span;
            enforceViewBounds();
        }
    }
}

void Timeline::Frame(const char* label, std::vector<TimelineTrack>& tracks, TimelineEdit* out_edit){
    if (out_edit) *out_edit = {};

    ImGuiIO& io = ImGui::GetIO();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    // Track height reference (used for individual track sizing)
    // const float fixed_track_height = 26.0f; // Reference height per track - removed unused variable
    // Use full available height to ensure all tracks are always visible
    // No more fixed height limitation - timeline extends to 100% of parent window
    float full_h = avail.y; // Full available height from parent window
    
    // Width locking mechanism: use initial parent width once, then lock it forever
    if (!width_locked_) {
        locked_width_ = (cfg_.timeline_width > 0.0f) ? cfg_.timeline_width : avail.x;
        width_locked_ = true;
    }
    ImVec2 size(locked_width_, full_h);

    ImGui::BeginChild(label, size, true, ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
    ImRect rect(ImGui::GetWindowPos(), ImVec2(ImGui::GetWindowPos().x + size.x, ImGui::GetWindowPos().y + size.y));

    // Top bar area - grey background for future controls
    const float top_bar_height = 40.0f; // Height for the top bar
    ImRect top_bar_rect(rect.Min, ImVec2(rect.Max.x, rect.Min.y + top_bar_height));
    dl->AddRectFilled(top_bar_rect.Min, top_bar_rect.Max, IM_COL32(80, 80, 80, 255)); // Grey background

    // Global track control buttons below the top bar
    const float global_controls_height = 32.0f;
    const float button_size = 24.0f;
    const float button_spacing = 4.0f;
    const float controls_start_x = rect.Min.x + 12.0f; // Slightly more padding for better visual balance
    const float controls_start_y = rect.Min.y + top_bar_height + 6.0f; // Slightly more padding below top bar for cleaner separation
    
    // Master controls blend seamlessly with the top-left corner
    // No separate background or border - clean, integrated appearance
    
    // Create a child window for the global controls
    ImGui::SetCursorScreenPos(ImVec2(controls_start_x, controls_start_y));
    ImGui::BeginChild("##global_controls", ImVec2(cfg_.label_width - 16, global_controls_height), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    
    // Professional master controls label - integrated with the corner
    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_Text), "Master");
    ImGui::SameLine();
    
    // Check current state for visual feedback
    bool all_muted = !tracks.empty();
    bool all_solo = !tracks.empty();
    for (const auto& tr : tracks) {
        if (!tr.mute) all_muted = false;
        if (!tr.solo) all_solo = false;
    }
    
    // Master Mute button (affects all tracks)
    ImGui::PushStyleColor(ImGuiCol_Button, all_muted ? IM_COL32(255, 0, 0, 200) : IM_COL32(255, 0, 0, 100));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, all_muted ? IM_COL32(255, 0, 0, 250) : IM_COL32(255, 0, 0, 150));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, all_muted ? IM_COL32(255, 0, 0, 255) : IM_COL32(255, 0, 0, 200));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
    if (ImGui::Button("M##master_mute", ImVec2(button_size, button_size))) {
        // Toggle mute for all tracks
        for (auto& tr : tracks) {
            tr.mute = !all_muted;
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Master Mute: Toggle mute for all tracks");
    }
    ImGui::PopStyleColor(4);
    
    ImGui::SameLine(0, button_spacing);
    
    // Master Solo button (affects all tracks)
    ImGui::PushStyleColor(ImGuiCol_Button, all_solo ? IM_COL32(255, 255, 0, 200) : IM_COL32(255, 255, 0, 100));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, all_solo ? IM_COL32(255, 255, 0, 250) : IM_COL32(255, 255, 0, 150));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, all_solo ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 255, 0, 200));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
    if (ImGui::Button("S##master_solo", ImVec2(button_size, button_size))) {
        // Toggle solo for all tracks
        for (auto& tr : tracks) {
            tr.solo = !all_solo;
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Master Solo: Toggle solo for all tracks");
    }
    ImGui::PopStyleColor(4);
    
    ImGui::SameLine(0, button_spacing);
    
    // Master Height Up button (increases height for all tracks)
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_Button));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(ImGuiCol_ButtonActive));
    if (ImGui::Button("▲##master_up", ImVec2(button_size, button_size))) {
        // Increase height for all tracks
        for (auto& tr : tracks) {
            tr.height = std::min(tr.height + 4.0f, tr.max_height);
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Master Height Up: Increase height for all tracks");
    }
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine(0, button_spacing);
    
    // Master Height Down button (decreases height for all tracks)
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(ImGuiCol_Button));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetColorU32(ImGuiCol_ButtonHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetColorU32(ImGuiCol_ButtonActive));
    if (ImGui::Button("▼##master_down", ImVec2(button_size, button_size))) {
        // Decrease height for all tracks
        for (auto& tr : tracks) {
            tr.height = std::max(tr.height - 4.0f, std::max(tr.min_height, cfg_.min_track_height));
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Master Height Down: Decrease height for all tracks");
    }
    ImGui::PopStyleColor(3);
    
    ImGui::SameLine(0, button_spacing);
    
    // Add Track button
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 255, 0, 100));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0, 255, 0, 150));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0, 255, 0, 200));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 255));
    if (ImGui::Button("+##add_track", ImVec2(button_size, button_size))) {
        // Generate a unique name for the new track
        std::string new_track_name = generateUniqueTrackName(tracks);
        
        // Create a new track with default values
        TimelineTrack new_track;
        new_track.name = new_track_name;
        new_track.height = 26.0f;  // Default height
        new_track.min_height = 20.0f;
        new_track.max_height = 100.0f;
        new_track.solo = false;
        new_track.mute = false;
        new_track.selected = false;
        new_track.items.clear();  // Empty items list
        
        // Add the new track to the vector
        tracks.push_back(new_track);
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Add Track: Add a new track");
    }
    ImGui::PopStyleColor(4);
    
    ImGui::SameLine(0, button_spacing);
    
    // Remove Track button
    ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(255, 0, 0, 100));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255, 0, 0, 150));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(255, 0, 0, 200));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
    if (ImGui::Button("-##remove_track", ImVec2(button_size, button_size))) {
        // Check if any track is selected
        bool has_selected_track = false;
        std::string selected_track_name;
        for (size_t i = 0; i < tracks.size(); ++i) {
            const auto& tr = tracks[i];
            if (tr.selected) {
                has_selected_track = true;
                selected_track_name = tr.name;
                break;
            }
        }
        
        if (has_selected_track) {
            // Find the index of the selected track
            int selected_track_index = -1;
            for (size_t i = 0; i < tracks.size(); ++i) {
                if (tracks[i].selected) {
                    selected_track_index = static_cast<int>(i);
                    break;
                }
            }
            ShowDeleteTrackModal(selected_track_name, selected_track_index);
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Remove Track: Remove the selected track");
    }
    ImGui::PopStyleColor(4);
    
    ImGui::EndChild();

    // Ruler starts on the same row as global controls (below top bar)
    const float controls_and_ruler_y = rect.Min.y + top_bar_height + 4.0f; // Top bar + padding
    ImRect rulerRect(ImVec2(rect.Min.x, controls_and_ruler_y), ImVec2(rect.Max.x, controls_and_ruler_y + cfg_.ruler_height));
    
    ImGui::SetCursorScreenPos(rulerRect.Min);
    ImGui::InvisibleButton("##ruler", rulerRect.GetSize());
    
    // Mouse-following shadow line in ruler area
    if (ImGui::IsItemHovered()) {
        const ImVec2& mouse_pos = io.MousePos;
        if (mouse_pos.x >= rect.Min.x + cfg_.label_width && mouse_pos.x <= rect.Max.x) {
            // Draw a very light grey vertical line that follows the mouse in ruler
            const ImU32 shadow_color = IM_COL32(128, 128, 128, 80); // More visible light grey
            dl->AddLine(
                ImVec2(mouse_pos.x, controls_and_ruler_y), 
                ImVec2(mouse_pos.x, rect.Max.y), 
                shadow_color, 
                1.0f
            );
        }
    }
    
    // Bulletproof fast ruler click to set current time
    if (ImGui::IsItemHovered() && io.MouseDown[0]){
        // Ultra-fast pixel-to-time conversion using pre-computed factors
        const float x0 = rect.Min.x + cfg_.label_width;
        const float x1 = rect.Max.x;
        const float pixel_width = x1 - x0;
        const double span = std::max(st_.view_max - st_.view_min, kEps);
        const float time_to_pixel_factor = static_cast<float>(pixel_width / span);
        
        // Direct calculation without function call overhead
        double t = st_.view_min + (io.MousePos.x - x0) / time_to_pixel_factor;
        SetCurrentTime(t);
    }
    


    // Calculate bottom bar position first
    const float bottom_bar_height = top_bar_height * 0.8f; // 80% of top bar height
    const float bottom_bar_y = rect.Max.y - bottom_bar_height;
    
    // Define the tracks area - starts below controls/ruler and ends above bottom bar
    ImRect area(ImVec2(rect.Min.x, rulerRect.Max.y), ImVec2(rect.Max.x, bottom_bar_y));
    
    // Draw bottom bar FIRST (before ruler) so grid lines appear on top
    float separator_x = area.Min.x + cfg_.label_width;
    ImRect rightBottomBarRect(ImVec2(separator_x, bottom_bar_y), ImVec2(rect.Max.x, rect.Max.y));
    const ImU32 bottom_bar_bg = IM_COL32(60, 60, 60, 255); // Darker grey for bottom bar
    dl->AddRectFilled(rightBottomBarRect.Min, rightBottomBarRect.Max, bottom_bar_bg, 0.0f);
    
    // Add integer input widgets to the bottom bar
    ImGui::SetCursorScreenPos(ImVec2(separator_x + 10, bottom_bar_y + 8));
    ImGui::PushItemWidth(80);
    
    // Start input (always 0 for now)
    ImGui::Text("Start:");
    ImGui::SameLine();
    static int start_value = 0;
    ImGui::InputInt("##start", &start_value, 0, 0, ImGuiInputTextFlags_ReadOnly);
    
    ImGui::SameLine(0, 20);
    
    // End input (default 50) - controls timeline zoom
    ImGui::Text("End:");
    ImGui::SameLine();
    static int end_value = 50;
    if (ImGui::InputInt("##end", &end_value)) {
        // Update timeline view when End value changes
        if (end_value > start_value) {
            st_.view_min = static_cast<double>(start_value);
            st_.view_max = static_cast<double>(end_value);
            enforceViewBounds(); // Ensure view stays within valid range
        }
    }
    
    ImGui::PopItemWidth();
    
    // Draw ruler AFTER bottom bar so grid lines appear on top
    drawRuler(dl, rulerRect, bottom_bar_y);
    
    // Update scrollbar dimensions and handle scrolling
    updateScrollbar(tracks, area);
    
    // Create an invisible button that covers the full timeline area for mouse interaction
    // This ensures all tracks are interactive regardless of their position
    ImGui::SetCursorScreenPos(area.Min);
    ImGui::InvisibleButton("##area", area.GetSize(), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    const bool area_hovered = ImGui::IsItemHovered();

    // Draw only right border for the content area (ruler + tracks) below the top bar
    const ImU32 border_color = IM_COL32(100, 100, 100, 255); // Grey border
    float content_start_y = controls_and_ruler_y;
    float content_end_y = rect.Max.y;
    // Only draw right border line
    dl->AddLine(ImVec2(rect.Max.x, content_start_y), ImVec2(rect.Max.x, content_end_y), border_color, 1.0f);
    
    // Draw vertical separator line between left sidebar and right content area
    dl->AddLine(ImVec2(separator_x, content_start_y), ImVec2(separator_x, content_end_y), border_color, 1.0f);
    
    // Draw the custom scrollbar on the right side
    drawScrollbar(dl, area, tracks);
    
    handleZoomPan(area);

    // Space bar control for play/pause
    if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
        TogglePlayPause();
    }

    // Auto-play functionality with velocity control
    if (st_.is_playing) {
        double current_time = ImGui::GetTime();
        double elapsed = current_time - st_.play_start_time;
        // Apply velocity multiplier for 1x, 2x, 5x speeds
        double new_time = st_.current_time + (elapsed * st_.play_velocity);
        
        if (new_time >= cfg_.max_time) {
            st_.is_playing = false;
            new_time = cfg_.max_time;
        }
        
        SetCurrentTime(new_time);
        st_.play_start_time = current_time;
    }

    
  
    // Start tracks with scroll offset - tracks can extend beyond the visible area
    float y = area.Min.y + 8.0f - scroll_offset_; // Apply scroll offset
    const float x0 = area.Min.x + cfg_.label_width;
    const float x1 = area.Max.x;
    
    // Pre-compute time-to-pixels conversion factors for maximum performance
    const float pixel_width = x1 - x0;
    const double span = std::max(st_.view_max - st_.view_min, kEps);
    const float time_to_pixel_factor = static_cast<float>(pixel_width / span);

    int hovered_track = -1, hovered_item = -1;

    // Set clipping to prevent tracks from drawing above the top area
    ImGui::PushClipRect(area.Min, area.Max, true);

    for (int ti = 0; ti < static_cast<int>(tracks.size()); ++ti){
        auto& tr = tracks[static_cast<size_t>(ti)];
        
        // Only render tracks that are at least partially visible
        if (y + tr.height >= area.Min.y && y <= area.Max.y) {
            // Render track using the reusable function
            drawTrack(dl, tr, area, x0, x1, y, time_to_pixel_factor, st_.view_min, area_hovered, hovered_track, hovered_item, ti, tracks, out_edit);
        }
        
        y += tr.height;
    }
    
    // Restore clipping
    ImGui::PopClipRect();
    
    // Handle track selection on click

    
    if (area_hovered && io.MouseClicked[0] && hovered_track >= 0 && hovered_item < 0) {
        // Clicked on a track but not on an item - select the track
        if (!io.KeyCtrl) {
            // Clear previous selection if not holding Ctrl
            for (auto& tr : tracks) {
                tr.selected = false;
            }
        }
        // Toggle selection for clicked track
        tracks[static_cast<size_t>(hovered_track)].selected = !tracks[static_cast<size_t>(hovered_track)].selected || io.KeyCtrl;
        selected_track_index_ = tracks[static_cast<size_t>(hovered_track)].selected ? hovered_track : -1;
        

        
        // Update edit flags
        if (out_edit) {
            out_edit->track_selection_changed = true;
            out_edit->selected_track_index = selected_track_index_;
            out_edit->changed = true;
        }
    }
    
    // Mouse-following shadow line - appears when hovering over tracks area
    if (area_hovered) {
        const ImVec2& mouse_pos = io.MousePos;
        if (mouse_pos.x >= x0 && mouse_pos.x <= x1 && mouse_pos.y >= area.Min.y && mouse_pos.y <= area.Max.y) {
            // Draw a very light grey vertical line that follows the mouse
            const ImU32 shadow_color = IM_COL32(128, 128, 128, 80); // More visible light grey
            dl->AddLine(
                ImVec2(mouse_pos.x, area.Min.y), 
                ImVec2(mouse_pos.x, area.Max.y), 
                shadow_color, 
                1.0f
            );
        }
    }
    
    // Enhanced red line positioning: allow click-and-drag following on tracks
    static bool is_tracking_mouse = false;
    if (area_hovered && io.MouseDown[0] && drag_mode_ == DragMode::None){
        if (!is_tracking_mouse) {
            is_tracking_mouse = true;
        }
        // Real-time red line following during drag - works on ALL track areas
        double t = st_.view_min + (io.MousePos.x - x0) / time_to_pixel_factor;
        SetCurrentTime(t);
    } else if (!io.MouseDown[0]) {
        is_tracking_mouse = false;
    }

    // High-performance mouse click detection with zero latency
    if (hovered_item >= 0 && hovered_track >= 0 && io.MouseClicked[0]){
        auto &it = tracks[static_cast<size_t>(hovered_track)].items[static_cast<size_t>(hovered_item)];
        bool was = it.selected;
        if (!io.KeyCtrl){
            for (auto &tr: tracks) for (auto &itm: tr.items) itm.selected = false;
        }
        it.selected = !was || io.KeyCtrl;
        
        // Always select the track when clicking on media items (more efficient)
        if (!io.KeyCtrl) {
            // Clear previous track selection if not holding Ctrl
            for (auto& tr : tracks) {
                tr.selected = false;
            }
        }
        tracks[static_cast<size_t>(hovered_track)].selected = true;
        selected_track_index_ = hovered_track;

        // Move red line to clicked position on media item
        double t = st_.view_min + (io.MousePos.x - x0) / time_to_pixel_factor;
        SetCurrentTime(t);

        // Ultra-fast time-to-pixels conversion for drag detection
        float rx0 = x0 + (static_cast<float>(it.t0) - static_cast<float>(st_.view_min)) * time_to_pixel_factor;
        float rx1 = x0 + (static_cast<float>(it.t1) - static_cast<float>(st_.view_min)) * time_to_pixel_factor;
        float mx  = io.MousePos.x;
        const float edge_px = 6.f;
        drag_mode_ = (std::abs(mx - rx0) < edge_px) ? DragMode::ResizeL : (std::abs(mx - rx1) < edge_px) ? DragMode::ResizeR : DragMode::Move;
        drag_item_track_idx_ = hovered_track; drag_item_idx_ = hovered_item; drag_item_t0_ = it.t0; drag_item_t1_ = it.t1;
    }

    if (drag_mode_ != DragMode::None){
        // Bulletproof fast drag handling with velocity of light performance
        if (io.MouseDown[0]){
            auto &it = tracks[static_cast<size_t>(drag_item_track_idx_)].items[static_cast<size_t>(drag_item_idx_)];
            float dx = io.MouseDelta.x;
            // Use pre-computed factor for ultra-fast time calculation
            const double dt = static_cast<double>(dx) / time_to_pixel_factor;
            if (drag_mode_ == DragMode::Move){
                double new_t0 = clampd(drag_item_t0_ + dt, cfg_.min_time, cfg_.max_time);
                double new_t1 = clampd(drag_item_t1_ + dt, cfg_.min_time, cfg_.max_time);
                const double len = drag_item_t1_ - drag_item_t0_;
                if (new_t1 - new_t0 != len){ if (new_t0 <= cfg_.min_time) new_t1 = new_t0 + len; if (new_t1 >= cfg_.max_time) new_t0 = new_t1 - len; }
                it.t0 = new_t0; it.t1 = new_t1;
                // Update drag anchor to prevent snapping back
                drag_item_t0_ = new_t0; drag_item_t1_ = new_t1;
            } else {
                if (drag_mode_ == DragMode::ResizeL){ 
                    it.t0 = clampd(std::min(drag_item_t0_ + dt, it.t1 - kEps), cfg_.min_time, it.t1 - kEps); 
                    drag_item_t0_ = it.t0; // Update anchor
                }
                if (drag_mode_ == DragMode::ResizeR){ 
                    it.t1 = clampd(std::max(drag_item_t1_ + dt, it.t0 + kEps), it.t0 + kEps, cfg_.max_time); 
                    drag_item_t1_ = it.t1; // Update anchor
                }
            }
        } else {
            drag_mode_ = DragMode::None; drag_item_idx_ = -1; drag_item_track_idx_ = -1;
        }
    }

    // Draw the red playhead line on top of everything in the main timeline area
    float playhead_x = x0 + (static_cast<float>(st_.current_time) - static_cast<float>(st_.view_min)) * time_to_pixel_factor;
    if (playhead_x >= x0 && playhead_x <= x1) {
        const ImU32 playhead_color = ImGui::GetColorU32(ImGuiCol_PlotLinesHovered);
        dl->AddLine(ImVec2(playhead_x, area.Min.y), ImVec2(playhead_x, area.Max.y), playhead_color, 2.0f);
    }
    
    ImGui::EndChild();
    
    // Render the delete track modal if it's open
    renderDeleteTrackModal(tracks);
}




void Timeline::drawTrack(ImDrawList* dl, TimelineTrack& track, const ImRect& area, 
                         float x0, float x1, float y, float time_to_pixel_factor, double view_min, 
                         bool area_hovered, int& hovered_track, int& hovered_item, int track_index,
                         std::vector<TimelineTrack>& tracks, TimelineEdit* out_edit) {
    // Suppress unused parameter warning
    (void)out_edit;
    const float row_h = track.height;
    ImRect row_rect(ImVec2(area.Min.x, y), ImVec2(area.Max.x, y + row_h));
    
    // Add track background - full rectangle with darker grey color
    //const ImU32 track_bg = IM_COL32(60, 60, 60, 255); // Darker grey for track background
    //dl->AddRectFilled(row_rect.Min, row_rect.Max, track_bg, 0.0f);
    

    

    
    // Professional left sidebar styling with fixed width
    const float sidebar_width = cfg_.label_width;
    ImRect sidebar_rect(ImVec2(area.Min.x, y), ImVec2(area.Min.x + sidebar_width, y + row_h));
    
    // Create a child window for the sidebar to constrain ImGui widgets FIRST
    ImGui::SetCursorScreenPos(sidebar_rect.Min);
    ImGui::BeginChild(("##track_sidebar" + std::to_string(track_index)).c_str(), ImVec2(sidebar_width - 4, row_h - 4), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    
    // Override ImGui style to ensure no rounded corners
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_GrabRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 0.0f);
    
    // Add subtle background for the sidebar INSIDE the child window (so it's behind the widgets)
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0, 0), ImVec2(sidebar_width - 4, row_h - 4), IM_COL32(60, 60, 60, 255), 0.0f);
    
    // Track name text input (editable) - optimized width with balanced spacing
    const float button_width = 20.0f;
    const float spacing_width = 6.0f; // Balanced spacing between elements
    const float left_padding = 6.0f;  // Left margin for visual breathing room
    const float right_padding = 8.0f; // Right margin before buttons
    const float text_input_width = std::max(100.0f, sidebar_width - (button_width * 4) - (spacing_width * 3) - left_padding - right_padding);
    
    // Add a small left margin for the text input
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + left_padding);
    ImGui::PushItemWidth(text_input_width);
    
    // Ensure consistent height with buttons for better visual alignment
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
    
    char track_name_buffer[256];
    strncpy(track_name_buffer, track.name.c_str(), sizeof(track_name_buffer) - 1);
    track_name_buffer[sizeof(track_name_buffer) - 1] = '\0';
    
    // Enhanced styling for better text visibility and contrast
    if (track.name.empty()) {
        // Empty track name - use placeholder styling
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(150, 150, 150, 255)); // Light grey text
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(80, 80, 80, 255)); // Darker background for contrast
        ImGui::InputText(("##track_name" + std::to_string(track_index)).c_str(), track_name_buffer, sizeof(track_name_buffer), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopStyleColor(2);
    } else {
        // Non-empty track name - use high contrast styling
        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255)); // White text for maximum visibility
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(100, 100, 100, 255)); // Medium grey background
        if (ImGui::InputText(("##track_name" + std::to_string(track_index)).c_str(), track_name_buffer, sizeof(track_name_buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
            track.name = std::string(track_name_buffer);
        }
        ImGui::PopStyleColor(2);
    }
    
    ImGui::PopStyleVar();
    ImGui::PopItemWidth();
    
    // Solo and Mute buttons (consistent size and positioning)
    ImGui::SameLine(0, spacing_width);
    
    // Solo button
    ImGui::PushStyleColor(ImGuiCol_Button, track.solo ? IM_COL32(255, 255, 0, 100) : ImGui::GetColorU32(ImGuiCol_Button));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, track.solo ? IM_COL32(255, 255, 0, 150) : ImGui::GetColorU32(ImGuiCol_ButtonHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, track.solo ? IM_COL32(255, 255, 0, 200) : ImGui::GetColorU32(ImGuiCol_ButtonActive));
    ImGui::PushStyleColor(ImGuiCol_Text, track.solo ? IM_COL32(0, 0, 0, 255) : ImGui::GetColorU32(ImGuiCol_Text));
    
        if (ImGui::Button(("S##solo" + std::to_string(track_index)).c_str(), ImVec2(20, 20))) {
            track.solo = !track.solo;
            if (track.solo) {
                // Unmute other tracks when soloing this one
                for (auto& other_tr : tracks) {
                    if (&other_tr != &track) {
                        other_tr.mute = true;
                    }
                }
            }
        }
        ImGui::PopStyleColor(4);
        
        ImGui::SameLine(0, spacing_width);
        
        // Mute button
        ImGui::PushStyleColor(ImGuiCol_Button, track.mute ? IM_COL32(255, 0, 0, 100) : ImGui::GetColorU32(ImGuiCol_Button));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, track.mute ? IM_COL32(255, 0, 0, 150) : ImGui::GetColorU32(ImGuiCol_ButtonHovered));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, track.mute ? IM_COL32(255, 0, 0, 200) : ImGui::GetColorU32(ImGuiCol_ButtonActive));
        ImGui::PushStyleColor(ImGuiCol_Text, track.mute ? IM_COL32(255, 255, 255, 255) : ImGui::GetColorU32(ImGuiCol_Text));
        
        if (ImGui::Button(("M##mute" + std::to_string(track_index)).c_str(), ImVec2(20, 20))) {
            track.mute = !track.mute;
        }
        ImGui::PopStyleColor(4);
        
        // Track height control buttons (consistent size)
        ImGui::SameLine(0, spacing_width);
        
        // Up arrow button for increasing height
        if (ImGui::Button(("▲##up" + std::to_string(track_index)).c_str(), ImVec2(20, 20))) {
            track.height = std::min(track.height + 4.0f, track.max_height);
        }
        
        ImGui::SameLine(0, spacing_width);
        
        // Down arrow button for decreasing height
        if (ImGui::Button(("▼##down" + std::to_string(track_index)).c_str(), ImVec2(20, 20))) {
            track.height = std::max(track.height - 4.0f, std::max(track.min_height, cfg_.min_track_height));
        }
        
        ImGui::PopStyleVar(3); // Pop the rounding style overrides
        ImGui::EndChild();
        
        // Track height dragging functionality
        ImRect height_drag_rect(ImVec2(area.Min.x + cfg_.label_width - 4, y), ImVec2(area.Min.x + cfg_.label_width, y + row_h));
        ImGui::SetCursorScreenPos(height_drag_rect.Min);
        if (ImGui::InvisibleButton(("##height_drag" + std::to_string(track_index)).c_str(), height_drag_rect.GetSize())) {
            // This button handles the drag area for resizing track height
        }
        
        // Handle height dragging
        static bool is_resizing_height = false;
        static int resizing_track = -1;
        static float initial_height = 0.0f;
        static float initial_mouse_y = 0.0f;
        
        if (ImGui::IsItemHovered() && ImGui::GetIO().MouseDown[0]) {
            if (!is_resizing_height) {
                is_resizing_height = true;
                resizing_track = track_index;
                initial_height = track.height;
                initial_mouse_y = ImGui::GetIO().MousePos.y;
            }
        }
        
        if (is_resizing_height && resizing_track == track_index && ImGui::GetIO().MouseDown[0]) {
            float delta_y = ImGui::GetIO().MousePos.y - initial_mouse_y;
            float new_height = initial_height + delta_y;
            track.height = std::max(track.min_height, std::min(track.max_height, new_height));
        } else if (!ImGui::GetIO().MouseDown[0]) {
            is_resizing_height = false;
            resizing_track = -1;
        }
    
    // Subtle separator line
    dl->AddLine(ImVec2(area.Min.x, row_rect.Max.y), ImVec2(area.Max.x, row_rect.Max.y), ImGui::GetColorU32(ImGuiCol_Border), 1.0f);
    
    // Add subtle border around the track row (no rounded corners)
    dl->AddRect(row_rect.Min, row_rect.Max, ImGui::GetColorU32(ImGuiCol_Border), 0.0f, 0, 1.0f);

    // Render track items (regular timeline items)
    for (int ii = 0; ii < static_cast<int>(track.items.size()); ++ii){
        auto& it = track.items[static_cast<size_t>(ii)];
        if (it.t1 < it.t0) std::swap(it.t0, it.t1);
        double t0 = clampd(it.t0, cfg_.min_time, cfg_.max_time);
        double t1 = clampd(it.t1, cfg_.min_time, cfg_.max_time);
        if (t1 < view_min || t0 > view_min + (x1 - x0) / time_to_pixel_factor) continue;

        // Ultra-fast time-to-pixels conversion for items
        float rx0 = x0 + (static_cast<float>(t0) - static_cast<float>(view_min)) * time_to_pixel_factor;
        float rx1 = x0 + (static_cast<float>(t1) - static_cast<float>(view_min)) * time_to_pixel_factor;
        ImRect ir(ImVec2(clampf(rx0, x0, x1), y + 2), ImVec2(clampf(rx1, x0, x1), y + row_h - 2));
        ImU32 col = it.selected ? ImGui::GetColorU32(ImGuiCol_Header) : it.color;
        dl->AddRectFilled(ir.Min, ir.Max, col, cfg_.item_rounding);
        dl->AddRect(ir.Min, ir.Max, ImGui::GetColorU32(ImGuiCol_Border), cfg_.item_rounding);
        if (!it.label.empty()){
            ImVec2 sz = ImGui::CalcTextSize(it.label.c_str());
            ImVec2 p  = ImVec2(std::max(ir.Min.x + 6.f, std::min(ir.Max.x - sz.x - 4.f, ir.Min.x + 6.f)), ir.Min.y + (ir.GetHeight()-sz.y)*0.5f);
            dl->AddText(p, ImGui::GetColorU32(ImGuiCol_Text), it.label.c_str());
        }
        if (area_hovered){
            // High-performance mouse tracking with zero latency
            const ImVec2& mp = ImGui::GetIO().MousePos;
            if (mp.x >= ir.Min.x && mp.x <= ir.Max.x && mp.y >= ir.Min.y && mp.y <= ir.Max.y){
                hovered_track = track_index; hovered_item = ii;
            }
        }
    }
    
    // Set hovered_track for the entire track area (not just media items)
    if (area_hovered) {
        const ImVec2& mp = ImGui::GetIO().MousePos;
        if (mp.x >= area.Min.x && mp.x <= area.Max.x && mp.y >= y && mp.y <= y + row_h) {
            hovered_track = track_index;
            
            // Handle track selection when clicking on track area
            if (ImGui::GetIO().MouseClicked[0]) {
                // Clear previous track selection if not holding Ctrl
                if (!ImGui::GetIO().KeyCtrl) {
                    for (auto& tr : tracks) {
                        tr.selected = false;
                    }
                }
                track.selected = true;
                selected_track_index_ = track_index;
            }
        }
    }
    
    // Additional track selection for the left sidebar area (S/M buttons, text input, etc.)
    if (area_hovered) {
        const ImVec2& mp = ImGui::GetIO().MousePos;
        // Check if mouse is in the left sidebar area (where the controls are)
        if (mp.x >= area.Min.x && mp.x <= area.Min.x + cfg_.label_width && mp.y >= y && mp.y <= y + row_h) {
            // Handle track selection when clicking in the sidebar area
            if (ImGui::GetIO().MouseClicked[0]) {
                // Clear previous track selection if not holding Ctrl
                if (!ImGui::GetIO().KeyCtrl) {
                    for (auto& tr : tracks) {
                        tr.selected = false;
                    }
                }
                track.selected = true;
                selected_track_index_ = track_index;
            }
        }
    }
    
    // Draw selection highlight LAST so it appears on top of everything
    if (track.selected) {
        const ImU32 selection_bg = IM_COL32(255, 0, 0, 50); // Light red with low opacity
        dl->AddRectFilled(row_rect.Min, row_rect.Max, selection_bg, 0.0f);
    }

}



void Timeline::ShowDeleteTrackModal(const std::string& track_name, int track_index) {
    track_to_delete_name_ = track_name;
    track_to_delete_index_ = track_index;
    show_delete_modal_ = true;
}

void Timeline::renderDeleteTrackModal(std::vector<TimelineTrack>& tracks) {
    if (!show_delete_modal_) return;
    
    // Center the modal on screen
    ImVec2 display_size = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(display_size.x * 0.5f, display_size.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    
    // Use regular popup instead of modal to avoid context issues
    if (ImGui::Begin("Delete Track", &show_delete_modal_, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse)) {
        ImGui::Text("Do you really want to delete track \"%s\"?", track_to_delete_name_.c_str());
        ImGui::Spacing();
        
        // Center the buttons
        float button_width = 80.0f;
        float total_width = button_width * 2 + 20.0f; // 2 buttons + spacing
        ImGui::SetCursorPosX((ImGui::GetWindowSize().x - total_width) * 0.5f);
        
        if (ImGui::Button("Yes", ImVec2(button_width, 0))) {
            // Delete the track using the stored index
            if (track_to_delete_index_ >= 0 && track_to_delete_index_ < static_cast<int>(tracks.size())) {
                // Remove the track from the vector
                tracks.erase(tracks.begin() + track_to_delete_index_);
                
                // Reset selection state
                selected_track_index_ = -1;
                
                // Close modal after deletion
                show_delete_modal_ = false;
            }
        }
        
        ImGui::SameLine();
        
        if (ImGui::Button("No", ImVec2(button_width, 0))) {
            show_delete_modal_ = false;
        }
        
        ImGui::End();
    }
}



std::string Timeline::generateUniqueTrackName(const std::vector<TimelineTrack>& tracks) {
    // Generate a random 10-character name
    const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string name;
    
    // Keep generating names until we find a unique one
    do {
        name.clear();
        for (int i = 0; i < 10; ++i) {
            name += chars[static_cast<size_t>(rand()) % chars.length()];
        }
    } while (std::any_of(tracks.begin(), tracks.end(), 
                         [&name](const TimelineTrack& track) { return track.name == name; }));
    
    return name;
}

void Timeline::updateScrollbar(const std::vector<TimelineTrack>& tracks, const ImRect& area) {
    const float scrollbar_width = 10.0f; // Thinner scrollbar width
    const float scrollbar_x = area.Max.x - scrollbar_width;
    
    // Calculate total height needed for all tracks
    float total_tracks_height = 8.0f; // Initial padding
    for (const auto& track : tracks) {
        total_tracks_height += track.height;
    }
    
    // Calculate available height for tracks
    float available_height = area.GetHeight() - 8.0f; // Subtract initial padding
    
    // Only show scrollbar if content exceeds available height
    if (total_tracks_height <= available_height) {
        scroll_offset_ = 0.0f;
        scroll_handle_height_ = 0.0f;
        return;
    }
    
    // Calculate scrollbar handle height (proportional to visible area)
    scroll_handle_height_ = (available_height / total_tracks_height) * available_height;
    scroll_handle_height_ = std::max(scroll_handle_height_, 20.0f); // Minimum handle height
    
    // Handle mouse wheel scrolling
    if (ImGui::IsWindowHovered()) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            float scroll_step = 20.0f; // Scroll step per wheel movement
            scroll_offset_ -= wheel * scroll_step;
        }
    }
    
    // Handle scrollbar dragging
    ImRect scrollbar_rect(ImVec2(scrollbar_x, area.Min.y), ImVec2(area.Max.x, area.Max.y));
    if (ImGui::IsMouseHoveringRect(scrollbar_rect.Min, scrollbar_rect.Max)) {
        if (ImGui::IsMouseClicked(0)) {
            is_scrolling_ = true;
        }
    }
    
    if (is_scrolling_ && ImGui::IsMouseDown(0)) {
        float mouse_y = ImGui::GetIO().MousePos.y;
        float scrollable_height = total_tracks_height - available_height;
        
        // Calculate scroll offset based on mouse position
        float relative_y = (mouse_y - area.Min.y) / available_height;
        scroll_offset_ = relative_y * scrollable_height;
    } else {
        is_scrolling_ = false;
    }
    
    // Clamp scroll offset to ensure handle can reach the bottom
    scroll_offset_ = std::max(0.0f, std::min(scroll_offset_, total_tracks_height - available_height));
}

void Timeline::drawScrollbar(ImDrawList* dl, const ImRect& area, const std::vector<TimelineTrack>& tracks) {
    const float scrollbar_width = 10.0f;
    const float scrollbar_x = area.Max.x - scrollbar_width;
    
    // Only draw scrollbar if there's content to scroll
    if (scroll_handle_height_ <= 0.0f) return;
    
    // Draw scrollbar background
    ImRect scrollbar_bg(ImVec2(scrollbar_x, area.Min.y), ImVec2(area.Max.x, area.Max.y));
    dl->AddRectFilled(scrollbar_bg.Min, scrollbar_bg.Max, IM_COL32(40, 40, 40, 200), 0.0f);
    
    // Calculate proper handle position
    float total_tracks_height = 8.0f; // Initial padding
    for (const auto& track : tracks) {
        total_tracks_height += track.height;
    }
    float available_height = area.GetHeight() - 8.0f;
    float scrollable_height = total_tracks_height - available_height;
    
    // Calculate handle position based on scroll offset
    float relative_offset = scroll_offset_ / scrollable_height;
    float handle_y = area.Min.y + (relative_offset * (available_height - scroll_handle_height_));
    
    // Ensure handle doesn't go below the bottom
    handle_y = std::min(handle_y, area.Max.y - scroll_handle_height_);
    
    ImRect handle_rect(ImVec2(scrollbar_x + 1, handle_y), ImVec2(area.Max.x - 1, handle_y + scroll_handle_height_));
    dl->AddRectFilled(handle_rect.Min, handle_rect.Max, IM_COL32(100, 100, 100, 255), 2.0f);
    dl->AddRect(handle_rect.Min, handle_rect.Max, IM_COL32(150, 150, 150, 255), 2.0f, 0.0f, 1.0f);
}

} // namespace ImGuiX

