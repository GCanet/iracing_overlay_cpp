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
      m_editMode(false),                // Start LOCKED (no edición)
      m_globalAlpha(0.92f),             // Valor inicial razonable
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
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);           // Start hidden

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
    // Puedes cargar m_globalAlpha desde config si existe
    m_globalAlpha = utils::Config::getValue<float>("GlobalAlpha", 0.92f);

    applyWindowAttributes();

    setupImGui();
    setupStyle();

    m_sdk = std::make_unique<iracing::IRSDKManager>();
    m_relative = std::make_unique<iracing::RelativeCalculator>(m_sdk.get());

    m_relativeWidget = std::make_unique<RelativeWidget>();
    m_telemetryWidget = std::make_unique<TelemetryWidget>();

    // Render 2 frames transparentes antes de mostrar → evita flash negro
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(m_window);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(m_window);

    glfwShowWindow(m_window);

    m_running = true;

    std::cout << "Overlay initialized\n";
    std::cout << "Controls:\n";
    std::cout << "  Q           - Quit\n";
    std::cout << "  Insert      - Toggle Edit Mode (right-click widgets for options)\n";
    std::cout << "  Right-click on widget (in edit mode) → context menu\n\n";
    std::cout << "Status: LOCKED (edit mode off)\n";

    return true;
}

void OverlayWindow::setupImGui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavNoCaptureKeyboard;
    io.FontGlobalScale = utils::Config::getFontScale();

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void OverlayWindow::setupStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark();

    auto& colors = style.Colors;
    colors[ImGuiCol_WindowBg]   = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_ChildBg]    = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_Border]     = ImVec4(0.43f, 0.43f, 0.50f, 0.50f);
    colors[ImGuiCol_Text]       = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);

    style.WindowRounding    = 6.0f;
    style.FrameRounding     = 4.0f;
    style.GrabRounding      = 4.0f;
    style.WindowPadding     = ImVec2(8, 8);
    style.FramePadding      = ImVec2(6, 4);
}

void OverlayWindow::applyWindowAttributes() {
#ifdef _WIN32
    HWND hwnd = glfwGetWin32Window(m_window);
    if (!hwnd) return;

    // DWM per-pixel alpha (muy importante para ImGui + transparencia real)
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    exStyle |= WS_EX_LAYERED | WS_EX_TOPMOST;

    if (!m_editMode) {
        exStyle |= WS_EX_TRANSPARENT;     // click-through
    } else {
        exStyle &= ~WS_EX_TRANSPARENT;    // recibe clics
    }

    SetWindowLong(hwnd, GWL_EXSTYLE, exStyle);

    // Alpha global (usamos LWA_ALPHA → más predecible con ImGui)
    BYTE alphaByte = static_cast<BYTE>(m_globalAlpha * 255);
    SetLayeredWindowAttributes(hwnd, 0, alphaByte, LWA_ALPHA);

    // Forzar topmost sin robar foco
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
#endif
}

void OverlayWindow::toggleEditMode() {
    m_editMode = !m_editMode;
    applyWindowAttributes();

    // Opcional: guardar en config
    // utils::Config::setValue("EditMode", m_editMode);

    std::cout << (m_editMode ? "EDIT MODE ON – right-click widgets" : "LOCKED") << "\n";
}

void OverlayWindow::run() {
    bool wasConnected = false;
    int connectionAttempts = 0;

    while (!glfwWindowShouldClose(m_window) && m_running) {
        glfwPollEvents();
        processInput();

        // Re-forzar topmost cada ~30 frames (barato y previene perder z-order)
#ifdef _WIN32
        static int topmostCounter = 0;
        if (++topmostCounter % 30 == 0) {
            HWND hwnd = glfwGetWin32Window(m_window);
            if (hwnd) {
                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
        }
#endif

        // iRacing connection logic (igual que antes)
        if (!m_sdk->isSessionActive()) {
            // ... (tu lógica de reconexión) ...
            if (m_sdk->startup() && m_sdk->isSessionActive()) {
                std::cout << "Connected to iRacing!\n";
                wasConnected = true;
            }
        } else {
            m_sdk->waitForData(1);
            m_relative->update();
        }

        renderFrame();
    }

    saveConfigOnExit();
}

void OverlayWindow::renderFrame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Fondo completamente transparente cada frame
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Render widgets
    if (m_sdk->isSessionActive()) {
        // Pasamos editMode a los widgets para que activen drag y menú contextual
        m_relativeWidget->render(m_relative.get(), m_editMode);
        m_telemetryWidget->render(m_sdk.get(), m_editMode);
    } else {
        // Status window (sin cambios)
        // ...
    }

    ImGui::Render();
    int w, h;
    glfwGetFramebufferSize(m_window, &w, &h);
    glViewport(0, 0, w, h);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(m_window);
}

void OverlayWindow::processInput() {
    // Q → salir
    if (glfwGetKey(m_window, GLFW_KEY_Q) == GLFW_PRESS) {
        m_running = false;
    }

    // Insert → toggle edit mode
    static bool insertWasDown = false;
    bool insertNow = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    if (insertNow && !insertWasDown) {
        toggleEditMode();
    }
    insertWasDown = insertNow;
}

void OverlayWindow::saveConfigOnExit() {
    utils::Config::setValue("GlobalAlpha", m_globalAlpha);
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
