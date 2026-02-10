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
    
    void render(iracing::IRSDKManager* sdk);

private:
    void updateHistory(float throttle, float brake);
    void renderGraph(ImDrawList* draw_list, ImVec2 canvas_pos, 
                     const char* label, const std::deque<float>& data, float color[3]);
    
    std::deque<float> m_throttleHistory;
    std::deque<float> m_brakeHistory;
    
    static constexpr int MAX_HISTORY = 180; // 3 seconds at 60Hz
    static constexpr float GRAPH_WIDTH = 300.0f;
    static constexpr float GRAPH_HEIGHT = 60.0f;
};

} // namespace ui

#endif // TELEMETRY_WIDGET_H
