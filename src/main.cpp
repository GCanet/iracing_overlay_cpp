#define GLFW_INCLUDE_NONE
#include "ui/overlay_window.h"
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "========================================" << std::endl;
    std::cout << "  iRacing Overlay v1.0" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << std::endl;

    // Create and initialize overlay (loads config internally)
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

    // Main loop
    overlay.run();

    // Cleanup
    overlay.shutdown();

    std::cout << "Goodbye!" << std::endl;

    return 0;
}
