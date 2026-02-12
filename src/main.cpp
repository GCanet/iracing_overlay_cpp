#define GLFW_INCLUDE_NONE
#include "ui/overlay_window.h"
#include <iostream>
#include <memory>

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "  iRacing Overlay v1.0" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // FIXED: Use unique_ptr to avoid instantiating std::default_delete<TelemetryWidget>
    // in this translation unit where TelemetryWidget is an incomplete type.
    auto overlay = std::make_unique<ui::OverlayWindow>();

    if (!overlay->initialize()) {
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
    overlay->run();

    // FIXED: Removed duplicate overlay.shutdown() - run() already handles it

    std::cout << "Goodbye!" << std::endl;

    return 0;
}