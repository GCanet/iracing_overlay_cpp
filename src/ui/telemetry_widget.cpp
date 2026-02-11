#include "telemetry_widget.h"
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include "utils/config.h"
#include "data/irsdk_manager.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
        m_currentSpeed    = sdk->getFloat("Speed", 0.0f) * 3.6f;

        updateTachometer(sdk);
        updateHistory(m_currentThrottle, m_currentBrake, m_currentClutch, m_currentSteer);
    }

    auto& config = utils::Config::getTelemetryConfig();

    // ── Compact layout: match iRacing game widget height ──
    // Row 1: RPM shift lights (thin strip)
    // Row 2: ABS | Trace | Bars | Gear+Speed | Steering (single row)
    float rowH   = 36.0f * m_scale;       // main content row height
    float rpmH   = 10.0f * m_scale;       // thin RPM strip
    float padY   = 3.0f * m_scale;
    float totalH = rpmH + padY + rowH + padY * 2.0f;  // ~58 px at 1x
    float totalW = 480.0f * m_scale;

    ImGui::SetNextWindowSize(ImVec2(totalW, totalH), ImGuiCond_Always);

    if (config.posX < 0 || config.posY < 0) {
        ImGuiIO& io = ImGui::GetIO();
        config.posX = (io.DisplaySize.x - totalW) * 0.5f;
        config.posY = io.DisplaySize.y - totalH - 20.0f;
    }

    ImGui::SetNextWindowPos(ImVec2(config.posX, config.posY), ImGuiCond_Once);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, config.alpha));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, padY));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (!editMode) {
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs;
    }

    ImGui::Begin("##TELEMETRY", nullptr, flags);
    ImGui::SetWindowFontScale(m_scale);

    ImVec2 pos = ImGui::GetWindowPos();
    config.posX = pos.x;
    config.posY = pos.y;

    float contentW = ImGui::GetContentRegionAvail().x;

    // ── Row 1: RPM shift lights (full width, thin) ─────────
    renderShiftLights(contentW, rpmH);

    ImGui::Spacing();

    // ── Row 2: All elements in one horizontal lane ──────────
    // Sizes for each element
    float absW    = rowH;              // square
    float traceW  = contentW * 0.42f;  // biggest chunk
    float barsW   = 30.0f * m_scale;   // 3 thin bars
    float gearW   = 44.0f * m_scale;   // gear + speed stacked
    float steerW  = rowH;              // square
    float gap     = 4.0f * m_scale;    // spacing between elements

    // ABS
    renderABSIndicator(absW, rowH);
    ImGui::SameLine(0, gap);

    // History trace
    renderInputTrace(traceW, rowH);
    ImGui::SameLine(0, gap);

    // Input bars
    renderInputBarsCompact(barsW, rowH);
    ImGui::SameLine(0, gap);

    // Gear + Speed (stacked vertically, centred)
    renderGearDisplay(gearW, rowH);
    ImGui::SameLine(0, gap);

    // Steering wheel
    renderSteeringWheelCompact(steerW, rowH);

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

// ──────────────────────────────────────────────────────────────
// Update
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
// Render: ABS  (irdashies-style half-circle icon)
// ──────────────────────────────────────────────────────────────

void TelemetryWidget::renderABSIndicator(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 c(p.x + width * 0.5f, p.y + height * 0.5f);
    float r = std::min(width, height) * 0.42f;

    // Outer ring / half-circle arcs (like the irdashies "((ABS))" icon)
    ImU32 ringCol = m_absActive ? IM_COL32(255, 255, 255, 255)
                                : IM_COL32(90, 90, 90, 200);
    ImU32 bgCol   = m_absActive ? IM_COL32(255, 255, 255, 255)
                                : IM_COL32(50, 50, 50, 200);
    ImU32 txtCol  = m_absActive ? IM_COL32(0, 0, 0, 255)
                                : IM_COL32(140, 140, 140, 255);

    // Filled circle background
    dl->AddCircleFilled(c, r, bgCol, 32);

    // Inner ring
    dl->AddCircle(c, r, ringCol, 32, 1.5f * m_scale);

    // Outer half-arcs  (( and ))
    float arcR  = r + 3.0f * m_scale;
    float arcR2 = r + 6.0f * m_scale;
    float thick = 1.5f * m_scale;
    // Left arcs
    dl->PathArcTo(c, arcR,  (float)(M_PI * 0.65), (float)(M_PI * 1.35), 16);
    dl->PathStroke(ringCol, 0, thick);
    dl->PathArcTo(c, arcR2, (float)(M_PI * 0.7),  (float)(M_PI * 1.3),  16);
    dl->PathStroke(ringCol, 0, thick);
    // Right arcs
    dl->PathArcTo(c, arcR,  (float)(-M_PI * 0.35), (float)(M_PI * 0.35), 16);
    dl->PathStroke(ringCol, 0, thick);
    dl->PathArcTo(c, arcR2, (float)(-M_PI * 0.3),  (float)(M_PI * 0.3),  16);
    dl->PathStroke(ringCol, 0, thick);

    // "ABS" text, scaled down to fit
    float prevScale = ImGui::GetFont()->Scale;
    float fontSize  = r * 0.85f;
    const char* txt = "ABS";
    // Estimate text size at desired font size
    ImVec2 ts = ImGui::CalcTextSize(txt);
    float scale = fontSize / ts.y;
    ImVec2 tsScaled(ts.x * scale, ts.y * scale);
    // Use AddText overload with font size
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * scale,
                ImVec2(c.x - tsScaled.x * 0.5f, c.y - tsScaled.y * 0.5f),
                txtCol, txt);

    ImGui::Dummy(ImVec2(width, height));
}

// ──────────────────────────────────────────────────────────────
// Render: RPM shift lights
// ──────────────────────────────────────────────────────────────

void TelemetryWidget::renderShiftLights(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float rpmPct = (m_maxRPM > 0) ? std::min(m_currentRPM / m_maxRPM, 1.0f) : 0.0f;
    int numLights = 13;
    float gap = 2.0f * m_scale;
    float lightW = (width - (numLights - 1) * gap) / numLights;
    int activeLights = (int)(rpmPct * numLights);

    for (int i = 0; i < numLights; i++) {
        float x = p.x + i * (lightW + gap);
        ImVec2 tl(x, p.y);
        ImVec2 br(x + lightW, p.y + height);

        ImU32 col = IM_COL32(30, 30, 30, 200);
        if (i < activeLights) {
            float pct = (float)i / numLights;
            if (m_blinkRPM > 0 && m_currentRPM >= m_blinkRPM) {
                int blink = (int)(ImGui::GetTime() * 10) % 2;
                col = blink ? IM_COL32(255, 0, 0, 255) : IM_COL32(80, 0, 0, 255);
            } else if (pct > 0.8f) col = IM_COL32(0, 200, 0, 255);
            else if (pct > 0.55f) col = IM_COL32(255, 255, 0, 255);
            else col = IM_COL32(0, 200, 0, 255);
        }
        dl->AddRectFilled(tl, br, col, 2.0f);
    }
    ImGui::Dummy(ImVec2(width, height));
}

// ──────────────────────────────────────────────────────────────
// Render: Input trace graph
// ──────────────────────────────────────────────────────────────

void TelemetryWidget::renderInputTrace(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    dl->AddRectFilled(p, ImVec2(p.x + width, p.y + height), IM_COL32(15, 15, 15, 220));
    dl->AddRect(p, ImVec2(p.x + width, p.y + height), IM_COL32(50, 50, 50, 200));

    int n = static_cast<int>(m_throttleHistory.size());
    if (n < 2) { ImGui::Dummy(ImVec2(width, height)); return; }

    float step = width / (float)(n - 1);

    auto trace = [&](const std::deque<float>& h, ImU32 col, float lw) {
        for (int i = 1; i < n; i++) {
            float x0 = p.x + (i - 1) * step;
            float x1 = p.x + i * step;
            float y0 = p.y + height - h[i - 1] * height;
            float y1 = p.y + height - h[i] * height;
            dl->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), col, lw);
        }
    };

    if (m_showThrottle) trace(m_throttleHistory, IM_COL32(0, 220, 0, 220), 2.0f * m_scale);
    if (m_showBrake)    trace(m_brakeHistory,    IM_COL32(255, 0, 0, 220), 2.0f * m_scale);
    if (m_showClutch)   trace(m_clutchHistory,   IM_COL32(60, 120, 255, 160), 1.5f * m_scale);
    if (m_showSteering) trace(m_steerHistory,    IM_COL32(255, 255, 255, 90), 1.0f * m_scale);

    if (m_showABS) {
        for (int i = 0; i < n; i++) {
            if (m_absActiveHistory[i]) {
                float x = p.x + i * step;
                dl->AddLine(ImVec2(x, p.y), ImVec2(x, p.y + height), IM_COL32(255, 255, 0, 30));
            }
        }
    }

    ImGui::Dummy(ImVec2(width, height));
}

// ──────────────────────────────────────────────────────────────
// Render: Input bars (T / B / C) – thin vertical
// ──────────────────────────────────────────────────────────────

void TelemetryWidget::renderInputBarsCompact(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 sp = ImGui::GetCursorScreenPos();

    float gap = 2.0f * m_scale;
    float barW = (width - gap * 2.0f) / 3.0f;

    struct Bar { float val; ImU32 col; };
    Bar bars[3] = {
        { m_currentThrottle, IM_COL32(0, 220, 0, 230) },
        { m_currentBrake,    (m_absActive ? IM_COL32(255, 255, 0, 230) : IM_COL32(255, 0, 0, 230)) },
        { m_currentClutch,   IM_COL32(60, 120, 255, 230) }
    };

    for (int i = 0; i < 3; i++) {
        float x = sp.x + i * (barW + gap);
        float h = bars[i].val * height;
        // Outline
        dl->AddRect(ImVec2(x, sp.y), ImVec2(x + barW, sp.y + height), IM_COL32(180, 180, 180, 80));
        // Fill from bottom
        dl->AddRectFilled(ImVec2(x, sp.y + height - h), ImVec2(x + barW, sp.y + height), bars[i].col);
    }

    ImGui::Dummy(ImVec2(width, height));
}

// ──────────────────────────────────────────────────────────────
// Render: Gear (large) + Speed (small) stacked vertically
// ──────────────────────────────────────────────────────────────

void TelemetryWidget::renderGearDisplay(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // Gear string
    char gearStr[4];
    if (m_currentGear == -1) snprintf(gearStr, 4, "R");
    else if (m_currentGear == 0) snprintf(gearStr, 4, "N");
    else snprintf(gearStr, 4, "%d", m_currentGear);

    // Speed string
    char spdStr[16];
    snprintf(spdStr, 16, "%.0f", m_currentSpeed);

    // Gear color
    ImU32 gearCol = IM_COL32(255, 255, 255, 255);
    if (m_currentGear == -1) gearCol = IM_COL32(255, 100, 100, 255);
    else if (m_currentGear == 0) gearCol = IM_COL32(180, 180, 180, 255);

    // Gear text – big, upper 60% of height
    float gearFontScale = (height * 0.55f) / ImGui::GetFontSize();
    ImVec2 gearSz = ImGui::CalcTextSize(gearStr);
    gearSz.x *= gearFontScale;
    gearSz.y *= gearFontScale;
    float gearX = p.x + (width - gearSz.x) * 0.5f;
    float gearY = p.y + height * 0.05f;
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * gearFontScale,
                ImVec2(gearX, gearY), gearCol, gearStr);

    // Speed text – small, lower portion
    float spdFontScale = (height * 0.28f) / ImGui::GetFontSize();
    ImVec2 spdSz = ImGui::CalcTextSize(spdStr);
    spdSz.x *= spdFontScale;
    spdSz.y *= spdFontScale;
    float spdX = p.x + (width - spdSz.x) * 0.5f;
    float spdY = p.y + height * 0.62f;
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * spdFontScale,
                ImVec2(spdX, spdY), IM_COL32(200, 200, 200, 220), spdStr);

    // "km/h" label tiny
    const char* unit = "km/h";
    float unitScale = (height * 0.16f) / ImGui::GetFontSize();
    ImVec2 uSz = ImGui::CalcTextSize(unit);
    uSz.x *= unitScale; uSz.y *= unitScale;
    float uX = p.x + (width - uSz.x) * 0.5f;
    float uY = spdY + spdSz.y;
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * unitScale,
                ImVec2(uX, uY), IM_COL32(140, 140, 140, 180), unit);

    ImGui::Dummy(ImVec2(width, height));
}

// ──────────────────────────────────────────────────────────────
// Render: Steering wheel (circle + indicator line)
// ──────────────────────────────────────────────────────────────

void TelemetryWidget::renderSteeringWheelCompact(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 c(p.x + width * 0.5f, p.y + height * 0.5f);
    float r = std::min(width, height) * 0.40f;

    // Wheel rim
    dl->AddCircle(c, r, IM_COL32(200, 200, 200, 200), 32, 2.0f * m_scale);

    // Cross-bar (horizontal spoke)
    dl->AddLine(ImVec2(c.x - r * 0.6f, c.y), ImVec2(c.x + r * 0.6f, c.y),
                IM_COL32(160, 160, 160, 160), 1.5f * m_scale);

    // Indicator dot showing rotation
    float angle = -m_currentSteer;
    float dotX = c.x + r * 0.75f * sinf(angle);
    float dotY = c.y - r * 0.75f * cosf(angle);
    dl->AddLine(c, ImVec2(dotX, dotY), IM_COL32(255, 255, 0, 220), 1.5f * m_scale);
    dl->AddCircleFilled(ImVec2(dotX, dotY), 2.5f * m_scale, IM_COL32(255, 255, 0, 255));

    ImGui::Dummy(ImVec2(width, height));
}

} // namespace ui
