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
    
    // OPTIMIZACIÓN: Un solo DrawList para ambos gráficos
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    
    // Throttle graph
    ImGui::Text("Throttle");
    ImVec2 throttle_pos = ImGui::GetCursorScreenPos();
    float green[3] = {0.3f, 1.0f, 0.3f};
    renderGraph(draw_list, throttle_pos, "Throttle", m_throttleHistory, green);
    ImGui::Dummy(ImVec2(GRAPH_WIDTH, GRAPH_HEIGHT));
    
    // Brake graph
    ImGui::Text("Brake");
    ImVec2 brake_pos = ImGui::GetCursorScreenPos();
    float red[3] = {1.0f, 0.3f, 0.3f};
    renderGraph(draw_list, brake_pos, "Brake", m_brakeHistory, red);
    ImGui::Dummy(ImVec2(GRAPH_WIDTH, GRAPH_HEIGHT));
    
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

void TelemetryWidget::renderGraph(ImDrawList* draw_list, ImVec2 canvas_pos, 
                                   const char* label, const std::deque<float>& data, 
                                   float color[3]) {
    if (data.empty()) return;
    
    ImVec2 canvas_size(GRAPH_WIDTH, GRAPH_HEIGHT);
    
    // OPTIMIZACIÓN: Colors precalculadas
    ImU32 bg_color = IM_COL32(20, 20, 20, 180);
    ImU32 line_color = IM_COL32(
        static_cast<int>(color[0] * 255),
        static_cast<int>(color[1] * 255),
        static_cast<int>(color[2] * 255),
        255
    );
    ImU32 border_color = IM_COL32(100, 100, 100, 255);
    
    // Background (1 draw call)
    draw_list->AddRectFilled(
        canvas_pos,
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        bg_color
    );
    
    // OPTIMIZACIÓN: PathLineTo en vez de múltiples AddLine (1 draw call en vez de N)
    if (data.size() > 1) {
        float step = canvas_size.x / static_cast<float>(data.size());
        
        draw_list->PathClear();
        for (size_t i = 0; i < data.size(); i++) {
            float x = canvas_pos.x + i * step;
            float y = canvas_pos.y + canvas_size.y * (1.0f - data[i]);
            draw_list->PathLineTo(ImVec2(x, y));
        }
        draw_list->PathStroke(line_color, 0, 2.0f);
    }
    
    // Border (1 draw call)
    draw_list->AddRect(
        canvas_pos,
        ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
        border_color
    );
}

} // namespace ui
