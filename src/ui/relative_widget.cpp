#include "ui/relative_widget.h"
#include "data/relative_calc.h"
#include "imgui.h"
#include <sstream>
#include <iomanip>

namespace ui {

RelativeWidget::RelativeWidget()
    : m_rangeAhead(5)
    , m_rangeBehind(5)
    , m_showIRating(true)
{
}

void RelativeWidget::render(iracing::RelativeCalculator* relative) {
    if (!relative) return;
    
    // Get drivers
    auto drivers = relative->getRelative(m_rangeAhead, m_rangeBehind);
    
    // Window position (top right)
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 420, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 0), ImGuiCond_FirstUseEver);
    
    ImGui::Begin("RELATIVE", nullptr, 
        ImGuiWindowFlags_NoCollapse);
    
    // Settings
    if (ImGui::BeginPopupContextWindow()) {
        ImGui::SliderInt("Ahead", &m_rangeAhead, 1, 10);
        ImGui::SliderInt("Behind", &m_rangeBehind, 1, 10);
        ImGui::Checkbox("Show iRating", &m_showIRating);
        ImGui::EndPopup();
    }
    
    // Header
    ImGui::Text("Pos");
    ImGui::SameLine(50);
    ImGui::Text("Driver");
    if (m_showIRating) {
        ImGui::SameLine(250);
        ImGui::Text("iR");
    }
    ImGui::SameLine(300);
    ImGui::Text("Gap");
    ImGui::SameLine(370);
    ImGui::Text("Last");
    
    ImGui::Separator();
    
    // Driver rows
    for (const auto& driver : drivers) {
        renderDriverRow(driver);
    }
    
    ImGui::End();
}

void RelativeWidget::renderDriverRow(const iracing::Driver& driver) {
    // Highlight player
    if (driver.isPlayer) {
        ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.6f, 0.0f, 0.5f));
        ImGui::Selectable("##player", true, 0, ImVec2(0, 25));
        ImGui::PopStyleColor();
        ImGui::SameLine(0, 0);
        ImGui::SetCursorPosX(10);
    }
    
    // Position
    ImGui::Text("%2d", driver.position);
    
    // Car brand icon (placeholder)
    ImGui::SameLine(30);
    ImGui::Text("🏎");
    
    // Driver name
    ImGui::SameLine(50);
    ImGui::Text("%s", driver.driverName.c_str());
    
    // iRating
    if (m_showIRating) {
        ImGui::SameLine(250);
        ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.0f, 1.0f), "%d", driver.iRating);
    }
    
    // Gap to player
    ImGui::SameLine(300);
    if (driver.position == 1) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "LEAD");
    } else {
        ImVec4 color = driver.gapToPlayer < 0.0f ? 
            ImVec4(0.3f, 1.0f, 0.3f, 1.0f) : // Behind (green)
            ImVec4(1.0f, 0.3f, 0.3f, 1.0f);  // Ahead (red)
        
        ImGui::TextColored(color, "%s", formatGap(driver.gapToPlayer).c_str());
    }
    
    // Last lap time
    ImGui::SameLine(370);
    if (driver.lastLapTime > 0.0f) {
        ImGui::Text("%s", formatTime(driver.lastLapTime).c_str());
    } else {
        ImGui::Text("---");
    }
}

std::string RelativeWidget::formatGap(float gap) {
    std::ostringstream oss;
    
    if (gap < 0.0f) {
        oss << "+" << std::fixed << std::setprecision(1) << -gap;
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

} // namespace ui
