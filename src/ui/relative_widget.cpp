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

    // Get drivers FIRST so we can size the window to content
    auto drivers = relative->getRelative(4, 4);
    int numDrivers = static_cast<int>(drivers.size());

    // ── Calculate tight window size based on actual content ──
    float rowH    = ImGui::GetTextLineHeightWithSpacing();
    float headerH = rowH + 4.0f;  // header text + separator
    float tableH  = rowH * std::max(numDrivers, 1) + 4.0f;
    float padY    = 6.0f;  // top + bottom padding
    float totalH  = headerH + tableH + padY * 2.0f;
    float totalW  = 380.0f * m_scale;

    ImGui::SetNextWindowSize(ImVec2(totalW, totalH), ImGuiCond_Always);

    if (config.posX < 0 || config.posY < 0) {
        config.posX = 20.0f;
        config.posY = 200.0f;
    }
    ImGui::SetNextWindowPos(ImVec2(config.posX, config.posY), ImGuiCond_Once);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.12f, config.alpha));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(6, padY));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(3, 1));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize   |
                             ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;
    if (!editMode) {
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
    }

    ImGui::Begin("##RELATIVE", nullptr, flags);
    ImGui::SetWindowFontScale(m_scale);

    // Save position
    ImVec2 pos = ImGui::GetWindowPos();
    config.posX  = pos.x;
    config.posY  = pos.y;
    config.width = ImGui::GetWindowSize().x;
    config.height = ImGui::GetWindowSize().y;

    // ── Header (single line, compact) ───────────────────────
    renderHeader(relative);
    ImGui::Separator();

    // ── Driver table ────────────────────────────────────────
    // Columns: Pos | #Num Name | SR | iR | Last | Gap
    // We merge car number + name into one column to avoid the overlap bug
    char buffer[128];

    if (ImGui::BeginTable("RT", 6,
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_SizingFixedFit |
            ImGuiTableFlags_NoHostExtendX)) {

        ImGui::TableSetupColumn("Pos",   ImGuiTableColumnFlags_WidthFixed, 24.0f * m_scale);
        ImGui::TableSetupColumn("Driver",ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("SR",    ImGuiTableColumnFlags_WidthFixed, 38.0f * m_scale);
        ImGui::TableSetupColumn("iR",    ImGuiTableColumnFlags_WidthFixed, 38.0f * m_scale);
        ImGui::TableSetupColumn("Last",  ImGuiTableColumnFlags_WidthFixed, 56.0f * m_scale);
        ImGui::TableSetupColumn("Gap",   ImGuiTableColumnFlags_WidthFixed, 48.0f * m_scale);

        for (const auto& driver : drivers) {
            renderDriverRow(driver, driver.isPlayer, buffer);
        }

        ImGui::EndTable();
    }

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void RelativeWidget::renderHeader(iracing::RelativeCalculator* relative) {
    // Compact single-line header
    const char* series = relative->getSeriesName().c_str();
    std::string lapInfo = relative->getLapInfo();
    int sof = relative->getSOF();

    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.9f, 1.0f), "%s", series);
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "|");
    ImGui::SameLine();
    ImGui::Text("%s", lapInfo.c_str());
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "|");
    ImGui::SameLine();
    ImGui::Text("SOF: %d", sof);
}

void RelativeWidget::renderDriverRow(const iracing::Driver& driver, bool isPlayer, char* buffer) {
    ImGui::TableNextRow();

    // Highlight player row
    if (isPlayer) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 215, 0, 35));
    }

    // ── Col 1: Position (tight) ─────────────────────────────
    ImGui::TableNextColumn();
    if (driver.isOnPit) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "PIT");
    } else {
        ImGui::Text("P%d", driver.relativePosition);
    }

    // ── Col 2: #Number + Name (merged, no overlap) ──────────
    ImGui::TableNextColumn();
    // Car number in yellow, then name
    ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "#%s", driver.carNumber.c_str());
    ImGui::SameLine(0, 4);
    if (isPlayer) {
        ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.0f, 1.0f), "%s", driver.driverName.c_str());
    } else {
        ImGui::Text("%s", driver.driverName.c_str());
    }

    // ── Col 3: Safety Rating ────────────────────────────────
    ImGui::TableNextColumn();
    {
        float r, g, b;
        getSafetyRatingColor(driver.safetyRating, r, g, b);
        const char* letter = getSafetyRatingLetter(driver.safetyRating);
        // Show fractional part: e.g. "B2.49" → show "B2.5"
        snprintf(buffer, 128, "%s%.1f", letter, driver.safetyRating);
        ImGui::TextColored(ImVec4(r, g, b, 1.0f), "%s", buffer);
    }

    // ── Col 4: iRating ──────────────────────────────────────
    ImGui::TableNextColumn();
    {
        if (driver.iRating >= 1000) {
            snprintf(buffer, 128, "%.1fk", driver.iRating / 1000.0f);
        } else {
            snprintf(buffer, 128, "%d", driver.iRating);
        }
        if (driver.iRatingProjection != 0) {
            int diff = driver.iRatingProjection - driver.iRating;
            if (diff > 0) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", buffer);
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", buffer);
            }
        } else {
            ImGui::Text("%s", buffer);
        }
    }

    // ── Col 5: Last lap time ────────────────────────────────
    ImGui::TableNextColumn();
    formatTime(driver.lastLapTime, buffer);
    ImGui::Text("%s", buffer);

    // ── Col 6: Gap to player ────────────────────────────────
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
        if (gap >= 1.0f) {
            snprintf(buffer, 128, "+%.0fL", gap);
        } else {
            snprintf(buffer, 128, "+%.1fs", gap);
        }
    } else {
        float a = std::abs(gap);
        if (a >= 1.0f) {
            snprintf(buffer, 128, "-%.0fL", a);
        } else {
            snprintf(buffer, 128, "-%.1fs", a);
        }
    }
}

void RelativeWidget::formatTime(float seconds, char* buffer) {
    if (seconds < 0.0f) {
        snprintf(buffer, 128, "---");
        return;
    }
    int min = static_cast<int>(seconds / 60.0f);
    float sec = seconds - (min * 60.0f);
    if (min > 0) {
        snprintf(buffer, 128, "%d:%05.2f", min, sec);
    } else {
        snprintf(buffer, 128, "%.2f", sec);
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
    if (sr < 1.0f)      { r = 1.0f; g = 0.2f; b = 0.2f; }   // R – Red
    else if (sr < 2.0f) { r = 1.0f; g = 0.6f; b = 0.0f; }   // D – Orange
    else if (sr < 3.0f) { r = 1.0f; g = 1.0f; b = 0.0f; }   // C – Yellow
    else if (sr < 4.0f) { r = 0.2f; g = 1.0f; b = 0.2f; }   // B – Green
    else                { r = 0.3f; g = 0.6f; b = 1.0f; }   // A – Blue
}

} // namespace ui
