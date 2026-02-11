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
        m_currentClutch   = 1.0f - sdk->getFloat("Clutch", 0.0f);
        m_currentSteer    = sdk->getFloat("SteeringWheelAngle", 0.0f);
        m_absActive       = sdk->getBool("BrakeABSactive", false);
        m_currentGear     = sdk->getInt("Gear", 0);
        m_currentSpeed    = sdk->getFloat("Speed", 0.0f) * 3.6f;
        updateTachometer(sdk);
        updateHistory(m_currentThrottle, m_currentBrake, m_currentClutch, m_currentSteer);
    }

    auto& config = utils::Config::getTelemetryConfig();

    float rowH   = 42.0f * m_scale;
    float rpmH   = 5.0f * m_scale;
    float padY   = 3.0f * m_scale;
    float padBot = 2.0f * m_scale;
    float gapRpm = 1.0f * m_scale;
    float totalH = rpmH + gapRpm + rowH + padY + padBot;
    float totalW = 380.0f * m_scale;

    ImGui::SetNextWindowSize(ImVec2(totalW, totalH), ImGuiCond_Always);
    if (config.posX < 0 || config.posY < 0) {
        ImGuiIO& io = ImGui::GetIO();
        config.posX = (io.DisplaySize.x - totalW) * 0.5f;
        config.posY = io.DisplaySize.y - totalH - 20.0f;
    }
    ImGui::SetNextWindowPos(ImVec2(config.posX, config.posY), ImGuiCond_Once);

    // Dark background matching relative widget
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.12f, config.alpha));
    ImGui::PushStyleColor(ImGuiCol_Border,   ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
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

    renderShiftLights(contentW, rpmH);
    ImGui::Dummy(ImVec2(0, gapRpm));

    float absW     = rowH;
    float barsW    = 30.0f * m_scale;
    float gearW    = 44.0f * m_scale;
    float steerW   = rowH;
    float gap      = 4.0f * m_scale;
    float gapTight = 1.0f * m_scale;
    float traceW   = contentW - absW - barsW - gearW - steerW - gap * 2.0f - gapTight * 2.0f;

    renderABSIndicator(absW, rowH);
    ImGui::SameLine(0, gap);
    renderInputTrace(traceW, rowH);
    ImGui::SameLine(0, gap);
    renderInputBarsCompact(barsW, rowH);
    ImGui::SameLine(0, gapTight);
    renderGearDisplay(gearW, rowH);
    ImGui::SameLine(0, gapTight);
    renderSteeringWheelCompact(steerW, rowH);

    ImGui::End();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void TelemetryWidget::updateHistory(float throttle, float brake, float clutch, float steer) {
    m_throttleHistory.pop_front(); m_throttleHistory.push_back(throttle);
    m_brakeHistory.pop_front();    m_brakeHistory.push_back(brake);
    m_clutchHistory.pop_front();   m_clutchHistory.push_back(clutch);
    float sn = (steer + 1.0f) * 0.5f;
    sn = std::max(0.0f, std::min(1.0f, sn));
    m_steerHistory.pop_front();    m_steerHistory.push_back(sn);
    m_absActiveHistory.pop_front(); m_absActiveHistory.push_back(m_absActive);
}

void TelemetryWidget::updateTachometer(iracing::IRSDKManager* sdk) {
    m_currentRPM = sdk->getFloat("RPM", 0.0f);
    float sg = sdk->getFloat("ShiftGrindRPM", 0.0f);
    if (sg > 0) m_maxRPM = sg;
    m_shiftRPM = sdk->getFloat("DriverCarSLShiftRPM", 0.0f);
    m_blinkRPM = sdk->getFloat("DriverCarSLBlinkRPM", 0.0f);
}

void TelemetryWidget::renderABSIndicator(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 c(p.x + width * 0.5f, p.y + height * 0.5f);
    float r = std::min(width, height) * 0.42f;

    ImU32 ringCol = m_absActive ? IM_COL32(255, 255, 255, 255) : IM_COL32(90, 90, 90, 200);
    ImU32 bgCol   = m_absActive ? IM_COL32(255, 255, 255, 255) : IM_COL32(50, 50, 50, 200);
    ImU32 txtCol  = m_absActive ? IM_COL32(0, 0, 0, 255)       : IM_COL32(140, 140, 140, 255);

    dl->AddCircleFilled(c, r, bgCol, 32);
    dl->AddCircle(c, r, ringCol, 32, 1.5f * m_scale);

    float arcR = r + 3.0f * m_scale;
    float arcR2 = r + 6.0f * m_scale;
    float thick = 1.5f * m_scale;
    dl->PathArcTo(c, arcR,  (float)(M_PI * 0.65), (float)(M_PI * 1.35), 16); dl->PathStroke(ringCol, 0, thick);
    dl->PathArcTo(c, arcR2, (float)(M_PI * 0.7),  (float)(M_PI * 1.3),  16); dl->PathStroke(ringCol, 0, thick);
    dl->PathArcTo(c, arcR,  (float)(-M_PI * 0.35),(float)(M_PI * 0.35), 16); dl->PathStroke(ringCol, 0, thick);
    dl->PathArcTo(c, arcR2, (float)(-M_PI * 0.3), (float)(M_PI * 0.3),  16); dl->PathStroke(ringCol, 0, thick);

    float fontSize = r * 0.85f;
    const char* txt = "ABS";
    ImVec2 ts = ImGui::CalcTextSize(txt);
    float sc = fontSize / ts.y;
    ImVec2 tss(ts.x * sc, ts.y * sc);
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * sc,
                ImVec2(c.x - tss.x * 0.5f, c.y - tss.y * 0.5f), txtCol, txt);

    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderShiftLights(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    float rpmPct = (m_maxRPM > 0) ? std::min(m_currentRPM / m_maxRPM, 1.0f) : 0.0f;
    int numLights = 8;
    float g = 2.0f * m_scale;
    float lw = (width - (numLights - 1) * g) / numLights;
    int active = (int)(rpmPct * numLights + 0.5f);

    for (int i = 0; i < numLights; i++) {
        float x = p.x + i * (lw + g);
        ImU32 col = IM_COL32(30, 30, 30, 200);
        if (i < active) {
            if (m_blinkRPM > 0 && m_currentRPM >= m_blinkRPM) {
                int blink = (int)(ImGui::GetTime() * 10) % 2;
                col = blink ? IM_COL32(255, 0, 0, 255) : IM_COL32(80, 0, 0, 255);
            } else if (i == numLights - 1) {
                col = IM_COL32(255, 0, 0, 255);
            } else {
                float pct = (float)i / numLights;
                col = (pct > 0.7f) ? IM_COL32(255, 255, 0, 255) : IM_COL32(0, 200, 0, 255);
            }
        }
        dl->AddRectFilled(ImVec2(x, p.y), ImVec2(x + lw, p.y + height), col, 1.0f);
    }
    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderInputTrace(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    dl->AddRectFilled(p, ImVec2(p.x + width, p.y + height), IM_COL32(15, 15, 15, 220));
    dl->AddRect(p, ImVec2(p.x + width, p.y + height), IM_COL32(50, 50, 50, 200));

    int n = (int)m_throttleHistory.size();
    if (n < 2) { ImGui::Dummy(ImVec2(width, height)); return; }
    float step = width / (float)(n - 1);

    auto trace = [&](const std::deque<float>& h, ImU32 col, float lw) {
        for (int i = 1; i < n; i++) {
            dl->AddLine(ImVec2(p.x + (i-1)*step, p.y + height - h[i-1]*height),
                        ImVec2(p.x + i*step,     p.y + height - h[i]*height), col, lw);
        }
    };

    if (m_showThrottle) trace(m_throttleHistory, IM_COL32(0, 220, 0, 220), 2.0f * m_scale);
    if (m_showBrake)    trace(m_brakeHistory,    IM_COL32(255, 0, 0, 220), 2.0f * m_scale);
    if (m_showClutch)   trace(m_clutchHistory,   IM_COL32(60, 120, 255, 160), 1.5f * m_scale);

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

void TelemetryWidget::renderInputBarsCompact(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 sp = ImGui::GetCursorScreenPos();
    float g = 2.0f * m_scale;
    float bw = (width - g * 2.0f) / 3.0f;

    struct B { float v; ImU32 c; };
    B bars[3] = {
        { m_currentThrottle, IM_COL32(0, 220, 0, 230) },
        { m_currentBrake,    m_absActive ? IM_COL32(255, 255, 0, 230) : IM_COL32(255, 0, 0, 230) },
        { m_currentClutch,   IM_COL32(60, 120, 255, 230) }
    };
    for (int i = 0; i < 3; i++) {
        float x = sp.x + i * (bw + g);
        float h = bars[i].v * height;
        dl->AddRect(ImVec2(x, sp.y), ImVec2(x + bw, sp.y + height), IM_COL32(180, 180, 180, 80));
        dl->AddRectFilled(ImVec2(x, sp.y + height - h), ImVec2(x + bw, sp.y + height), bars[i].c);
    }
    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderGearDisplay(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    char gs[4];
    if (m_currentGear == -1) snprintf(gs, 4, "R");
    else if (m_currentGear == 0) snprintf(gs, 4, "N");
    else snprintf(gs, 4, "%d", m_currentGear);

    char ss[16];
    snprintf(ss, 16, "%.0f", m_currentSpeed);

    ImU32 gc = IM_COL32(255, 255, 255, 255);
    if (m_currentGear == -1) gc = IM_COL32(255, 100, 100, 255);
    else if (m_currentGear == 0) gc = IM_COL32(180, 180, 180, 255);

    float gfs = (height * 0.55f) / ImGui::GetFontSize();
    ImVec2 gsz = ImGui::CalcTextSize(gs);
    gsz.x *= gfs; gsz.y *= gfs;
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * gfs,
                ImVec2(p.x + (width - gsz.x) * 0.5f, p.y + height * 0.05f), gc, gs);

    float sfs = (height * 0.28f) / ImGui::GetFontSize();
    ImVec2 ssz = ImGui::CalcTextSize(ss);
    ssz.x *= sfs; ssz.y *= sfs;
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * sfs,
                ImVec2(p.x + (width - ssz.x) * 0.5f, p.y + height * 0.62f),
                IM_COL32(200, 200, 200, 220), ss);

    const char* u = "km/h";
    float ufs = (height * 0.16f) / ImGui::GetFontSize();
    ImVec2 usz = ImGui::CalcTextSize(u);
    usz.x *= ufs; usz.y *= ufs;
    dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * ufs,
                ImVec2(p.x + (width - usz.x) * 0.5f, p.y + height * 0.62f + ssz.y),
                IM_COL32(140, 140, 140, 180), u);

    ImGui::Dummy(ImVec2(width, height));
}

void TelemetryWidget::renderSteeringWheelCompact(float width, float height) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImVec2 c(p.x + width * 0.5f, p.y + height * 0.5f);
    float r = std::min(width, height) * 0.40f;

    dl->AddCircle(c, r, IM_COL32(200, 200, 200, 200), 32, 2.0f * m_scale);
    dl->AddLine(ImVec2(c.x - r * 0.6f, c.y), ImVec2(c.x + r * 0.6f, c.y),
                IM_COL32(160, 160, 160, 160), 1.5f * m_scale);

    float angle = -m_currentSteer;
    float dx = c.x + r * 0.75f * sinf(angle);
    float dy = c.y - r * 0.75f * cosf(angle);
    dl->AddLine(c, ImVec2(dx, dy), IM_COL32(255, 255, 0, 220), 1.5f * m_scale);
    dl->AddCircleFilled(ImVec2(dx, dy), 2.5f * m_scale, IM_COL32(255, 255, 0, 255));

    ImGui::Dummy(ImVec2(width, height));
}

} // namespace ui
