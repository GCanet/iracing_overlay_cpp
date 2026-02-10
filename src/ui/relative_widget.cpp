#include "ui/relative_widget.h"
#include "data/relative_calc.h"
#include "utils/config.h"
#include "imgui.h"
#include <sstream>
#include <iomanip>
#include <cmath>

namespace ui {

RelativeWidget::RelativeWidget() {
}

void RelativeWidget::render(iracing::RelativeCalculator* relative) {
    if (!relative) return;
    
    // Get drivers (4 ahead, 4 behind)
    auto drivers = relative->getRelative(4, 4);
    
    // Load config
    auto& config = utils::Config::getRelativeConfig();
    
    // Window position from config (or default)
    if (config.posX < 0 || config.posY < 0) {
        // First use - top right default
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 520, 20), ImGuiCond_FirstUseEver);
    } else {
        ImGui::SetNextWindowPos(ImVec2(config.posX, config.posY), ImGuiCond_Always);
    }
    
    // Window size from config
    if (config.width > 0 && config.height > 0) {
        ImGui::SetNextWindowSize(ImVec2(config.width, config.height), ImGuiCond_Always);
    } else {
        ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_FirstUseEver);
    }
    
    // Window alpha from config
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, config.alpha));
    
    ImGui::Begin("##RELATIVE", nullptr, 
        ImGuiWindowFlags_NoCollapse | 
        ImGuiWindowFlags_NoTitleBar);
    
    // Save position and size back to config
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 size = ImGui::GetWindowSize();
    config.posX = pos.x;
    config.posY = pos.y;
    config.width = size.x;
    config.height = size.y;
    
    // Header
    renderHeader(relative);
    ImGui::Separator();
    
    // Pre-allocate buffer for string formatting
    char buffer[256];
    
    // Driver rows
    for (const auto& driver : drivers) {
        renderDriverRow(driver, driver.isPlayer, buffer);
    }
    
    ImGui::End();
    ImGui::PopStyleColor();
}

void RelativeWidget::renderHeader(iracing::RelativeCalculator* relative) {
    std::string seriesName = relative->getSeriesName();
    std::string lapInfo = relative->getLapInfo();
    int sof = relative->getSOF();
    
    ImGui::Text("%s", seriesName.c_str());
    ImGui::SameLine(200);
    ImGui::Text("%s", lapInfo.c_str());
    ImGui::SameLine(350);
    ImGui::Text("SOF: %d", sof);
}

void RelativeWidget::renderDriverRow(const iracing::Driver& driver, bool isPlayer, char* buffer) {
    // Player background
    if (isPlayer) {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.7f, 0.0f, 0.6f));
        ImGui::Selectable("##player", true, 0, ImVec2(0, 28));
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 0);
        ImGui::SetCursorPosX(10);
    }
    
    // Position number
    ImGui::Text("%2d", driver.position);
    
    // Car number + Driver name
    ImGui::SameLine(35);
    if (driver.isOnPit) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "#%s %s", 
            driver.carNumber.c_str(), driver.driverName.c_str());
    } else {
        ImGui::Text("#%s %s", driver.carNumber.c_str(), driver.driverName.c_str());
    }
    
    // Safety Rating
    ImGui::SameLine(220);
    float r, g, b;
    getSafetyRatingColor(driver.safetyRating, r, g, b);
    const char* srLetter = getSafetyRatingLetter(driver.safetyRating);
    ImGui::TextColored(ImVec4(r, g, b, 1.0f), "%.1f%s", driver.safetyRating, srLetter);
    
    // iRating
    ImGui::SameLine(280);
    ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "%d", driver.iRating);
    
    // iRating projection
    ImGui::SameLine(330);
    int projection = driver.iRatingProjection;
    if (projection > 0) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "+%d", projection);
    } else if (projection < 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%d", projection);
    } else {
        ImGui::Text("0");
    }
    
    // Car brand logo placeholder
    ImGui::SameLine(380);
    ImGui::Dummy(ImVec2(24, 24));
    ImGui::SameLine(0.0f, 8.0f);
    
    // Last lap time
    ImGui::SameLine(420);
    if (driver.lastLapTime > 0.0f) {
        formatTime(driver.lastLapTime, buffer);
        ImGui::Text("%s", buffer);
    } else {
        ImGui::Text("---");
    }
    
    // Gap
    ImGui::SameLine(470);
    if (driver.position == 1) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "LEAD");
    } else {
        ImVec4 color = driver.gapToPlayer < 0.0f ? 
            ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : 
            ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        
        formatGap(driver.gapToPlayer, buffer);
        ImGui::TextColored(color, "%s", buffer);
    }
}

void RelativeWidget::formatGap(float gap, char* buffer) {
    if (gap < 0.0f) {
        snprintf(buffer, 64, "+%.1f", std::abs(gap));
    } else {
        snprintf(buffer, 64, "-%.1f", gap);
    }
}

void RelativeWidget::formatTime(float seconds, char* buffer) {
    int mins = static_cast<int>(seconds) / 60;
    float secs = seconds - (mins * 60);
    
    if (mins > 0) {
        snprintf(buffer, 64, "%d:%.3f", mins, secs);
    } else {
        snprintf(buffer, 64, "%.3f", secs);
    }
}

const char* RelativeWidget::getSafetyRatingLetter(float sr) {
    if (sr >= 4.0f) return "A";
    if (sr >= 3.0f) return "B";
    if (sr >= 2.0f) return "C";
    if (sr >= 1.0f) return "D";
    return "R";
}

void RelativeWidget::getSafetyRatingColor(float sr, float& r, float& g, float& b) {
    if (sr >= 4.0f) {
        r = 0.2f; g = 0.6f; b = 1.0f; // blue
    } else if (sr >= 3.0f) {
        r = 0.2f; g = 1.0f; b = 0.2f; // green
    } else if (sr >= 2.0f) {
        r = 1.0f; g = 1.0f; b = 0.0f; // yellow
    } else if (sr >= 1.0f) {
        r = 1.0f; g = 0.6f; b = 0.0f; // orange
    } else {
        r = 1.0f; g = 0.2f; b = 0.2f; // red
    }
}

} // namespace ui
