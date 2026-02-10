#ifndef IRATING_CALC_H
#define IRATING_CALC_H

#include <cmath>
#include <vector>

namespace iracing {

class iRatingCalculator {
public:
    // Calculate expected iRating change
    static int calculateDelta(
        int currentIR,
        int sof,
        int currentPosition,
        int totalDrivers
    ) {
        if (totalDrivers <= 1) return 0;
        
        // ELO-style expected probability
        double expected = 1.0 / (1.0 + std::pow(10.0, (sof - currentIR) / 1600.0));
        
        // Actual result (normalized 0-1)
        double actualResult = static_cast<double>(totalDrivers - currentPosition) / 
                             static_cast<double>(totalDrivers - 1);
        
        // K-factor (iRacing typically uses ~80-100)
        const double kFactor = 80.0;
        
        return static_cast<int>(kFactor * (actualResult - expected));
    }
    
    // Calculate Strength of Field
    static int calculateSOF(const std::vector<int>& iRatings) {
        if (iRatings.empty()) return 0;
        
        int sum = 0;
        for (int ir : iRatings) {
            sum += ir;
        }
        
        return sum / static_cast<int>(iRatings.size());
    }
    
    // Get projected iRating
    static int getProjected(int currentIR, int delta) {
        return std::max(1, currentIR + delta);
    }
};

} // namespace iracing

#endif // IRATING_CALC_H
