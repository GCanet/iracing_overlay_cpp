#include "ui/overlay_window.h"
#include "ui/relative_widget.h"
#include "ui/telemetry_widget.h"
#include "data/irsdk_manager.h"
#include "data/relative_calc.h"
#include "utils/config.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "glad/glad.h"

#ifdef _WIN32
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3native.h>
    #include <dwmapi.h>
    #pragma comment(lib, "dwmapi.lib")
#endif

#include <iostream>

namespace ui {

OverlayWindow::OverlayWindow()
    : m_window(nullptr)
    , m_running(false)
    , m_editMode(false)
    , m_globalAlpha(0.7f)
    , m_windowWidth(1920)
    , m_windowHeight(1080)
{
}

OverlayWindow::~OverlayWindow() {
}

bool OverlayWindow::initialize() {
#ifdef _WIN32
    // Allocate console for debug output
    if (AllocConsole()) {
        FILE* fDummy;
        freopen_s(&fDummy, "CONOUT$", "w", stdout);
        freopen_s(&fDummy, "CONOUT$", "w", stderr);
        freopen_s(&fDummy, "CONIN$", "r", stdin);
        std::cout.clear();
        std::clog.clear();
        std::cerr.clear();
        std::cin.clear();
    }
#endif

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);

    m_window = glfwCreateWindow(m_windowWidth, m_windowHeight, "iRacing Overlay", nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // Enable vsync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    setupImGui();
    setupStyle();

    // Load config
    auto& cfg = utils::Config::getRelativeConfig();
    m_editMode = !utils::Config::isClickThrough();
    m_globalAlpha = utils::Config::getGlobalAlpha();

    // Create widgets
    m_sdk = std::make_unique<iracing::IRSDKManager>();
    m_relative = std::make_unique<iracing::RelativeCalculator>(m_sdk.get());
    m_relativeWidget = std::make_unique<RelativeWidget>(this);
    m_telemetryWidget = std::make_unique<TelemetryWidget>(this);

    applyWindowAttributes();

    m_running = true;

    std::cout << "============================================" << std::endl;
    std::cout << "  iRacing Overlay - Initialized" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  Q      - Quit" << std::endl;
    std::cout << "  L      - Toggle Lock (Edit/Locked mode)" << std::endl;
    std::cout << "  Drag   - Move widgets (when unlocked)" << std::endl;
    std::cout << std::endl;
    std::cout << "Status: " << (m_editMode ? "EDIT MODE" : "LOCKED") << std::endl;

    return true;
}

void OverlayWindow::setupImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;
    io.IniFilename = nullptr; // Don't save imgui.ini

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void OverlayWindow::setupStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    
    ImGui::StyleColorsDark();
}

void OverlayWindow::applyWindowAttributes() {
#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(m_window);
    if (!hwnd) {
        std::cerr << "Failed to get Win32 window handle" << std::endl;
        return;
    }

    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_LAYERED | WS_EX_TOPMOST;

    if (m_editMode) {
        exStyle &= ~WS_EX_TRANSPARENT;
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    } else {
        exStyle |= WS_EX_TRANSPARENT;
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
        BYTE alphaByte = static_cast<BYTE>(m_globalAlpha * 255);
        SetLayeredWindowAttributes(hwnd, 0, alphaByte, LWA_ALPHA);
    }

    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);

    std::cout << "[Window] Attributes applied - EditMode: " << m_editMode << std::endl;
#endif
}

void OverlayWindow::toggleEditMode() {
    m_editMode = !m_editMode;
    applyWindowAttributes();
    
    std::cout << (m_editMode ?
        "EDIT MODE - You can drag widgets" : "LOCKED - Click-through enabled") << std::endl;
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

    // ──────────────────────────────────────────────────
    // ONLY render these two widgets - NO debug windows
    // ──────────────────────────────────────────────────
    m_relativeWidget->render(m_relative.get(), m_editMode);
    m_telemetryWidget->render(m_sdk.get(), m_editMode);

    // ──────────────────────────────────────────────────
    // Status indicator in top-right corner
    // FIX: Declare 'io' before using it (was missing → C2065 + C2440)
    // ──────────────────────────────────────────────────
    {
        ImGuiIO& io = ImGui::GetIO();   // ← THIS WAS MISSING

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
        if (!m_editMode) {
            flags |= ImGuiWindowFlags_NoInputs;
        }

        ImGui::Begin("##Status", nullptr, flags);
        if (m_editMode) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "EDIT MODE");
            ImGui::Text("Press L to lock");
        } else {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "LOCKED");
        }
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

void OverlayWindow::processInput() {
    if (glfwGetKey(m_window, GLFW_KEY_Q) == GLFW_PRESS) {
        m_running = false;
    }

    static bool lKeyWasPressed = false;
    bool lKeyPressed = glfwGetKey(m_window, GLFW_KEY_L) == GLFW_PRESS;
    if (lKeyPressed && !lKeyWasPressed) {
        toggleEditMode();
    }
    lKeyWasPressed = lKeyPressed;
}

void OverlayWindow::shutdown() {
    saveConfigOnExit();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();

    if (m_sdk) {
        m_sdk->shutdown();
    }

#ifdef _WIN32
    FreeConsole();
#endif
}

void OverlayWindow::saveConfigOnExit() {
    utils::Config::setClickThrough(!m_editMode);
    utils::Config::setGlobalAlpha(m_globalAlpha);
    utils::Config::save("config.ini");
    std::cout << "Config saved." << std::endl;
}

} // namespace ui
