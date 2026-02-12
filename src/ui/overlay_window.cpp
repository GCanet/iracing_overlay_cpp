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

namespace ui {

OverlayWindow::OverlayWindow()
    : m_window(nullptr), m_running(false), m_editMode(false),
      m_globalAlpha(0.9f), m_windowWidth(1920), m_windowHeight(1080) {}

OverlayWindow::~OverlayWindow() {
    shutdown();
}

bool OverlayWindow::initialize() {
    if (!glfwInit()) {
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);  // Borderless window
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);    // Always on top

    // Get primary monitor dimensions for fullscreen borderless
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    m_windowWidth = mode->width;
    m_windowHeight = mode->height;

    m_window = glfwCreateWindow(m_windowWidth, m_windowHeight, "iRacing Overlay", nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        return false;
    }

    // Position at (0,0) for true fullscreen borderless
    glfwSetWindowPos(m_window, 0, 0);

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(0);  // No vsync - run as fast as possible

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(m_window);
        glfwTerminate();
        return false;
    }

    setupImGui();
    setupStyle();

    m_sdk = std::make_unique<iracing::IRSDKManager>();
    m_relative = std::make_unique<iracing::RelativeCalculator>(m_sdk.get());
    m_relativeWidget = std::make_unique<RelativeWidget>(this);
    m_telemetryWidget = std::make_unique<TelemetryWidget>(this);

    utils::Config::load("config.ini");

    m_running = true;
    applyWindowAttributes();
    return true;
}

void OverlayWindow::setupImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void OverlayWindow::setupStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(4, 4);
    style.FramePadding = ImVec2(3, 3);
    style.ItemSpacing = ImVec2(8, 4);
    style.WindowRounding = 0.0f;
    style.FrameRounding = 2.0f;
    style.Alpha = 0.95f;
    style.AntiAliasedLines = true;
    style.AntiAliasedLinesUseTex = true;
}

void OverlayWindow::processInput() {
    if (glfwGetKey(m_window, GLFW_KEY_Q) == GLFW_PRESS) {
        m_running = false;
        saveConfigOnExit();
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
