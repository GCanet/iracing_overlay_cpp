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
    if (!sdk || !sdk->isSessionActive()) return;

    // Leer inputs actuales
    m_currentThrottle = sdk->getFloat("Throttle", 0.0f);
    m_currentBrake = sdk->getFloat("Brake", 0.0f);
    // Clutch: invertir (0=presionado, 1=suelto en SDK → invertimos para visual)
    float clutchRaw = sdk->getFloat("Clutch", 0.0f);
    m_currentClutch = 1.0f - clutchRaw;
    m_currentSteer = sdk->getFloat("SteeringWheelAngle", 0.0f);
    m_absActive = sdk->getBool("BrakeABSactive", false);
    m_currentGear = sdk->getInt("Gear", 0);
    m_currentSpeed = sdk->getFloat("Speed", 0.0f);
    
    updateTachometer(sdk);
    updateHistory(m_currentThrottle, m_currentBrake, m_currentClutch, m_currentSteer);

    auto& config = utils::Config::getTelemetryConfig();

    ImGui::SetWindowFontScale(m_scale);

    float scaledWidth = WIDGET_WIDTH * m_scale;
    float scaledHeight = WIDGET_HEIGHT * m_scale;
    ImGui::SetNextWindowSize(ImVec2(scaledWidth, scaledHeight), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(config.posX, config.posY), ImGuiCond_Once);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, config.alpha));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse |
                             ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoScrollbar;

    if (!editMode) {
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
    }

    ImGui::Begin("##TELEMETRY", nullptr, flags);

    // Guardar posición
    ImVec2 pos = ImGui::GetWindowPos();
    config.posX = pos.x;
    config.posY = pos.y;

    float contentWidth = ImGui::GetContentRegionAvail().x;
    
    // Layout vertical de componentes
    if (m_showTachometer) {
        renderTachometer(contentWidth, TACHO_HEIGHT * m_scale);
        ImGui::Spacing();
    }
    
    if (m_showInputTrace) {
        renderInputTrace(contentWidth, TRACE_HEIGHT * m_scale);
        ImGui::Spacing();
    }
    
    if (m_showInputBars) {
        renderInputBars(contentWidth, BAR_HEIGHT * m_scale);
        ImGui::Spacing();
    }
    
    // Fila con gear/speed y steering wheel
    ImGui::BeginGroup();
    if (m_showGearSpeed) {
        renderGearSpeed(contentWidth * 0.6f, GEAR_HEIGHT * m_scale);
    }
    if (m_showSteeringWheel) {
        ImGui::SameLine();
        renderSteeringWheel(STEERING_SIZE * m_scale, STEERING_SIZE * m_scale);
    }
    ImGui::EndGroup();

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
    
    // Normalizar steering: -1 a 1 → 0 a 1 para visualización
    float steerNormalized = (steer + 1.0f) * 0.5f;
    m_steerHistory.pop_front();
    m_steerHistory.push_back(steerNormalized);
    
    m_absActiveHistory.pop_front();
    m_absActiveHistory.push_back(m_absActive);
}

void TelemetryWidget::updateTachometer(iracing::IRSDKManager* sdk) {
    m_currentRPM = sdk->getFloat("RPM", 0.0f);
    
    // Obtener RPM máximo del coche
    float shiftGrindRPM = sdk->getFloat("ShiftGrindRPM", 0.0f);
    if (shiftGrindRPM > 0) {
        m_maxRPM = shiftGrindRPM;
    }
    
    // Shift lights (purple y blink)
    m_shiftRPM = sdk->getFloat("DriverCarSLShiftRPM", 0.0f);
    m_blinkRPM = sdk->getFloat("DriverCarSLBlinkRPM", 0.0f);
}

void TelemetryWidget::renderInputTrace(float width, float height) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize(width, height);
    
    // Background
    drawList->AddRectFilled(canvasPos, 
                           ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                           IM_COL32(20, 20, 20, 200));
    
    // Grid lines
    for (int i = 0; i <= 4; i++) {
        float y = canvasPos.y + (canvasSize.y * i / 4.0f);
        drawList->AddLine(ImVec2(canvasPos.x, y), 
                         ImVec2(canvasPos.x + canvasSize.x, y),
                         IM_COL32(50, 50, 50, 100));
    }
    
    int numSamples = m_throttleHistory.size();
    float xStep = canvasSize.x / (numSamples - 1);
    
    // Draw traces (bottom to top: throttle, brake, clutch, steering)
    auto drawTrace = [&](const std::deque<float>& history, ImU32 color, float strokeWidth) {
        for (int i = 0; i < numSamples - 1; i++) {
            float x1 = canvasPos.x + i * xStep;
            float x2 = canvasPos.x + (i + 1) * xStep;
            float y1 = canvasPos.y + canvasSize.y - (history[i] * canvasSize.y);
            float y2 = canvasPos.y + canvasSize.y - (history[i + 1] * canvasSize.y);
            drawList->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), color, strokeWidth);
        }
    };
    
    if (m_showThrottle) {
        drawTrace(m_throttleHistory, IM_COL32(0, 255, 0, 255), m_strokeWidth);
    }
    
    if (m_showBrake) {
        ImU32 brakeColor = m_absActive && m_showABS ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 0, 0, 255);
        drawTrace(m_brakeHistory, brakeColor, m_strokeWidth);
    }
    
    if (m_showClutch) {
        drawTrace(m_clutchHistory, IM_COL32(100, 150, 255, 255), m_strokeWidth);
    }
    
    if (m_showSteering) {
        drawTrace(m_steerHistory, IM_COL32(200, 200, 200, 200), m_strokeWidth * 0.7f);
        // Center line para steering
        float centerY = canvasPos.y + canvasSize.y * 0.5f;
        drawList->AddLine(ImVec2(canvasPos.x, centerY), 
                         ImVec2(canvasPos.x + canvasSize.x, centerY),
                         IM_COL32(100, 100, 100, 150), 1.0f);
    }
    
    ImGui::Dummy(canvasSize);
}

void TelemetryWidget::renderInputBars(float width, float height) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 startPos = ImGui::GetCursorScreenPos();
    
    // Calcular cuántas barras mostrar
    int numBars = 0;
    if (m_showClutch) numBars++;
    if (m_showBrake) numBars++;
    if (m_showThrottle) numBars++;
    
    if (numBars == 0) return;
    
    float barWidth = (width - (numBars - 1) * 4.0f) / numBars;
    float currentX = startPos.x;
    
    auto drawBar = [&](float value, ImU32 color, const char* label, bool showAbs = false) {
        ImVec2 barPos(currentX, startPos.y);
        ImVec2 barSize(barWidth, height);
        
        // Background
        drawList->AddRectFilled(barPos, 
                               ImVec2(barPos.x + barSize.x, barPos.y + barSize.y),
                               IM_COL32(30, 30, 30, 200));
        
        // Value bar (bottom to top)
        float fillHeight = barSize.y * value;
        ImVec2 fillPos(barPos.x, barPos.y + barSize.y - fillHeight);
        drawList->AddRectFilled(fillPos, 
                               ImVec2(barPos.x + barSize.x, barPos.y + barSize.y),
                               color);
        
        // Percentage text at top
        char percentText[16];
        snprintf(percentText, sizeof(percentText), "%.0f%%", value * 100.0f);
        ImVec2 textSize = ImGui::CalcTextSize(percentText);
        drawList->AddText(ImVec2(barPos.x + (barSize.x - textSize.x) * 0.5f, barPos.y + 2),
                         IM_COL32(255, 255, 255, 255), percentText);
        
        // ABS indicator
        if (showAbs) {
            ImVec2 absTextSize = ImGui::CalcTextSize("ABS");
            drawList->AddText(ImVec2(barPos.x + (barSize.x - absTextSize.x) * 0.5f, 
                                    barPos.y + barSize.y - absTextSize.y - 4),
                             IM_COL32(255, 255, 255, 255), "ABS");
        }
        
        currentX += barWidth + 4.0f;
    };
    
    if (m_showClutch) {
        drawBar(m_currentClutch, IM_COL32(100, 150, 255, 255), "CLU");
    }
    if (m_showBrake) {
        ImU32 brakeColor = m_absActive && m_showABS ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 0, 0, 255);
        drawBar(m_currentBrake, brakeColor, "BRK", m_absActive && m_showABS);
    }
    if (m_showThrottle) {
        drawBar(m_currentThrottle, IM_COL32(0, 255, 0, 255), "THR");
    }
    
    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderGearSpeed(float width, float height) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    
    // Background
    drawList->AddRectFilled(pos, 
                           ImVec2(pos.x + width, pos.y + height),
                           IM_COL32(20, 20, 20, 200));
    
    // Gear display (large)
    char gearText[8];
    if (m_currentGear == 0) {
        snprintf(gearText, sizeof(gearText), "N");
    } else if (m_currentGear == -1) {
        snprintf(gearText, sizeof(gearText), "R");
    } else {
        snprintf(gearText, sizeof(gearText), "%d", m_currentGear);
    }
    
    ImGui::PushFont(ImGui::GetFont()); // TODO: usar font más grande si está disponible
    ImVec2 gearSize = ImGui::CalcTextSize(gearText);
    drawList->AddText(ImVec2(pos.x + 10, pos.y + (height - gearSize.y) * 0.5f),
                     IM_COL32(255, 255, 255, 255), gearText);
    ImGui::PopFont();
    
    // Speed display
    // Convertir de m/s a km/h o mph según preferencia
    float speedKmh = m_currentSpeed * 3.6f;
    char speedText[32];
    snprintf(speedText, sizeof(speedText), "%.0f km/h", speedKmh);
    ImVec2 speedSize = ImGui::CalcTextSize(speedText);
    drawList->AddText(ImVec2(pos.x + width - speedSize.x - 10, pos.y + (height - speedSize.y) * 0.5f),
                     IM_COL32(200, 200, 200, 255), speedText);
    
    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderSteeringWheel(float width, float height) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 center = ImGui::GetCursorScreenPos();
    center.x += width * 0.5f;
    center.y += height * 0.5f;
    
    float radius = std::min(width, height) * 0.4f;
    
    // Wheel circle
    drawList->AddCircleFilled(center, radius, IM_COL32(40, 40, 40, 200), 32);
    drawList->AddCircle(center, radius, IM_COL32(150, 150, 150, 255), 32, 2.0f);
    
    // Steering indicator (top mark rotating with wheel)
    float angle = m_currentSteer; // radianes
    float indicatorX = center.x + std::sin(angle) * radius * 0.8f;
    float indicatorY = center.y - std::cos(angle) * radius * 0.8f;
    drawList->AddCircleFilled(ImVec2(indicatorX, indicatorY), 4.0f, IM_COL32(255, 200, 0, 255));
    
    // Center dot
    drawList->AddCircleFilled(center, 3.0f, IM_COL32(255, 255, 255, 255));
    
    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderTachometer(float width, float height) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    
    // Background
    drawList->AddRectFilled(pos, 
                           ImVec2(pos.x + width, pos.y + height),
                           IM_COL32(20, 20, 20, 200));
    
    // RPM bar
    float rpmPct = (m_maxRPM > 0) ? std::min(m_currentRPM / m_maxRPM, 1.0f) : 0.0f;
    
    // Color zones
    ImU32 barColor = IM_COL32(0, 255, 0, 255); // Green
    if (m_blinkRPM > 0 && m_currentRPM >= m_blinkRPM) {
        // Blinking red zone
        int blinkFrame = (int)(ImGui::GetTime() * 10) % 2;
        barColor = blinkFrame ? IM_COL32(255, 0, 0, 255) : IM_COL32(100, 0, 0, 255);
    } else if (m_shiftRPM > 0 && m_currentRPM >= m_shiftRPM) {
        barColor = IM_COL32(200, 0, 255, 255); // Purple shift zone
    }
    
    drawList->AddRectFilled(pos, 
                           ImVec2(pos.x + width * rpmPct, pos.y + height),
                           barColor);
    
    // RPM text
    char rpmText[32];
    snprintf(rpmText, sizeof(rpmText), "%.0f RPM", m_currentRPM);
    ImVec2 textSize = ImGui::CalcTextSize(rpmText);
    drawList->AddText(ImVec2(pos.x + (width - textSize.x) * 0.5f, pos.y + (height - textSize.y) * 0.5f),
                     IM_COL32(255, 255, 255, 255), rpmText);
    
    ImGui::Dummy(ImVec2(width, height));
}

} // namespace ui
