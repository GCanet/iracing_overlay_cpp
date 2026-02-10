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
    
    // Fixed size always
    float windowWidth = GRAPH_WIDTH + 20;
    float windowHeight = GRAPH_HEIGHT + 40;
    ImGui::SetNextWindowSize(ImVec2(windowWidth, windowHeight), ImGuiCond_Always);
    
    // Window position from config (or default)
    if (config.posX < 0 || config.posY < 0) {
        // First use - horizontal center, 20% up from bottom
        float x = (ImGui::GetIO().DisplaySize.x - windowWidth) / 2.0f;
        float y = ImGui::GetIO().DisplaySize.y * 0.8f - windowHeight;
        ImGui::SetNextWindowPos(ImVec2(x, y), ImGuiCond_FirstUseEver);
    } else {
        // Use saved position
        ImGui::SetNextWindowPos(ImVec2(config.posX, config.posY), ImGuiCond_Once);
    }
    
    // Window alpha from config
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, config.alpha));
    
    // Window flags - allow moving when not click-through, but never resize
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | 
                             ImGuiWindowFlags_NoTitleBar | 
                             ImGuiWindowFlags_NoResize;
    
    // Check if we're in click-through mode
    bool isLocked = utils::Config::isClickThrough();
    if (isLocked) {
        flags |= ImGuiWindowFlags_NoMove;
    }
    
    ImGui::Begin("##TELEMETRY", nullptr, flags);
    
    // Save position back to config (will update as user drags)
    ImVec2 pos = ImGui::GetWindowPos();
    config.posX = pos.x;
    config.posY = pos.y;
    config.width = windowWidth;
    config.height = windowHeight;
    
    // Get the draw list and canvas position
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size(GRAPH_WIDTH, GRAPH_HEIGHT);
    
    // Render BOTH graphs in the SAME box (moving from RIGHT to LEFT)
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
        
        // Draw THROTTLE line (RIGHT TO LEFT)
        float step = canvas_size.x / static_cast<float>(m_throttleHistory.size() - 1);
        
        draw_list->PathClear();
        for (size_t i = 0; i < m_throttleHistory.size(); i++) {
            // Start from RIGHT (canvas_pos.x + canvas_size.x) and go LEFT
            float x = canvas_pos.x + canvas_size.x - (i * step);
            // Get value from most recent (end of deque) to oldest (start of deque)
            size_t dataIndex = m_throttleHistory.size() - 1 - i;
            float y = canvas_pos.y + canvas_size.y * (1.0f - m_throttleHistory[dataIndex]);
            draw_list->PathLineTo(ImVec2(x, y));
        }
        draw_list->PathStroke(throttle_color, 0, 2.0f);
        
        // Draw BRAKE line (RIGHT TO LEFT, same graph)
        draw_list->PathClear();
        for (size_t i = 0; i < m_brakeHistory.size(); i++) {
            // Start from RIGHT and go LEFT
            float x = canvas_pos.x + canvas_size.x - (i * step);
            // Get value from most recent to oldest
            size_t dataIndex = m_brakeHistory.size() - 1 - i;
            float y = canvas_pos.y + canvas_size.y * (1.0f - m_brakeHistory[dataIndex]);
            draw_list->PathLineTo(ImVec2(x, y));
        }
        draw_list->PathStroke(brake_color, 0, 2.0f);
        
        // Border (1 draw call)
        draw_list->AddRect(
            canvas_pos,
            ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
            border_color
        );
        
        // NO T/B LABELS - removed as requested
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
