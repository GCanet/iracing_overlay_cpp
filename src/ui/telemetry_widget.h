#ifndef TELEMETRY_WIDGET_H
#define TELEMETRY_WIDGET_H

#include <deque>

// Forward declarations
struct ImDrawList;
struct ImVec2;

namespace iracing {
    class IRSDKManager;
}

namespace ui {

class TelemetryWidget {
public:
    TelemetryWidget();

    // Render ahora recibe editMode para controlar drag y menú contextual
    void render(iracing::IRSDKManager* sdk, bool editMode = false);

private:
    void updateHistory(float throttle, float brake);

    std::deque<float> m_throttleHistory;
    std::deque<float> m_brakeHistory;

    float m_scale = 1.0f;  // Escala del widget (ajustable desde menú contextual)

    static constexpr size_t MAX_HISTORY = 180;     // ~3 segundos a 60 Hz
    static constexpr float GRAPH_WIDTH = 255.0f;   // Tu valor original
    static constexpr float GRAPH_HEIGHT = 60.0f;
};

} // namespace ui

#endif // TELEMETRY_WIDGET_H
