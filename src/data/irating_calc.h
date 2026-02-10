#pragma once
#include <vector>

namespace iracing {

class IRatingCalculator {
public:
    static int calculateSOF(const std::vector<int>& iRatings);
    static int calculateDelta(int myIR, int sof, int position, int totalDrivers);
};

} // namespace iracing
