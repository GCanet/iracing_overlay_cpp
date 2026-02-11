#include "telemetry_widget.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include "utils/config.h"
#include "data/irsdk_manager.h"

namespace ui {

TelemetryWidget::TelemetryWidget(OverlayWindow* overlay)
    : m_overlay(overlay)
{
    m_throttleHistory.resize(m_maxSamples, 0.0f);
    m_brakeHistory.resize(m_maxSamples, 0.0f);
    m_clutchHistory.resize(m_maxSamples, 0.0f);
    m_steerHistory.resize(m_maxSamples, 0.5f);
    m_absActiveHistory.resize(m_maxSamples, false);
}

void TelemetryWidget::render(iracing::IRSDKManager* sdk, bool editMode) {
    if (!sdk) return;

    if (sdk->isSessionActive()) {
        m_currentThrottle = sdk->getFloat("Throttle", 0.0f);
        m_currentBrake    = sdk->getFloat("Brake", 0.0f);
        float clutchRaw   = sdk->getFloat("Clutch", 0.0f);
        m_currentClutch   = 1.0f - clutchRaw;
        m_currentSteer    = sdk->getFloat("SteeringWheelAngle", 0.0f);
        m_absActive       = sdk->getBool("BrakeABSactive", false);
        m_currentGear     = sdk->getInt("Gear", 0);
        m_currentSpeed    = sdk->getFloat("Speed", 0.0f) * 3.6f; // m/s -> km/h
        
        updateTachometer(sdk);
        updateHistory(m_currentThrottle, m_currentBrake, m_currentClutch, m_currentSteer);
    }

    auto& config = utils::Config::getTelemetryConfig();

    // FIX: Do NOT call SetWindowFontScale before Begin().
    // It was touching ImGui's default "Debug" window and making it visible.

    float scaledWidth  = 750.0f * m_scale;
    float scaledHeight = 140.0f * m_scale;
    
    ImGui::SetNextWindowSize(ImVec2(scaledWidth, scaledHeight), ImGuiCond_Always);
    
    if (config.posX < 0 || config.posY < 0) {
        ImGuiIO& io = ImGui::GetIO();
        config.posX = (io.DisplaySize.x - scaledWidth) * 0.5f;
        config.posY = io.DisplaySize.y - scaledHeight - 20.0f;
    }
    
    ImGui::SetNextWindowPos(ImVec2(config.posX, config.posY), ImGuiCond_Once);

    ImGui::PushStyleColor(ImGuiCol_WindowBg,   ImVec4(0.0f, 0.0f, 0.0f, config.alpha));
    ImGui::PushStyleColor(ImGuiCol_Border,     ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;

    if (!editMode) {
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
    }

    ImGui::Begin("##TELEMETRY", nullptr, flags);

    // FIX: SetWindowFontScale AFTER Begin() so it targets our window, not Debug
    ImGui::SetWindowFontScale(m_scale);

    ImVec2 pos = ImGui::GetWindowPos();
    config.posX = pos.x;
    config.posY = pos.y;

    float contentWidth = ImGui::GetContentRegionAvail().x;
    
    // Left: ABS
    ImGui::BeginGroup();
    renderABSIndicator(50.0f * m_scale, 50.0f * m_scale);
    ImGui::EndGroup();
    
    ImGui::SameLine();
    
    // Center: Shift lights + Input trace
    ImGui::BeginGroup();
    if (m_showTachometer) renderShiftLights(contentWidth - 170.0f * m_scale, 20.0f * m_scale);
    if (m_showInputTrace) renderInputTrace(contentWidth - 170.0f * m_scale, 90.0f * m_scale);
    ImGui::EndGroup();
    
    ImGui::SameLine();
    
    // Right: Input bars + Steering wheel
    ImGui::BeginGroup();
    if (m_showInputBars) renderInputBarsCompact(50.0f * m_scale, 60.0f * m_scale);
    if (m_showSteeringWheel) renderSteeringWheelCompact(50.0f * m_scale, 50.0f * m_scale);
    ImGui::EndGroup();
    
    // Bottom: Gear + Speed
    if (m_showGearSpeed) {
        renderGearDisplay(contentWidth, 20.0f * m_scale);
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

// ──────────────────────────────────────────────────────────────
// Update functions
// ──────────────────────────────────────────────────────────────

void TelemetryWidget::updateHistory(float throttle, float brake, float clutch, float steer) {
    m_throttleHistory.pop_front(); m_throttleHistory.push_back(throttle);
    m_brakeHistory.pop_front();    m_brakeHistory.push_back(brake);
    m_clutchHistory.pop_front();   m_clutchHistory.push_back(clutch);
    
    float steerNormalized = (steer + 1.0f) * 0.5f;
    steerNormalized = std::max(0.0f, std::min(1.0f, steerNormalized));
    m_steerHistory.pop_front();    m_steerHistory.push_back(steerNormalized);
    
    m_absActiveHistory.pop_front(); m_absActiveHistory.push_back(m_absActive);
}

void TelemetryWidget::updateTachometer(iracing::IRSDKManager* sdk) {
    m_currentRPM = sdk->getFloat("RPM", 0.0f);
    
    float shiftGrindRPM = sdk->getFloat("ShiftGrindRPM", 0.0f);
    if (shiftGrindRPM > 0) m_maxRPM = shiftGrindRPM;
    
    m_shiftRPM = sdk->getFloat("DriverCarSLShiftRPM", 0.0f);
    m_blinkRPM = sdk->getFloat("DriverCarSLBlinkRPM", 0.0f);
}

// ──────────────────────────────────────────────────────────────
// Render functions
// ──────────────────────────────────────────────────────────────

void TelemetryWidget::renderABSIndicator(float width, float height) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 center(pos.x + width * 0.5f, pos.y + height * 0.5f);
    float radius = std::min(width, height) * 0.4f;
    
    ImU32 bgColor   = m_absActive ? IM_COL32(255, 255, 255, 255) : IM_COL32(60, 60, 60, 200);
    ImU32 textColor = m_absActive ? IM_COL32(0, 0, 0, 255) : IM_COL32(150, 150, 150, 255);
    
    drawList->AddCircleFilled(center, radius, bgColor, 32);
    drawList->AddCircle(center, radius, IM_COL32(200, 200, 200, 255), 32, 2.0f);
    
    const char* text = "ABS";
    ImVec2 textSize = ImGui::CalcTextSize(text);
    drawList->AddText(ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f), textColor, text);
    
    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderShiftLights(float width, float height) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float rpmPct = (m_maxRPM > 0) ? std::min(m_currentRPM / m_maxRPM, 1.0f) : 0.0f;
    int numLights = 10;
    float lightWidth = (width - (numLights - 1) * 2.0f) / numLights;
    int activeLights = (int)(rpmPct * numLights);
    
    for (int i = 0; i < numLights; i++) {
        float x = pos.x + i * (lightWidth + 2.0f);
        ImVec2 lightPos(x, pos.y);
        ImVec2 lightEnd(x + lightWidth, pos.y + height);
        
        ImU32 color = IM_COL32(40, 40, 40, 255);
        if (i < activeLights) {
            float lightPct = (float)i / numLights;
            if (m_blinkRPM > 0 && m_currentRPM >= m_blinkRPM) {
                int blink = (int)(ImGui::GetTime() * 10) % 2;
                color = blink ? IM_COL32(255, 0, 0, 255) : IM_COL32(100, 0, 0, 255);
            } else if (lightPct > 0.8f) color = IM_COL32(255, 0, 0, 255);
            else if (lightPct > 0.6f) color = IM_COL32(255, 255, 0, 255);
            else color = IM_COL32(0, 255, 0, 255);
        }
        drawList->AddRectFilled(lightPos, lightEnd, color, 2.0f);
    }
    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderInputTrace(float width, float height) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    
    // Background
    drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(20, 20, 20, 200));
    drawList->AddRect(pos, ImVec2(pos.x + width, pos.y + height), IM_COL32(60, 60, 60, 255));
    
    int sampleCount = static_cast<int>(m_throttleHistory.size());
    if (sampleCount < 2) {
        ImGui::Dummy(ImVec2(width, height));
        return;
    }
    
    float stepX = width / (float)(sampleCount - 1);
    
    // Draw throttle (green), brake (red), clutch (blue), steering (white)
    auto drawTrace = [&](const std::deque<float>& history, ImU32 color, float lineWidth) {
        for (int i = 1; i < sampleCount; i++) {
            float x0 = pos.x + (i - 1) * stepX;
            float x1 = pos.x + i * stepX;
            float y0 = pos.y + height - history[i - 1] * height;
            float y1 = pos.y + height - history[i] * height;
            drawList->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), color, lineWidth);
        }
    };
    
    if (m_showThrottle) drawTrace(m_throttleHistory, IM_COL32(0, 255, 0, 220), m_strokeWidth);
    if (m_showBrake)    drawTrace(m_brakeHistory,    IM_COL32(255, 0, 0, 220), m_strokeWidth);
    if (m_showClutch)   drawTrace(m_clutchHistory,   IM_COL32(60, 120, 255, 180), m_strokeWidth * 0.7f);
    if (m_showSteering) drawTrace(m_steerHistory,    IM_COL32(255, 255, 255, 120), m_strokeWidth * 0.5f);
    
    // ABS highlight zones
    if (m_showABS) {
        for (int i = 0; i < sampleCount; i++) {
            if (m_absActiveHistory[i]) {
                float x = pos.x + i * stepX;
                drawList->AddLine(ImVec2(x, pos.y), ImVec2(x, pos.y + height), IM_COL32(255, 255, 0, 40));
            }
        }
    }
    
    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderInputBarsCompact(float width, float height) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    
    float barWidth = (width - 4.0f) / 3.0f;
    
    // Throttle
    {
        float h = m_currentThrottle * height;
        ImVec2 p1(startPos.x, startPos.y + height - h);
        ImVec2 p2(startPos.x + barWidth, startPos.y + height);
        drawList->AddRectFilled(p1, p2, IM_COL32(0, 255, 0, 220));
        drawList->AddRect(ImVec2(startPos.x, startPos.y), p2, IM_COL32(255, 255, 255, 80));
    }
    // Brake
    {
        float h = m_currentBrake * height;
        ImVec2 p1(startPos.x + barWidth + 2, startPos.y + height - h);
        ImVec2 p2(startPos.x + 2 * barWidth + 2, startPos.y + height);
        ImU32 col = (m_absActive && m_showABS) ? IM_COL32(255, 255, 0, 220) : IM_COL32(255, 0, 0, 220);
        drawList->AddRectFilled(p1, p2, col);
        drawList->AddRect(ImVec2(startPos.x + barWidth + 2, startPos.y), p2, IM_COL32(255, 255, 255, 80));
    }
    // Clutch
    {
        float h = m_currentClutch * height;
        ImVec2 p1(startPos.x + 2 * barWidth + 4, startPos.y + height - h);
        ImVec2 p2(startPos.x + 3 * barWidth + 4, startPos.y + height);
        drawList->AddRectFilled(p1, p2, IM_COL32(60, 120, 255, 220));
        drawList->AddRect(ImVec2(startPos.x + 2 * barWidth + 4, startPos.y), p2, IM_COL32(255, 255, 255, 80));
    }
    
    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderSteeringWheelCompact(float width, float height) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 center(pos.x + width * 0.5f, pos.y + height * 0.5f);
    float radius = std::min(width, height) * 0.4f;
    
    // Wheel circle
    drawList->AddCircle(center, radius, IM_COL32(200, 200, 200, 200), 32, 2.0f);
    
    // Indicator line showing steering angle
    float angle = -m_currentSteer; // Negate so left turn = left on screen
    float indicatorX = center.x + radius * 0.8f * sinf(angle);
    float indicatorY = center.y - radius * 0.8f * cosf(angle);
    drawList->AddLine(center, ImVec2(indicatorX, indicatorY), IM_COL32(255, 255, 0, 255), 2.0f);
    drawList->AddCircleFilled(ImVec2(indicatorX, indicatorY), 3.0f, IM_COL32(255, 255, 0, 255));
    
    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderGearDisplay(float width, float height) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    
    // Gear
    char gearStr[8];
    if (m_currentGear == -1) snprintf(gearStr, 8, "R");
    else if (m_currentGear == 0) snprintf(gearStr, 8, "N");
    else snprintf(gearStr, 8, "%d", m_currentGear);
    
    // Speed
    char speedStr[32];
    snprintf(speedStr, 32, "%.0f km/h", m_currentSpeed);
    
    ImVec2 gearSize = ImGui::CalcTextSize(gearStr);
    ImVec2 speedSize = ImGui::CalcTextSize(speedStr);
    
    float totalWidth = gearSize.x + 20.0f + speedSize.x;
    float startX = pos.x + (width - totalWidth) * 0.5f;
    
    // Gear text (colored by gear)
    ImU32 gearColor = IM_COL32(255, 255, 255, 255);
    if (m_currentGear == -1) gearColor = IM_COL32(255, 100, 100, 255);
    else if (m_currentGear == 0) gearColor = IM_COL32(200, 200, 200, 255);
    
    drawList->AddText(ImVec2(startX, pos.y), gearColor, gearStr);
    drawList->AddText(ImVec2(startX + gearSize.x + 20.0f, pos.y), IM_COL32(200, 200, 200, 255), speedStr);
    
    ImGui::Dummy(ImVec2(width, height));
}

} // namespace ui
