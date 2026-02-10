#include "ui/relative_widget.h"
#include "data/relative_calc.h"
#include "utils/config.h"
#include "imgui.h"
#include <sstream>
#include <iomanip>
#include <cmath>

namespace ui {

// Fixed dimensions (ajustables vía config en futuro)
static constexpr float ROW_HEIGHT = 28.0f;
static constexpr int MAX_VISIBLE_DRIVERS = 8;
static constexpr float HEADER_HEIGHT = 24.0f;
static constexpr float SEPARATOR_HEIGHT = 4.0f;
static constexpr float WINDOW_WIDTH_DEFAULT = 520.0f;
static constexpr float WINDOW_HEIGHT_DEFAULT = HEADER_HEIGHT + SEPARATOR_HEIGHT + (MAX_VISIBLE_DRIVERS * ROW_HEIGHT) + 20.0f;

RelativeWidget::RelativeWidget()
    : m_scale(1.0f)  // Escala inicial (se puede ajustar desde menú contextual)
{
}

void RelativeWidget::render(iracing::RelativeCalculator* relative, bool editMode) {
    if (!relative) return;

    // Obtener relative (4 delante / 4 detrás)
    auto drivers = relative->getRelative(4, 4);

    // Limitar a max visible
    if (drivers.size() > MAX_VISIBLE_DRIVERS) {
        drivers.resize(MAX_VISIBLE_DRIVERS);
    }

    // Config
    auto& config = utils::Config::getRelativeConfig();

    // Aplicar escala (desde menú contextual o config)
    ImGui::SetWindowFontScale(m_scale);

    // Tamaño fijo (pero escalado)
    float scaledWidth = WINDOW_WIDTH_DEFAULT * m_scale;
    float scaledHeight = WINDOW_HEIGHT_DEFAULT * m_scale;
    ImGui::SetNextWindowSize(ImVec2(scaledWidth, scaledHeight), ImGuiCond_Always);

    // Posición desde config
    ImGui::SetNextWindowPos(ImVec2(config.posX, config.posY), ImGuiCond_Once);

    // Fondo con alpha desde config
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, config.alpha));

    // Flags base
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize;

    // En modo locked → no mover
    if (!editMode) {
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
    }

    ImGui::Begin("##RELATIVE", nullptr, flags);

    // Guardar posición y tamaño actual en config (actualiza al arrastrar/redimensionar)
    ImVec2 pos = ImGui::GetWindowPos();
    config.posX = pos.x;
    config.posY = pos.y;
    config.width = ImGui::GetWindowSize().x;
    config.height = ImGui::GetWindowSize().y;

    // Header
    renderHeader(relative);
    ImGui::Separator();

    // Buffer para formateo
    char buffer[128];

    // Filas de pilotos
    for (const auto& driver : drivers) {
        renderDriverRow(driver, driver.isPlayer, buffer);
    }

    ImGui::End();
    ImGui::PopStyleColor();

    // Menú contextual (solo visible en editMode)
    if (editMode && ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        ImGui::OpenPopup("RelativeContextMenu");
    }

    if (ImGui::BeginPopup("RelativeContextMenu")) {
        if (ImGui::MenuItem("Lock (salir de edit mode)")) {
            // Aquí necesitas notificar al OverlayWindow que ponga editMode = false
            // Opción 1: variable global/extern bool& g_editMode (simple para prototipo)
            // Opción 2: callback o puntero a OverlayWindow (mejor diseño)
            // Por ahora, asumimos que tienes acceso a una función global o miembro
            // toggleEditMode();  // → implementa esto en overlay_window
        }

        ImGui::Separator();

        // Transparencia global (afecta toda la overlay)
        float& globalAlpha = config.alpha;  // o pasa referencia desde OverlayWindow
        if (ImGui::SliderFloat("Transparency", &globalAlpha, 0.10f, 1.00f, "%.2f")) {
            // Aquí llama a OverlayWindow::applyWindowAttributes() si tienes acceso
            // o guarda y aplica en el próximo frame
        }

        // Escala del widget
        if (ImGui::SliderFloat("Size Scale", &m_scale, 0.5f, 2.0f, "%.2f")) {
            // Se aplica inmediatamente vía SetWindowFontScale
        }

        ImGui::EndPopup();
    }

    // Reset font scale al salir (por si otros widgets)
    ImGui::SetWindowFontScale(1.0f);
}

void RelativeWidget::renderHeader(iracing::RelativeCalculator* relative) {
    std::string series = relative->getSeriesName();
    std::string laps = relative->getLapInfo();
    int sof = relative->getSOF();

    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "%s", series.c_str());
    ImGui::SameLine(220);
    ImGui::Text("%s", laps.c_str());
    ImGui::SameLine(380);
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "SOF: %d", sof);
}

void RelativeWidget::renderDriverRow(const iracing::Driver& driver, bool isPlayer, char* buffer) {
    // Fondo para jugador
    if (isPlayer) {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.6f, 0.0f, 0.65f));
        ImGui::Selectable("##playerrow", true, 0, ImVec2(0, ROW_HEIGHT));
        ImGui::PopStyleColor();
        ImGui::SameLine(0.0f, 0.0f);
    }

    ImGui::SetCursorPosX(10);

    // Posición (usamos relativePosition para orden real)
    ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%2d", driver.relativePosition);

    // Car # + Nombre (gris si en pits)
    ImGui::SameLine(50);
    if (driver.isOnPit) {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "#%s %s (PIT)", 
                           driver.carNumber.c_str(), driver.driverName.c_str());
    } else {
        ImGui::Text("#%s %s", driver.carNumber.c_str(), driver.driverName.c_str());
    }

    // Safety Rating + letra
    ImGui::SameLine(220);
    float r, g, b;
    getSafetyRatingColor(driver.safetyRating, r, g, b);
    const char* letter = getSafetyRatingLetter(driver.safetyRating);
    ImGui::TextColored(ImVec4(r, g, b, 1.0f), "%.1f%s", driver.safetyRating, letter);

    // iRating
    ImGui::SameLine(280);
    ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "%d", driver.iRating);

    // Proyección iRating
    ImGui::SameLine(330);
    int proj = driver.iRatingProjection;
    if (proj > 0) ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "+%d", proj);
    else if (proj < 0) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%d", proj);
    else ImGui::Text("0");

    // Placeholder logo (puedes cargar texturas después)
    ImGui::SameLine(380);
    ImGui::Dummy(ImVec2(24, 24));

    // Última vuelta
    ImGui::SameLine(420);
    if (driver.lastLapTime > 0.0f) {
        formatTime(driver.lastLapTime, buffer);
        ImGui::Text("%s", buffer);
    } else {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "---");
    }

    // Gap al jugador
    ImGui::SameLine(470);
    if (driver.relativePosition == 1) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.4f, 1.0f), "LEADER");
    } else if (std::abs(driver.gapToPlayer) < 0.01f) {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "EVEN");
    } else {
        ImVec4 color = (driver.gapToPlayer < 0.0f) ? ImVec4(0.4f, 1.0f, 0.4f, 1.0f)
                                                   : ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
        formatGap(driver.gapToPlayer, buffer);
        ImGui::TextColored(color, "%s", buffer);
    }
}

void RelativeWidget::formatGap(float gap, char* buffer) {
    if (gap < -0.01f) {
        snprintf(buffer, 128, "+%.1fs", std::abs(gap));
    } else if (gap > 0.01f) {
        snprintf(buffer, 128, "-%.1fs", gap);
    } else {
        snprintf(buffer, 128, "±0.0s");
    }
}

void RelativeWidget::formatTime(float seconds, char* buffer) {
    if (seconds <= 0.0f) {
        snprintf(buffer, 128, "---");
        return;
    }
    int mins = static_cast<int>(seconds) / 60;
    float secs = seconds - mins * 60.0f;
    if (mins > 0) {
        snprintf(buffer, 128, "%d:%05.3f", mins, secs);
    } else {
        snprintf(buffer, 128, "%05.3f", secs);
    }
}

// Resto sin cambios (getSafetyRatingLetter, getSafetyRatingColor)
const char* RelativeWidget::getSafetyRatingLetter(float sr) {
    if (sr >= 4.0f) return "A";
    if (sr >= 3.0f) return "B";
    if (sr >= 2.0f) return "C";
    if (sr >= 1.0f) return "D";
    return "R";
}

void RelativeWidget::getSafetyRatingColor(float sr, float& r, float& g, float& b) {
    if (sr >= 4.0f)      { r=0.2f; g=0.6f; b=1.0f; }
    else if (sr >= 3.0f) { r=0.2f; g=1.0f; b=0.2f; }
    else if (sr >= 2.0f) { r=1.0f; g=1.0f; b=0.0f; }
    else if (sr >= 1.0f) { r=1.0f; g=0.6f; b=0.0f; }
    else                 { r=1.0f; g=0.2f; b=0.2f; }
}

} // namespace ui
