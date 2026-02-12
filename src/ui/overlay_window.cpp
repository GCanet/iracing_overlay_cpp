#include "ui/overlay_window.h"
#include "utils/config.h"
#include "data/irsdk_manager.h"
#include "data/relative_calc.h"
#include "ui/relative_widget.h"
#include "ui/telemetry_widget.h"
#include <iostream>
#include <glad/glad.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#ifdef _WIN32
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3native.h>
    #include <Windows.h>
    #include <dwmapi.h>
#endif

namespace ui {

OverlayWindow::OverlayWindow()
    : m_window(nullptr)
    , m_windowWidth(1920)
    , m_windowHeight(1080)
    , m_running(true)
    , m_editMode(false)
    , m_globalAlpha(0.9f)
{
}

OverlayWindow::~OverlayWindow() {
    shutdown();
}

bool OverlayWindow::initialize() {
    if (!glfwInit()) {
        std::cerr << "[OverlayWindow] Failed to init GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    // Create window
    m_window = glfwCreateWindow(m_windowWidth, m_windowHeight, "iRacing Overlay", nullptr, nullptr);
    if (!m_window) {
        std::cerr << "[OverlayWindow] Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(0); // No VSync for max overlay responsiveness

    // Load OpenGL functions
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "[OverlayWindow] Failed to load OpenGL" << std::endl;
        return false;
    }

    std::cout << "[OverlayWindow] OpenGL " << GLVersion.major << "." << GLVersion.minor << std::endl;

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
    
    // FIXED: Disable imgui.ini to avoid double config files
    io.IniFilename = nullptr;  // Don't use imgui.ini, we have our own config.ini

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;

    // Load Roboto Mono Variable Font with weight 600 (SemiBold)
    ImFont* robotoFont = nullptr;
    ImFontConfig fontConfig;
    fontConfig.SizePixels = 13.0f;

    // Load RobotoMono-VariableFont_wght.ttf
    // This is the variable font that supports all weights dynamically
    robotoFont = io.Fonts->AddFontFromFileTTF(
        "assets/fonts/RobotoMono-VariableFont_wght.ttf",
        13.0f,
        &fontConfig,
        nullptr
    );

    // Fallback: if variable font doesn't exist, try static variants
    if (!robotoFont) {
        robotoFont = io.Fonts->AddFontFromFileTTF(
            "assets/fonts/RobotoMono-SemiBold.ttf",
            13.0f,
            &fontConfig
        );
    }

    // Fallback: try Bold variant
    if (!robotoFont) {
        robotoFont = io.Fonts->AddFontFromFileTTF(
            "assets/fonts/RobotoMono-Bold.ttf",
            13.0f,
            &fontConfig
        );
    }

    // Fallback: try Medium variant
    if (!robotoFont) {
        robotoFont = io.Fonts->AddFontFromFileTTF(
            "assets/fonts/RobotoMono-Medium.ttf",
            13.0f,
            &fontConfig
        );
    }

    // Fallback: try Regular variant
    if (!robotoFont) {
        robotoFont = io.Fonts->AddFontFromFileTTF(
            "assets/fonts/RobotoMono-Regular.ttf",
            13.0f,
            &fontConfig
        );
    }

    // Final fallback: default ImGui font
    if (!robotoFont) {
        std::cout << "[OverlayWindow] Warning: RobotoMono font not found, using default ImGui font" << std::endl;
        robotoFont = io.Fonts->AddFontDefault();
    } else {
        std::cout << "[OverlayWindow] Loaded RobotoMono font successfully" << std::endl;
    }

    io.Fonts->Build();

    const char* glsl_version = "#version 330";
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
}

void OverlayWindow::shutdown() {
    if (m_window) {
        utils::Config::save("config.ini");

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(m_window);
        glfwTerminate();
        m_window = nullptr;
    }
}

void OverlayWindow::run() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        // Handle input
        handleInput();

        // Update SDK - try to connect/reconnect, then poll for new data
        if (m_sdk) {
            m_sdk->startup();          // Attempts connection (no-op if already connected)
            m_sdk->waitForData(0);     // Non-blocking poll for new telemetry data
            if (m_sdk->isSessionActive()) {
                m_relative->update();
            }
        }

        // Render frame
        renderFrame();
    }
}

void OverlayWindow::handleInput() {
    // Q to quit
    if (glfwGetKey(m_window, GLFW_KEY_Q) == GLFW_PRESS) {
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    }

    // L to toggle edit mode
    static bool lKeyPressed = false;
    if (glfwGetKey(m_window, GLFW_KEY_L) == GLFW_PRESS) {
        if (!lKeyPressed) {
            m_editMode = !m_editMode;
            std::cout << "[OverlayWindow] Edit mode: " << (m_editMode ? "ON" : "OFF") << std::endl;

            #ifdef _WIN32
                HWND hwnd = glfwGetWin32Window(m_window);
                LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

                if (m_editMode) {
                    // Remove transparent flag: widgets are now draggable
                    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_TRANSPARENT);
                    glfwSetWindowAttrib(m_window, GLFW_MOUSE_PASSTHROUGH, GLFW_FALSE);
                } else {
                    // Add transparent flag: overlay is click-through
                    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle | WS_EX_TRANSPARENT);
                    glfwSetWindowAttrib(m_window, GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);
                }
            #endif

            lKeyPressed = true;
        }
    } else {
        lKeyPressed = false;
    }
}

void OverlayWindow::renderFrame() {
    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Render widgets
    if (m_relativeWidget && m_relative) {
        m_relativeWidget->render(m_relative.get(), m_editMode);
    }

    if (m_telemetryWidget && m_sdk) {
        auto& config = utils::Config::getTelemetryConfig();
        m_telemetryWidget->render(m_sdk.get(), config, m_editMode);
    }

    // Show edit mode indicator
    if (m_editMode) {
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                ImGuiWindowFlags_AlwaysAutoResize |
                                ImGuiWindowFlags_NoFocusOnAppearing |
                                ImGuiWindowFlags_NoNav;

        ImGui::Begin("##Status", nullptr, flags);
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "EDIT MODE");
        ImGui::Text("Press L to lock");
        ImGui::End();
    }

    ImGui::Render();

    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(m_window);
}

}  // namespace ui