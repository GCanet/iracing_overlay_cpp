#include "ui/relative_widget.h"
#include "data/relative_calc.h"
#include "data/irsdk_manager.h"
#include "utils/config.h"
#include <imgui.h>
#include <glad/glad.h>
#include <cstdio>
#include <algorithm>
#include <cmath>

// stb_image.h included for declaration only — STB_IMAGE_IMPLEMENTATION lives in telemetry_widget.cpp
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

    auto& config = utils::Config::getRelativeConfig();
    auto drivers = relative->getRelative(4, 4);
    int numDrivers = static_cast<int>(drivers.size());

    // Calculate dimensions
    float rowH = ImGui::GetTextLineHeightWithSpacing();
    float headerH = rowH * 1.5f;
    float tableH = rowH * std::max(numDrivers, 1);
    float separatorH = 4.0f * m_scale;
    float footerH = rowH * 1.5f;
    float padY = 6.0f * m_scale;
    float totalH = headerH + separatorH + tableH + separatorH + footerH + padY * 2.0f;
    float totalW = 520.0f * m_scale;

    ImGui::SetNextWindowSize(ImVec2(totalW, totalH), ImGuiCond_Always);

    if (config.posX < 0 || config.posY < 0) {
        config.posX = 20.0f;
        config.posY = 200.0f;
    }
    ImGui::SetNextWindowPos(ImVec2(config.posX, config.posY), ImGuiCond_Once);

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.12f, config.alpha));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6, padY));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4, 2));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;
    if (editMode) {
        // allow move + resize
    } else {
        flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoResize;
    }

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

    // === FOOTER ===
    renderFooter(relative);

    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void RelativeWidget::renderHeader(iracing::RelativeCalculator* relative) {
    std::string series = relative->getSeriesName();
    if (series.empty() || series == "Unknown Series") {
        series = "Practice Session";
    }
    std::string lapInfo = relative->getLapInfo();
    int sof = relative->getSOF();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));

    // Left side: Series name
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "%s", series.c_str());
    ImGui::SameLine(0, 16);

    // Middle: Lap info
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", lapInfo.c_str());
    ImGui::SameLine(0, 16);

    // Right side: SOF (aligned to the right)
    float availW = ImGui::GetContentRegionAvail().x;
    ImVec2 sofTextSize = ImGui::CalcTextSize("SOF: 8888");
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + availW - sofTextSize.x);
    ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), "SOF: %d", sof);

    ImGui::PopStyleVar();
}

void RelativeWidget::renderFooter(iracing::RelativeCalculator* relative) {
    char buf[128];
    int incidents = relative->getPlayerIncidents();
    float lastLap = relative->getPlayerLastLap();
    float bestLap = relative->getPlayerBestLap();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12, 2));

    // Format incidents
    snprintf(buf, sizeof(buf), "Inc: %d", incidents);
    ImGui::TextColored(ImVec4(0.9f, 0.6f, 0.2f, 1.0f), "%s", buf);
    ImGui::SameLine(0, 12);

    // Format last lap
    formatTime(lastLap, buf);
    ImGui::Text("Last: %s", buf);
    ImGui::SameLine(0, 12);

    // Format best lap
    formatTime(bestLap, buf);
    ImGui::TextColored(ImVec4(0.6f, 0.3f, 0.9f, 1.0f), "Best: %s", buf);

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
        ImGui::SameLine(0, 4);

        // License letter (darker color box)
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
    if (club == "US") return "\xF0\x9F\x87\xBA\xF0\x9F\x87\xB8";
    if (club == "GB") return "\xF0\x9F\x87\xAC\xF0\x9F\x87\xA7";
    if (club == "DE") return "\xF0\x9F\x87\xA9\xF0\x9F\x87\xAA";
    if (club == "FR") return "\xF0\x9F\x87\xAB\xF0\x9F\x87\xB7";
    if (club == "ES") return "\xF0\x9F\x87\xAA\xF0\x9F\x87\xB8";
    if (club == "IT") return "\xF0\x9F\x87\xAE\xF0\x9F\x87\xB9";
    if (club == "NL") return "\xF0\x9F\x87\xB3\xF0\x9F\x87\xB1";
    if (club == "SE") return "\xF0\x9F\x87\xB8\xF0\x9F\x87\xAA";
    if (club == "NO") return "\xF0\x9F\x87\xB3\xF0\x9F\x87\xB4";
    if (club == "DK") return "\xF0\x9F\x87\xA9\xF0\x9F\x87\xB0";
    if (club == "FI") return "\xF0\x9F\x87\xAB\xF0\x9F\x87\xAE";
    if (club == "CA") return "\xF0\x9F\x87\xA8\xF0\x9F\x87\xA6";
    if (club == "AU") return "\xF0\x9F\x87\xA6\xF0\x9F\x87\xBA";
    if (club == "BR") return "\xF0\x9F\x87\xA7\xF0\x9F\x87\xB7";
    if (club == "JP") return "\xF0\x9F\x87\xAF\xF0\x9F\x87\xB5";
    if (club == "KR") return "\xF0\x9F\x87\xB0\xF0\x9F\x87\xB7";
    if (club == "MX") return "\xF0\x9F\x87\xB2\xF0\x9F\x87\xBD";
    if (club == "ZA") return "\xF0\x9F\x87\xBF\xF0\x9F\x87\xA6";
    if (club == "NZ") return "\xF0\x9F\x87\xB3\xF0\x9F\x87\xBF";
    if (club == "SG") return "\xF0\x9F\x87\xB8\xF0\x9F\x87\xAC";
    return "";
}

}  // namespace ui
