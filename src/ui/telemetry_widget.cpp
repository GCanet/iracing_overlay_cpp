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
    
    // Fixed size always - just the graph, no extra padding
    float windowWidth = GRAPH_WIDTH;
    float windowHeight = GRAPH_HEIGHT;
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
    
    // Fully transparent window background - no outer box
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    
    // Window flags - no resize, no title, no border
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | 
                             ImGuiWindowFlags_NoTitleBar | 
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoBackground;
    
    // Check if we're in locked mode
    bool isLocked = utils::Config::isClickThrough();
    if (isLocked) {
        flags |= ImGuiWindowFlags_NoMove;
    }
    
    ImGui::Begin("##TELEMETRY", nullptr, flags);
    
    // Save position back to config
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
        
        // Background (the graph's own background, semi-transparent)
        draw_list->AddRectFilled(
            canvas_pos,
            ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
            bg_color
        );
        
        // Draw THROTTLE line (RIGHT TO LEFT)
        float step = canvas_size.x / static_cast<float>(m_throttleHistory.size() - 1);
        
        draw_list->PathClear();
        for (size_t i = 0; i < m_throttleHistory.size(); i++) {
            float x = canvas_pos.x + canvas_size.x - (i * step);
            size_t dataIndex = m_throttleHistory.size() - 1 - i;
            float y = canvas_pos.y + canvas_size.y * (1.0f - m_throttleHistory[dataIndex]);
            draw_list->PathLineTo(ImVec2(x, y));
        }
        draw_list->PathStroke(throttle_color, 0, 2.0f);
        
        // Draw BRAKE line (RIGHT TO LEFT, same graph)
        draw_list->PathClear();
        for (size_t i = 0; i < m_brakeHistory.size(); i++) {
            float x = canvas_pos.x + canvas_size.x - (i * step);
            size_t dataIndex = m_brakeHistory.size() - 1 - i;
            float y = canvas_pos.y + canvas_size.y * (1.0f - m_brakeHistory[dataIndex]);
            draw_list->PathLineTo(ImVec2(x, y));
        }
        draw_list->PathStroke(brake_color, 0, 2.0f);
        
        // Thin border around the graph only
        draw_list->AddRect(
            canvas_pos,
            ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
            border_color
        );
    }
    
    // Reserve space for the graph
    ImGui::Dummy(ImVec2(GRAPH_WIDTH, GRAPH_HEIGHT));
    
    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
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
}

} // namespace ui
