#include "ui/overlay_window.h"
#include "ui/relative_widget.h"
#include "ui/telemetry_widget.h"
#include "data/irsdk_manager.h"
#include "data/relative_calc.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include <glad/glad.h>

#include <iostream>

namespace ui {

OverlayWindow::OverlayWindow()
    : m_window(nullptr)
    , m_showDemo(false)
    , m_running(false)
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
    
    // Transparent window
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    
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
    
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // Enable vsync
    
    // Load OpenGL
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return false;
    }
    
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
    std::cout << "Press ESC to toggle demo window" << std::endl;
    std::cout << "Press Q to quit" << std::endl;
    
    return true;
}

void OverlayWindow::setupImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;
    
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
    colors[ImGuiCol_WindowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.7f);
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

void OverlayWindow::run() {
    while (!glfwWindowShouldClose(m_window) && m_running) {
        glfwPollEvents();
        processInput();
        
        // Try to connect to iRacing
        if (!m_sdk->isConnected()) {
            m_sdk->startup();
        }
        
        // Wait for data
        if (m_sdk->isConnected()) {
            if (m_sdk->waitForData(16)) {
                m_relative->update();
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
    
    // Render widgets
    if (m_sdk->isConnected()) {
        m_relativeWidget->render(m_relative.get());
        m_telemetryWidget->render(m_sdk.get());
    } else {
        // Show connection status
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::Begin("Status", nullptr, 
            ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::Text("Waiting for iRacing...");
        ImGui::End();
    }
    
    // Demo window (for testing)
    if (m_showDemo) {
        ImGui::ShowDemoWindow(&m_showDemo);
    }
    
    // Rendering
    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f); // Transparent
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    glfwSwapBuffers(m_window);
}

void OverlayWindow::processInput() {
    // ESC to toggle demo
    if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        static bool escPressed = false;
        if (!escPressed) {
            m_showDemo = !m_showDemo;
            escPressed = true;
        }
    } else {
        static bool escPressed = false;
        escPressed = false;
    }
    
    // Q to quit
    if (glfwGetKey(m_window, GLFW_KEY_Q) == GLFW_PRESS) {
        m_running = false;
    }
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
    
    std::cout << "Overlay shutdown" << std::endl;
}

} // namespace ui
