#include "ui/overlay_window.h"
#include "ui/relative_widget.h"
#include "ui/telemetry_widget.h"
#include "data/irsdk_manager.h"
#include "data/relative_calc.h"
#include "utils/config.h"
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>

#ifdef _WIN32
    #include <windows.h>
    #include <dwmapi.h>
    #pragma comment(lib, "dwmapi.lib")
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3native.h>
#endif

namespace ui {

bool OverlayWindow::initialize(const char* title, int width, int height) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // GLFW hints for OpenGL context
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);  // Enable vsync

    // Load OpenGL functions with GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to load OpenGL functions" << std::endl;
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }

    std::cout << "OpenGL Version: " << GLVersion.major << "." << GLVersion.minor << std::endl;

    // Windows-specific: Make window click-through
    #ifdef _WIN32
        HWND hwnd = glfwGetWin32Window(m_window);
        LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED | WS_EX_TRANSPARENT);

        // DWM extend frame
        MARGINS margins = { -1 };
        DwmExtendFrameIntoClientArea(hwnd, &margins);
    #endif

    // Setup ImGui
    setupImGui();

    // Load config
    utils::Config::load("config.ini");

    // Create widgets
    m_relativeWidget = std::make_unique<RelativeWidget>(this);
    m_telemetryWidget = std::make_unique<TelemetryWidget>(this);

    // Create SDK
    m_sdk = std::make_unique<iracing::IRSDKManager>();
    m_relative = std::make_unique<iracing::RelativeCalculator>(m_sdk.get());

    return true;
}

void OverlayWindow::setupImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    // Disable imgui.ini to use our own config
    io.IniFilename = nullptr;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;

    // FIXED: Font loading - try different fonts, fallback to default
    ImFont* robotoFont = nullptr;

    // Try to load RobotoMono-VariableFont (preferred)
    robotoFont = io.Fonts->AddFontFromFileTTF(
        "assets/fonts/RobotoMono-VariableFont_wght.ttf",
        13.0f
    );

    // Fallback variants
    if (!robotoFont) {
        robotoFont = io.Fonts->AddFontFromFileTTF(
            "assets/fonts/RobotoMono-SemiBold.ttf",
            13.0f
        );
    }

    if (!robotoFont) {
        robotoFont = io.Fonts->AddFontFromFileTTF(
            "assets/fonts/RobotoMono-Bold.ttf",
            13.0f
        );
    }

    if (!robotoFont) {
        robotoFont = io.Fonts->AddFontFromFileTTF(
            "assets/fonts/RobotoMono-Medium.ttf",
            13.0f
        );
    }

    if (!robotoFont) {
        robotoFont = io.Fonts->AddFontFromFileTTF(
            "assets/fonts/RobotoMono-Regular.ttf",
            13.0f
        );
    }

    // Final fallback to default ImGui font
    if (!robotoFont) {
        std::cout << "[OverlayWindow] Warning: RobotoMono font not found, using default ImGui font" << std::endl;
        robotoFont = io.Fonts->AddFontDefault();
    } else {
        std::cout << "[OverlayWindow] Loaded RobotoMono font successfully" << std::endl;
    }

    io.Fonts->Build();

    // Setup backends
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 430 core");
}

void OverlayWindow::run() {
    m_lockKeyPressed = false;  // FIXED: Initialize member variable

    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        // Handle input
        if (glfwGetKey(m_window, GLFW_KEY_Q) == GLFW_PRESS) {
            glfwSetWindowShouldClose(m_window, GLFW_TRUE);
        }
        if (glfwGetKey(m_window, GLFW_KEY_L) == GLFW_PRESS) {
            if (!m_lockKeyPressed) {
                m_lockKeyPressed = true;
                utils::Config::getInstance().uiLocked = !utils::Config::getInstance().uiLocked;
            }
        } else {
            m_lockKeyPressed = false;
        }

        // Update SDK data
        if (m_sdk) m_sdk->update();
        if (m_relative) m_relative->update();

        // Render
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Render widgets
        bool editMode = !utils::Config::getInstance().uiLocked;
        if (m_relativeWidget) m_relativeWidget->render(m_relative.get(), editMode);
        if (m_telemetryWidget) m_telemetryWidget->render(editMode);  // FIXED: Removed extra parameters

        ImGui::Render();

        // Clear and render OpenGL
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(m_window);
    }

    shutdown();
}

void OverlayWindow::shutdown() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_window) {
        glfwDestroyWindow(m_window);
    }
    glfwTerminate();

    // Save config
    utils::Config::save("config.ini");
}

}  // namespace ui