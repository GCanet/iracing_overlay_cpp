#ifndef TELEMETRY_WIDGET_H
#define TELEMETRY_WIDGET_H

#include <deque>
#include <vector>

namespace iracing {
    class IRSDKManager;
}

namespace ui {
    class OverlayWindow;

    class TelemetryWidget {
    public:
        TelemetryWidget(OverlayWindow* overlay = nullptr);

        void render(iracing::IRSDKManager* sdk, bool editMode = false);

    private:
        // Rendering components
        void renderInputTrace(float width, float height);
        void renderInputBars(float width, float height);
        void renderGearSpeed(float width, float height);
        void renderSteeringWheel(float width, float height);
        void renderTachometer(float width, float height);
        
        // Update functions
        void updateHistory(float throttle, float brake, float clutch, float steer);
        void updateTachometer(iracing::IRSDKManager* sdk);
        
        // History buffers
        std::deque<float> m_throttleHistory;
        std::deque<float> m_brakeHistory;
        std::deque<float> m_clutchHistory;
        std::deque<float> m_steerHistory;
        std::deque<bool> m_absActiveHistory;
        
        // Current state
        float m_currentThrottle = 0.0f;
        float m_currentBrake = 0.0f;
        float m_currentClutch = 0.0f;
        float m_currentSteer = 0.0f;
        bool m_absActive = false;
        int m_currentGear = 0;
        float m_currentSpeed = 0.0f;
        float m_currentRPM = 0.0f;
        float m_maxRPM = 7500.0f;
        float m_shiftRPM = 0.0f;
        float m_blinkRPM = 0.0f;
        
        // Display settings (pueden ser configurables en el futuro)
        bool m_showInputTrace = true;
        bool m_showInputBars = true;
        bool m_showGearSpeed = true;
        bool m_showSteeringWheel = true;
        bool m_showTachometer = true;
        bool m_showThrottle = true;
        bool m_showBrake = true;
        bool m_showClutch = true;
        bool m_showABS = true;
        bool m_showSteering = true;
        
        float m_strokeWidth = 3.0f;
        int m_maxSamples = 180;
        
        float m_scale = 1.0f;
        OverlayWindow* m_overlay;

        static constexpr float WIDGET_WIDTH = 400.0f;
        static constexpr float WIDGET_HEIGHT = 300.0f;
        static constexpr float TRACE_HEIGHT = 100.0f;
        static constexpr float BAR_HEIGHT = 80.0f;
        static constexpr float GEAR_HEIGHT = 40.0f;
        static constexpr float STEERING_SIZE = 60.0f;
        static constexpr float TACHO_HEIGHT = 30.0f;
    };

} // namespace ui

#endif // TELEMETRY_WIDGET_H
