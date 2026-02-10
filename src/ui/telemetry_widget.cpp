#include "ui/telemetry_widget.h"
#include "data/irsdk_manager.h"
#include "utils/config.h"
#include "imgui.h"
#include "ui/overlay_window.h"
#include <cmath>
#include <algorithm>

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

    // Update data if session is active
    if (sdk->isSessionActive()) {
        m_currentThrottle = sdk->getFloat("Throttle", 0.0f);
        m_currentBrake = sdk->getFloat("Brake", 0.0f);
        float clutchRaw = sdk->getFloat("Clutch", 0.0f);
        m_currentClutch = 1.0f - clutchRaw;
        m_currentSteer = sdk->getFloat("SteeringWheelAngle", 0.0f);
        m_absActive = sdk->getBool("BrakeABSactive", false);
        m_currentGear = sdk->getInt("Gear", 0);
        m_currentSpeed = sdk->getFloat("Speed", 0.0f);
        
        updateTachometer(sdk);
        updateHistory(m_currentThrottle, m_currentBrake, m_currentClutch, m_currentSteer);
    }

    auto& config = utils::Config::getTelemetryConfig();
    ImGui::SetWindowFontScale(m_scale);

    // COMPACT SIZE - similar to reference image
    float scaledWidth = 750.0f * m_scale;
    float scaledHeight = 140.0f * m_scale;
    
    ImGui::SetNextWindowSize(ImVec2(scaledWidth, scaledHeight), ImGuiCond_Always);
    
    // Default position: bottom center
    if (config.posX < 0 || config.posY < 0) {
        ImGuiIO& io = ImGui::GetIO();
        config.posX = (io.DisplaySize.x - scaledWidth) * 0.5f;
        config.posY = io.DisplaySize.y - scaledHeight - 20.0f;
    }
    
    ImGui::SetNextWindowPos(ImVec2(config.posX, config.posY), ImGuiCond_Once);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, config.alpha));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoScrollbar;

    if (!editMode) {
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
    }

    ImGui::Begin("##TELEMETRY", nullptr, flags);

    // Save position
    ImVec2 pos = ImGui::GetWindowPos();
    config.posX = pos.x;
    config.posY = pos.y;

    float contentWidth = ImGui::GetContentRegionAvail().x;
    
    // ===== HORIZONTAL LAYOUT LIKE REFERENCE IMAGE =====
    
    // Left side: ABS icon
    ImGui::BeginGroup();
    renderABSIndicator(50.0f * m_scale, 50.0f * m_scale);
    ImGui::EndGroup();
    
    ImGui::SameLine();
    
    // Center: Input traces and shift lights
    ImGui::BeginGroup();
    if (m_showTachometer) {
        renderShiftLights(contentWidth - 170.0f * m_scale, 20.0f * m_scale);
    }
    if (m_showInputTrace) {
        renderInputTrace(contentWidth - 170.0f * m_scale, 90.0f * m_scale);
    }
    ImGui::EndGroup();
    
    ImGui::SameLine();
    
    // Right side: Input bars and steering wheel
    ImGui::BeginGroup();
    if (m_showInputBars) {
        renderInputBarsCompact(50.0f * m_scale, 60.0f * m_scale);
    }
    if (m_showSteeringWheel) {
        renderSteeringWheelCompact(50.0f * m_scale, 50.0f * m_scale);
    }
    ImGui::EndGroup();
    
    // Bottom: Gear display
    if (m_showGearSpeed) {
        renderGearDisplay(contentWidth, 20.0f * m_scale);
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void TelemetryWidget::updateHistory(float throttle, float brake, float clutch, float steer) {
    m_throttleHistory.pop_front();
    m_throttleHistory.push_back(throttle);
    
    m_brakeHistory.pop_front();
    m_brakeHistory.push_back(brake);
    
    m_clutchHistory.pop_front();
    m_clutchHistory.push_back(clutch);
    
    float steerNormalized = (steer + 1.0f) * 0.5f;
    m_steerHistory.pop_front();
    m_steerHistory.push_back(steerNormalized);
    
    m_absActiveHistory.pop_front();
    m_absActiveHistory.push_back(m_absActive);
}

void TelemetryWidget::updateTachometer(iracing::IRSDKManager* sdk) {
    m_currentRPM = sdk->getFloat("RPM", 0.0f);
    
    float shiftGrindRPM = sdk->getFloat("ShiftGrindRPM", 0.0f);
    if (shiftGrindRPM > 0) {
        m_maxRPM = shiftGrindRPM;
    }
    
    m_shiftRPM = sdk->getFloat("DriverCarSLShiftRPM", 0.0f);
    m_blinkRPM = sdk->getFloat("DriverCarSLBlinkRPM", 0.0f);
}

void TelemetryWidget::renderABSIndicator(float width, float height) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    
    // Circle background
    ImVec2 center(pos.x + width * 0.5f, pos.y + height * 0.5f);
    float radius = std::min(width, height) * 0.4f;
    
    ImU32 bgColor = m_absActive ? 
        IM_COL32(255, 255, 255, 255) : IM_COL32(60, 60, 60, 200);
    ImU32 textColor = m_absActive ?
        IM_COL32(0, 0, 0, 255) : IM_COL32(150, 150, 150, 255);
    
    drawList->AddCircleFilled(center, radius, bgColor, 32);
    drawList->AddCircle(center, radius, IM_COL32(200, 200, 200, 255), 32, 2.0f);
    
    // ABS text
    const char* text = "ABS";
    ImVec2 textSize = ImGui::CalcTextSize(text);
    drawList->AddText(ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f),
                     textColor, text);
    
    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderShiftLights(float width, float height) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    
    float rpmPct = (m_maxRPM > 0) ? std::min(m_currentRPM / m_maxRPM, 1.0f) : 0.0f;
    
    // Draw 10 LED lights
    int numLights = 10;
    float lightWidth = (width - (numLights - 1) * 2.0f) / numLights;
    int activeLights = (int)(rpmPct * numLights);
    
    for (int i = 0; i < numLights; i++) {
        float x = pos.x + i * (lightWidth + 2.0f);
        ImVec2 lightPos(x, pos.y);
        ImVec2 lightSize(lightWidth, height);
        
        ImU32 color;
        if (i < activeLights) {
            // Determine color based on RPM zones
            float lightPct = (float)i / numLights;
            if (m_blinkRPM > 0 && m_currentRPM >= m_blinkRPM) {
                int blinkFrame = (int)(ImGui::GetTime() * 10) % 2;
                color = blinkFrame ? IM_COL32(255, 0, 0, 255) : IM_COL32(100, 0, 0, 255);
            } else if (lightPct > 0.8f) {
                color = IM_COL32(255, 0, 0, 255); // Red zone
            } else if (lightPct > 0.6f) {
                color = IM_COL32(255, 255, 0, 255); // Yellow zone
            } else {
                color = IM_COL32(0, 255, 0, 255); // Green zone
            }
        } else {
            color = IM_COL32(40, 40, 40, 255); // Off
        }
        
        drawList->AddRectFilled(lightPos, 
                               ImVec2(lightPos.x + lightSize.x, lightPos.y + lightSize.y),
                               color, 2.0f);
    }
    
    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderInputTrace(float width, float height) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize(width, height);
    
    // Background
    drawList->AddRectFilled(canvasPos, 
                           ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                           IM_COL32(20, 20, 20, 220));
    
    // Grid lines
    for (int i = 0; i <= 2; i++) {
        float y = canvasPos.y + (canvasSize.y * i / 2.0f);
        drawList->AddLine(ImVec2(canvasPos.x, y), 
                         ImVec2(canvasPos.x + canvasSize.x, y),
                         IM_COL32(50, 50, 50, 100), 1.0f);
    }
    
    int numSamples = m_throttleHistory.size();
    float xStep = canvasSize.x / (numSamples - 1);
    
    // Draw traces with anti-aliasing
    auto drawTrace = [&](const std::deque<float>& history, ImU32 color, float strokeWidth) {
        for (int i = 0; i < numSamples - 1; i++) {
            float x1 = canvasPos.x + i * xStep;
            float x2 = canvasPos.x + (i + 1) * xStep;
            float y1 = canvasPos.y + canvasSize.y - (history[i] * canvasSize.y);
            float y2 = canvasPos.y + canvasSize.y - (history[i + 1] * canvasSize.y);
            drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), color, strokeWidth);
        }
    };
    
    // Draw in order: throttle (green), brake (red/yellow with ABS), clutch (blue)
    if (m_showThrottle) {
        drawTrace(m_throttleHistory, IM_COL32(0, 255, 0, 255), 2.5f);
    }
    
    if (m_showBrake) {
        ImU32 brakeColor = m_absActive && m_showABS ?
            IM_COL32(255, 255, 0, 255) : IM_COL32(255, 0, 0, 255);
        drawTrace(m_brakeHistory, brakeColor, 2.5f);
    }
    
    if (m_showClutch) {
        drawTrace(m_clutchHistory, IM_COL32(255, 150, 0, 255), 2.5f);
    }
    
    if (m_showSteering) {
        drawTrace(m_steerHistory, IM_COL32(200, 200, 200, 180), 1.5f);
        // Center line
        float centerY = canvasPos.y + canvasSize.y * 0.5f;
        drawList->AddLine(ImVec2(canvasPos.x, centerY), 
                         ImVec2(canvasPos.x + canvasSize.x, centerY),
                         IM_COL32(100, 100, 100, 120), 1.0f);
    }
    
    ImGui::Dummy(canvasSize);
}

void TelemetryWidget::renderInputBarsCompact(float width, float height) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    
    // Three vertical bars: Throttle (green), Brake (red/yellow), Clutch (orange)
    float barWidth = (width - 4.0f) / 3.0f;
    
    auto drawBar = [&](float x, float value, ImU32 color) {
        ImVec2 barPos(x, startPos.y);
        
        // Background
        drawList->AddRectFilled(barPos, 
                               ImVec2(barPos.x + barWidth, barPos.y + height),
                               IM_COL32(30, 30, 30, 220), 2.0f);
        
        // Value fill (bottom to top)
        float fillHeight = height * value;
        ImVec2 fillPos(barPos.x, barPos.y + height - fillHeight);
        drawList->AddRectFilled(fillPos, 
                               ImVec2(barPos.x + barWidth, barPos.y + height),
                               color, 2.0f);
    };
    
    if (m_showThrottle) {
        drawBar(startPos.x, m_currentThrottle, IM_COL32(0, 255, 0, 255));
    }
    if (m_showBrake) {
        ImU32 brakeColor = m_absActive ?
            IM_COL32(255, 255, 0, 255) : IM_COL32(255, 0, 0, 255);
        drawBar(startPos.x + barWidth + 2.0f, m_currentBrake, brakeColor);
    }
    if (m_showClutch) {
        drawBar(startPos.x + (barWidth + 2.0f) * 2, m_currentClutch, IM_COL32(255, 150, 0, 255));
    }
    
    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderSteeringWheelCompact(float width, float height) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 center = ImGui::GetCursorScreenPos();
    center.x += width * 0.5f;
    center.y += height * 0.5f;
    
    float radius = std::min(width, height) * 0.4f;
    
    // Wheel circle
    drawList->AddCircleFilled(center, radius, IM_COL32(40, 40, 40, 220), 32);
    drawList->AddCircle(center, radius, IM_COL32(150, 150, 150, 255), 32, 2.0f);
    
    // Steering indicator rotating with wheel
    float angle = m_currentSteer;
    float indicatorX = center.x + std::sin(angle) * radius * 0.7f;
    float indicatorY = center.y - std::cos(angle) * radius * 0.7f;
    drawList->AddCircleFilled(ImVec2(indicatorX, indicatorY), 3.5f, IM_COL32(255, 200, 0, 255));
    
    // Center dot
    drawList->AddCircleFilled(center, 2.5f, IM_COL32(255, 255, 255, 255));
    
    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderGearDisplay(float width, float height) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    
    // Gear text
    char gearText[8];
    if (m_currentGear == 0) {
        snprintf(gearText, sizeof(gearText), "N");
    } else if (m_currentGear == -1) {
        snprintf(gearText, sizeof(gearText), "R");
    } else {
        snprintf(gearText, sizeof(gearText), "%d", m_currentGear);
    }
    
    ImVec2 gearSize = ImGui::CalcTextSize(gearText);
    drawList->AddText(ImVec2(pos.x + (width - gearSize.x) * 0.5f, pos.y),
                     IM_COL32(255, 255, 255, 255), gearText);
    
    ImGui::Dummy(ImVec2(width, height));
}

// Dummy implementations for unused methods
void TelemetryWidget::renderInputBars(float width, float height) {}
void TelemetryWidget::renderGearSpeed(float width, float height) {}
void TelemetryWidget::renderSteeringWheel(float width, float height) {}
void TelemetryWidget::renderTachometer(float width, float height) {}

} // namespace ui
