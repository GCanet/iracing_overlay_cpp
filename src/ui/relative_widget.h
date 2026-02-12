#pragma once

#include <string>
#include <unordered_map>

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
        
        // Logo/texture loading
        bool loadCarBrandLogo(const std::string& brand);
        void* getCarBrandTexture(const std::string& brand);

        OverlayWindow* m_overlay = nullptr;
        float m_scale = 1.0f;
        
        // Cache for loaded textures (brand name -> texture pointer)
        std::unordered_map<std::string, void*> m_logoCache;
    };

} // namespace ui
