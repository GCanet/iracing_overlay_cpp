#include "ui/relative_widget.h"
#include "data/relative_calc.h"
#include "data/irsdk_manager.h"
#include "utils/config.h"
#include "imgui.h"
#include "ui/overlay_window.h"
#include <cstdio>
#include <algorithm>
#include <cmath>

namespace ui {

RelativeWidget::RelativeWidget(OverlayWindow* overlay)
    : m_overlay(overlay)
{
}

void RelativeWidget::render(iracing::RelativeCalculator* relative, bool editMode) {
    if (!relative) return;

    auto& config = utils::Config::getRelativeConfig();

    // FIX: Do NOT call SetWindowFontScale before Begin().
    // Calling it before Begin() touches ImGui's default "Debug" window,
    // making it visible as a ghost widget.

    // Reasonable minimum size
    float minWidth = 400.0f * m_scale;
    float minHeight = 300.0f * m_scale;

    if (config.width < 0) config.width = minWidth;
    if (config.height < 0) config.height = minHeight;

    ImGui::SetNextWindowSize(ImVec2(config.width, config.height), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(config.posX, config.posY), ImGuiCond_Once);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, config.alpha));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;

    if (!editMode) {
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
    }

    ImGui::Begin("##RELATIVE", nullptr, flags);

    // FIX: SetWindowFontScale AFTER Begin() so it targets our window, not Debug
    ImGui::SetWindowFontScale(m_scale);

    // Save updated position
    ImVec2 pos = ImGui::GetWindowPos();
    config.posX = pos.x;
    config.posY = pos.y;
    config.width = ImGui::GetWindowSize().x;
    config.height = ImGui::GetWindowSize().y;

    // Header
    renderHeader(relative);
    ImGui::Separator();

    // Format buffer
    char buffer[128];

    // Get relative drivers (4 ahead, 4 behind)
    auto drivers = relative->getRelative(4, 4);

    // Driver table
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
        
        // Render each driver
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

    // Highlight player row
    if (isPlayer) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 215, 0, 40));
    }

    // Column: Position
    ImGui::TableNextColumn();
    if (driver.isOnPit) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "PIT");
    } else {
        ImGui::Text("P%d", driver.relativePosition);
    }

    // Column: Car Number
    ImGui::TableNextColumn();
    ImGui::Text("#%s", driver.carNumber.c_str());

    // Column: Driver Name
    ImGui::TableNextColumn();
    if (isPlayer) {
        ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.0f, 1.0f), "%s", driver.driverName.c_str());
    } else {
        ImGui::Text("%s", driver.driverName.c_str());
    }

    // Column: Safety Rating
    ImGui::TableNextColumn();
    float r, g, b;
    getSafetyRatingColor(driver.safetyRating, r, g, b);
    float srDecimal = driver.safetyRating - static_cast<float>(static_cast<int>(driver.safetyRating));
    if (srDecimal < 0.0f) srDecimal += 1.0f;
    ImGui::TextColored(ImVec4(r, g, b, 1.0f), "%s%.2f", 
                      getSafetyRatingLetter(driver.safetyRating),
                      srDecimal);

    // Column: iRating
    ImGui::TableNextColumn();
    snprintf(buffer, 128, "%dk", driver.iRating / 1000);
    if (driver.iRatingProjection != 0) {
        int diff = driver.iRatingProjection - driver.iRating;
        if (diff > 0) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s +%d", buffer, diff);
        } else {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s %d", buffer, diff);
        }
    } else {
        ImGui::Text("%s", buffer);
    }

    // Column: Last Lap Time
    ImGui::TableNextColumn();
    formatTime(driver.lastLapTime, buffer);
    ImGui::Text("%s", buffer);

    // Column: Gap
    ImGui::TableNextColumn();
    formatGap(driver.gapToPlayer, buffer);
    if (driver.gapToPlayer > 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "%s", buffer);
    } else if (driver.gapToPlayer < 0) {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", buffer);
    } else {
        ImGui::Text("%s", buffer);
    }
}

void RelativeWidget::formatGap(float gap, char* buffer) {
    if (std::abs(gap) < 0.01f) {
        snprintf(buffer, 128, "---");
    } else if (gap > 0) {
        // Driver behind the player
        if (gap >= 1.0f) {
            snprintf(buffer, 128, "+%.0f L", gap);
        } else {
            snprintf(buffer, 128, "+%.1fs", gap);
        }
    } else {
        // Driver ahead of the player
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
