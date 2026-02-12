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
    m_clutchHistory.resize(256, 0.0f);
    
    loadAssets();
}

TelemetryWidget::~TelemetryWidget() {
    // Clean up OpenGL textures
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

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Load texture data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);

    return textureID;
}

void TelemetryWidget::loadAssets() {
    // Load steering wheel texture
    m_steeringTexture = loadTextureFromFile("assets/telemetry/steering_wheel.png");
    
    // Load ABS on texture
    m_absOnTexture = loadTextureFromFile("assets/telemetry/abs_on.png");
    
    // Load ABS off texture
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
        m_clutch = sdk->getFloat("Clutch", 0.0f);
        m_gear = sdk->getInt("Gear", 0);
        m_speed = static_cast<int>(sdk->getFloat("Speed", 0.0f));
        m_steeringAngle = sdk->getFloat("SteeringWheelAngle", 0.0f);
        m_absActive = sdk->getBool("BrakeABSactive", false);

        // Update history
        m_throttleHistory[m_historyHead] = m_throttle * 100.0f;
        m_brakeHistory[m_historyHead] = m_brake * 100.0f;
        m_clutchHistory[m_historyHead] = m_clutch * 100.0f;
        m_historyHead = (m_historyHead + 1) % 256;
    }

    const float rpmH = 16.0f * m_scale;
    const float rowH = 32.0f * m_scale;
    const float gapRpm = 2.0f * m_scale;
    const float padY = 4.0f * m_scale;

    float totalW = 300.0f * m_scale;
    float totalH = rpmH + gapRpm + rowH + padY * 2.0f;

    ImGui::SetNextWindowSize(ImVec2(totalW, totalH), ImGuiCond_Always);

    if (config.posX < 0 || config.posY < 0) {
        config.posX = 690.0f;
        config.posY = 720.0f;
    }

    ImGui::SetNextWindowPos(ImVec2(config.posX, config.posY), ImGuiCond_Once);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.12f, config.alpha));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, padY));
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

    renderShiftLights(contentW, rpmH);
    ImGui::Dummy(ImVec2(0, gapRpm));

    float absW = 22.0f * m_scale;
    float steerW = 22.0f * m_scale;
    float barsW = 24.0f * m_scale;
    float gearW = 36.0f * m_scale;
    float gap = 2.0f * m_scale;

    float traceW = contentW - absW - steerW - barsW - gearW - gap * 5.0f;

    // Render all components in row
    renderABS(absW, rowH);
    ImGui::SameLine(0.0f, gap);

    renderHistoryTrace(traceW, rowH);
    ImGui::SameLine(0.0f, gap);

    renderPedalBars(barsW, rowH);
    ImGui::SameLine(0.0f, gap);

    renderGearSpeed(gearW, rowH);
    ImGui::SameLine(0.0f, gap);

    renderSteering(steerW, rowH);

    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void TelemetryWidget::setScale(float scale) {
    m_scale = scale;
}

void TelemetryWidget::renderShiftLights(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    float rpmPct = (m_maxRPM > 0) ? std::min(m_currentRPM / m_maxRPM, 1.0f) : 0.0f;
    int numLights = 7;
    float gap = 1.5f * m_scale;
    float lightW = (width - (numLights - 1) * gap) / numLights;
    int activeLights = static_cast<int>(rpmPct * numLights + 0.5f);

    for (int i = 0; i < numLights; i++) {
        float x = p.x + i * (lightW + gap);
        ImVec2 tl(x, p.y);
        ImVec2 br(x + lightW, p.y + height);

        ImU32 col = IM_COL32(30, 30, 30, 200);

        if (i < activeLights) {
            if (m_blinkRPM > 0 && m_currentRPM >= m_blinkRPM) {
                int blink = static_cast<int>(ImGui::GetTime() * 10) % 2;
                col = blink ? IM_COL32(255, 0, 0, 255) : IM_COL32(80, 0, 0, 255);
            } else if (i == numLights - 1) {
                col = IM_COL32(255, 0, 0, 255);
            } else {
                float pct = static_cast<float>(i) / numLights;
                col = (pct > 0.7f) ? IM_COL32(255, 165, 0, 255) : 
                      (pct > 0.5f) ? IM_COL32(255, 255, 0, 255) : IM_COL32(0, 255, 0, 255);
            }
        }

        dl->AddRectFilled(tl, br, col);
    }

    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderABS(float width, float height) {
    ImVec2 p = ImGui::GetCursorScreenPos();

    if (m_absActive && m_absOnTexture) {
        // Render ABS on texture
        ImGui::Image((ImTextureID)(intptr_t)m_absOnTexture, ImVec2(width, height));
    } else if (!m_absActive && m_absOffTexture) {
        // Render ABS off texture
        ImGui::Image((ImTextureID)(intptr_t)m_absOffTexture, ImVec2(width, height));
    } else {
        // Fallback: draw circles
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 c = ImGui::GetCursorScreenPos();
        c.x += width * 0.5f;
        c.y += height * 0.5f;

        float r = std::min(width, height) * 0.4f;
        ImU32 bgCol = m_absActive ? IM_COL32(255, 200, 0, 220) : IM_COL32(50, 50, 50, 200);
        ImU32 ringCol = m_absActive ? IM_COL32(255, 255, 255, 255) : IM_COL32(100, 100, 100, 180);

        dl->AddCircleFilled(c, r, bgCol, 16);
        dl->AddCircle(c, r, ringCol, 16, 1.0f * m_scale);

        const char* txt = "ABS";
        ImVec2 ts = ImGui::CalcTextSize(txt);
        float textScale = (r * 0.8f) / ts.y;
        dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * textScale,
                    ImVec2(c.x - ts.x * textScale * 0.5f, c.y - ts.y * textScale * 0.5f),
                    IM_COL32(0, 0, 0, 255), txt);

        ImGui::Dummy(ImVec2(width, height));
    }
}

void TelemetryWidget::renderPedalBars(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    float barHeight = height * 0.8f;
    float topGap = (height - barHeight) * 0.5f;
    float barW = (width - 4.0f * m_scale) / 3.0f;
    float gap = 1.0f * m_scale;

    float pedals[3] = {m_clutch, m_brake, m_throttle};
    ImU32 colors[3] = {
        IM_COL32(100, 149, 237, 255),
        IM_COL32(255, 0, 0, 255),
        IM_COL32(0, 255, 0, 255)
    };

    for (int i = 0; i < 3; i++) {
        float x = p.x + i * (barW + gap);
        float filledH = barHeight * pedals[i];
        float y = p.y + topGap + (barHeight - filledH);

        // Background
        dl->AddRectFilled(ImVec2(x, p.y + topGap), ImVec2(x + barW, p.y + topGap + barHeight),
                         IM_COL32(20, 20, 20, 200));

        // Filled bar
        dl->AddRectFilled(ImVec2(x, y), ImVec2(x + barW, p.y + topGap + barHeight), colors[i]);
    }

    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderHistoryTrace(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // Background
    dl->AddRectFilled(p, ImVec2(p.x + width, p.y + height), IM_COL32(20, 20, 20, 200));

    // Grid lines at 25%, 50%, 75%
    ImU32 gridCol = IM_COL32(80, 80, 80, 100);
    float gridYs[3] = {p.y + height * 0.25f, p.y + height * 0.5f, p.y + height * 0.75f};
    
    for (int g = 0; g < 3; g++) {
        for (float x = p.x; x < p.x + width; x += 3.0f * m_scale) {
            dl->AddLine(ImVec2(x, gridYs[g]), ImVec2(x + 1.5f * m_scale, gridYs[g]), gridCol, 0.5f);
        }
    }

    // Draw traces
    float pixelW = width / 256.0f;
    float traceThickness = 1.5f * m_scale;

    auto drawTrace = [&](const std::vector<float>& history, ImU32 color) {
        for (int i = 0; i < 256; i++) {
            int next = (i + 1) % 256;
            float x1 = p.x + i * pixelW;
            float x2 = p.x + next * pixelW;
            float y1 = p.y + height * (1.0f - history[i] / 100.0f);
            float y2 = p.y + height * (1.0f - history[next] / 100.0f);
            dl->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), color, traceThickness);
        }
    };

    drawTrace(m_clutchHistory, IM_COL32(100, 149, 237, 200));
    drawTrace(m_brakeHistory, IM_COL32(255, 0, 0, 200));
    drawTrace(m_throttleHistory, IM_COL32(0, 255, 0, 200));

    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderGearSpeed(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    char gearBuf[16];
    char speedBuf[16];
    
    // Format gear
    if (m_gear == 0) {
        snprintf(gearBuf, sizeof(gearBuf), "R");
    } else if (m_gear < 0) {
        snprintf(gearBuf, sizeof(gearBuf), "P");
    } else {
        snprintf(gearBuf, sizeof(gearBuf), "%d", m_gear);
    }
    
    // Format speed
    snprintf(speedBuf, sizeof(speedBuf), "%d", m_speed);

    ImVec2 gearSize = ImGui::CalcTextSize(gearBuf);
    ImVec2 speedSize = ImGui::CalcTextSize(speedBuf);

    float gearX = p.x + width * 0.25f - gearSize.x * 0.5f;
    float speedX = p.x + width * 0.75f - speedSize.x * 0.5f;
    float textY = p.y + height * 0.5f - gearSize.y * 0.5f;

    dl->AddText(ImVec2(gearX, textY), IM_COL32(255, 200, 0, 255), gearBuf);
    dl->AddText(ImVec2(speedX, textY), IM_COL32(0, 200, 255, 255), speedBuf);

    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderSteering(float width, float height) {
    ImVec2 p = ImGui::GetCursorScreenPos();

    if (m_steeringTexture) {
        // Render steering wheel texture
        ImGui::Image((ImTextureID)(intptr_t)m_steeringTexture, ImVec2(width, height));
    } else {
        // Fallback: draw steering wheel
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 c = p;
        c.x += width * 0.5f;
        c.y += height * 0.5f;

        float r = std::min(width, height) * 0.35f;
        float angle = (m_steeringAngle / m_steeringAngleMax) * static_cast<float>(M_PI);

        dl->AddCircle(c, r, IM_COL32(100, 100, 100, 200), 16, 1.0f * m_scale);

        ImVec2 lineEnd(c.x + r * std::sin(angle), c.y - r * std::cos(angle));
        dl->AddLine(c, lineEnd, IM_COL32(255, 200, 0, 255), 2.0f * m_scale);

        ImGui::Dummy(ImVec2(width, height));
    }
}

}  // namespace ui
