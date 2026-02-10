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
    void renderDriverRow(const iracing::Driver& driver, bool isPlayer);
    std::string formatGap(float gap);
    std::string formatTime(float seconds);
    std::string getSafetyRatingColor(float sr);
    std::string getSafetyRatingLetter(float sr);
};

} // namespace ui

#endif // RELATIVE_WIDGET_H
