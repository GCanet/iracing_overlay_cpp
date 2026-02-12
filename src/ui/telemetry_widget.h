#pragma once

#include "../utils/config.h"
#include <vector>

namespace iracing {
    class IRSDKManager;
}

namespace ui {
    class OverlayWindow;

    class TelemetryWidget {
    public:
        TelemetryWidget(OverlayWindow* overlay = nullptr);
        ~TelemetryWidget();

        void render(iracing::IRSDKManager* sdk, utils::WidgetConfig& config, bool editMode = false);
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

        // History buffers (256 samples) - REMOVED clutch history
        std::vector<float> m_throttleHistory;
        std::vector<float> m_brakeHistory;
        int m_historyHead;

        // Scaling
        float m_scale;
        OverlayWindow* m_overlay;

        // Asset textures (OpenGL texture IDs)
        // Place PNG files at: assets/telemetry/steering_wheel.png (128x128)
        //                     assets/telemetry/abs_on.png (128x128)
        //                     assets/telemetry/abs_off.png (128x128)
        unsigned int m_steeringTexture;
        unsigned int m_absOnTexture;
        unsigned int m_absOffTexture;

        // Asset loading
        void loadAssets();
        unsigned int loadTextureFromFile(const char* filepath);

        // Render functions
        void renderShiftLights(float width, float height);
        void renderABS(float width, float height);
        void renderPedalBars(float width, float height);
        void renderHistoryTrace(float width, float height);
        void renderGearSpeed(float width, float height);
        void renderSteering(float width, float height);
    };

}  // namespace ui
