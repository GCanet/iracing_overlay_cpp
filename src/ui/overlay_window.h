#ifndef OVERLAY_WINDOW_H
#define OVERLAY_WINDOW_H

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <memory>

namespace iracing {
    class IRSDKManager;
    class RelativeCalculator;
}

namespace ui {

class RelativeWidget;
class TelemetryWidget;

class OverlayWindow {
public:
    OverlayWindow() = default;
    ~OverlayWindow();  // FIXED: declared here, defined in .cpp (incomplete type fix)

    bool initialize(const char* title = "iRacing Overlay", int width = 1920, int height = 1080);
    void run();
    void shutdown();

private:
    void setupImGui();

    GLFWwindow* m_window = nullptr;
    bool m_lockKeyPressed = false;

    // iRacing data
    std::unique_ptr<iracing::IRSDKManager> m_sdk;
    std::unique_ptr<iracing::RelativeCalculator> m_relative;

    // UI Widgets
    std::unique_ptr<RelativeWidget> m_relativeWidget;
    std::unique_ptr<TelemetryWidget> m_telemetryWidget;
};

} // namespace ui

#endif // OVERLAY_WINDOW_H