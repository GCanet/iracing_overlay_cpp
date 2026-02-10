#include "ui/overlay_window.h"
#include "ui/relative_widget.h"
#include "ui/telemetry_widget.h"
#include "data/irsdk_manager.h"
#include "data/relative_calc.h"
#include "utils/config.h"

// OpenGL loader FIRST
#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

// ImGui
#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include <iostream>

#ifdef _WIN32
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <GLFW/glfw3native.h>
    #include <Windows.h>
    #include <dwmapi.h>
    #pragma comment(lib, "dwmapi.lib")
#endif

namespace ui {

OverlayWindow::OverlayWindow()
    : m_window(nullptr),
      m_running(false),
      m_editMode(false),                // Start LOCKED
      m_globalAlpha(0.92f),
      m_windowWidth(1920),
      m_windowHeight(1080)
{
}

OverlayWindow::~OverlayWindow() {
    shutdown();
}

bool OverlayWindow::initialize() {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primary);
    m_windowWidth = mode->width;
    m_windowHeight = mode->height;

    m_window = glfwCreateWindow(m_windowWidth, m_windowHeight,
                                "iRacing Overlay", nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return false;
    }

    glfwSetWindowPos(m_window, 0, 0);
    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return false;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    utils::Config::load("config.ini");
    m_globalAlpha = utils::Config::getValue<float>("GlobalAlpha", 0.92f);
    // Opcional: cargar editMode si lo quieres persistente
    // m_editMode = utils::Config::getValue<bool>("EditMode", false);

    applyWindowAttributes();

    setupImGui();
    setupStyle();

    m_sdk = std::make_unique<iracing::IRSDKManager>();
    m_relative = std::make_unique<iracing::RelativeCalculator>(m_sdk.get());

    m_relativeWidget = std::make_unique<RelativeWidget>();
    m_telemetryWidget = std::make_unique<TelemetryWidget>();

    // Frames transparentes iniciales
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(m_window);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(m_window);

    glfwShowWindow(m_window);

    m_running = true;

    std::cout << "Overlay initialized\n";
    std::cout << "Controls:\n  Q - Quit\n  Insert - Toggle Edit Mode\n  Right-click widget (edit mode) → menu\n";
    std::cout << "Status: " << (m_editMode ? "EDIT MODE ON" : "LOCKED") << "\n";

    return true;
}

// setupImGui y setupStyle sin cambios (están bien)

void OverlayWindow::applyWindowAttributes() {
#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(m_window);
    if (!hwnd) return;

    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_LAYERED | WS_EX_TOPMOST;

    if (!m_editMode) {
        exStyle |= WS_EX_TRANSPARENT;
    } else {
        exStyle &= ~WS_EX_TRANSPARENT;
    }

    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);

    BYTE alphaByte = static_cast<BYTE>(m_globalAlpha * 255);
    SetLayeredWindowAttributes(hwnd, 0, alphaByte, LWA_ALPHA);

    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
#endif
}

void OverlayWindow::toggleEditMode() {
    m_editMode = !m_editMode;
    applyWindowAttributes();
    std::cout << (m_editMode ? "EDIT MODE ON – right-click widgets" : "LOCKED") << "\n";
}

void OverlayWindow::run() {
    bool wasConnected = false;

    while (!glfwWindowShouldClose(m_window) && m_running) {
        glfwPollEvents();
        processInput();

        // Re-forzar topmost (ya lo tienes bien)

#ifdef _WIN32
        static int topmostCounter = 0;
        if (++topmostCounter % 30 == 0) {
            HWND hwnd = glfwGetWin32Window(m_window);
            if (hwnd) SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
#endif

        if (!m_sdk->isSessionActive()) {
            if (m_sdk->startup() && m_sdk->isSessionActive()) {
                std::cout << "Connected to iRacing!\n";
                wasConnected = true;
            }
        } else {
            // Cambio clave: timeout 16 ms para ~60 Hz, menos CPU
            if (m_sdk->waitForData(16)) {
                m_relative->update();  // solo si hay datos nuevos
            }
        }

        renderFrame();
    }

    saveConfigOnExit();
}

void OverlayWindow::renderFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (m_sdk->isSessionActive()) {
        m_relativeWidget->render(m_relative.get(), m_editMode);
        m_telemetryWidget->render(m_sdk.get(), m_editMode);
    } else {
        // Añadido: ventana de status cuando no conectado
        ImGui::SetNextWindowPos(ImVec2(20, m_windowHeight - 150), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.7f);
        ImGui::Begin("##Status", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Waiting for iRacing session...");
        ImGui::Text("Start a test/practice/race in iRacing");
        ImGui::End();
    }

    ImGui::Render();
    int w, h;
    glfwGetFramebufferSize(m_window, &w, &h);
    glViewport(0, 0, w, h);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(m_window);
}

// processInput sin cambios (Insert + Q bien)

void OverlayWindow::saveConfigOnExit() {
    utils::Config::setValue("GlobalAlpha", m_globalAlpha);
    // Opcional: utils::Config::setValue("EditMode", m_editMode);
    utils::Config::save("config.ini");
}

void OverlayWindow::shutdown() {
    if (m_sdk) m_sdk->shutdown();

    if (m_window) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }
}

} // namespace ui
