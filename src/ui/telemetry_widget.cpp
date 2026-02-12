#include "ui/telemetry_widget.h"
#include "ui/overlay_window.h"
#include "data/irsdk_manager.h"
#include "utils/config.h"
#include <imgui.h>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <glad/glad.h>   // FIXED: was <GL/gl.h> - need GLAD for GL_CLAMP_TO_EDGE, glGenerateMipmap etc.
#include <stb_image.h>
#include <cstdint>

namespace ui {

TelemetryWidget::TelemetryWidget(OverlayWindow* overlay)
    : m_currentRPM(0.0f)
    , m_maxRPM(7000.0f)
    , m_blinkRPM(6500.0f)
    , m_shiftFirstRPM(0.0f)
    , m_shiftLastRPM(0.0f)
    , m_throttle(0.0f)
    , m_brake(0.0f)
    , m_clutch(0.0f)
    , m_gear(0)
    , m_speed(0)
    , m_steeringAngle(0.0f)
    , m_steeringAngleMax(7.854f)
    , m_absActive(false)
    , m_historyHead(0)
    , m_scale(1.0f)
    , m_overlay(overlay)
    , m_steeringTexture(0)
    , m_absOnTexture(0)
    , m_absOffTexture(0)
{
    m_throttleHistory.resize(256, 0.0f);
    m_brakeHistory.resize(256, 0.0f);

    // Load textures
    loadAssets();
}

TelemetryWidget::~TelemetryWidget() {
    if (m_steeringTexture) glDeleteTextures(1, &m_steeringTexture);
    if (m_absOnTexture) glDeleteTextures(1, &m_absOnTexture);
    if (m_absOffTexture) glDeleteTextures(1, &m_absOffTexture);
}

unsigned int TelemetryWidget::loadTextureFromFile(const char* filepath) {
    GLuint textureID = 0;

    int width, height, nrChannels;
    unsigned char* data = stbi_load(filepath, &width, &height, &nrChannels, STBI_rgb_alpha);

    if (!data) {
        std::cout << "[Telemetry] Failed to load texture: " << filepath << std::endl;
        return 0;
    }

    std::cout << "[Telemetry] Loaded texture: " << filepath << " (" << width << "x" << height << ")" << std::endl;

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);

    return textureID;
}

void TelemetryWidget::loadAssets() {
    std::cout << "[Telemetry] Loading assets..." << std::endl;

    // Try multiple path variations
    const char* steerPaths[] = {
        "assets/telemetry/steering_wheel.png",
        "assets/steering_wheel.png",
        "steering_wheel.png"
    };

    for (const char* path : steerPaths) {
        m_steeringTexture = loadTextureFromFile(path);
        if (m_steeringTexture) break;
    }

    const char* absPaths[] = {
        "assets/telemetry/abs_on.png",
        "assets/abs_on.png",
        "abs_on.png"
    };

    for (const char* path : absPaths) {
        m_absOnTexture = loadTextureFromFile(path);
        if (m_absOnTexture) break;
    }

    const char* absOffPaths[] = {
        "assets/telemetry/abs_off.png",
        "assets/abs_off.png",
        "abs_off.png"
    };

    for (const char* path : absOffPaths) {
        m_absOffTexture = loadTextureFromFile(path);
        if (m_absOffTexture) break;
    }
}

void TelemetryWidget::render(bool editMode) {
    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
    if (!editMode) {
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoResize;
    }

    ImGui::Begin("##TELEMETRY", nullptr, flags);
    ImGui::SetWindowFontScale(m_scale);

    utils::Config& config = utils::Config::getInstance();

    // Get window size
    ImVec2 pos = ImGui::GetWindowPos();
    config.posX = pos.x;
    config.posY = pos.y;
    config.width = ImGui::GetWindowSize().x;
    config.height = ImGui::GetWindowSize().y;

    ImGui::Text("RPM: %.0f / %.0f", m_currentRPM, m_maxRPM);
    ImGui::Text("Speed: %d km/h", m_speed);
    ImGui::Text("Gear: %d", m_gear);
    ImGui::Text("Throttle: %.1f%%", m_throttle * 100.0f);
    ImGui::Text("Brake: %.1f%%", m_brake * 100.0f);
    ImGui::Text("Clutch: %.1f%%", m_clutch * 100.0f);

    if (m_steeringTexture) {
        ImGui::Image((ImTextureID)(intptr_t)m_steeringTexture, ImVec2(128, 128));
    }

    if (m_absActive && m_absOnTexture) {
        ImGui::Image((ImTextureID)(intptr_t)m_absOnTexture, ImVec2(64, 64));
    } else if (m_absOffTexture) {
        ImGui::Image((ImTextureID)(intptr_t)m_absOffTexture, ImVec2(64, 64));
    }

    ImGui::End();
}

void TelemetryWidget::setScale(float scale) {
    m_scale = scale;
}

}  // namespace ui