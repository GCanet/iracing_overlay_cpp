#ifndef RELATIVE_WIDGET_H
#define RELATIVE_WIDGET_H

#include <string>

namespace iracing {
    class RelativeCalculator;
    struct Driver;
}

namespace ui {

class RelativeWidget {
public:
    RelativeWidget();
    
    void render(iracing::RelativeCalculator* relative);

private:
    void renderHeader(iracing::RelativeCalculator* relative);
    void renderDriverRow(const iracing::Driver& driver, bool isPlayer, char* buffer);
    
    // OPTIMIZACIÓN: Evitar std::string temporales, usar buffers
    void formatGap(float gap, char* buffer);
    void formatTime(float seconds, char* buffer);
    
    const char* getSafetyRatingLetter(float sr);
    void getSafetyRatingColor(float sr, float& r, float& g, float& b);
};

} // namespace ui

#endif // RELATIVE_WIDGET_H
