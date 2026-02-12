#include "telemetry_widget.h"
#include "../data/irsdk_manager.h"
#include "overlay_window.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <iostream>
#include <cmath>
#include <algorithm>

// FIXED: Removed STB_IMAGE_IMPLEMENTATION - now in dedicated stb_impl.cpp
#include <stb_image.h>

#include <glad/glad.h>

namespace ui {

TelemetryWidget::TelemetryWidget(OverlayWindow* overlay)
    : m_currentRPM(0.0f)
    , m_maxRPM(7000.0f)
    , m_blinkRPM(6500.0f)
    , m_shiftFirstRPM(0.0f)
    , m_shiftLastRPM(0.0f)
    , m_throttle(0.0f)
    , m_brake(0.0f)
    , m_clutch(0.0f)
    , m_gear(0)
    , m_speed(0)
    , m_steeringAngle(0.0f)
    , m_steeringAngleMax(7.854f)
    , m_absActive(false)
    , m_historyHead(0)
    , m_scale(1.0f)
    , m_overlay(overlay)
    , m_steeringTexture(0)
    , m_absOnTexture(0)
    , m_absOffTexture(0)
{
    m_throttleHistory.resize(256, 0.0f);
    m_brakeHistory.resize(256, 0.0f);

    // Load textures
    loadAssets();
}

TelemetryWidget::~TelemetryWidget() {
    if (m_steeringTexture) glDeleteTextures(1, &m_steeringTexture);
    if (m_absOnTexture) glDeleteTextures(1, &m_absOnTexture);
    if (m_absOffTexture) glDeleteTextures(1, &m_absOffTexture);
}

unsigned int TelemetryWidget::loadTextureFromFile(const char* filepath) {
    GLuint textureID = 0;

    int width, height, nrChannels;
    unsigned char* data = stbi_load(filepath, &width, &height, &nrChannels, STBI_rgb_alpha);

    if (!data) {
        std::cout << "[Telemetry] Failed to load texture: " << filepath << std::endl;
        return 0;
    }

    std::cout << "[Telemetry] Loaded texture: " << filepath << " (" << width << "x" << height << ")" << std::endl;

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
    std::cout << "[Telemetry] Loading assets..." << std::endl;

    // Try multiple path variations — FIXED: steering_wheel.png (not steer_wheel.png)
    const char* steerPaths[] = {
        "assets/telemetry/steering_wheel.png",
        "..\\assets\\telemetry\\steering_wheel.png",
        "../../assets/telemetry/steering_wheel.png"
    };
    const char* absOnPaths[] = {
        "assets/telemetry/abs_on.png",
        "..\\assets\\telemetry\\abs_on.png",
        "../../assets/telemetry/abs_on.png"
    };
    const char* absOffPaths[] = {
        "assets/telemetry/abs_off.png",
        "..\\assets\\telemetry\\abs_off.png",
        "../../assets/telemetry/abs_off.png"
    };

    // Load steering texture
    for (const auto& path : steerPaths) {
        m_steeringTexture = loadTextureFromFile(path);
        if (m_steeringTexture) break;
    }
    if (!m_steeringTexture) {
        std::cout << "[Telemetry] Warning: Could not load steering texture from any path" << std::endl;
    }

    // Load ABS ON texture
    for (const auto& path : absOnPaths) {
        m_absOnTexture = loadTextureFromFile(path);
        if (m_absOnTexture) break;
    }
    if (!m_absOnTexture) {
        std::cout << "[Telemetry] Warning: Could not load ABS ON texture from any path" << std::endl;
    }

    // Load ABS OFF texture
    for (const auto& path : absOffPaths) {
        m_absOffTexture = loadTextureFromFile(path);
        if (m_absOffTexture) break;
    }
    if (!m_absOffTexture) {
        std::cout << "[Telemetry] Warning: Could not load ABS OFF texture from any path" << std::endl;
    }

    std::cout << "[Telemetry] Asset loading complete - Steering: " << m_steeringTexture
              << " ABS ON: " << m_absOnTexture << " ABS OFF: " << m_absOffTexture << std::endl;
}

void TelemetryWidget::setScale(float scale) {
    m_scale = scale;
}

// =============================================================================
// SHIFT LIGHTS - 10 RPM indicator circles (green -> yellow -> orange -> red)
// =============================================================================
void TelemetryWidget::renderShiftLights(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // Calculate RPM percentage (0 to 1)
    float rpmPct = (m_maxRPM > 0) ? (m_currentRPM / m_maxRPM) : 0.0f;
    rpmPct = std::min(1.0f, std::max(0.0f, rpmPct));

    // Redline zone - blink if we're past the blink RPM threshold
    bool blink = std::fmod(ImGui::GetTime() * 2.5f, 1.0f) < 0.5f;
    bool isRedline = (rpmPct >= (m_blinkRPM / m_maxRPM)) && blink;

    // 10 lights: center them in the available width
    float dotRadius = height * 0.35f;
    float spacing = (width / 10.0f);
    float startX = p.x + spacing * 0.5f;

    // Draw lights
    for (int i = 0; i < 10; i++) {
        float x = startX + i * spacing;
        float cy = p.y + height * 0.5f;

        // Calculate threshold for THIS light (0.1, 0.2, 0.3... 1.0)
        float threshold = (i + 1) * 0.1f;

        // Determine if this light should be ON
        bool isActive = rpmPct >= threshold;

        if (isActive) {
            // Light is active - determine color based on position
            ImU32 lightCol;
            if (i < 2) {
                lightCol = IM_COL32(0, 220, 0, 255);     // Green (first 2)
            } else if (i < 8) {
                lightCol = IM_COL32(220, 220, 0, 255);   // Yellow (middle 6)
            } else {
                lightCol = IM_COL32(255, 100, 0, 255);   // Orange (last 2)
            }

            // Override with red if in redline zone AND blinking
            if (isRedline) {
                lightCol = IM_COL32(255, 0, 0, 255);
            }

            // Draw filled circle
            dl->AddCircleFilled(ImVec2(x, cy), dotRadius, lightCol, 12);
        } else {
            // Light is OFF - draw outline only
            ImU32 outlineCol = IM_COL32(80, 80, 80, 150);
            dl->AddCircle(ImVec2(x, cy), dotRadius, outlineCol, 12, 1.5f);
        }
    }

    ImGui::Dummy(ImVec2(width, height));
}

// =============================================================================
// ABS - Asset texture or fallback circle with text (NO BLACK BACKGROUND)
// =============================================================================
void TelemetryWidget::renderABS(float width, float height) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Determine which texture to display
    unsigned int textureToUse = m_absActive ? m_absOnTexture : m_absOffTexture;

    // If we have a texture, render it (HORIZONTAL FLIP ONLY)
    if (textureToUse != 0) {
        float targetSize = std::min(width, height);
        float xOffset = (width - targetSize) * 0.5f;
        float yOffset = (height - targetSize) * 0.5f;

        ImVec2 topLeft = ImVec2(p.x + xOffset, p.y + yOffset);
        ImVec2 bottomRight = ImVec2(topLeft.x + targetSize, topLeft.y + targetSize);

        // HORIZONTAL FLIP ONLY: normal orientation but mirrored left-right
        ImVec2 uv0(1.0f, 1.0f);  // Bottom-right
        ImVec2 uv1(0.0f, 0.0f);  // Top-left

        dl->AddImage((ImTextureID)(intptr_t)textureToUse, topLeft, bottomRight, uv0, uv1,
                     IM_COL32(255, 255, 255, 255));
    } else {
        // Fallback: draw a simple ABS indicator (circle outline + text, NO BACKGROUND)
        ImVec2 center = ImVec2(p.x + width * 0.5f, p.y + height * 0.5f);
        float radius = std::min(width, height) * 0.35f;

        // Outer circle (NO FILL, just outline)
        ImU32 circleCol = m_absActive ? IM_COL32(255, 100, 0, 255) : IM_COL32(80, 80, 80, 200);
        dl->AddCircle(center, radius, circleCol, 12, 2.0f);

        // Text "ABS"
        ImVec2 textSize = ImGui::CalcTextSize("ABS");
        ImVec2 textPos = ImVec2(center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f);
        ImU32 textCol = m_absActive ? IM_COL32(255, 255, 0, 255) : IM_COL32(100, 100, 100, 200);
        dl->AddText(textPos, textCol, "ABS");
    }

    ImGui::Dummy(ImVec2(width, height));
}

// =============================================================================
// PEDAL BARS - Clutch, Brake, Throttle as vertical bars with numbers (REORDERED)
// =============================================================================
void TelemetryWidget::renderPedalBars(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // NEW ORDER: Clutch, Brake, Throttle
    float pedals[] = {m_clutch, m_brake, m_throttle};
    ImU32 colors[] = {
        IM_COL32(0, 150, 255, 255),    // Clutch blue (was index 2)
        IM_COL32(255, 0, 0, 255),      // Brake red (was index 1)
        IM_COL32(0, 220, 0, 255)       // Throttle green (was index 0)
    };

    // FIXED: Match bar height with history trace (start and end at same y positions)
    float barTop = p.y;
    float barBottom = p.y + height;
    float barH = barBottom - barTop;

    // Bar width is the same for all - NO EXTRA OFFSET FOR CLUTCH
    for (int i = 0; i < 3; i++) {
        float columnW = width / 3.0f;
        float barW = columnW * 0.6f;
        
        // Evenly space all bars - 1-2 pixel gap between them
        float x = p.x + i * columnW + (columnW - barW) * 0.5f;

        // Background bar
        dl->AddRectFilled(ImVec2(x, barTop), ImVec2(x + barW, barBottom),
                          IM_COL32(20, 20, 20, 150));

        // Filled bar
        float filledH = barH * pedals[i];
        if (filledH > 0.5f) {
            dl->AddRectFilled(ImVec2(x, barBottom - filledH), ImVec2(x + barW, barBottom), colors[i]);
        }

        // FIXED: Smaller numeric value on top of bar
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", static_cast<int>(pedals[i] * 100.0f));
        
        // Use smaller font for pedal percentage values
        ImVec2 textSize = ImGui::CalcTextSize(buf);
        textSize.x *= 0.85f;  // Reduce text size by ~15%
        textSize.y *= 0.85f;

        // Center text horizontally within each bar's column space
        float columnCenterX = p.x + i * columnW + columnW * 0.5f;
        float textX = columnCenterX - textSize.x * 0.5f;

        // Position numbers above bars with consistent spacing
        float textY = barTop - textSize.y - 4.0f * m_scale;

        // Ensure text doesn't go above the widget bounds
        if (textY < p.y - textSize.y - 2.0f * m_scale) {
            textY = p.y - textSize.y - 2.0f * m_scale;
        }

        // Draw smaller text
        dl->AddText(nullptr, ImGui::GetFontSize() * 0.85f * m_scale, ImVec2(textX, textY), colors[i], buf);
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

    for (int i = 0; i < 255; i++) {
        int idx = (m_historyHead + i) % 256;
        int nextIdx = (m_historyHead + i + 1) % 256;

        float x1 = p.x + i * pixelW;
        float x2 = p.x + (i + 1) * pixelW;

        // Throttle (green) - FIXED: 2px thicker lines
        float y1_throttle = p.y + height - (m_throttleHistory[idx] / 100.0f) * height;
        float y2_throttle = p.y + height - (m_throttleHistory[nextIdx] / 100.0f) * height;
        dl->AddLine(ImVec2(x1, y1_throttle), ImVec2(x2, y2_throttle), IM_COL32(0, 220, 0, 200), 2.0f);

        // Brake (red) - FIXED: 2px thicker lines
        float y1_brake = p.y + height - (m_brakeHistory[idx] / 100.0f) * height;
        float y2_brake = p.y + height - (m_brakeHistory[nextIdx] / 100.0f) * height;
        dl->AddLine(ImVec2(x1, y1_brake), ImVec2(x2, y2_brake), IM_COL32(255, 0, 0, 200), 2.0f);
    }

    ImGui::Dummy(ImVec2(width, height));
}

// =============================================================================
// GEAR/SPEED - Gear number and speed in km/h
// =============================================================================
void TelemetryWidget::renderGearSpeed(float width, float height) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Get gear label
    const char* gearLabel = "N";
    if (m_gear == -1) {
        gearLabel = "R";
    } else if (m_gear > 0) {
        static char gearStr[4];
        snprintf(gearStr, sizeof(gearStr), "%d", m_gear);
        gearLabel = gearStr;
    }

    // FIXED: Gear box - NO black background (transparent), text 1px RIGHT, moved 10px up
    float boxW = width * 0.5f;
    float boxH = height * 0.35f;
    ImVec2 gearTopLeft = ImVec2(p.x + (width - boxW) * 0.5f, p.y + height * 0.1f - 10.0f * m_scale);
    
    // NO background fill - just render the text directly

    // Gear text (white, large, centered, moved 1px RIGHT for alignment with speed text)
    ImVec2 gearSize = ImGui::CalcTextSize(gearLabel);
    ImVec2 gearPos = ImVec2(gearTopLeft.x + (boxW - gearSize.x) * 0.5f + 1.0f * m_scale,
                            gearTopLeft.y + (boxH - gearSize.y) * 0.5f);
    dl->AddText(nullptr, ImGui::GetFontSize() * 1.4f * m_scale, gearPos,
                IM_COL32(255, 255, 255, 255), gearLabel);

    // Speed and km/h below (centered with gear box, moved 10px up, moved 1px RIGHT)
    char speedStr[16];
    snprintf(speedStr, sizeof(speedStr), "%d", m_speed);
    ImVec2 speedSize = ImGui::CalcTextSize(speedStr);
    ImVec2 speedPos = ImVec2(p.x + (width - speedSize.x) * 0.5f + 1.0f * m_scale, p.y + height * 0.55f - 10.0f * m_scale);
    dl->AddText(speedPos, IM_COL32(200, 200, 200, 255), speedStr);

    ImVec2 kmhSize = ImGui::CalcTextSize("km/h");
    ImVec2 kmhPos = ImVec2(p.x + (width - kmhSize.x) * 0.5f + 1.0f * m_scale, p.y + height * 0.75f - 10.0f * m_scale);
    dl->AddText(kmhPos, IM_COL32(150, 150, 150, 200), "km/h");

    ImGui::Dummy(ImVec2(width, height));
}

// =============================================================================
// STEERING - Steering wheel with angle indicator (HORIZONTAL FLIP ONLY)
// =============================================================================
void TelemetryWidget::renderSteering(float width, float height) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // If we have a texture, render it
    if (m_steeringTexture != 0) {
        // FIXED: HORIZONTAL FLIP ONLY and maintain aspect ratio
        float targetSize = std::min(width, height);
        float xOffset = (width - targetSize) * 0.5f;
        float yOffset = (height - targetSize) * 0.5f;

        ImVec2 topLeft = ImVec2(p.x + xOffset, p.y + yOffset);
        ImVec2 bottomRight = ImVec2(topLeft.x + targetSize, topLeft.y + targetSize);

        // HORIZONTAL FLIP ONLY: normal orientation but mirrored left-right
        ImVec2 uv0(1.0f, 1.0f);  // Bottom-right
        ImVec2 uv1(0.0f, 0.0f);  // Top-left

        dl->AddImage((ImTextureID)(intptr_t)m_steeringTexture, topLeft, bottomRight, uv0, uv1,
                     IM_COL32(255, 255, 255, 255));
    } else {
        // Fallback: draw a simple steering wheel representation
        ImVec2 center = ImVec2(p.x + width * 0.5f, p.y + height * 0.5f);
        float radius = std::min(width, height) * 0.35f;

        // Outer circle (wheel rim)
        dl->AddCircle(center, radius, IM_COL32(150, 150, 150, 255), 12, 2.5f);

        // Inner circle
        dl->AddCircle(center, radius * 0.6f, IM_COL32(100, 100, 100, 200), 12, 1.5f);

        // Center point
        dl->AddCircleFilled(center, radius * 0.15f, IM_COL32(200, 200, 200, 255), 8);

        // Steering angle indicator (line from center)
        float angleRad = (m_steeringAngle / m_steeringAngleMax) * 3.14159f;
        float indicatorLen = radius * 0.5f;
        ImVec2 indicatorEnd = ImVec2(center.x + std::sin(angleRad) * indicatorLen,
                                     center.y - std::cos(angleRad) * indicatorLen);
        dl->AddLine(center, indicatorEnd, IM_COL32(255, 200, 0, 255), 2.0f);
    }

    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::render(iracing::IRSDKManager* sdk, utils::WidgetConfig& config, bool editMode) {
    if (!sdk) return;

    // Read from SDK
    if (sdk->isSessionActive()) {
        m_currentRPM = sdk->getFloat("RPM", 0.0f);
        m_maxRPM = sdk->getFloat("ShiftGrindRPM", 7000.0f);
        m_blinkRPM = sdk->getFloat("DriverCarSLBlinkRPM", 6500.0f);
        m_shiftFirstRPM = sdk->getFloat("DriverCarSLFirstRPM", 0.0f);
        m_shiftLastRPM = sdk->getFloat("DriverCarSLLastRPM", 0.0f);
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

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::End();
}

}  // namespace ui
