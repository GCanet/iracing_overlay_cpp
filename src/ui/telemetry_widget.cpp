#include "ui/telemetry_widget.h"
#include "data/irsdk_manager.h"
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
    
    // Window position (bottom right)
    ImGui::SetNextWindowPos(
        ImVec2(ImGui::GetIO().DisplaySize.x - 340, 
               ImGui::GetIO().DisplaySize.y - 180), 
        ImGuiCond_FirstUseEver
    );
    ImGui::SetNextWindowSize(ImVec2(320, 160), ImGuiCond_FirstUseEver);
    
    ImGui::Begin("TELEMETRY", nullptr, ImGuiWindowFlags_NoCollapse);
    
    // Graphs only
    float green[3] = {0.3f, 1.0f, 0.3f};
    float red[3] = {1.0f, 0.3f, 0.3f};
    
    ImGui::Text("Throttle");
    renderGraph("Throttle", m_throttleHistory, green);
    
    ImGui::Text("Brake");
    renderGraph("Brake", m_brakeHistory, red);
    
    ImGui::End();
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

void TelemetryWidget::renderGraph(const char* label, const std::deque<float>& data, float color[3]) {
    if (data.empty()) return;
    
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size(GRAPH_WIDTH, GRAPH_HEIGHT);
    
    // Background
    draw_list->AddRectFilled(
        canvas_pos,
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        IM_COL32(20, 20, 20, 180)
    );
    
    // Draw graph line
    ImU32 line_color = IM_COL32(
        static_cast<int>(color[0] * 255),
        static_cast<int>(color[1] * 255),
        static_cast<int>(color[2] * 255),
        255
    );
    
    float step = canvas_size.x / static_cast<float>(data.size());
    
    for (size_t i = 1; i < data.size(); i++) {
        float x1 = canvas_pos.x + (i - 1) * step;
        float y1 = canvas_pos.y + canvas_size.y * (1.0f - data[i - 1]);
        
        float x2 = canvas_pos.x + i * step;
        float y2 = canvas_pos.y + canvas_size.y * (1.0f - data[i]);
        
        draw_list->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), line_color, 2.0f);
    }
    
    // Border
    draw_list->AddRect(
        canvas_pos,
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        IM_COL32(100, 100, 100, 255)
    );
    
    ImGui::Dummy(canvas_size);
}

} // namespace ui
