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
    void renderDriverRow(const iracing::Driver& driver);
    std::string formatGap(float gap);
    std::string formatTime(float seconds);
    
    int m_rangeAhead;
    int m_rangeBehind;
    bool m_showIRating;
};

} // namespace ui

#endif // RELATIVE_WIDGET_H
