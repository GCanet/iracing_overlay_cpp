#include "ui/telemetry_widget.h"
#include "data/irsdk_manager.h"
#include "utils/config.h"
#include "imgui.h"

namespace ui {

TelemetryWidget::TelemetryWidget() {
}

void TelemetryWidget::render(iracing::IRSDKManager* sdk) {
    if (!sdk || !sdk->isConnected()) return;
    
    // Get current values
    float throttle = sdk->getFloat("Throttle", 0.0f);
    float brake = sdk->getFloat("Brake", 0.0f);
    
    // Update history
    updateHistory(throttle, brake);
    
    // Load config
    auto& config = utils::Config::getTelemetryConfig();
    
    // Window position from config (or default)
    if (config.posX < 0 || config.posY < 0) {
        // First use - bottom right default
        ImGui::SetNextWindowPos(
            ImVec2(ImGui::GetIO().DisplaySize.x - 340, 
                   ImGui::GetIO().DisplaySize.y - 120), 
            ImGuiCond_FirstUseEver
        );
    } else {
        ImGui::SetNextWindowPos(ImVec2(config.posX, config.posY), ImGuiCond_Always);
    }
    
    // Window size from config
    if (config.width > 0 && config.height > 0) {
        ImGui::SetNextWindowSize(ImVec2(config.width, config.height), ImGuiCond_Always);
    } else {
        ImGui::SetNextWindowSize(ImVec2(320, 100), ImGuiCond_FirstUseEver);
    }
    
    // Window alpha from config
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, config.alpha));
    
    // Single window with no title bar, just the graphs
    ImGui::Begin("##TELEMETRY", nullptr, 
        ImGuiWindowFlags_NoCollapse | 
        ImGuiWindowFlags_NoTitleBar);
    
    // Save position and size back to config
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();
    config.posX = pos.x;
    config.posY = pos.y;
    config.width = size.x;
    config.height = size.y;
    
    // Get the draw list and canvas position
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size(GRAPH_WIDTH, GRAPH_HEIGHT);
    
    // Render BOTH graphs in the SAME box
    if (!m_throttleHistory.empty() && !m_brakeHistory.empty()) {
        // Pre-calculated colors
        ImU32 bg_color = IM_COL32(20, 20, 20, 180);
        ImU32 throttle_color = IM_COL32(80, 255, 80, 255);  // Green
        ImU32 brake_color = IM_COL32(255, 80, 80, 255);     // Red
        ImU32 border_color = IM_COL32(100, 100, 100, 255);
        
        // Background (1 draw call)
        draw_list->AddRectFilled(
            canvas_pos,
            ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
            bg_color
        );
        
        // Draw THROTTLE line
        float step = canvas_size.x / static_cast<float>(m_throttleHistory.size());
        
        draw_list->PathClear();
        for (size_t i = 0; i < m_throttleHistory.size(); i++) {
            float x = canvas_pos.x + i * step;
            float y = canvas_pos.y + canvas_size.y * (1.0f - m_throttleHistory[i]);
            draw_list->PathLineTo(ImVec2(x, y));
        }
        draw_list->PathStroke(throttle_color, 0, 2.0f);
        
        // Draw BRAKE line (same graph)
        draw_list->PathClear();
        for (size_t i = 0; i < m_brakeHistory.size(); i++) {
            float x = canvas_pos.x + i * step;
            float y = canvas_pos.y + canvas_size.y * (1.0f - m_brakeHistory[i]);
            draw_list->PathLineTo(ImVec2(x, y));
        }
        draw_list->PathStroke(brake_color, 0, 2.0f);
        
        // Border (1 draw call)
        draw_list->AddRect(
            canvas_pos,
            ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
            border_color
        );
        
        // Labels inside the graph (top corners)
        draw_list->AddText(
            ImVec2(canvas_pos.x + 5, canvas_pos.y + 5),
            throttle_color,
            "T"
        );
        draw_list->AddText(
            ImVec2(canvas_pos.x + canvas_size.x - 15, canvas_pos.y + 5),
            brake_color,
            "B"
        );
    }
    
    // Reserve space for the graph
    ImGui::Dummy(ImVec2(GRAPH_WIDTH, GRAPH_HEIGHT));
    
    ImGui::End();
    ImGui::PopStyleColor();
}

void TelemetryWidget::updateHistory(float throttle, float brake) {
    m_throttleHistory.push_back(throttle);
    m_brakeHistory.push_back(brake);
    
    if (m_throttleHistory.size() > MAX_HISTORY) {
        m_throttleHistory.pop_front();
    }
    if (m_brakeHistory.size() > MAX_HISTORY) {
        m_brakeHistory.pop_front();
    }
}

void TelemetryWidget::renderGraph(ImDrawList* draw_list, ImVec2 canvas_pos, 
                                   const char* label, const std::deque<float>& data, 
                                   float color[3]) {
    // This function is no longer needed - kept for compatibility
    // Everything is now rendered in the main render() function
}

} // namespace ui
