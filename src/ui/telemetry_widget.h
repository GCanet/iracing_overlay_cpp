#ifndef TELEMETRY_WIDGET_H
#define TELEMETRY_WIDGET_H

#include <deque>

namespace iracing {
    class IRSDKManager;
}

namespace ui {
    class OverlayWindow;  // forward declaration

    class TelemetryWidget {
    public:
        // Constructor ahora recibe puntero al overlay (puede ser nullptr si no se usa)
        TelemetryWidget(OverlayWindow* overlay = nullptr);

        void render(iracing::IRSDKManager* sdk, bool editMode = false);

    private:
        void updateHistory(float throttle, float brake);

        std::deque<float> m_throttleHistory;
        std::deque<float> m_brakeHistory;

        float m_scale = 1.0f;

        OverlayWindow* m_overlay;  // ← puntero para acceder a editMode y alpha

        static constexpr size_t MAX_HISTORY = 180;
        static constexpr float GRAPH_WIDTH = 255.0f;
        static constexpr float GRAPH_HEIGHT = 60.0f;
    };

} // namespace ui

#endif // TELEMETRY_WIDGET_H
