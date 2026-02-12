#pragma once

#include <string>
#include <map>

namespace iracing {
    class RelativeCalculator;
    struct Driver;
}

namespace ui {
    class OverlayWindow;

    class RelativeWidget {
    public:
        RelativeWidget(OverlayWindow* overlay = nullptr);
        ~RelativeWidget();

        void render(iracing::RelativeCalculator* relative, bool editMode = false);

    private:
        void renderHeader(iracing::RelativeCalculator* relative);
        void renderDriverRow(const iracing::Driver& driver, bool isPlayer, char* buffer);
        void renderFooter(iracing::RelativeCalculator* relative);

        void formatGap(float gap, char* buffer);
        void formatTime(float seconds, char* buffer);

        const char* getSafetyRatingLetter(float sr);
        void getSafetyRatingColor(float sr, float& r, float& g, float& b);
        const char* getClubFlag(const std::string& club);

        // Car brand logo textures
        void loadCarBrandTextures();
        unsigned int loadTextureFromFile(const char* filepath);
        unsigned int getCarBrandTexture(const std::string& brand) const;

        OverlayWindow* m_overlay = nullptr;
        float m_scale = 1.0f;

        // brand name -> OpenGL texture ID
        std::map<std::string, unsigned int> m_carBrandTextures;
    };

} // namespace ui
