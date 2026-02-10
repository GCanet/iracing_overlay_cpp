#ifndef RELATIVE_CALC_H
#define RELATIVE_CALC_H

#include <vector>
#include <string>

namespace iracing {

struct Driver {
    int carIdx;
    int position;
    std::string carNumber;
    std::string driverName;
    float lapDistPct;
    float lastLapTime;
    float gapToLeader;
    float gapToPlayer;
    bool isOnPit;
    bool isPlayer;
    std::string carClass;
    std::string carBrand;
    int iRating;
    float safetyRating;
};

class IRSDKManager;

class RelativeCalculator {
public:
    RelativeCalculator(IRSDKManager* sdk);
    
    // Update driver list
    void update();
    
    // Get all drivers sorted by position
    const std::vector<Driver>& getAllDrivers() const { return m_allDrivers; }
    
    // Get relative (drivers near player)
    std::vector<Driver> getRelative(int ahead = 5, int behind = 5);
    
    // Get player car index
    int getPlayerCarIdx() const { return m_playerCarIdx; }

private:
    void calculateGaps();
    std::string getCarBrand(const std::string& carPath);
    
    IRSDKManager* m_sdk;
    std::vector<Driver> m_allDrivers;
    int m_playerCarIdx;
};

} // namespace iracing

#endif // RELATIVE_CALC_H
