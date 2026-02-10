#include "ui/telemetry_widget.h"
#include "data/irsdk_manager.h"
#include "utils/config.h"
#include "imgui.h"
#include "ui/overlay_window.h"  // ← para acceso a OverlayWindow (si usas puntero o extern)

#include <deque>

namespace ui {

static constexpr float GRAPH_WIDTH = 400.0f;
static constexpr float GRAPH_HEIGHT = 120.0f;
static constexpr size_t MAX_HISTORY = 300;  // ~5 segundos a 60 Hz

TelemetryWidget::TelemetryWidget()
    : m_scale(1.0f)
{
    m_throttleHistory.reserve(MAX_HISTORY);
    m_brakeHistory.reserve(MAX_HISTORY);
}

void TelemetryWidget::render(iracing::IRSDKManager* sdk, bool editMode) {
    if (!sdk || !sdk->isSessionActive()) return;

    // Leer valores solo si hay sesión activa
    float throttle = sdk->getFloat("Throttle", 0.0f);
    float brake = sdk->getFloat("Brake", 0.0f);

    // Actualizar historia
    updateHistory(throttle, brake);

    // Config
    auto& config = utils::Config::getTelemetryConfig();

    // Aplicar escala
    ImGui::SetWindowFontScale(m_scale);

    // Tamaño escalado
    float scaledWidth = GRAPH_WIDTH * m_scale;
    float scaledHeight = GRAPH_HEIGHT * m_scale;
    ImGui::SetNextWindowSize(ImVec2(scaledWidth, scaledHeight), ImGuiCond_Always);

    // Posición desde config
    ImGui::SetNextWindowPos(ImVec2(config.posX, config.posY), ImGuiCond_Once);

    // Fondo transparente total
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    // Flags
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoBackground;

    if (!editMode) {
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
    }

    ImGui::Begin("##TELEMETRY", nullptr, flags);

    // Guardar posición
    ImVec2 pos = ImGui::GetWindowPos();
    config.posX = pos.x;
    config.posY = pos.y;
    config.width = ImGui::GetWindowSize().x;
    config.height = ImGui::GetWindowSize().y;

    // Dibujar gráfico
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size(scaledWidth, scaledHeight);

    if (!m_throttleHistory.empty() && !m_brakeHistory.empty()) {
        ImU32 bg_color       = IM_COL32(20, 20, 20, 140);     // fondo semi-transparente
        ImU32 throttle_color = IM_COL32(100, 255, 100, 220);  // verde claro
        ImU32 brake_color    = IM_COL32(255, 100, 100, 220);  // rojo claro
        ImU32 border_color   = IM_COL32(80, 80, 80, 180);

        // Fondo del gráfico
        draw_list->AddRectFilled(canvas_pos,
                                 ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
                                 bg_color);

        float step = canvas_size.x / static_cast<float>(MAX_HISTORY - 1);

        // Throttle (verde)
        draw_list->PathClear();
        for (size_t i = 0; i < m_throttleHistory.size(); ++i) {
            float x = canvas_pos.x + canvas_size.x - (i * step);
            float y = canvas_pos.y + canvas_size.y * (1.0f - m_throttleHistory[m_throttleHistory.size() - 1 - i]);
            draw_list->PathLineTo(ImVec2(x, y));
        }
        draw_list->PathStroke(throttle_color, false, 2.5f);

        // Brake (rojo)
        draw_list->PathClear();
        for (size_t i = 0; i < m_brakeHistory.size(); ++i) {
            float x = canvas_pos.x + canvas_size.x - (i * step);
            float y = canvas_pos.y + canvas_size.y * (1.0f - m_brakeHistory[m_brakeHistory.size() - 1 - i]);
            draw_list->PathLineTo(ImVec2(x, y));
        }
        draw_list->PathStroke(brake_color, false, 2.5f);

        // Borde fino
        draw_list->AddRect(canvas_pos,
                           ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + canvas_size.y),
                           border_color, 0.0f, 0, 1.0f);
    }

    // Espacio reservado
    ImGui::Dummy(ImVec2(scaledWidth, scaledHeight));

    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    // Reset escala
    ImGui::SetWindowFontScale(1.0f);

    // Menú contextual (solo en editMode)
    if (editMode && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("TelemetryContextMenu");
    }

    if (ImGui::BeginPopup("TelemetryContextMenu")) {
        // Asumiendo que tienes acceso al overlay (ej: extern OverlayWindow g_overlay;)
        // O pasa OverlayWindow* al constructor del widget

        if (ImGui::MenuItem("Lock (exit edit mode)")) {
            // g_overlay.getEditModeRef() = false;
            // g_overlay.applyWindowAttributes();
        }

        ImGui::Separator();

        // float& alpha = g_overlay.getGlobalAlphaRef();
        // ImGui::SliderFloat("Transparency", &alpha, 0.1f, 1.0f, "%.2f");

        if (ImGui::SliderFloat("Widget Scale", &m_scale, 0.5f, 2.0f, "%.2f")) {
            // se aplica inmediatamente
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
