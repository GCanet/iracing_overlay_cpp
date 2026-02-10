#include "ui/relative_widget.h"
#include "data/relative_calc.h"
#include "utils/config.h"
#include "imgui.h"
#include "ui/overlay_window.h"
#include <cstdio>
#include <algorithm>

namespace ui {

RelativeWidget::RelativeWidget(OverlayWindow* overlay)
    : m_overlay(overlay)
{
}

void RelativeWidget::render(iracing::RelativeCalculator* relative, bool editMode) {
    if (!relative) return;

    auto& config = utils::Config::getRelativeConfig();

    ImGui::SetWindowFontScale(m_scale);

    // Tamaño mínimo razonable
    float minWidth = 400.0f * m_scale;
    float minHeight = 300.0f * m_scale;

    if (config.width < 0) config.width = minWidth;
    if (config.height < 0) config.height = minHeight;

    ImGui::SetNextWindowSize(ImVec2(config.width, config.height), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(config.posX, config.posY), ImGuiCond_Once);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, config.alpha));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui:PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize;

    if (!editMode) {
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
    }

    ImGui::Begin("##RELATIVE", nullptr, flags);

    // Guardar posición actualizada
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

    // Obtener drivers relativos (4 adelante, 4 atrás)
    auto drivers = relative->getRelative(4, 4);

    // Tabla de drivers
    if (ImGui::BeginTable("RelativeTable", 7, 
                         ImGuiTableFlags_RowBg | 
                         ImGuiTableFlags_BordersInnerV)) {
        
        // Headers
        ImGui::TableSetupColumn("Pos", ImGuiTableColumnFlags_WidthFixed, 40);
        ImGui::TableSetupColumn("Car", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Driver", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("SR", ImGuiTableColumnFlags_WidthFixed, 35);
        ImGui::TableSetupColumn("iR", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Last", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Gap", ImGuiTableColumnFlags_WidthFixed, 70);
        
        // Renderizar cada driver
        for (const auto& driver : drivers) {
            renderDriverRow(driver, driver.isPlayer, buffer);
        }

        ImGui::EndTable();
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void RelativeWidget::renderHeader(iracing::RelativeCalculator* relative) {
    ImGui::Text("%s", relative->getSeriesName().c_str());
    ImGui::SameLine();
    ImGui::Text(" | ");
    ImGui::SameLine();
    ImGui::Text("%s", relative->getLapInfo().c_str());
    ImGui::SameLine();
    ImGui::Text(" | SOF: %d", relative->getSOF());
}

void RelativeWidget::renderDriverRow(const iracing::Driver& driver, bool isPlayer, char* buffer) {
    ImGui::TableNextRow();

    // Colorear la fila del jugador
    if (isPlayer) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(0, 100, 0, 100));
    }

    // Grisear si está en pits
    if (driver.isOnPit) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    }

    // Column: Position
    ImGui::TableNextColumn();
    ImGui::Text("P%d", driver.relativePosition);

    // Column: Car Number
    ImGui::TableNextColumn();
    ImGui::Text("#%s", driver.carNumber.c_str());

    // Column: Driver Name
    ImGui::TableNextColumn();
    ImGui::Text("%s", driver.driverName.c_str());

    // Column: Safety Rating
    ImGui::TableNextColumn();
    const char* srLetter = getSafetyRatingLetter(driver.safetyRating);
    float srR, srG, srB;
    getSafetyRatingColor(driver.safetyRating, srR, srG, srB);
    ImGui::TextColored(ImVec4(srR, srG, srB, 1.0f), "%s", srLetter);

    // Column: iRating
    ImGui::TableNextColumn();
    ImGui::Text("%d", driver.iRating);
    
    // iRating projection (optional, small text below)
    if (driver.iRatingProjection != 0) {
        ImGui::SameLine();
        if (driver.iRatingProjection > 0) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "+%d", driver.iRatingProjection);
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%d", driver.iRatingProjection);
        }
    }

    // Column: Last Lap Time
    ImGui::TableNextColumn();
    if (driver.lastLapTime > 0.0f) {
        formatTime(driver.lastLapTime, buffer);
        ImGui::Text("%s", buffer);
    } else {
        ImGui::Text("---");
    }

    // Column: Gap
    ImGui::TableNextColumn();
    formatGap(driver.gapToPlayer, buffer);
    ImGui::Text("%s", buffer);

    if (driver.isOnPit) {
        ImGui::PopStyleColor();
    }
}

void RelativeWidget::formatGap(float gap, char* buffer) {
    if (std::abs(gap) < 0.01f) {
        snprintf(buffer, 128, "---");
    } else if (gap > 0) {
        // Piloto detrás del jugador
        if (gap >= 1.0f) {
            snprintf(buffer, 128, "+%.0f L", gap);
        } else {
            snprintf(buffer, 128, "+%.1fs", gap);
        }
    } else {
        // Piloto delante del jugador
        float absGap = std::abs(gap);
        if (absGap >= 1.0f) {
            snprintf(buffer, 128, "-%.0f L", absGap);
        } else {
            snprintf(buffer, 128, "-%.1fs", absGap);
        }
    }
}

void RelativeWidget::formatTime(float seconds, char* buffer) {
    if (seconds < 0.0f) {
        snprintf(buffer, 128, "---");
        return;
    }

    int minutes = static_cast<int>(seconds / 60.0f);
    float secs = seconds - (minutes * 60.0f);

    if (minutes > 0) {
        snprintf(buffer, 128, "%d:%05.2f", minutes, secs);
    } else {
        snprintf(buffer, 128, "%.2fs", secs);
    }
}

const char* RelativeWidget::getSafetyRatingLetter(float sr) {
    if (sr < 1.0f) return "R";
    if (sr < 2.0f) return "D";
    if (sr < 3.0f) return "C";
    if (sr < 4.0f) return "B";
    return "A";
}

void RelativeWidget::getSafetyRatingColor(float sr, float& r, float& g, float& b) {
    if (sr < 1.0f) {
        // R - Red
        r = 1.0f; g = 0.0f; b = 0.0f;
    } else if (sr < 2.0f) {
        // D - Orange
        r = 1.0f; g = 0.6f; b = 0.0f;
    } else if (sr < 3.0f) {
        // C - Yellow
        r = 1.0f; g = 1.0f; b = 0.0f;
    } else if (sr < 4.0f) {
        // B - Green
        r = 0.0f; g = 1.0f; b = 0.0f;
    } else {
        // A - Blue
        r = 0.0f; g = 0.5f; b = 1.0f;
    }
}

} // namespace ui
