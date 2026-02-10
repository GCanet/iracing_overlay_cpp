#ifndef RELATIVE_CALC_H
#define RELATIVE_CALC_H

#include "utils/yaml_parser.h"
#include <vector>
#include <string>
#include <map>

namespace utils {
    struct YAMLParser;
}

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
    int iRatingProjection;
};

class IRSDKManager;

class RelativeCalculator {
public:
    RelativeCalculator(IRSDKManager* sdk);
    
    // Update driver list
    void update();
    
    // Get all drivers sorted by position
    const std::vector<Driver>& getAllDrivers() const { return m_allDrivers; }
    
    // Get relative (drivers near player) - FIXED: 4 ahead, 4 behind with smart adjustment
    std::vector<Driver> getRelative(int ahead = 4, int behind = 4);
    
    // Get player car index
    int getPlayerCarIdx() const { return m_playerCarIdx; }
    
    // Get race header info
    std::string getSeriesName() const;
    std::string getLapInfo() const;
    int getSOF() const;

private:
    void updateSessionInfo();
    void calculateGaps();
    void calculateiRatingProjections();
    std::string getCarBrand(const std::string& carPath);
    
    IRSDKManager* m_sdk;
    std::vector<Driver> m_allDrivers;
    int m_playerCarIdx;
    int m_sof;
    std::string m_seriesName;
    int m_lapsComplete;
    int m_totalLaps;
    float m_sessionTime;
    float m_sessionTimeRemain;
    
    // Session info caching
    int m_lastSessionInfoUpdate;
    std::map<int, utils::YAMLParser::DriverInfo> m_driverInfoMap;
};

} // namespace iracing

#endif // RELATIVE_CALC_H
