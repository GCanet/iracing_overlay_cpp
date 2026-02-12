#define GLFW_INCLUDE_NONE
#include "ui/overlay_window.h"
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "  iRacing Overlay v1.0" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // FIXED: Use stack allocation instead of std::make_unique to avoid
    // instantiating std::default_delete<OverlayWindow> in this translation unit,
    // which would transitively require complete types for all unique_ptr members
    // (TelemetryWidget, RelativeWidget, etc.) that are only forward-declared in the header.
    ui::OverlayWindow overlay;

    if (!overlay.initialize()) {
        std::cerr << "Failed to initialize overlay" << std::endl;
        return 1;
    }

    std::cout << "Overlay running!" << std::endl;
    std::cout << "Waiting for iRacing to start..." << std::endl;
    std::cout << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  Q   - Quit" << std::endl;
    std::cout << "  L   - Toggle Lock/Edit mode" << std::endl;
    std::cout << std::endl;

    // Main loop (run() calls shutdown() internally when done)
    overlay.run();

    std::cout << "Goodbye!" << std::endl;

    return 0;
}