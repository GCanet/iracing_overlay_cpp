#pragma once

#include "../utils/config.h"
#include <vector>
#include <cstdint>

namespace idata {
    struct TelemetryData;
}

namespace ui {

class TelemetryWidget {
public:
    TelemetryWidget();
    ~TelemetryWidget();

    void update(const idata::TelemetryData& data);
    void render(utils::WidgetConfig& config, bool editMode = false);
    void setScale(float scale);

private:
    // Current values
    float m_currentRPM;
    float m_maxRPM;
    float m_blinkRPM;
    float m_throttle;
    float m_brake;
    float m_clutch;
    int m_gear;
    int m_speed;
    float m_steeringAngle;
    float m_steeringAngleMax;
    bool m_absActive;

    // History buffers
    std::vector<float> m_throttleHistory;
    std::vector<float> m_brakeHistory;
    std::vector<float> m_clutchHistory;
    int m_historyHead;

    // Scaling
    float m_scale;

    // Render functions
    void renderShiftLights(float width, float height);
    void renderABS(float width, float height);
    void renderPedalBars(float width, float height);
    void renderHistoryTrace(float width, float height);
    void renderGearSpeed(float width, float height);
    void renderSteering(float width, float height);
};

}  // namespace ui
