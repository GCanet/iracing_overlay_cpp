#ifndef TELEMETRY_WIDGET_H
#define TELEMETRY_WIDGET_H

#include <vector>
#include <deque>

namespace iracing {
    class IRSDKManager;
}

namespace ui {

struct TelemetryPoint {
    float throttle;
    float brake;
    float speed;
};

class TelemetryWidget {
public:
    TelemetryWidget();
    
    void render(iracing::IRSDKManager* sdk);

private:
    void updateHistory(float throttle, float brake, float speed);
    void renderGraph(const char* label, const std::deque<float>& data, float color[3]);
    
    std::deque<float> m_throttleHistory;
    std::deque<float> m_brakeHistory;
    
    static constexpr int MAX_HISTORY = 180; // 3 seconds at 60Hz
    static constexpr float GRAPH_WIDTH = 200.0f;
    static constexpr float GRAPH_HEIGHT = 80.0f;
};

} // namespace ui

#endif // TELEMETRY_WIDGET_H
