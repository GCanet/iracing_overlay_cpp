#include "irating_calc.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace iracing {

int IRatingCalculator::calculateSOF(const std::vector<int>& iRatings) {
    if (iRatings.empty()) return 0;
    
    double sum = 0.0;
    for (int ir : iRatings) {
        sum += ir;
    }
    return static_cast<int>(std::round(sum / iRatings.size()));
}

int IRatingCalculator::calculateDelta(int myIR, int sof, int position, int totalDrivers) {
    if (totalDrivers <= 1) return 0;
    
    // Very simple but realistic projection:
    // - Top 3: small gain/loss
    // - Mid pack: bigger swings
    // - Back: big losses if bad finish
    double expectedFinish = (myIR / static_cast<double>(sof)) * totalDrivers;
    double actualFinish = position;
    
    double performance = (expectedFinish - actualFinish) / totalDrivers;
    
    int delta = static_cast<int>(performance * 120.0);  // scale factor feels natural
    
    // Clamp to reasonable values
    if (delta > 50) delta = 50;
    if (delta < -80) delta = -80;
    
    return delta;
}

} // namespace iracing