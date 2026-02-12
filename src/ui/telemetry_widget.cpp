#include "telemetry_widget.h"
#include "../data/irsdk_manager.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace ui {

TelemetryWidget::TelemetryWidget()
    : m_currentRPM(0.0f), m_maxRPM(7000.0f), m_blinkRPM(6500.0f),
      m_throttle(0.0f), m_brake(0.0f), m_clutch(0.0f), m_gear(0), m_speed(0),
      m_steeringAngle(0.0f), m_steeringAngleMax(180.0f),
      m_absActive(false), m_scale(1.0f) {
    // Initialize history buffers
    m_throttleHistory.resize(256, 0.0f);
    m_brakeHistory.resize(256, 0.0f);
    m_clutchHistory.resize(256, 0.0f);
    m_historyHead = 0;
}

TelemetryWidget::~TelemetryWidget() {}

void TelemetryWidget::update(const idata::TelemetryData& data) {
    m_currentRPM = data.rpm;
    m_maxRPM = data.maxRPM;
    m_blinkRPM = data.blinkRPM;
    m_throttle = data.throttle;
    m_brake = data.brake;
    m_clutch = data.clutch;
    m_gear = data.gear;
    m_speed = data.speed;
    m_steeringAngle = data.steeringAngle;
    m_absActive = data.absActive;

    // Push to history (0-100 range)
    m_throttleHistory[m_historyHead] = m_throttle * 100.0f;
    m_brakeHistory[m_historyHead] = m_brake * 100.0f;
    m_clutchHistory[m_historyHead] = m_clutch * 100.0f;
    m_historyHead = (m_historyHead + 1) % 256;
}

void TelemetryWidget::setScale(float scale) {
    m_scale = scale;
}

void TelemetryWidget::render(utils::WidgetConfig& config, bool editMode) {
    const float padY = 6.0f * m_scale;
    const float rpmH = 24.0f * m_scale;
    const float rowH = 28.0f * m_scale;
    const float gapRpm = 6.0f * m_scale;  // Reduced gap between RPM and input row

    float totalW = 300.0f * m_scale;
    float totalH = rpmH + gapRpm + rowH + padY * 2.0f;

    ImGui::SetNextWindowSize(ImVec2(totalW, totalH), ImGuiCond_Always);

    if (config.posX < 0 || config.posY < 0) {
        // Default position: PosX=690 PosY=720
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
    if (editMode) {
        // allow move + resize
    } else {
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoResize;
    }

    ImGui::Begin("##TELEMETRY", nullptr, flags);
    ImGui::SetWindowFontScale(m_scale);

    ImVec2 pos = ImGui::GetWindowPos();
    config.posX = pos.x;
    config.posY = pos.y;

    float contentW = ImGui::GetContentRegionAvail().x;

    // RPM lights (8 lights, first light removed so now 7)
    renderShiftLights(contentW, rpmH);
    ImGui::Dummy(ImVec2(0, gapRpm));

    // Layout dimensions
    float absW = rowH;
    float barsW = 30.0f * m_scale;
    float gearW = 44.0f * m_scale;
    float steerW = rowH;
    float gap = 4.0f * m_scale;
    float gapTight = 1.0f * m_scale;
    float marginAbsLR = 2.0f * m_scale;  // Margin left and right of ABS

    // History trace width reduced by 65 pixels
    float traceW = contentW - absW - barsW - gearW - steerW - gap * 2.0f - gapTight * 2.0f - marginAbsLR * 2.0f - 65.0f;

    // Main row with all inputs
    ImVec2 rowCursorStart = ImGui::GetCursorScreenPos();

    // ABS widget with left and right margin
    ImGui::SetCursorScreenPos(ImVec2(rowCursorStart.x + marginAbsLR, rowCursorStart.y));
    renderABS(absW, rowH);

    ImGui::SameLine(0.0f, gap);

    // Pedal bars section
    ImGui::SetCursorScreenPos(ImVec2(ImGui::GetCursorScreenPos().x + gap, rowCursorStart.y));
    renderPedalBars(barsW, rowH);

    ImGui::SameLine(0.0f, gapTight);

    // History trace
    renderHistoryTrace(traceW, rowH);

    ImGui::SameLine(0.0f, gapTight);

    // Gear + Speed section (removed pixels from gap)
    renderGearSpeed(gearW, rowH);

    ImGui::SameLine(0.0f, gapTight);

    // Steering wheel
    renderSteering(steerW, rowH);

    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void TelemetryWidget::renderShiftLights(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    float rpmPct = (m_maxRPM > 0) ? std::min(m_currentRPM / m_maxRPM, 1.0f) : 0.0f;
    int numLights = 7;  // Reduced from 8 - first light removed
    float gap = 2.0f * m_scale;
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
                if (pct > 0.7f) {
                    col = IM_COL32(255, 165, 0, 255);
                } else if (pct > 0.5f) {
                    col = IM_COL32(255, 255, 0, 255);
                } else {
                    col = IM_COL32(0, 255, 0, 255);
                }
            }
        }

        dl->AddRectFilled(tl, br, col);
    }

    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderABS(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 c = ImGui::GetCursorScreenPos();
    c.x += width * 0.5f;
    c.y += height * 0.5f;

    float r = std::min(width, height) * 0.35f;
    ImU32 ringCol = m_absActive ? IM_COL32(255, 255, 255, 255) : IM_COL32(90, 90, 90, 200);
    ImU32 bgCol = m_absActive ? IM_COL32(255, 255, 255, 255) : IM_COL32(50, 50, 50, 200);
    ImU32 txtCol = m_absActive ? IM_COL32(0, 0, 0, 255) : IM_COL32(140, 140, 140, 255);

    dl->AddCircleFilled(c, r, bgCol, 32);
    dl->AddCircle(c, r, ringCol, 32, 1.5f * m_scale);

    // Only 1 outer arc (removed one of the two)
    float arcR = r + 3.0f * m_scale;
    float thick = 1.5f * m_scale;

    // Left arcs
    dl->PathArcTo(c, arcR, static_cast<float>(M_PI * 0.65), static_cast<float>(M_PI * 1.35), 16);
    dl->PathStroke(ringCol, false, thick);

    // Right arcs
    dl->PathArcTo(c, arcR, static_cast<float>(-M_PI * 0.35), static_cast<float>(M_PI * 0.35), 16);
    dl->PathStroke(ringCol, false, thick);

    // "ABS" text - bold and pixelated
    const char* txt = "ABS";
    ImVec2 ts = ImGui::CalcTextSize(txt);
    float desiredFontSize = r * 0.85f;
    float textScale = desiredFontSize / ts.y;

    ImGui::PushFont(ImGui::GetFont());
    dl->AddText(ImGui::GetFont(),
                ImGui::GetFontSize() * textScale * 1.2f,  // Make font larger
                ImVec2(c.x - ts.x * textScale * 0.5f,
                       c.y - ts.y * textScale * 0.5f),
                txtCol, txt);
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderPedalBars(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // Reduce pedal bars by 15% vertically
    float barHeight = height * 0.85f;
    float topGap = (height - barHeight) * 0.5f;  // Center the gap

    float barW = (width - 8.0f * m_scale) / 3.0f;
    float gap = 2.0f * m_scale;

    // Order: Clutch, Brake, Throttle
    float pedals[3] = {m_clutch, m_brake, m_throttle};
    ImU32 colors[3] = {
        IM_COL32(100, 149, 237, 255),  // Clutch - cornflower blue
        IM_COL32(255, 0, 0, 255),      // Brake - red
        IM_COL32(0, 255, 0, 255)       // Throttle - green
    };

    for (int i = 0; i < 3; i++) {
        float x = p.x + i * (barW + gap);
        float filledH = barHeight * pedals[i];
        float y = p.y + topGap + (barHeight - filledH);

        // Background bar
        ImVec2 bgTL(x, p.y + topGap);
        ImVec2 bgBR(x + barW, p.y + topGap + barHeight);
        dl->AddRectFilled(bgTL, bgBR, IM_COL32(20, 20, 20, 200));
        dl->AddRect(bgTL, bgBR, IM_COL32(100, 100, 100, 150), 0.0f, 0, 1.0f * m_scale);

        // Filled bar (stronger/bolder)
        ImVec2 filledTL(x, y);
        ImVec2 filledBR(x + barW, p.y + topGap + barHeight);
        dl->AddRectFilled(filledTL, filledBR, colors[i]);

        // Percentage text - center vertically in the gap
        float pctValue = pedals[i] * 100.0f;
        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f", pctValue);

        ImVec2 textSize = ImGui::CalcTextSize(buf);
        float textX = x + barW * 0.5f - textSize.x * 0.5f;
        float textY = p.y + topGap * 0.5f - textSize.y * 0.5f;

        dl->AddText(ImVec2(textX, textY), IM_COL32(255, 255, 255, 255), buf);
    }

    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderHistoryTrace(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // Draw background without border
    dl->AddRectFilled(p, ImVec2(p.x + width, p.y + height), IM_COL32(20, 20, 20, 200));

    // Draw horizontal dotted lines at 0%, 25%, 50%, 75%, 100%
    float lineThickness = 1.0f * m_scale;
    ImU32 gridCol = IM_COL32(80, 80, 80, 100);
    float percentages[5] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (int i = 0; i < 5; i++) {
        float y = p.y + height * (1.0f - percentages[i]);
        // Draw dotted line
        for (float x = p.x; x < p.x + width; x += 4.0f * m_scale) {
            dl->AddLine(
                ImVec2(x, y),
                ImVec2(std::min(x + 2.0f * m_scale, p.x + width), y),
                gridCol,
                lineThickness
            );
        }
    }

    // Draw pedal traces (rendered on top of grid)
    float pixelW = width / 256.0f;
    float traceThickness = 2.0f * m_scale;  // Bolder/stronger traces

    // Clutch (blue) - drawn first (behind)
    for (int i = 0; i < 256; i++) {
        int next = (i + 1) % 256;
        float x1 = p.x + i * pixelW;
        float x2 = p.x + next * pixelW;
        float y1 = p.y + height * (1.0f - m_clutchHistory[i] / 100.0f);
        float y2 = p.y + height * (1.0f - m_clutchHistory[next] / 100.0f);
        dl->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(100, 149, 237, 200), traceThickness);
    }

    // Brake (red)
    for (int i = 0; i < 256; i++) {
        int next = (i + 1) % 256;
        float x1 = p.x + i * pixelW;
        float x2 = p.x + next * pixelW;
        float y1 = p.y + height * (1.0f - m_brakeHistory[i] / 100.0f);
        float y2 = p.y + height * (1.0f - m_brakeHistory[next] / 100.0f);
        dl->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(255, 0, 0, 200), traceThickness);
    }

    // Throttle (green) - drawn last (on top)
    for (int i = 0; i < 256; i++) {
        int next = (i + 1) % 256;
        float x1 = p.x + i * pixelW;
        float x2 = p.x + next * pixelW;
        float y1 = p.y + height * (1.0f - m_throttleHistory[i] / 100.0f);
        float y2 = p.y + height * (1.0f - m_throttleHistory[next] / 100.0f);
        dl->AddLine(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(0, 255, 0, 200), traceThickness);
    }

    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderGearSpeed(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // Split into two columns (gear and speed)
    float colW = width * 0.5f;

    // GEAR
    char gearBuf[8];
    if (m_gear == 0) {
        snprintf(gearBuf, sizeof(gearBuf), "R");
    } else {
        snprintf(gearBuf, sizeof(gearBuf), "%d", m_gear);
    }

    ImVec2 gearTextSize = ImGui::CalcTextSize(gearBuf);
    float gearScale = (height * 0.6f) / gearTextSize.y;
    float gearX = p.x + colW * 0.5f - gearTextSize.x * gearScale * 0.5f;
    float gearY = p.y + height * 0.5f - gearTextSize.y * gearScale * 0.5f;

    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * gearScale * 1.25f,  // Bold and 2px bigger
                ImVec2(gearX, gearY), IM_COL32(255, 255, 255, 255), gearBuf);

    // SPEED
    char speedBuf[16];
    snprintf(speedBuf, sizeof(speedBuf), "%d", m_speed);

    ImVec2 speedTextSize = ImGui::CalcTextSize(speedBuf);
    float speedScale = (height * 0.6f) / speedTextSize.y;
    float speedX = p.x + colW + colW * 0.5f - speedTextSize.x * speedScale * 0.5f;
    float speedY = p.y + height * 0.5f - speedTextSize.y * speedScale * 0.5f;

    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * speedScale * 1.25f,  // Bold and 2px bigger
                ImVec2(speedX, speedY), IM_COL32(255, 255, 255, 255), speedBuf);

    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderSteering(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 c = ImGui::GetCursorScreenPos();
    c.x += width * 0.5f;
    c.y += height * 0.5f;

    float r = std::min(width, height) * 0.35f;
    float angle = (m_steeringAngle / m_steeringAngleMax) * static_cast<float>(M_PI);

    // Steering wheel circle
    ImU32 wheelCol = IM_COL32(100, 100, 100, 200);
    dl->AddCircle(c, r, wheelCol, 32, 1.5f * m_scale);

    // Steering indicator line
    ImVec2 lineEnd(c.x + r * std::sin(angle), c.y - r * std::cos(angle));
    dl->AddLine(c, lineEnd, IM_COL32(255, 255, 255, 255), 2.0f * m_scale);

    ImGui::Dummy(ImVec2(width, height));
}

}  // namespace ui
