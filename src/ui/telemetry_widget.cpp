#include "ui/telemetry_widget.h"
#include "data/irsdk_manager.h"
#include "utils/config.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glad/glad.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace ui {

TelemetryWidget::TelemetryWidget(OverlayWindow* overlay)
    : m_currentRPM(0.0f), m_maxRPM(7000.0f), m_blinkRPM(6500.0f),
      m_throttle(0.0f), m_brake(0.0f), m_clutch(0.0f), m_gear(0), m_speed(0),
      m_steeringAngle(0.0f), m_steeringAngleMax(180.0f),
      m_absActive(false), m_scale(1.0f), m_historyHead(0), m_overlay(overlay),
      m_steeringTexture(0), m_absOnTexture(0), m_absOffTexture(0) {
    m_throttleHistory.resize(256, 0.0f);
    m_brakeHistory.resize(256, 0.0f);

    loadAssets();
}

TelemetryWidget::~TelemetryWidget() {
    if (m_steeringTexture) glDeleteTextures(1, &m_steeringTexture);
    if (m_absOnTexture) glDeleteTextures(1, &m_absOnTexture);
    if (m_absOffTexture) glDeleteTextures(1, &m_absOffTexture);
}

unsigned int TelemetryWidget::loadTextureFromFile(const char* filepath) {
    unsigned int textureID = 0;

    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filepath, &width, &height, &channels, 4);

    if (!data) {
        return 0;
    }

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);

    return textureID;
}

void TelemetryWidget::loadAssets() {
    m_steeringTexture = loadTextureFromFile("assets/telemetry/steering_wheel.png");
    m_absOnTexture = loadTextureFromFile("assets/telemetry/abs_on.png");
    m_absOffTexture = loadTextureFromFile("assets/telemetry/abs_off.png");
}

void TelemetryWidget::render(iracing::IRSDKManager* sdk, utils::WidgetConfig& config, bool editMode) {
    if (!sdk) return;

    // Read from SDK
    if (sdk->isSessionActive()) {
        m_currentRPM = sdk->getFloat("RPM", 0.0f);
        m_maxRPM = sdk->getFloat("ShiftGrindRPM", 7000.0f);
        m_blinkRPM = sdk->getFloat("DriverCarSLBlinkRPM", 6500.0f);
        m_throttle = sdk->getFloat("Throttle", 0.0f);
        m_brake = sdk->getFloat("Brake", 0.0f);
        // Use real pedal clutch inverted (0% = pressed, 100% = released) for launch control
        m_clutch = 1.0f - sdk->getFloat("ClutchRaw", 0.0f);
        m_gear = sdk->getInt("Gear", 0);
        // Speed: iRacing gives m/s, convert to km/h
        m_speed = static_cast<int>(sdk->getFloat("Speed", 0.0f) * 3.6f);
        m_steeringAngle = sdk->getFloat("SteeringWheelAngle", 0.0f);
        m_steeringAngleMax = sdk->getFloat("SteeringWheelAngleMax", 7.854f); // ~450 deg default
        m_absActive = sdk->getBool("BrakeABSactive", false);

        // Update history (ring buffer) - ONLY throttle and brake, NO clutch
        m_throttleHistory[m_historyHead] = m_throttle * 100.0f;
        m_brakeHistory[m_historyHead] = m_brake * 100.0f;
        m_historyHead = (m_historyHead + 1) % 256;
    }

    // --- Layout constants (iRdashies-inspired: wider, compact height) ---
    const float rpmH = 14.0f * m_scale;       // shift lights row height
    const float rowH = 52.0f * m_scale;        // main content row height (taller for readability)
    const float gapRpm = 7.0f * m_scale;       // gap between RPM lights and content - INCREASED from 3.0f
    const float padX = 6.0f * m_scale;
    const float padY = 4.0f * m_scale;

    float totalW = 330.0f * m_scale;           // reduced width (50 pixels smaller)
    float totalH = rpmH + gapRpm + rowH + padY * 3.0f + 8.0f;

    ImGui::SetNextWindowSize(ImVec2(totalW, totalH), ImGuiCond_Always);

    if (config.posX < 0 || config.posY < 0) {
        config.posX = 690.0f;
        config.posY = 720.0f;
    }

    ImGui::SetNextWindowPos(ImVec2(config.posX, config.posY), ImGuiCond_Once);

    // Pure black background like iRdashies
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, config.alpha));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(padX, padY));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;
    if (!editMode) {
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoResize;
    }

    ImGui::Begin("##TELEMETRY", nullptr, flags);
    ImGui::SetWindowFontScale(m_scale);

    ImVec2 pos = ImGui::GetWindowPos();
    config.posX = pos.x;
    config.posY = pos.y;

    float contentW = ImGui::GetContentRegionAvail().x;

    // Row 1: Shift lights (centered circles)
    renderShiftLights(contentW, rpmH);
    ImGui::Dummy(ImVec2(0, gapRpm));

    // Row 2: Main content - ABS | Trace | Pedal Numbers | Gear/Speed | Steering
    float absW = 42.0f * m_scale;        // ABS icon (wider for circle + parentheses)
    float steerW = 42.0f * m_scale;      // Steering wheel
    float pedalNumW = 44.0f * m_scale;   // Pedal % numbers (3 digits each)
    float gearW = 56.0f * m_scale;       // Gear + speed + km/h
    float gap = 4.0f * m_scale;

    float traceW = contentW - absW - steerW - pedalNumW - gearW - gap * 4.0f;
    if (traceW < 40.0f * m_scale) traceW = 40.0f * m_scale; // minimum trace width

    renderABS(absW, rowH);
    ImGui::SameLine(0.0f, gap);

    renderHistoryTrace(traceW, rowH);
    ImGui::SameLine(0.0f, gap);

    renderPedalBars(pedalNumW, rowH);
    ImGui::SameLine(0.0f, gap);

    renderGearSpeed(gearW, rowH);
    ImGui::SameLine(0.0f, gap);

    renderSteering(steerW, rowH);

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void TelemetryWidget::setScale(float scale) {
    m_scale = scale;
}

// =============================================================================
// SHIFT LIGHTS - Circular dots like iRdashies
// Pattern: grey by default, lights up with color when RPM increases
// Blinks red at redline
// =============================================================================
void TelemetryWidget::renderShiftLights(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    float rpmPct = (m_maxRPM > 0) ? (m_currentRPM / m_maxRPM) : 0.0f;
    float blinkPct = (m_maxRPM > 0) ? (m_blinkRPM / m_maxRPM) : 0.8f;

    bool blink = std::fmod(ImGui::GetTime() * 2.5f, 1.0f) < 0.5f;
    bool isRedline = (rpmPct >= blinkPct) && blink;

    // 10 lights: center them in the available width
    float dotRadius = height * 0.35f;
    float spacing = (width / 10.0f);
    float startX = p.x + spacing * 0.5f;

    // Draw lights
    for (int i = 0; i < 10; i++) {
        float x = startX + i * spacing;
        ImU32 lightCol = IM_COL32(100, 100, 100, 180);  // Default grey

        // Draw filled circle if RPM exceeds this light's threshold
        float threshold = (i + 1) / 10.0f;
        if (rpmPct >= threshold) {
            // Light is active - use color pattern
            if (i < 2) {
                lightCol = IM_COL32(0, 180, 0, 220);  // Green
            } else if (i < 8) {
                lightCol = IM_COL32(180, 180, 0, 220);  // Yellow
            } else {
                lightCol = IM_COL32(100, 100, 100, 180);  // Grey for last 2
            }

            // If in redline zone and blinking, show red
            if (isRedline) {
                lightCol = IM_COL32(255, 0, 0, 220);
            }

            dl->AddCircleFilled(ImVec2(x, p.y + height * 0.5f), dotRadius, lightCol, 8);
        } else {
            // Unfilled circle (grey when off)
            dl->AddCircle(ImVec2(x, p.y + height * 0.5f), dotRadius, lightCol, 8, 1.5f);
        }
    }

    ImGui::Dummy(ImVec2(width, height));
}

// =============================================================================
// ABS - Circle with parentheses or icon
// =============================================================================
void TelemetryWidget::renderABS(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    float cx = p.x + width * 0.5f;
    float cy = p.y + height * 0.5f;
    float radius = std::min(width, height) * 0.35f;

    ImU32 absCol = m_absActive ?
        IM_COL32(255, 100, 0, 220) : IM_COL32(80, 80, 80, 180);

    // Draw circle
    dl->AddCircle(ImVec2(cx, cy), radius, absCol, 12, 2.0f * m_scale);

    // Draw "ABS" text inside circle
    char absBuf[] = "ABS";
    ImVec2 textSize = ImGui::CalcTextSize(absBuf);
    ImVec2 textPos = ImVec2(cx - textSize.x * 0.5f, cy - textSize.y * 0.5f);
    dl->AddText(textPos, absCol, absBuf);

    ImGui::Dummy(ImVec2(width, height));
}

// =============================================================================
// PEDAL BARS - Throttle, Brake, Clutch as vertical bars with numeric values above
// Bars now fill the vertical space (0.05 to 0.95)
// =============================================================================
void TelemetryWidget::renderPedalBars(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    float pedals[3] = { m_throttle, m_brake, m_clutch };
    ImU32 colors[3] = {
        IM_COL32(0, 220, 0, 220),      // Throttle = green
        IM_COL32(255, 40, 40, 220),    // Brake = red
        IM_COL32(80, 140, 220, 220)    // Clutch = blue
    };

    float barW = width / 3.0f * 0.8f;
    float barStartX = p.x + width * 0.1f;
    // INCREASED from 0.65 to 0.90 to fill more vertical space, now 8px taller on top, 2px on bottom
    float barH = height * 0.90f + 8.0f * m_scale + 2.0f * m_scale;  // Total +10 pixels taller
    // Adjusted top position to start from near top (0.05 instead of 0.15) - MOVED DOWN 4 pixels, then UP 8 for bar height
    float barTop = p.y + height * 0.05f + 4.0f * m_scale - 8.0f * m_scale;  // 8px up to extend top

    for (int i = 0; i < 3; i++) {
        float x = barStartX + i * width / 3.0f;
        float filledH = barH * pedals[i];
        float barBottom = barTop + barH;

        // Background bar
        dl->AddRectFilled(ImVec2(x, barTop), ImVec2(x + barW, barBottom),
                         IM_COL32(15, 15, 15, 220));

        // Filled portion (from bottom up)
        if (filledH > 0.5f) {
            dl->AddRectFilled(ImVec2(x, barBottom - filledH), ImVec2(x + barW, barBottom), colors[i]);
        }

        // Numeric value on top of bar (above the bar area) - MOVED DOWN 3 more pixels, more compact
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", static_cast<int>(pedals[i] * 100.0f));
        ImVec2 ts = ImGui::CalcTextSize(buf);
        float textX = x + (barW - ts.x) * 0.5f;
        
        // Position numbers higher, with extra offset for better spacing - MOVED DOWN 5 pixels total (3+2)
        float textY = barTop - ts.y - 10.0f * m_scale + 5.0f * m_scale;
        
        // Ensure text doesn't go above the widget bounds
        if (textY < p.y - ts.y - 2.0f * m_scale) {
            textY = p.y - ts.y - 2.0f * m_scale;
        }
        
        // Draw with tighter letter spacing (using smaller font rendering)
        ImGui::PushFont(nullptr);
        dl->AddText(ImVec2(textX, textY), colors[i], buf);
        ImGui::PopFont();
    }

    ImGui::Dummy(ImVec2(width, height));
}

// =============================================================================
// HISTORY TRACE - Throttle/Brake lines on dark background with grid
// NO CLUTCH history anymore
// =============================================================================
void TelemetryWidget::renderHistoryTrace(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // Dark background
    dl->AddRectFilled(p, ImVec2(p.x + width, p.y + height), IM_COL32(10, 10, 10, 220));

    // Subtle grid lines at 25%, 50%, 75%
    ImU32 gridCol = IM_COL32(40, 40, 40, 100);
    for (int g = 1; g <= 3; g++) {
        float gy = p.y + height * (g * 0.25f);
        // Dashed line
        for (float x = p.x; x < p.x + width; x += 4.0f * m_scale) {
            float endX = std::min(x + 2.0f * m_scale, p.x + width);
            dl->AddLine(ImVec2(x, gy), ImVec2(endX, gy), gridCol, 0.5f);
        }
    }

    // Draw traces from history buffer (ring buffer, draw from oldest to newest)
    float pixelW = width / 256.0f;
    float traceThickness = 3.5f * m_scale;

    auto drawTrace = [&](const std::vector<float>& history, ImU32 color) {
        // Draw from oldest sample to newest
        for (int i = 0; i < 255; i++) {
            int idx0 = (m_historyHead + i) % 256;
            int idx1 = (m_historyHead + i + 1) % 256;
            float x1 = p.x + i * pixelW;
            float x2 = p.x + (i + 1) * pixelW;
            float y1 = p.y + height * (1.0f - history[idx0] / 100.0f);
            float y2 = p.y + height * (1.0f - history[idx1] / 100.0f);
            dl->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), color, traceThickness);
        }
    };

    // Draw order: brake (back), throttle (front) - NO CLUTCH
    drawTrace(m_brakeHistory, IM_COL32(255, 40, 40, 220));
    drawTrace(m_throttleHistory, IM_COL32(0, 220, 0, 220));

    ImGui::Dummy(ImVec2(width, height));
}

// =============================================================================
// GEAR + SPEED - Large gear number, speed below with "km/h" label
// Like iRdashies: big gear, speed + unit below (increased font sizes)
// =============================================================================
void TelemetryWidget::renderGearSpeed(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    char gearBuf[8];
    char speedBuf[16];
    char unitBuf[] = "km/h";

    // Format gear
    if (m_gear == 0) {
        snprintf(gearBuf, sizeof(gearBuf), "N");
    } else if (m_gear == -1) {
        snprintf(gearBuf, sizeof(gearBuf), "R");
    } else {
        snprintf(gearBuf, sizeof(gearBuf), "%d", m_gear);
    }

    snprintf(speedBuf, sizeof(speedBuf), "%d", m_speed);

    // Gear: large, centered, top portion - MOVED UP 15 pixels, increase size by 2px
    float gearFontSize = height * 0.58f + 2.0f * m_scale;  // INCREASED by 2 pixels
    ImVec2 gearSize = ImGui::CalcTextSize(gearBuf);
    float gearScale = gearFontSize / gearSize.y;
    float gearX = p.x + (width - gearSize.x * gearScale) * 0.5f;
    float gearY = p.y + 2.0f * m_scale - 15.0f * m_scale + 4.0f * m_scale;  // MOVED DOWN 4 pixels

    // Gear color: white, neutral = orange, reverse = red
    ImU32 gearCol = IM_COL32(255, 255, 255, 255);
    if (m_gear == 0) gearCol = IM_COL32(255, 165, 0, 255);  // N = orange
    else if (m_gear == -1) gearCol = IM_COL32(255, 60, 60, 255); // R = red

    // Draw gear text UNBOLD (regular weight)
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * gearScale,
                ImVec2(gearX, gearY), gearCol, gearBuf);

    // Speed: medium, below gear - increase size by 2px
    float speedFontSize = height * 0.28f + 2.0f * m_scale;  // INCREASED by 2 pixels
    ImVec2 speedSize = ImGui::CalcTextSize(speedBuf);
    float speedScale = speedFontSize / speedSize.y;
    float speedX = p.x + (width - speedSize.x * speedScale) * 0.5f;
    float speedY = gearY + gearSize.y * gearScale + 1.0f * m_scale;

    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * speedScale,
                ImVec2(speedX, speedY), IM_COL32(200, 200, 200, 255), speedBuf);

    // "km/h" label: small, below speed - BOLD and increase size by 2px
    float unitFontSize = height * 0.18f + 2.0f * m_scale;  // INCREASED by 2 pixels
    ImVec2 unitSize = ImGui::CalcTextSize(unitBuf);
    float unitScale = unitFontSize / unitSize.y;
    float unitX = p.x + (width - unitSize.x * unitScale) * 0.5f;
    float unitY = speedY + speedSize.y * speedScale + 1.0f * m_scale;

    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * unitScale,
                ImVec2(unitX, unitY), IM_COL32(120, 120, 120, 200), unitBuf);

    ImGui::Dummy(ImVec2(width, height));
}

// =============================================================================
// STEERING WHEEL - Rotated wheel icon or procedural fallback
// Shows actual steering angle with rotation
// =============================================================================
void TelemetryWidget::renderSteering(float width, float height) {
    ImVec2 p = ImGui::GetCursorScreenPos();

    if (m_steeringTexture) {
        // With texture: we can't easily rotate ImGui::Image, so use draw list with UV rotation
        // For simplicity, draw the texture without rotation and overlay a rotation indicator
        float imgSize = std::min(width, height) * 0.85f;
        float imgX = p.x + (width - imgSize) * 0.5f;
        float imgY = p.y + (height - imgSize) * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(imgX, imgY));
        ImGui::Image((ImTextureID)(intptr_t)m_steeringTexture, ImVec2(imgSize, imgSize));

        // Add rotation line overlay
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float cx = imgX + imgSize * 0.5f;
        float cy = imgY + imgSize * 0.5f;
        float r = imgSize * 0.35f;
        float maxAngle = (m_steeringAngleMax > 0.01f) ? m_steeringAngleMax : 7.854f;
        float angle = -(m_steeringAngle / maxAngle) * static_cast<float>(M_PI);
        ImVec2 lineEnd(cx + r * std::sin(angle), cy - r * std::cos(angle));
        dl->AddLine(ImVec2(cx, cy), lineEnd, IM_COL32(255, 200, 0, 200), 2.0f * m_scale);

        // Reset cursor for proper layout
        ImGui::SetCursorScreenPos(ImVec2(p.x + width, p.y));
        ImGui::Dummy(ImVec2(0, height));
    } else {
        // Fallback: draw steering wheel procedurally
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float cx = p.x + width * 0.5f;
        float cy = p.y + height * 0.5f;
        float r = std::min(width, height) * 0.35f;
        float maxAngle = (m_steeringAngleMax > 0.01f) ? m_steeringAngleMax : 7.854f;
        float angle = -(m_steeringAngle / maxAngle) * static_cast<float>(M_PI);

        // Wheel rim
        dl->AddCircle(ImVec2(cx, cy), r, IM_COL32(180, 180, 180, 220), 24, 2.5f * m_scale);

        // Spokes (3 spokes, rotated by steering angle)
        for (int i = 0; i < 3; i++) {
            float spokeAngle = angle + (i * 2.0f * static_cast<float>(M_PI) / 3.0f) - static_cast<float>(M_PI) * 0.5f;
            ImVec2 spokeEnd(cx + r * 0.7f * std::cos(spokeAngle), cy + r * 0.7f * std::sin(spokeAngle));
            dl->AddLine(ImVec2(cx, cy), spokeEnd, IM_COL32(150, 150, 150, 200), 2.0f * m_scale);
        }

        // Center hub
        dl->AddCircleFilled(ImVec2(cx, cy), r * 0.15f, IM_COL32(100, 100, 100, 200), 12);

        // Top marker (reference point, rotates with wheel)
        float markerAngle = angle - static_cast<float>(M_PI) * 0.5f;
        ImVec2 markerPos(cx + r * std::cos(markerAngle), cy + r * std::sin(markerAngle));
        dl->AddCircleFilled(markerPos, 3.0f * m_scale, IM_COL32(255, 200, 0, 255), 8);

        ImGui::Dummy(ImVec2(width, height));
    }
}

}  // namespace ui
