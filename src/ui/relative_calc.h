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
    int carIdx = -1;
    int position = 0;              // CarIdxPosition (oficial del SDK)
    int relativePosition = 0;      // Posición calculada tras orden robusto (1 = líder)
    int lap = 0;                   // CarIdxLap (vuelta actual en curso)
    int lapCompleted = 0;          // CarIdxLapCompleted (vueltas terminadas)
    float lapDistPct = 0.0f;       // Porcentaje de la vuelta actual
    float lastLapTime = 0.0f;
    float gapToLeader = 0.0f;
    float gapToPlayer = 0.0f;
    bool isOnPit = false;
    bool isPlayer = false;
    std::string carNumber;
    std::string driverName;
    std::string carClass;
    std::string carBrand;
    int iRating = 1500;
    float safetyRating = 2.5f;
    int iRatingProjection = 0;
};

class IRSDKManager;

class RelativeCalculator {
public:
    RelativeCalculator(IRSDKManager* sdk);
    
    void update();
    
    // Get all drivers (ya ordenados por update())
    const std::vector<Driver>& getAllDrivers() const { return m_allDrivers; }
    
    // Relative alrededor del jugador (smart adjustment)
    std::vector<Driver> getRelative(int ahead = 4, int behind = 4) const;
    
    int getPlayerCarIdx() const { return m_playerCarIdx; }
    
    std::string getSeriesName() const;
    std::string getLapInfo() const;
    int getSOF() const;

private:
    void updateSessionInfo();
    void calculateGaps(const float* f2Times, int f2Count);
    void calculateiRatingProjections();
    std::string getCarBrand(const std::string& carPath);
    
    IRSDKManager* m_sdk;
    std::vector<Driver> m_allDrivers;
    int m_playerCarIdx = -1;
    int m_sof = 0;
    std::string m_seriesName;
    int m_lapsComplete = 0;
    int m_totalLaps = 0;
    float m_sessionTime = 0.0f;
    float m_sessionTimeRemain = 0.0f;
    
    // Caché de session string
    int m_lastSessionInfoUpdate = -1;
    std::map<int, utils::YAMLParser::DriverInfo> m_driverInfoMap;
};

} // namespace iracing

#endif // RELATIVE_CALC_H
