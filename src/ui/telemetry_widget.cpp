#include "ui/telemetry_widget.h"
#include "data/irsdk_manager.h"
#include "utils/config.h"
#include "imgui.h"
#include "ui/overlay_window.h"  // necesario para OverlayWindow

#include <deque>

namespace ui {

TelemetryWidget::TelemetryWidget(OverlayWindow* overlay)
    : m_overlay(overlay)
{
    m_throttleHistory.reserve(MAX_HISTORY);
    m_brakeHistory.reserve(MAX_HISTORY);
}

void TelemetryWidget::render(iracing::IRSDKManager* sdk, bool editMode) {
    if (!sdk || !sdk->isSessionActive()) return;

    float throttle = sdk->getFloat("Throttle", 0.0f);
    float brake    = sdk->getFloat("Brake", 0.0f);

    updateHistory(throttle, brake);

    auto& config = utils::Config::getTelemetryConfig();

    ImGui::SetWindowFontScale(m_scale);

    float scaledWidth  = GRAPH_WIDTH  * m_scale;
    float scaledHeight = GRAPH_HEIGHT * m_scale;
    ImGui::SetNextWindowSize(ImVec2(scaledWidth, scaledHeight), ImGuiCond_Always);

    ImGui::SetNextWindowPos(ImVec2(config.posX, config.posY), ImGuiCond_Once);

    ImGui::PushStyleColor(ImGuiCol_WindowBg,   ImVec4(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Border,     ImVec4(0,0,0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoBackground;

    if (!editMode) {
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
    }

    ImGui::Begin("##TELEMETRY", nullptr, flags);

    ImVec2 pos = ImGui::GetWindowPos();
    config.posX = pos.x;
    config.posY = pos.y;
    config.width = ImGui::GetWindowSize().x;
    config.height = ImGui::GetWindowSize().y;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size(scaledWidth, scaledHeight);

    if (!m_throttleHistory.empty() && !m_brakeHistory.empty()) {
        ImU32 bg_color       = IM_COL32(20, 20, 20, 140);
        ImU32 throttle_color = IM_COL32(100, 255, 100, 220);
        ImU32 brake_color    = IM_COL32(255, 100, 100, 220);
        ImU32 border_color   = IM_COL32(80, 80, 80, 180);

        draw_list->AddRectFilled(canvas_pos,
                                 ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
                                 bg_color);

        float step = canvas_size.x / static_cast<float>(MAX_HISTORY - 1);

        // Throttle
        draw_list->PathClear();
        for (size_t i = 0; i < m_throttleHistory.size(); ++i) {
            float x = canvas_pos.x + canvas_size.x - (i * step);
            float y = canvas_pos.y + canvas_size.y * (1.0f - m_throttleHistory[m_throttleHistory.size() - 1 - i]);
            draw_list->PathLineTo(ImVec2(x, y));
        }
        draw_list->PathStroke(throttle_color, false, 2.5f);

        // Brake
        draw_list->PathClear();
        for (size_t i = 0; i < m_brakeHistory.size(); ++i) {
            float x = canvas_pos.x + canvas_size.x - (i * step);
            float y = canvas_pos.y + canvas_size.y * (1.0f - m_brakeHistory[m_brakeHistory.size() - 1 - i]);
            draw_list->PathLineTo(ImVec2(x, y));
        }
        draw_list->PathStroke(brake_color, false, 2.5f);

        draw_list->AddRect(canvas_pos,
                           ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
                           border_color, 0.0f, 0, 1.0f);
    }

    ImGui::Dummy(ImVec2(scaledWidth, scaledHeight));

    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    ImGui::SetWindowFontScale(1.0f);

    // Menú contextual
    if (editMode && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("TelemetryCtxMenu");
    }

    if (ImGui::BeginPopup("TelemetryCtxMenu")) {
        if (m_overlay) {
            if (ImGui::MenuItem("Lock (exit edit mode)")) {
                m_overlay->getEditModeRef() = false;
                m_overlay->applyWindowAttributes();
            }

            ImGui::Separator();

            float& alpha = m_overlay->getGlobalAlphaRef();
            if (ImGui::SliderFloat("Transparency", &alpha, 0.10f, 1.00f, "%.2f")) {
                m_overlay->applyWindowAttributes();
            }

            if (ImGui::SliderFloat("Widget Scale", &m_scale, 0.5f, 2.0f, "%.2f")) {
                // se aplica automáticamente en el próximo render
            }
        } else {
            ImGui::TextDisabled("Overlay not connected");
        }

        ImGui::EndPopup();
    }
}

void TelemetryWidget::updateHistory(float throttle, float brake) {
    m_throttleHistory.push_back(throttle);
    m_brakeHistory.push_back(brake);

    if (m_throttleHistory.size() > MAX_HISTORY) m_throttleHistory.pop_front();
    if (m_brakeHistory.size() > MAX_HISTORY) m_brakeHistory.pop_front();
}

} // namespace ui
