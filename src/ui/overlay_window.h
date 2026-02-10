#ifndef OVERLAY_WINDOW_H
#define OVERLAY_WINDOW_H

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
    OverlayWindow();
    ~OverlayWindow();
    
    bool initialize();
    void run();
    void shutdown();

private:
    void setupImGui();
    void setupStyle();
    void renderFrame();
    void processInput();
    
    GLFWwindow* m_window;
    
    // iRacing data
    std::unique_ptr<iracing::IRSDKManager> m_sdk;
    std::unique_ptr<iracing::RelativeCalculator> m_relative;
    
    // UI Widgets
    std::unique_ptr<RelativeWidget> m_relativeWidget;
    std::unique_ptr<TelemetryWidget> m_telemetryWidget;
    
    // State
    bool m_running;
    
    // Window settings
    int m_windowWidth;
    int m_windowHeight;
};

} // namespace ui

#endif // OVERLAY_WINDOW_H
