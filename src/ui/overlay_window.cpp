#include "ui/overlay_window.h"
#include "ui/relative_widget.h"
#include "ui/telemetry_widget.h"
#include "data/irsdk_manager.h"
#include "data/relative_calc.h"
#include "utils/config.h"

// OpenGL loader FIRST – must come before anything that might pull GL
#include <glad/glad.h>

// Tell GLFW not to include any GL headers itself
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// ImGui (safe after glad + GLFW)
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <iostream>

// Windows-specific stuff – do this LAST
#ifdef _WIN32
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3native.h>

    // Let Windows define its own APIENTRY – undef Glad's version first to avoid warning
    #ifdef APIENTRY
        #undef APIENTRY
    #endif

    #include <Windows.h>
#endif

namespace ui {

OverlayWindow::OverlayWindow()
    : m_window(nullptr)
    , m_running(false)
    , m_clickThrough(false)
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
    
    // Load config BEFORE applying window attributes
    utils::Config::load("config.ini");
    
    // Check if we should start locked
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
    
    m_running = true;
    
    std::cout << "✅ Overlay initialized" << std::endl;
    std::cout << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  Q      - Quit" << std::endl;
    std::cout << "  L      - Toggle Lock (click-through mode)" << std::endl;
    std::cout << "  Drag   - Move widgets (when unlocked)" << std::endl;
    std::cout << std::endl;
    std::cout << "Status: " << (m_clickThrough ? "🔒 LOCKED" : "🔓 UNLOCKED") << std::endl;
    std::cout << std::endl;
    
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
    
    // Apply global alpha from config
    float globalAlpha = utils::Config::getGlobalAlpha();
    colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, globalAlpha);
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
    
    // Get current style
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    
    // ALWAYS make it a layered window (required for transparency)
    exStyle |= WS_EX_LAYERED;
    
    if (m_clickThrough) {
        // Enable click-through: add WS_EX_TRANSPARENT
        exStyle |= WS_EX_TRANSPARENT;
        std::cout << "🔒 Click-through ENABLED" << std::endl;
    } else {
        // Disable click-through: remove WS_EX_TRANSPARENT
        exStyle &= ~WS_EX_TRANSPARENT;
        std::cout << "🔓 Click-through DISABLED" << std::endl;
    }
    
    // Apply the style
    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);
    
    // Set transparency (255 = fully opaque for the window frame, but ImGui controls alpha per-pixel)
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    
    // ALWAYS keep it topmost
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, 
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    
    std::cout << "Window attributes updated" << std::endl;
#endif
}

void OverlayWindow::toggleClickThrough() {
    m_clickThrough = !m_clickThrough;
    updateWindowAttributes();
    utils::Config::setClickThrough(m_clickThrough);
    
    std::cout << (m_clickThrough ? "🔒 LOCKED (click-through enabled)" : "🔓 UNLOCKED (click-through disabled)") << std::endl;
}

void OverlayWindow::run() {
    int connectionAttempts = 0;
    bool wasConnected = false;
    
    while (!glfwWindowShouldClose(m_window) && m_running) {
        glfwPollEvents();
        processInput();
        
        // Try to connect to iRacing
        if (!m_sdk->isConnected()) {
            if (!wasConnected) {
                connectionAttempts++;
                if (connectionAttempts % 60 == 0) {  // Every ~1 second at 60fps
                    std::cout << "⏳ Attempting to connect to iRacing... (attempt " 
                              << (connectionAttempts / 60) << ")" << std::endl;
                }
            }
            
            if (m_sdk->startup()) {
                std::cout << "✅ Connected to iRacing!" << std::endl;
                wasConnected = true;
                connectionAttempts = 0;
            }
        } else {
            // Connected - wait for data and update
            if (m_sdk->waitForData(16)) {
                m_relative->update();
                
                // Debug: print first successful data read
                static bool firstData = true;
                if (firstData) {
                    std::cout << "📊 Receiving telemetry data!" << std::endl;
                    firstData = false;
                }
            }
            
            // Check if we lost connection
            if (!wasConnected) {
                wasConnected = true;
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
    
    // Show lock status indicator
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.7f);
    ImGui::Begin("##LockStatus", nullptr, 
        ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoFocusOnAppearing);
    
    if (m_clickThrough) {
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "🔒 LOCKED");
        ImGui::Text("Press L to unlock");
    } else {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "🔓 UNLOCKED");
        ImGui::Text("Drag widgets to move");
        ImGui::Text("Press L to lock");
    }
    ImGui::End();
    
    // Render widgets or connection status
    if (m_sdk->isConnected()) {
        m_relativeWidget->render(m_relative.get());
        m_telemetryWidget->render(m_sdk.get());
    } else {
        // Show connection status
        ImGui::SetNextWindowPos(ImVec2(10, 100), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.8f);
        ImGui::Begin("##Status", nullptr, 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoFocusOnAppearing);
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "⏳ Waiting for iRacing...");
        ImGui::Separator();
        ImGui::TextWrapped("Make sure iRacing is running and you're in a session (not menus)");
        ImGui::Text(" ");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Troubleshooting:");
        ImGui::BulletText("Start a Test Drive session");
        ImGui::BulletText("Go to Options → Enable SDK");
        ImGui::BulletText("Restart the overlay after starting iRacing");
        ImGui::End();
    }
    
    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // Fully transparent
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
    
    // L to toggle lock (click-through)
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
    std::cout << "💾 Saving configuration..." << std::endl;
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
