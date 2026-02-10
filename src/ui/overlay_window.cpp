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
    // =========================================================
    // CONSOLE: Allocate a console window for debug output
    // This ensures std::cout / std::cerr always have somewhere to go,
    // regardless of WIN32_EXECUTABLE setting.
    // =========================================================
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) {
        AllocConsole();
    }
    // Redirect stdout/stderr to the console
    FILE* fp = nullptr;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);
    std::cout.clear();
    std::cerr.clear();
#endif

    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }

    // GL 3.3 + GLSL 330
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    // Start hidden so we can set up attributes before showing
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    // Create window
    m_window = glfwCreateWindow(m_windowWidth, m_windowHeight, "iRacing Overlay", nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // Enable vsync

    // Load OpenGL
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }

    // Enable transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Load config
    utils::Config::load("config.ini");
    m_globalAlpha = utils::Config::getGlobalAlpha();
    m_editMode = !utils::Config::isClickThrough(); // clickThrough=true → locked → editMode=false

    // Setup ImGui
    setupImGui();
    setupStyle();

    // Initialize iRacing SDK
    m_sdk = std::make_unique<iracing::IRSDKManager>();
    m_relative = std::make_unique<iracing::RelativeCalculator>(m_sdk.get());

    // Initialize widgets
    m_relativeWidget = std::make_unique<RelativeWidget>(this);
    m_telemetryWidget = std::make_unique<TelemetryWidget>(this);

    // Apply Windows-specific attributes BEFORE showing window
    applyWindowAttributes();

    // Clear background frames (transparency)
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(m_window);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(m_window);

    // Show window AFTER everything is set up
    glfwShowWindow(m_window);

    m_running = true;

    std::cout << "============================================" << std::endl;
    std::cout << "  iRacing Overlay initialized successfully!" << std::endl;
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
    io.IniFilename = nullptr; // No guardar imgui.ini

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void OverlayWindow::setupStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    
    // Dark theme
    ImGui::StyleColorsDark();
}

void OverlayWindow::applyWindowAttributes() {
#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(m_window);
    if (!hwnd) {
        std::cerr << "Failed to get Win32 window handle" << std::endl;
        return;
    }

    // Extend frame into client area (for full transparency)
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    // Get current extended style
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);

    // Always set LAYERED and TOPMOST
    exStyle |= WS_EX_LAYERED | WS_EX_TOPMOST;

    if (m_editMode) {
        // =====================================================
        // EDIT MODE (UNLOCKED):
        // - Remove WS_EX_TRANSPARENT so the window CAN receive input
        // - Use LWA_COLORKEY with pure black (RGB 0,0,0) so that
        //   transparent background pixels are click-through,
        //   but opaque widget pixels remain clickable.
        // =====================================================
        exStyle &= ~WS_EX_TRANSPARENT;
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);

        // ColorKey = pure black → transparent black areas are click-through
        SetLayeredWindowAttributes(hwnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
    } else {
        // =====================================================
        // LOCKED MODE:
        // - Set WS_EX_TRANSPARENT so entire window is click-through
        // - Use LWA_ALPHA for global opacity control
        // =====================================================
        exStyle |= WS_EX_TRANSPARENT;
        SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);

        BYTE alphaByte = static_cast<BYTE>(m_globalAlpha * 255);
        SetLayeredWindowAttributes(hwnd, 0, alphaByte, LWA_ALPHA);
    }

    // CRITICAL: Force window to stay on top
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);

    std::cout << "[Window] Attributes applied - EditMode: " << m_editMode << std::endl;
#endif
}

void OverlayWindow::toggleEditMode() {
    m_editMode = !m_editMode;
    applyWindowAttributes(); // Re-apply window attributes to update click-through
    
    std::cout << (m_editMode ?
        "EDIT MODE - You can drag widgets" : "LOCKED - Click-through enabled") << std::endl;
}

void OverlayWindow::run() {
    while (m_running && !glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        processInput();

        // Update iRacing data
        if (m_sdk) {
            // Try to connect if not connected
            if (!m_sdk->isConnected()) {
                m_sdk->startup();
            }
            
            // Wait and process new data
            if (m_sdk->waitForData(16)) {
                // New data available, update relative
                if (m_sdk->isSessionActive()) {
                    m_relative->update();
                }
            }
        }

        renderFrame();
    }
}

void OverlayWindow::renderFrame() {
    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // =========================================================
    // ALWAYS render widgets — each widget's render() method
    // already has its own guard (checks sdk/session internally).
    // This ensures they draw whenever data is available.
    // =========================================================
    m_relativeWidget->render(m_relative.get(), m_editMode);
    m_telemetryWidget->render(m_sdk.get(), m_editMode);

    // Status indicator in top-left corner
    {
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.5f);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                                ImGuiWindowFlags_NoMove |
                                ImGuiWindowFlags_NoSavedSettings |
                                ImGuiWindowFlags_AlwaysAutoResize;
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

    // Render ImGui
    ImGui::Render();
    
    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // Transparent background
    glClear(GL_COLOR_BUFFER_BIT);
    
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    glfwSwapBuffers(m_window);
}

void OverlayWindow::processInput() {
    // Q - Quit
    if (glfwGetKey(m_window, GLFW_KEY_Q) == GLFW_PRESS) {
        m_running = false;
    }

    // L - Toggle lock mode (with debounce)
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
