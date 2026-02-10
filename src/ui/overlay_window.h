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
    OverlayWindow();
    ~OverlayWindow();

    bool initialize();
    void run();
    void shutdown();

    // Acceso público para que los widgets puedan modificar desde menú contextual (opcional por ahora)
    bool& getEditModeRef() { return m_editMode; }
    float& getGlobalAlphaRef() { return m_globalAlpha; }

    // Si quieres exponer applyWindowAttributes para que los widgets lo llamen
    void applyWindowAttributes();

private:
    void setupImGui();
    void setupStyle();
    void renderFrame();
    void processInput();
    void toggleEditMode();           // ← cambiado de toggleClickThrough
    void saveConfigOnExit();

    GLFWwindow* m_window;

    // iRacing data
    std::unique_ptr<iracing::IRSDKManager> m_sdk;
    std::unique_ptr<iracing::RelativeCalculator> m_relative;

    // UI Widgets
    std::unique_ptr<RelativeWidget> m_relativeWidget;
    std::unique_ptr<TelemetryWidget> m_telemetryWidget;

    // State
    bool m_running;
    bool m_editMode;                 // true = edición (drag + menú), false = locked
    float m_globalAlpha;             // transparencia global

    // Window settings
    int m_windowWidth;
    int m_windowHeight;
};

} // namespace ui

#endif // OVERLAY_WINDOW_H
