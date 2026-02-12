#include "ui/relative_widget.h"
#include "data/relative_calc.h"
#include "data/irsdk_manager.h"
#include "utils/config.h"
#include <imgui.h>
#include <glad/glad.h>
#include <cstdio>
#include <algorithm>
#include <cmath>

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

    utils::Config& config = utils::Config::getInstance();  // FIXED: Proper Config singleton usage
    bool locked = editMode && config.uiLocked;  // FIXED: Logic corrected

    auto drivers = relative->getRelative(4, 4);
    if (drivers.empty()) return;

    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize;
    if (locked) {  // FIXED: Condition logic
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoResize;
    }

    // FIXED: Added proper style initialization before Begin
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.7f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));

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
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(1);  // FIXED: Pop correct number of colors
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
        if (driver.gapToPlayer > 0) {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "%s", buffer);
        } else if (driver.gapToPlayer < 0) {
            ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", buffer);
        } else {
            ImGui::Text("%s", buffer);
        }
    }
}

void RelativeWidget::renderFooter(iracing::RelativeCalculator* relative) {
    char buf[128];
    int incidents = relative->getPlayerIncidents();
    float lastLap = relative->getPlayerLastLap();
    float bestLap = relative->getPlayerBestLap();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    float totalWidth = ImGui::GetContentRegionAvail().x;

    // Format incidents
    snprintf(buf, sizeof(buf), "Inc: %d", incidents);
    ImVec2 incSize = ImGui::CalcTextSize(buf);

    // Format last lap time
    formatTime(lastLap, buf);
    ImVec2 lastSize = ImGui::CalcTextSize(buf);
    char lastBuf[128];
    snprintf(lastBuf, sizeof(lastBuf), "Last: %s", buf);

    // Format best lap time
    formatTime(bestLap, buf);
    ImVec2 bestSize = ImGui::CalcTextSize(buf);
    char bestBuf[128];
    snprintf(bestBuf, sizeof(bestBuf), "Best: %s", buf);

    // Left side: Inc
    ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "Inc: %d", incidents);
    ImGui::SameLine();

    // Middle: Last lap (centered)
    float middleX = (totalWidth - lastSize.x) * 0.5f;
    float currentX = ImGui::GetCursorPosX();
    float spacerW = std::max(0.0f, middleX - currentX);
    ImGui::Dummy(ImVec2(spacerW, 0.0f));
    ImGui::SameLine(0, 0);
    ImGui::Text("%s", buf);
    ImGui::SameLine();

    // Right side: Best lap (aligned to the right)
    float rightX = totalWidth - bestSize.x;
    currentX = ImGui::GetCursorPosX();
    spacerW = std::max(0.0f, rightX - currentX);
    ImGui::Dummy(ImVec2(spacerW, 0.0f));
    ImGui::SameLine(0, 0);
    ImGui::TextColored(ImVec4(0.6f, 0.3f, 0.9f, 1.0f), "%s", buf);

    ImGui::PopStyleVar();
}

void RelativeWidget::formatGap(float gap, char* buffer) {
    if (std::abs(gap) < 0.01f) {
        snprintf(buffer, 256, "---");
    } else if (gap > 0) {
        if (gap >= 1.0f) {
            snprintf(buffer, 256, "+%.0fL", gap);
        } else {
            snprintf(buffer, 256, "+%.1fs", gap);
        }
    } else {
        float a = std::abs(gap);
        if (a >= 1.0f) {
            snprintf(buffer, 256, "-%.0fL", a);
        } else {
            snprintf(buffer, 256, "-%.1fs", a);
        }
    }
}

void RelativeWidget::formatTime(float seconds, char* buffer) {
    if (seconds < 0.0f) {
        snprintf(buffer, 256, "---");
        return;
    }
    int mins = static_cast<int>(seconds / 60.0f);
    float secs = seconds - (mins * 60.0f);
    if (mins > 0) {
        snprintf(buffer, 256, "%d:%05.2f", mins, secs);
    } else {
        snprintf(buffer, 256, "%.2f", secs);
    }
}

const char* RelativeWidget::getSafetyRatingLetter(float sr) {
    if (sr < 1.0f) return "R";
    if (sr < 2.0f) return "D";
    if (sr < 3.0f) return "C";
    if (sr < 4.0f) return "B";
    return "A";
}

void RelativeWidget::getSafetyRatingColor(float sr, float& r, float& g, float& b) {
    if (sr < 1.0f) {
        r = 1.0f; g = 0.2f; b = 0.2f;  // Red
    } else if (sr < 2.0f) {
        r = 1.0f; g = 0.6f; b = 0.0f;  // Orange
    } else if (sr < 3.0f) {
        r = 1.0f; g = 1.0f; b = 0.0f;  // Yellow
    } else if (sr < 4.0f) {
        r = 0.2f; g = 1.0f; b = 0.2f;  // Green
    } else {
        r = 0.3f; g = 0.6f; b = 1.0f;  // Blue
    }
}

const char* RelativeWidget::getClubFlag(const std::string& club) {
    if (club == "US") return "🇺🇸";
    if (club == "GB") return "🇬🇧";
    if (club == "DE") return "🇩🇪";
    if (club == "FR") return "🇫🇷";
    if (club == "ES") return "🇪🇸";
    if (club == "IT") return "🇮🇹";
    if (club == "NL") return "🇳🇱";
    if (club == "SE") return "🇸🇪";
    if (club == "NO") return "🇳🇴";
    if (club == "DK") return "🇩🇰";
    if (club == "BR") return "🇧🇷";
    if (club == "AU") return "🇦🇺";
    if (club == "JP") return "🇯🇵";
    if (club == "CN") return "🇨🇳";
    return "";
}

}  // namespace ui