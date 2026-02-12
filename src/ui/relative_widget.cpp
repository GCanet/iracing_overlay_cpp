#include "ui/relative_widget.h"
#include "ui/overlay_window.h"
#include "data/relative_calc.h"
#include "data/irsdk_manager.h"
#include "utils/config.h"
#include <imgui.h>
#include <glad/glad.h>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <cstdint>

// stb_image.h included for declaration only — STB_IMAGE_IMPLEMENTATION lives in stb_impl.cpp
#include "stb_image.h"

namespace ui {

RelativeWidget::RelativeWidget(OverlayWindow* overlay)
    : m_overlay(overlay)
{
    loadCarBrandTextures();
}

RelativeWidget::~RelativeWidget() {
    for (auto& [brand, texId] : m_carBrandTextures) {
        if (texId) glDeleteTextures(1, &texId);
    }
    m_carBrandTextures.clear();
}

unsigned int RelativeWidget::loadTextureFromFile(const char* filepath) {
    unsigned int textureID = 0;
    int width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filepath, &width, &height, &channels, 4);
    if (!data) return 0;

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

void RelativeWidget::loadCarBrandTextures() {
    // Try to load each known brand PNG from assets/car_brands/
    static const char* brands[] = {
        "bmw", "mercedes", "audi", "porsche", "ferrari", "lamborghini",
        "aston_martin", "mclaren", "ford", "chevrolet", "toyota", "mazda"
    };

    for (const char* brand : brands) {
        char path[256];
        snprintf(path, sizeof(path), "assets/car_brands/%s.png", brand);
        unsigned int tex = loadTextureFromFile(path);
        if (tex) {
            m_carBrandTextures[brand] = tex;
        }
    }
}

unsigned int RelativeWidget::getCarBrandTexture(const std::string& brand) const {
    auto it = m_carBrandTextures.find(brand);
    return (it != m_carBrandTextures.end()) ? it->second : 0;
}

void RelativeWidget::render(iracing::RelativeCalculator* relative, bool editMode) {
    if (!relative) return;

    utils::Config& config = utils::Config::getInstance();
    bool locked = !editMode && config.uiLocked;

    auto drivers = relative->getRelative(4, 4);
    if (drivers.empty()) return;

    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
    if (!locked) {
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoResize;
    } else {
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoResize;
    }

    // FIXED: Added matching Push calls for the Pop calls at the end
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.05f, 0.1f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.3f, 0.4f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 6));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.0f);

    ImGui::Begin("##RELATIVE", nullptr, flags);
    ImGui::SetWindowFontScale(m_scale);

    // Get window size
    ImVec2 pos = ImGui::GetWindowPos();
    config.posX = pos.x;
    config.posY = pos.y;
    config.width = ImGui::GetWindowSize().x;
    config.height = ImGui::GetWindowSize().y;

    // === HEADER ===
    renderHeader(relative);
    ImGui::Separator();

    char buffer[256];

    // === TABLE ===
    if (ImGui::BeginTable("RelativeTable", 8,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX)) {

        // Setup columns with adjusted widths
        ImGui::TableSetupColumn("Pos", ImGuiTableColumnFlags_WidthFixed, 28.0f * m_scale);
        ImGui::TableSetupColumn("Logo", ImGuiTableColumnFlags_WidthFixed, 22.0f * m_scale);
        ImGui::TableSetupColumn("Driver", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("SR", ImGuiTableColumnFlags_WidthFixed, 52.0f * m_scale);
        ImGui::TableSetupColumn("iR", ImGuiTableColumnFlags_WidthFixed, 60.0f * m_scale);
        ImGui::TableSetupColumn("Last", ImGuiTableColumnFlags_WidthFixed, 54.0f * m_scale);
        ImGui::TableSetupColumn("Gap", ImGuiTableColumnFlags_WidthFixed, 44.0f * m_scale);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 4.0f * m_scale);  // Filler

        // Render all drivers
        for (const auto& driver : drivers) {
            renderDriverRow(driver, driver.isPlayer, buffer);
        }

        ImGui::EndTable();
    }

    ImGui::Separator();

    // === FOOTER - ALWAYS DISPLAY ===
    renderFooter(relative);

    ImGui::End();
    ImGui::PopStyleVar(3);   // Matches 3 PushStyleVar calls above
    ImGui::PopStyleColor(2); // Matches 2 PushStyleColor calls above
}

void RelativeWidget::renderHeader(iracing::RelativeCalculator* relative) {
    std::string series = relative->getSeriesName();
    if (series.empty() || series == "Unknown Series") {
        series = "Practice Session";
    }
    std::string lapInfo = relative->getLapInfo();
    int sof = relative->getSOF();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    float totalWidth = ImGui::GetContentRegionAvail().x;
    ImVec2 seriesSize = ImGui::CalcTextSize(series.c_str());
    ImVec2 lapSize = ImGui::CalcTextSize(lapInfo.c_str());
    ImVec2 sofSize = ImGui::CalcTextSize("SOF: 8888");

    // Left side: Series name
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "%s", series.c_str());
    ImGui::SameLine();

    // Middle: Lap info (centered)
    float middleX = (totalWidth - lapSize.x) * 0.5f;
    float currentX = ImGui::GetCursorPosX();
    float spacerW = std::max(0.0f, middleX - currentX);
    ImGui::Dummy(ImVec2(spacerW, 0.0f));
    ImGui::SameLine(0, 0);
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", lapInfo.c_str());
    ImGui::SameLine();

    // Right side: SOF (aligned to the right)
    float rightX = totalWidth - sofSize.x;
    currentX = ImGui::GetCursorPosX();
    spacerW = std::max(0.0f, rightX - currentX);
    ImGui::Dummy(ImVec2(spacerW, 0.0f));
    ImGui::SameLine(0, 0);
    ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "SOF: %d", sof);

    ImGui::PopStyleVar();
}

void RelativeWidget::renderDriverRow(const iracing::Driver& driver, bool isPlayer, char* buffer) {
    ImGui::TableNextRow();
    float rowH = ImGui::GetTextLineHeight();

    // Highlight player row
    if (isPlayer) {
        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 215, 0, 40));
    }

    // === Col 1: Position ===
    ImGui::TableNextColumn();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + rowH * 0.15f);
    if (driver.isOnPit) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "PIT");
    } else {
        ImGui::Text("P%d", driver.relativePosition);
    }

    // === Col 2: Car Brand Logo ===
    ImGui::TableNextColumn();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + rowH * 0.15f);
    {
        float logoSize = 16.0f * m_scale;
        unsigned int tex = getCarBrandTexture(driver.carBrand);
        if (tex) {
            ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(logoSize, logoSize));
        } else {
            // No texture: show a small colored square as placeholder
            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImVec2 cp = ImGui::GetCursorScreenPos();
            dl->AddRectFilled(cp, ImVec2(cp.x + logoSize, cp.y + logoSize),
                             IM_COL32(60, 60, 60, 150), 2.0f);
            ImGui::Dummy(ImVec2(logoSize, logoSize));
        }
    }

    // === Col 3: Flag + #Number + Name ===
    ImGui::TableNextColumn();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + rowH * 0.05f);
    {
        // Club/country flag
        const char* flag = getClubFlag(driver.countryCode);
        if (flag && flag[0] != '\0') {
            ImGui::TextColored(ImVec4(0.7f, 0.8f, 0.9f, 1.0f), "%s", flag);
            ImGui::SameLine(0, 4);
        }

        // Car number (yellow, bold)
        ImGui::TextColored(ImVec4(1.0f, 0.95f, 0.3f, 1.0f), "#%s", driver.carNumber.c_str());
        ImGui::SameLine(0, 6);

        // Driver name
        if (isPlayer) {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.5f, 1.0f), "%s", driver.driverName.c_str());
        } else {
            ImGui::Text("%s", driver.driverName.c_str());
        }
    }

    // === Col 4: Safety Rating ===
    ImGui::TableNextColumn();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + rowH * 0.1f);
    {
        float r, g, b;
        getSafetyRatingColor(driver.safetyRating, r, g, b);
        const char* letter = getSafetyRatingLetter(driver.safetyRating);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 cp = ImGui::GetCursorScreenPos();
        ImVec2 textSize = ImGui::CalcTextSize("9.9");
        float boxW = textSize.x + 6.0f;
        float boxH = rowH * 0.9f;

        // Lighter background for SR value
        ImU32 lightBgCol = IM_COL32(
            (int)(r * 255 * 0.6f + 100),
            (int)(g * 255 * 0.6f + 100),
            (int)(b * 255 * 0.6f + 100),
            200
        );
        dl->AddRectFilled(ImVec2(cp.x, cp.y), ImVec2(cp.x + boxW, cp.y + boxH), lightBgCol, 2.0f);

        // SR value text (white, bold)
        snprintf(buffer, 256, "%.1f", driver.safetyRating);
        ImVec2 textPos = ImVec2(cp.x + boxW * 0.5f - textSize.x * 0.5f, cp.y + boxH * 0.5f - textSize.y * 0.5f);
        dl->AddText(textPos, IM_COL32(255, 255, 255, 255), buffer);

        ImGui::Dummy(ImVec2(boxW, boxH));

        // Spacing between SR and license with SAME color background
        ImGui::SameLine(0, 0);
        cp = ImGui::GetCursorScreenPos();
        float spacingW = 2.0f;
        dl->AddRectFilled(ImVec2(cp.x, cp.y), ImVec2(cp.x + spacingW, cp.y + boxH), lightBgCol, 0.0f);
        ImGui::Dummy(ImVec2(spacingW, boxH));

        // License letter (darker color box)
        ImGui::SameLine(0, 0);
        cp = ImGui::GetCursorScreenPos();
        textSize = ImGui::CalcTextSize("A");
        boxW = textSize.x + 6.0f;

        ImU32 darkBgCol = IM_COL32(
            (int)(r * 255 * 0.8f),
            (int)(g * 255 * 0.8f),
            (int)(b * 255 * 0.8f),
            220
        );
        dl->AddRectFilled(ImVec2(cp.x, cp.y), ImVec2(cp.x + boxW, cp.y + boxH), darkBgCol, 2.0f);

        textPos = ImVec2(cp.x + boxW * 0.5f - textSize.x * 0.5f, cp.y + boxH * 0.5f - textSize.y * 0.5f);
        dl->AddText(textPos, IM_COL32(255, 255, 255, 255), letter);

        ImGui::Dummy(ImVec2(boxW, boxH));
    }

    // === Col 5: iRating ===
    ImGui::TableNextColumn();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + rowH * 0.15f);
    {
        // Format iRating
        if (driver.iRating >= 1000) {
            snprintf(buffer, 256, "%.1fk", driver.iRating / 1000.0f);
        } else {
            snprintf(buffer, 256, "%d", driver.iRating);
        }

        // Draw white background box
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 cp = ImGui::GetCursorScreenPos();
        ImVec2 textSize = ImGui::CalcTextSize(buffer);
        float boxW = textSize.x + 8.0f;
        float boxH = rowH * 0.85f;

        dl->AddRectFilled(ImVec2(cp.x, cp.y), ImVec2(cp.x + boxW, cp.y + boxH), IM_COL32(255, 255, 255, 200), 2.0f);

        // iRating text (black, bold)
        ImVec2 textPos = ImVec2(cp.x + boxW * 0.5f - textSize.x * 0.5f, cp.y + boxH * 0.5f - textSize.y * 0.5f);
        dl->AddText(textPos, IM_COL32(0, 0, 0, 255), buffer);

        ImGui::Dummy(ImVec2(boxW, boxH));

        // iRating delta projection
        int delta = driver.iRatingProjection;
        if (delta != 0) {
            ImGui::SameLine(0, 4);
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + rowH * 0.1f);
            char deltaBuf[32];
            if (delta > 0) {
                snprintf(deltaBuf, 32, "+%d", delta);
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "%s", deltaBuf);
            } else {
                snprintf(deltaBuf, 32, "%d", delta);
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", deltaBuf);
            }
        }
    }

    // === Col 6: Last Lap Time ===
    ImGui::TableNextColumn();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + rowH * 0.15f);
    {
        formatTime(driver.lastLapTime, buffer);
        ImGui::Text("%s", buffer);
    }

    // === Col 7: Gap ===
    ImGui::TableNextColumn();
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + rowH * 0.15f);
    {
        formatGap(driver.gapToPlayer, buffer);
        if (driver.isPlayer) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "---");
        } else if (driver.gapToPlayer > 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", buffer);
        } else {
            ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", buffer);
        }
    }

    // === Col 8: Filler ===
    ImGui::TableNextColumn();
}

void RelativeWidget::renderFooter(iracing::RelativeCalculator* relative) {
    if (!relative) return;

    int incidents = relative->getPlayerIncidents();
    float lastLap = relative->getPlayerLastLap();
    float bestLap = relative->getPlayerBestLap();

    char lastBuf[32], bestBuf[32];
    formatTime(lastLap, lastBuf);
    formatTime(bestLap, bestBuf);

    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Inc: %dx", incidents);
    ImGui::SameLine(0, 16);
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.9f, 1.0f), "Last: %s", lastBuf);
    ImGui::SameLine(0, 16);
    ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "Best: %s", bestBuf);
}

void RelativeWidget::formatGap(float gap, char* buffer) {
    if (gap == 0.0f) {
        snprintf(buffer, 32, "---");
    } else if (std::abs(gap) < 60.0f) {
        snprintf(buffer, 32, "%+.1f", gap);
    } else {
        int mins = (int)(std::abs(gap) / 60.0f);
        float secs = std::fmod(std::abs(gap), 60.0f);
        snprintf(buffer, 32, "%s%d:%04.1f", gap < 0 ? "-" : "+", mins, secs);
    }
}

void RelativeWidget::formatTime(float seconds, char* buffer) {
    if (seconds <= 0.0f) {
        snprintf(buffer, 32, "--:--.---");
        return;
    }
    int mins = (int)(seconds / 60.0f);
    float secs = std::fmod(seconds, 60.0f);
    if (mins > 0) {
        snprintf(buffer, 32, "%d:%06.3f", mins, secs);
    } else {
        snprintf(buffer, 32, "%.3f", secs);
    }
}

const char* RelativeWidget::getSafetyRatingLetter(float sr) {
    if (sr >= 4.0f) return "A";
    if (sr >= 3.0f) return "B";
    if (sr >= 2.0f) return "C";
    if (sr >= 1.0f) return "D";
    return "R";
}

void RelativeWidget::getSafetyRatingColor(float sr, float& r, float& g, float& b) {
    if (sr >= 4.0f)      { r = 0.0f; g = 0.4f; b = 1.0f; }   // Blue - A
    else if (sr >= 3.0f) { r = 0.0f; g = 0.8f; b = 0.0f; }   // Green - B
    else if (sr >= 2.0f) { r = 1.0f; g = 0.8f; b = 0.0f; }   // Yellow - C
    else if (sr >= 1.0f) { r = 1.0f; g = 0.4f; b = 0.0f; }   // Orange - D
    else                 { r = 1.0f; g = 0.0f; b = 0.0f; }   // Red - R
}

const char* RelativeWidget::getClubFlag(const std::string& club) {
    (void)club;
    return "";
}

}  // namespace ui