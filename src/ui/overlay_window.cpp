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
#include <iostream>

// Windows-specific includes for DWM and window manipulation
#ifdef _WIN32
    #include <dwmapi.h>
    #pragma comment(lib, "dwmapi.lib")
#endif

namespace ui {

OverlayWindow::OverlayWindow()
    : m_window(nullptr), m_running(true), m_editMode(false), m_globalAlpha(0.9f),
      m_windowWidth(1920), m_windowHeight(1080) {}

OverlayWindow::~OverlayWindow() {
    shutdown();
}

bool OverlayWindow::initialize() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "[OverlayWindow] Failed to initialize GLFW" << std::endl;
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

    // If no Roboto Mono is available, use default font
    if (!robotoFont) {
        robotoFont = io.Fonts->GetDefaultFont();
        std::cout << "[ImGui] Roboto Mono not found, using default font. "
                  << "Place RobotoMono-VariableFont_wght.ttf in assets/fonts/" << std::endl;
    } else {
        io.FontDefault = robotoFont;
        std::cout << "[ImGui] Loaded Roboto Mono Variable Font (weight 600)" << std::endl;
    }

    io.Fonts->Build();

    const char* glsl_version = "#version 330";
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);
}

void OverlayWindow::setupStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.Alpha = 0.95f;
    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;
    style.FrameRounding = 4.0f;
    style.WindowRounding = 8.0f;
}

void OverlayWindow::processInput() {
    if (glfwGetKey(m_window, GLFW_KEY_Q) == GLFW_PRESS) {
        m_running = false;
        glfwSetWindowShouldClose(m_window, GLFW_TRUE);
    }

    static double lastLToggle = 0.0;
    if (glfwGetKey(m_window, GLFW_KEY_L) == GLFW_PRESS) {
        double currentTime = glfwGetTime();
        if (currentTime - lastLToggle > 0.3) {
            toggleEditMode();
            lastLToggle = currentTime;
        }
    }
}

void OverlayWindow::toggleEditMode() {
    m_editMode = !m_editMode;
    applyWindowAttributes();
}

void OverlayWindow::applyWindowAttributes() {
    if (!m_window) return;

    if (m_editMode) {
        glfwSetWindowAttrib(m_window, GLFW_MOUSE_PASSTHROUGH, GLFW_FALSE);
    } else {
        glfwSetWindowAttrib(m_window, GLFW_MOUSE_PASSTHROUGH, GLFW_TRUE);
    }
}

void OverlayWindow::saveConfigOnExit() {
    utils::Config::save("config.ini");
}

void OverlayWindow::run() {
    while (m_running && !glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        processInput();

        if (m_sdk) {
            if (!m_sdk->isConnected()) {
                m_sdk->startup();
            }

            if (m_sdk->waitForData(16)) {
                if (m_sdk->isSessionActive()) {
                    m_relative->update();
                }
            }
        }

        renderFrame();
    }
}

void OverlayWindow::renderFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Render widgets only
    m_relativeWidget->render(m_relative.get(), m_editMode);

    auto& telemetryConfig = utils::Config::getTelemetryConfig();
    m_telemetryWidget->render(m_sdk.get(), telemetryConfig, m_editMode);

    // Status indicator (only in edit mode to minimize draw calls)
    if (m_editMode) {
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(
            ImVec2(io.DisplaySize.x - 10.0f, 10.0f),
            ImGuiCond_Always,
            ImVec2(1.0f, 0.0f)
        );
        ImGui::SetNextWindowBgAlpha(0.5f);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                ImGuiWindowFlags_NoMove |
                                ImGuiWindowFlags_NoSavedSettings |
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

void OverlayWindow::shutdown() {
    saveConfigOnExit();

    if (m_window) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();

        glfwDestroyWindow(m_window);
    }

    glfwTerminate();
}

}  // namespace ui
