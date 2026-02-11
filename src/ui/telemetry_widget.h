#ifndef TELEMETRY_WIDGET_H
#define TELEMETRY_WIDGET_H

#include <deque>
#include <imgui.h>

namespace iracing {
    class IRSDKManager;
}

namespace ui {

class OverlayWindow;

class TelemetryWidget {
public:
    explicit TelemetryWidget(OverlayWindow* overlay = nullptr);

    void render(iracing::IRSDKManager* sdk, bool editMode = false);

private:
    // Rendering helpers
    void renderABSIndicator(float width, float height);
    void renderShiftLights(float width, float height);
    void renderInputTrace(float width, float height);
    void renderInputBarsCompact(float width, float height);
    void renderGearDisplay(float width, float height);
    void renderSteeringWheelCompact(float width, float height);

    // Update logic
    void updateHistory(float throttle, float brake, float clutch, float steer);
    void updateTachometer(iracing::IRSDKManager* sdk);

    // ── State ───────────────────────────────────────────────────
    OverlayWindow* m_overlay = nullptr;

    // Current telemetry values
    float m_currentThrottle = 0.0f;
    float m_currentBrake    = 0.0f;
    float m_currentClutch   = 0.0f;
    float m_currentSteer    = 0.0f;
    bool  m_absActive       = false;
    int   m_currentGear     = 0;
    float m_currentSpeed    = 0.0f;
    float m_currentRPM      = 0.0f;

    // Tachometer / shift lights
    float m_maxRPM   = 7500.0f;
    float m_shiftRPM = 0.0f;
    float m_blinkRPM = 0.0f;

    // History (rolling buffers)
    static constexpr size_t m_maxSamples = 180;
    std::deque<float> m_throttleHistory;
    std::deque<float> m_brakeHistory;
    std::deque<float> m_clutchHistory;
    std::deque<float> m_steerHistory;
    std::deque<bool>  m_absActiveHistory;

    // Visibility / style
    bool  m_showThrottle = true;
    bool  m_showBrake    = true;
    bool  m_showClutch   = true;
    bool  m_showABS      = true;

    float m_scale        = 1.0f;
    float m_strokeWidth  = 2.0f;
};

} // namespace ui

#endif // TELEMETRY_WIDGET_H
