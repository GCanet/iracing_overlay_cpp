#pragma once
#include <vector>

namespace iracing {

class iRatingCalculator {
public:
    static int calculateSOF(const std::vector<int>& iRatings);
    static int calculateDelta(int myIR, int sof, int position, int totalDrivers);
};

} // namespace iracing
