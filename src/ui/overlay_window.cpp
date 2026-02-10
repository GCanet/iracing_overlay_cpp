#include "ui/overlay_window.h"
#include "ui/relative_widget.h"
#include "ui/telemetry_widget.h"
#include "data/irsdk_manager.h"
#include "data/relative_calc.h"
#include "utils/config.h"

// OpenGL loader FIRST - must come before anything that might pull GL
#include <glad/glad.h>

// Tell GLFW not to include any GL headers itself
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// ImGui (safe after glad + GLFW)
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <iostream>

// Windows-specific stuff - do this LAST
#ifdef _WIN32
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3native.h>

    // Suppress C4005: 'APIENTRY' macro redefinition coming from minwindef.h
    #pragma warning(push)
    #pragma warning(disable: 4005)

    // Let Windows define its own APIENTRY - undef Glad's version first
    #ifdef APIENTRY
        #undef APIENTRY
    #endif

    #include <Windows.h>
    #include <dwmapi.h>
    #pragma comment(lib, "dwmapi.lib")

    #pragma warning(pop)
#endif

namespace ui {

OverlayWindow::OverlayWindow()
    : m_window(nullptr)
    , m_running(false)
    , m_clickThrough(true)  // Start locked by default
    , m_lockKeyPressed(false)
    , m_windowWidth(1920)
    , m_windowHeight(1080)
{
}

OverlayWindow::~OverlayWindow() {
    shutdown();
}

bool OverlayWindow::initialize() {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return false;
    }
    
    // GL 3.3 + GLSL 330
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    // TRUE OVERLAY HINTS
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);          // Borderless
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);            // Always on top
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);          // Non-resizable
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);      // Don't steal focus
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);            // Start HIDDEN to avoid black flash
    
    // Get monitor size for fullscreen overlay
    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primary);
    m_windowWidth = mode->width;
    m_windowHeight = mode->height;
    
    // Create window
    m_window = glfwCreateWindow(
        m_windowWidth,
        m_windowHeight,
        "iRacing Overlay",
        nullptr,
        nullptr
    );
    
    if (!m_window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return false;
    }
    
    // Position at 0,0 (fullscreen overlay)
    glfwSetWindowPos(m_window, 0, 0);
    
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // Enable vsync
    
    // Load OpenGL
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }
    
    // Enable blending for proper transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Load config BEFORE applying window attributes
    utils::Config::load("config.ini");
    
    // Check if we should start locked (default to true if not in config)
    m_clickThrough = utils::Config::isClickThrough();
    
    // Apply Windows-specific overlay attributes
    updateWindowAttributes();
    
    // Setup ImGui
    setupImGui();
    setupStyle();
    
    // Initialize iRacing SDK
    m_sdk = std::make_unique<iracing::IRSDKManager>();
    m_relative = std::make_unique<iracing::RelativeCalculator>(m_sdk.get());
    
    // Initialize widgets
    m_relativeWidget = std::make_unique<RelativeWidget>();
    m_telemetryWidget = std::make_unique<TelemetryWidget>();
    
    // Render one fully transparent frame BEFORE showing the window
    // This prevents the black flash on startup
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(m_window);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(m_window);
    
    // NOW show the window - it will be fully transparent, no black flash
    glfwShowWindow(m_window);
    
    m_running = true;
    
    std::cout << "Overlay initialized" << std::endl;
    std::cout << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  Q      - Quit" << std::endl;
    std::cout << "  L      - Toggle Lock (drag mode)" << std::endl;
    std::cout << "  Drag   - Move widgets (when unlocked)" << std::endl;
    std::cout << std::endl;
    std::cout << "Status: " << (m_clickThrough ? "LOCKED (widgets fixed)" : "UNLOCKED (drag to move widgets)") << std::endl;
    
    return true;
}

void OverlayWindow::setupImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;
    
    // Apply font scale from config
    io.FontGlobalScale = utils::Config::getFontScale();
    
    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void OverlayWindow::setupStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Dark theme with transparency
    ImGui::StyleColorsDark();
    
    // Customize colors
    ImVec4* colors = style.Colors;
    
    // All window backgrounds fully transparent by default
    // Individual widgets control their own alpha
    colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_Border] = ImVec4(0.0f, 1.0f, 0.0f, 0.3f);
    colors[ImGuiCol_Text] = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    colors[ImGuiCol_Header] = ImVec4(0.0f, 0.5f, 0.0f, 0.8f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.0f, 0.7f, 0.0f, 0.8f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.0f, 0.9f, 0.0f, 0.8f);
    
    // Rounding
    style.WindowRounding = 8.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    
    // Padding
    style.WindowPadding = ImVec2(10, 10);
    style.FramePadding = ImVec2(8, 4);
}

void OverlayWindow::updateWindowAttributes() {
#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(m_window);
    if (!hwnd) return;
    
    // Extend DWM frame into client area for per-pixel alpha transparency
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);
    
    // Get current style
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    
    if (m_clickThrough) {
        // LOCKED: click-through entire window, no dragging
        exStyle |= WS_EX_LAYERED | WS_EX_TRANSPARENT;
    } else {
        // UNLOCKED: window captures input for widget dragging
        // Remove click-through but keep layered for DWM compatibility
        exStyle |= WS_EX_LAYERED;
        exStyle &= ~WS_EX_TRANSPARENT;
    }
    
    // Apply the style
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    
    // DO NOT call SetLayeredWindowAttributes - it overrides per-pixel alpha
    // The transparent framebuffer + DWM handles transparency
    
    // ALWAYS keep it topmost - use SWP_NOACTIVATE to not steal focus
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, 
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#endif
}

void OverlayWindow::toggleClickThrough() {
    m_clickThrough = !m_clickThrough;
    updateWindowAttributes();
    utils::Config::setClickThrough(m_clickThrough);
    
    std::cout << (m_clickThrough ? "LOCKED (widgets fixed)" : "UNLOCKED (drag to move widgets)") << std::endl;
}

void OverlayWindow::run() {
    int connectionAttempts = 0;
    bool wasConnected = false;
    
    while (!glfwWindowShouldClose(m_window) && m_running) {
        glfwPollEvents();
        processInput();
        
        // Re-assert topmost EVERY frame - cheap call, prevents losing z-order
#ifdef _WIN32
        {
            HWND hwnd = glfwGetWin32Window(m_window);
            if (hwnd) {
                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, 
                            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
        }
#endif
        
        // Connection management - use startup() which handles reconnection
        if (!m_sdk->isSessionActive()) {
            // Not connected or session not active - try to connect/reconnect
            if (wasConnected) {
                std::cout << "iRacing session disconnected, waiting for reconnection..." << std::endl;
                wasConnected = false;
                connectionAttempts = 0;
            }
            
            connectionAttempts++;
            if (connectionAttempts % 60 == 0) {
                std::cout << "Attempting to connect to iRacing... (attempt " 
                          << (connectionAttempts / 60) << ")" << std::endl;
            }
            
            // startup() now handles both first connection and reconnection
            if (m_sdk->startup() && m_sdk->isSessionActive()) {
                std::cout << "Connected to iRacing!" << std::endl;
                wasConnected = true;
                connectionAttempts = 0;
            }
        } else {
            // Connected and session active
            if (!wasConnected) {
                std::cout << "Connected to iRacing!" << std::endl;
                wasConnected = true;
            }
            
            // Wait for new data (short timeout, don't block rendering)
            m_sdk->waitForData(1);
            m_relative->update();
            
            // Debug: print first successful data read
            static bool firstData = true;
            if (firstData && !m_relative->getAllDrivers().empty()) {
                std::cout << "Receiving telemetry data! (" 
                          << m_relative->getAllDrivers().size() << " drivers found)" << std::endl;
                firstData = false;
            }
        }
        
        renderFrame();
    }
    
    saveConfigOnExit();
}

void OverlayWindow::renderFrame() {
    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    // Show lock status indicator ONLY when unlocked, at bottom right
    if (!m_clickThrough) {
        ImGui::SetNextWindowPos(
            ImVec2(ImGui::GetIO().DisplaySize.x - 220, 
                   ImGui::GetIO().DisplaySize.y - 110), 
            ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.5f);
        ImGui::Begin("##LockStatus", nullptr, 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing);
        
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "UNLOCKED");
        ImGui::Text("Drag widgets to move");
        ImGui::Text("Press L to lock");
        
        ImGui::End();
    }
    
    // Render widgets or connection status
    if (m_sdk->isSessionActive()) {
        m_relativeWidget->render(m_relative.get());
        m_telemetryWidget->render(m_sdk.get());
    } else {
        // Show connection status - bottom right when not connected
        ImGui::SetNextWindowPos(
            ImVec2(ImGui::GetIO().DisplaySize.x - 420, 
                   ImGui::GetIO().DisplaySize.y - 210), 
            ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.8f);
        ImGui::Begin("##Status", nullptr, 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Waiting for iRacing...");
        ImGui::Separator();
        ImGui::TextWrapped("Make sure iRacing is running and you're in a session");
        ImGui::Text(" ");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Troubleshooting:");
        ImGui::BulletText("Start a Test Drive or any session");
        ImGui::BulletText("Wait for the session to fully load");
        ImGui::End();
    }
    
    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // Fully transparent background
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    glfwSwapBuffers(m_window);
}

void OverlayWindow::processInput() {
    // Q to quit
    if (glfwGetKey(m_window, GLFW_KEY_Q) == GLFW_PRESS) {
        std::cout << "Quit key pressed..." << std::endl;
        m_running = false;
    }
    
    // L to toggle lock (drag mode)
    if (glfwGetKey(m_window, GLFW_KEY_L) == GLFW_PRESS) {
        if (!m_lockKeyPressed) {
            toggleClickThrough();
            m_lockKeyPressed = true;
        }
    } else {
        m_lockKeyPressed = false;
    }
}

void OverlayWindow::saveConfigOnExit() {
    std::cout << "Saving configuration..." << std::endl;
    utils::Config::save("config.ini");
}

void OverlayWindow::shutdown() {
    if (m_sdk) {
        m_sdk->shutdown();
    }
    
    if (m_window) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }
    
    std::cout << "Overlay shutdown complete" << std::endl;
}

} // namespace ui
