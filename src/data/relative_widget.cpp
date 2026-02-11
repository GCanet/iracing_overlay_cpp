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
    auto drivers = relative->getRelative(4, 4);
    int numDrivers = (int)drivers.size();

    float rowH    = ImGui::GetTextLineHeightWithSpacing();
    float headerH = rowH + 2.0f;
    float footerH = rowH + 2.0f;
    float tableH  = rowH * std::max(numDrivers, 1);
    float padY    = 4.0f;
    float totalH  = headerH + tableH + footerH + padY * 2.0f;
    float totalW  = 420.0f * m_scale;

    ImGui::SetNextWindowSize(ImVec2(totalW, totalH), ImGuiCond_Always);
    if (config.posX < 0 || config.posY < 0) { config.posX = 20.0f; config.posY = 200.0f; }
    ImGui::SetNextWindowPos(ImVec2(config.posX, config.posY), ImGuiCond_Once);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.12f, config.alpha));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.25f, 0.25f, 0.30f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,   ImVec2(6, padY));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding,     ImVec2(3, 1));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;
    if (editMode) {
        // allow move + resize in edit mode
    } else {
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoResize;
    }

    ImGui::Begin("##RELATIVE", nullptr, flags);
    ImGui::SetWindowFontScale(m_scale);

    ImVec2 pos = ImGui::GetWindowPos();
    config.posX  = pos.x;
    config.posY  = pos.y;
    config.width = ImGui::GetWindowSize().x;
    config.height = ImGui::GetWindowSize().y;

    renderHeader(relative);
    ImGui::Separator();

    char buffer[128];

    // Columns: Pos | Brand | Flag+#Name | SR | iR | Last | Gap
    if (ImGui::BeginTable("RT", 7,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {

        ImGui::TableSetupColumn("Pos",    ImGuiTableColumnFlags_WidthFixed, 22.0f * m_scale);
        ImGui::TableSetupColumn("Brand",  ImGuiTableColumnFlags_WidthFixed, 18.0f * m_scale);
        ImGui::TableSetupColumn("Driver", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("SR",     ImGuiTableColumnFlags_WidthFixed, 46.0f * m_scale);
        ImGui::TableSetupColumn("iR",     ImGuiTableColumnFlags_WidthFixed, 60.0f * m_scale);
        ImGui::TableSetupColumn("Last",   ImGuiTableColumnFlags_WidthFixed, 52.0f * m_scale);
        ImGui::TableSetupColumn("Gap",    ImGuiTableColumnFlags_WidthFixed, 44.0f * m_scale);

        for (const auto& d : drivers)
            renderDriverRow(d, d.isPlayer, buffer);

        ImGui::EndTable();
    }

    ImGui::Separator();
    renderFooter(relative);

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void RelativeWidget::renderHeader(iracing::RelativeCalculator* relative) {
    std::string series = relative->getSeriesName();
    if (series.empty() || series == "Unknown Series") series = "Practice Session";
    std::string lap = relative->getLapInfo();
    int sof = relative->getSOF();

    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.9f, 1.0f), "%s", series.c_str());
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "|");
    ImGui::SameLine();
    ImGui::Text("%s", lap.c_str());
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "|");
    ImGui::SameLine();
    ImGui::Text("SOF: %d", sof);
}

void RelativeWidget::renderFooter(iracing::RelativeCalculator* relative) {
    char buf[64];
    int inc = relative->getPlayerIncidents();
    float last = relative->getPlayerLastLap();
    float best = relative->getPlayerBestLap();

    ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "Inc: %d", inc);
    ImGui::SameLine(0, 12);

    if (last > 0.0f) {
        int m = (int)(last / 60.0f); float s = last - m * 60.0f;
        if (m > 0) snprintf(buf, 64, "%d:%05.2f", m, s); else snprintf(buf, 64, "%.2f", s);
    } else { snprintf(buf, 64, "---"); }
    ImGui::Text("Last: %s", buf);
    ImGui::SameLine(0, 12);

    if (best > 0.0f) {
        int m = (int)(best / 60.0f); float s = best - m * 60.0f;
        if (m > 0) snprintf(buf, 64, "%d:%05.2f", m, s); else snprintf(buf, 64, "%.2f", s);
    } else { snprintf(buf, 64, "---"); }
    ImGui::TextColored(ImVec4(0.6f, 0.3f, 0.9f, 1.0f), "Best: %s", buf);
}

void RelativeWidget::renderDriverRow(const iracing::Driver& driver, bool isPlayer, char* buffer) {
    ImGui::TableNextRow();
    float rowH = ImGui::GetTextLineHeight();

    if (isPlayer)
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 215, 0, 35));

    // Col: Position
    ImGui::TableNextColumn();
    if (driver.isOnPit)
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "PIT");
    else
        ImGui::Text("P%d", driver.relativePosition);

    // Col: Brand (placeholder — will show logo when assets available)
    ImGui::TableNextColumn();
    // Reserve space for brand logo (18x18 area)
    ImGui::Dummy(ImVec2(16.0f * m_scale, rowH));

    // Col: Flag + #Number + Name
    ImGui::TableNextColumn();
    {
        // Club/country flag as text abbreviation
        const char* flag = getClubFlag(driver.countryCode);
        if (flag[0] != '\0') {
            ImGui::TextColored(ImVec4(0.6f, 0.7f, 0.8f, 1.0f), "%s", flag);
            ImGui::SameLine(0, 3);
        }
        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.3f, 1.0f), "#%s", driver.carNumber.c_str());
        ImGui::SameLine(0, 4);
        if (isPlayer)
            ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.0f, 1.0f), "%s", driver.driverName.c_str());
        else
            ImGui::Text("%s", driver.driverName.c_str());
    }

    // Col: Safety Rating — styled like reference image: "3.9 C" with colored background pill
    ImGui::TableNextColumn();
    {
        float r, g, b;
        getSafetyRatingColor(driver.safetyRating, r, g, b);
        const char* letter = getSafetyRatingLetter(driver.safetyRating);

        // SR value
        float srDisplay = driver.safetyRating;
        // Clamp display to one decimal
        snprintf(buffer, 128, "%.1f", srDisplay);
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "%s", buffer);
        ImGui::SameLine(0, 2);

        // License letter in colored box
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 cp = ImGui::GetCursorScreenPos();
        float boxW = ImGui::CalcTextSize("A").x + 4.0f;
        float boxH = rowH;
        ImU32 bgCol = IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), 200);
        dl->AddRectFilled(ImVec2(cp.x, cp.y), ImVec2(cp.x + boxW, cp.y + boxH), bgCol, 2.0f);
        dl->AddText(ImVec2(cp.x + 2.0f, cp.y), IM_COL32(0, 0, 0, 255), letter);
        ImGui::Dummy(ImVec2(boxW, boxH));
    }

    // Col: iRating — styled like reference: "2.2k" then colored delta
    ImGui::TableNextColumn();
    {
        if (driver.iRating >= 1000)
            snprintf(buffer, 128, "%.1fk", driver.iRating / 1000.0f);
        else
            snprintf(buffer, 128, "%d", driver.iRating);

        ImGui::Text("%s", buffer);

        int delta = driver.iRatingProjection;
        if (delta != 0) {
            ImGui::SameLine(0, 2);
            char dbuf[32];
            if (delta > 0) {
                snprintf(dbuf, 32, "+%d", delta);
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s", dbuf);
            } else {
                snprintf(dbuf, 32, "%d", delta);
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", dbuf);
            }
        }
    }

    // Col: Last lap
    ImGui::TableNextColumn();
    formatTime(driver.lastLapTime, buffer);
    ImGui::Text("%s", buffer);

    // Col: Gap
    ImGui::TableNextColumn();
    formatGap(driver.gapToPlayer, buffer);
    if (driver.gapToPlayer > 0)
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "%s", buffer);
    else if (driver.gapToPlayer < 0)
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", buffer);
    else
        ImGui::Text("%s", buffer);
}

void RelativeWidget::formatGap(float gap, char* buffer) {
    if (std::abs(gap) < 0.01f) { snprintf(buffer, 128, "---"); return; }
    float a = std::abs(gap);
    if (gap > 0) {
        if (a >= 1.0f) snprintf(buffer, 128, "+%.0fL", a); else snprintf(buffer, 128, "+%.1fs", a);
    } else {
        if (a >= 1.0f) snprintf(buffer, 128, "-%.0fL", a); else snprintf(buffer, 128, "-%.1fs", a);
    }
}

void RelativeWidget::formatTime(float seconds, char* buffer) {
    if (seconds < 0.0f) { snprintf(buffer, 128, "---"); return; }
    int m = (int)(seconds / 60.0f);
    float s = seconds - m * 60.0f;
    if (m > 0) snprintf(buffer, 128, "%d:%05.2f", m, s);
    else snprintf(buffer, 128, "%.2f", s);
}

const char* RelativeWidget::getSafetyRatingLetter(float sr) {
    if (sr < 1.0f) return "R";
    if (sr < 2.0f) return "D";
    if (sr < 3.0f) return "C";
    if (sr < 4.0f) return "B";
    return "A";
}

void RelativeWidget::getSafetyRatingColor(float sr, float& r, float& g, float& b) {
    if (sr < 1.0f)      { r = 0.9f; g = 0.2f; b = 0.2f; }
    else if (sr < 2.0f) { r = 0.9f; g = 0.5f; b = 0.1f; }
    else if (sr < 3.0f) { r = 0.9f; g = 0.9f; b = 0.2f; }
    else if (sr < 4.0f) { r = 0.2f; g = 0.8f; b = 0.2f; }
    else                { r = 0.3f; g = 0.5f; b = 0.9f; }
}

const char* RelativeWidget::getClubFlag(const std::string& club) {
    // iRacing ClubName → short country code for display
    // This maps common iRacing club names to 2-letter abbreviations
    if (club.empty()) return "";
    if (club.find("Spain") != std::string::npos || club.find("Iberia") != std::string::npos) return "ES";
    if (club.find("Netherlands") != std::string::npos || club.find("Benelux") != std::string::npos) return "NL";
    if (club.find("DE-AT-CH") != std::string::npos || club.find("Germany") != std::string::npos) return "DE";
    if (club.find("France") != std::string::npos) return "FR";
    if (club.find("Italy") != std::string::npos) return "IT";
    if (club.find("UK") != std::string::npos || club.find("Ireland") != std::string::npos) return "GB";
    if (club.find("Scandinavia") != std::string::npos || club.find("Nordic") != std::string::npos) return "SE";
    if (club.find("Finland") != std::string::npos) return "FI";
    if (club.find("Central-Eastern") != std::string::npos) return "PL";
    if (club.find("Portugal") != std::string::npos) return "PT";
    if (club.find("Brazil") != std::string::npos) return "BR";
    if (club.find("Australia") != std::string::npos || club.find("NZ") != std::string::npos) return "AU";
    if (club.find("Japan") != std::string::npos) return "JP";
    // North American clubs
    if (club.find("New York") != std::string::npos) return "US";
    if (club.find("Georgia") != std::string::npos) return "US";
    if (club.find("Texas") != std::string::npos) return "US";
    if (club.find("California") != std::string::npos) return "US";
    if (club.find("Michigan") != std::string::npos) return "US";
    if (club.find("Carolina") != std::string::npos) return "US";
    if (club.find("Florida") != std::string::npos) return "US";
    if (club.find("Ohio") != std::string::npos) return "US";
    if (club.find("Indiana") != std::string::npos) return "US";
    if (club.find("Mid-South") != std::string::npos) return "US";
    if (club.find("Northwest") != std::string::npos) return "US";
    if (club.find("Plains") != std::string::npos) return "US";
    if (club.find("Canada") != std::string::npos) return "CA";
    if (club.find("Connecticut") != std::string::npos) return "US";
    if (club.find("Illinois") != std::string::npos) return "US";
    if (club.find("Pennsylvania") != std::string::npos) return "US";
    if (club.find("Virginia") != std::string::npos) return "US";
    if (club.find("New England") != std::string::npos) return "US";
    if (club.find("West") != std::string::npos) return "US";
    if (club.find("Atlantic") != std::string::npos) return "US";
    if (club.find("Argentina") != std::string::npos) return "AR";
    if (club.find("Mexico") != std::string::npos) return "MX";
    if (club.find("South America") != std::string::npos) return "BR";
    return "";
}

} // namespace ui
