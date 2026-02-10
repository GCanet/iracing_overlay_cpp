#include "ui/relative_widget.h"
#include "data/relative_calc.h"
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
    
    // Window position (top right)
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 520, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(500, 0), ImGuiCond_FirstUseEver);
    
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.6f)); // 60% transparency
    ImGui::Begin("##RELATIVE", nullptr, 
        ImGuiWindowFlags_NoCollapse | 
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize);
    
    // Header with race info
    renderHeader(relative);
    
    ImGui::Separator();
    
    // Driver rows
    for (const auto& driver : drivers) {
        renderDriverRow(driver, driver.isPlayer);
    }
    
    ImGui::End();
    ImGui::PopStyleColor();
}

void RelativeWidget::renderHeader(iracing::RelativeCalculator* relative) {
    // Series name | Laps/Time | SOF
    std::string seriesName = relative->getSeriesName();
    std::string lapInfo = relative->getLapInfo();
    int sof = relative->getSOF();
    
    ImGui::Text("%s", seriesName.c_str());
    ImGui::SameLine(200);
    ImGui::Text("%s", lapInfo.c_str());
    ImGui::SameLine(350);
    ImGui::Text("SOF: %d", sof);
}

void RelativeWidget::renderDriverRow(const iracing::Driver& driver, bool isPlayer) {
    // Different background for player
    if (isPlayer) {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.7f, 0.0f, 0.6f));
        ImGui::Selectable("##player", true, 0, ImVec2(0, 28));
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 0);
        ImGui::SetCursorPosX(10);
    }
    
    // Position
    ImGui::Text("%2d", driver.position);
    
    // Car number + Driver name (grayed if in pits)
    ImGui::SameLine(35);
    if (driver.isOnPit) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "#%s %s", 
            driver.carNumber.c_str(), driver.driverName.c_str());
    } else {
        ImGui::Text("#%s %s", driver.carNumber.c_str(), driver.driverName.c_str());
    }
    
    // Safety Rating (number + letter with color)
    ImGui::SameLine(220);
    std::string srLetter = getSafetyRatingLetter(driver.safetyRating);
    std::string srColor = getSafetyRatingColor(driver.safetyRating);
    
    float r = 1.0f, g = 1.0f, b = 1.0f;
    if (srColor == "red") { r = 1.0f; g = 0.2f; b = 0.2f; }
    else if (srColor == "orange") { r = 1.0f; g = 0.6f; b = 0.0f; }
    else if (srColor == "yellow") { r = 1.0f; g = 1.0f; b = 0.0f; }
    else if (srColor == "green") { r = 0.2f; g = 1.0f; b = 0.2f; }
    else if (srColor == "blue") { r = 0.2f; g = 0.6f; b = 1.0f; }
    
    ImGui::TextColored(ImVec4(r, g, b, 1.0f), "%.1f%s", driver.safetyRating, srLetter.c_str());
    
    // iRating
    ImGui::SameLine(280);
    ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "%d", driver.iRating);
    
    // iRating projection (green if positive, red if negative)
    ImGui::SameLine(330);
    int projection = driver.iRatingProjection;
    if (projection > 0) {
        ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "+%d", projection);
    } else if (projection < 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%d", projection);
    } else {
        ImGui::Text("0");
    }
    
    // Car brand logo area
    ImGui::SameLine(380);
    if (!driver.carBrand.empty() && driver.carBrand != "unknown") {
        // TODO: When you add texture loading, replace this with ImGui::Image(...)
        // For now we reserve space but show nothing
        ImGui::Dummy(ImVec2(24, 24));   // invisible but keeps perfect alignment
    } else {
        ImGui::Dummy(ImVec2(24, 24));   // same size when no logo
    }
    ImGui::SameLine(0.0f, 8.0f);   // small spacing after the logo column
    
    // Last lap time
    ImGui::SameLine(420);
    if (driver.lastLapTime > 0.0f) {
        ImGui::Text("%s", formatTime(driver.lastLapTime).c_str());
    } else {
        ImGui::Text("---");
    }
    
    // Gap (distance-based)
    ImGui::SameLine(470);
    if (driver.position == 1) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "LEAD");
    } else {
        ImVec4 color = driver.gapToPlayer < 0.0f ? 
            ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : // Behind (green)
            ImVec4(1.0f, 0.3f, 0.3f, 1.0f);  // Ahead (red)
        
        ImGui::TextColored(color, "%s", formatGap(driver.gapToPlayer).c_str());
    }
}

std::string RelativeWidget::formatGap(float gap) {
    std::ostringstream oss;
    
    if (gap < 0.0f) {
        oss << "+" << std::fixed << std::setprecision(1) << std::abs(gap);
    } else {
        oss << "-" << std::fixed << std::setprecision(1) << gap;
    }
    
    return oss.str();
}

std::string RelativeWidget::formatTime(float seconds) {
    int mins = static_cast<int>(seconds) / 60;
    float secs = seconds - (mins * 60);
    
    std::ostringstream oss;
    if (mins > 0) {
        oss << mins << ":" << std::fixed << std::setprecision(3) << std::setfill('0') << secs;
    } else {
        oss << std::fixed << std::setprecision(3) << secs;
    }
    
    return oss.str();
}

std::string RelativeWidget::getSafetyRatingLetter(float sr) {
    if (sr >= 4.0f) return "A";
    if (sr >= 3.0f) return "B";
    if (sr >= 2.0f) return "C";
    if (sr >= 1.0f) return "D";
    return "R";
}

std::string RelativeWidget::getSafetyRatingColor(float sr) {
    if (sr >= 4.0f) return "blue";
    if (sr >= 3.0f) return "green";
    if (sr >= 2.0f) return "yellow";
    if (sr >= 1.0f) return "orange";
    return "red";
}

} // namespace ui
