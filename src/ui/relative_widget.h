#ifndef RELATIVE_WIDGET_H
#define RELATIVE_WIDGET_H

#include <string>

namespace iracing {
    class RelativeCalculator;
    struct Driver;
}

namespace ui {
    class OverlayWindow;  // forward declaration (no necesitamos incluir el .h completo aquí)

    class RelativeWidget {
    public:
        // Constructor ahora recibe puntero opcional al overlay
        RelativeWidget(OverlayWindow* overlay = nullptr);

        void render(iracing::RelativeCalculator* relative, bool editMode = false);

    private:
        void renderHeader(iracing::RelativeCalculator* relative);
        void renderDriverRow(const iracing::Driver& driver, bool isPlayer, char* buffer);
        
        void formatGap(float gap, char* buffer);
        void formatTime(float seconds, char* buffer);
        
        const char* getSafetyRatingLetter(float sr);
        void getSafetyRatingColor(float sr, float& r, float& g, float& b);

        OverlayWindow* m_overlay = nullptr;  // puntero para acceder a editMode y alpha
        float m_scale = 1.0f;                // escala del widget (ajustable desde menú)
    };

} // namespace ui

#endif // RELATIVE_WIDGET_H
