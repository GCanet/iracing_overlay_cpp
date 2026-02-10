#include "data/relative_calc.h"
#include "data/irsdk_manager.h"
#include "data/irating_calc.h"
#include <algorithm>
#include <map>
#include <sstream>

namespace iracing {

RelativeCalculator::RelativeCalculator(IRSDKManager* sdk)
    : m_sdk(sdk)
    , m_playerCarIdx(-1)
    , m_sof(0)
    , m_seriesName("Unknown Series")
    , m_lapsComplete(0)
    , m_totalLaps(0)
    , m_sessionTime(0.0f)
    , m_sessionTimeRemain(0.0f)
{
}

void RelativeCalculator::update() {
    if (!m_sdk || !m_sdk->isConnected()) {
        m_allDrivers.clear();
        return;
    }
    
    m_allDrivers.clear();
    m_playerCarIdx = m_sdk->getInt("PlayerCarIdx", -1);
    
    // Get session info
    m_lapsComplete = m_sdk->getInt("Lap", 0);
    m_sessionTime = m_sdk->getFloat("SessionTime", 0.0f);
    m_sessionTimeRemain = m_sdk->getFloat("SessionTimeRemain", 0.0f);
    
    // Get arrays
    int posCount = 0;
    const int* positions = m_sdk->getIntArray("CarIdxPosition", posCount);
    
    int distCount = 0;
    const float* lapDistPct = m_sdk->getFloatArray("CarIdxLapDistPct", distCount);
    
    int lapTimeCount = 0;
    const float* lastLapTime = m_sdk->getFloatArray("CarIdxLastLapTime", lapTimeCount);
    
    int pitCount = 0;
    const int* onPitRoad = m_sdk->getIntArray("CarIdxOnPitRoad", pitCount);
    
    int surfaceCount = 0;
    const int* trackSurface = m_sdk->getIntArray("CarIdxTrackSurface", surfaceCount);
    
    if (!positions || !lapDistPct) return;
    
    // Collect all iRatings for SOF calculation
    std::vector<int> allIRatings;
    
    // Process each car
    for (int i = 0; i < posCount && i < 64; i++) {
        // Skip cars not on track
        if (trackSurface && trackSurface[i] == -1) continue;
        if (positions[i] <= 0) continue;
        
        Driver driver;
        driver.carIdx = i;
        driver.position = positions[i];
        driver.lapDistPct = lapDistPct ? lapDistPct[i] : 0.0f;
        driver.lastLapTime = lastLapTime ? lastLapTime[i] : 0.0f;
        driver.isOnPit = onPitRoad ? (onPitRoad[i] != 0) : false;
        driver.isPlayer = (i == m_playerCarIdx);
        driver.gapToLeader = 0.0f;
        driver.gapToPlayer = 0.0f;
        driver.iRatingProjection = 0;
        
        // These would come from session info parsing - for now, placeholder
        driver.carNumber = std::to_string(i);
        driver.driverName = "Driver " + std::to_string(i);
        driver.carClass = "GT3";
        driver.carBrand = "unknown";
        driver.iRating = 1500 + (i * 50); // Placeholder
        driver.safetyRating = 2.5f + (i * 0.1f); // Placeholder
        
        allIRatings.push_back(driver.iRating);
        m_allDrivers.push_back(driver);
    }
    
    // Calculate SOF
    if (!allIRatings.empty()) {
        m_sof = iRatingCalculator::calculateSOF(allIRatings);
    }
    
    // Sort by position
    std::sort(m_allDrivers.begin(), m_allDrivers.end(),
        [](const Driver& a, const Driver& b) {
            return a.position < b.position;
        });
    
    // Calculate gaps and projections
    calculateGaps();
    calculateiRatingProjections();
}

void RelativeCalculator::calculateGaps() {
    if (m_allDrivers.empty()) return;
    
    // Get F2Time for accurate gaps
    int f2Count = 0;
    const float* f2Times = m_sdk->getFloatArray("CarIdxF2Time", f2Count);
    
    if (!f2Times) return;
    
    Driver* leader = nullptr;
    Driver* player = nullptr;
    
    // Find leader and player
    for (auto& driver : m_allDrivers) {
        if (driver.position == 1) leader = &driver;
        if (driver.isPlayer) player = &driver;
    }
    
    if (!leader) return;
    
    // Calculate gaps
    for (auto& driver : m_allDrivers) {
        if (driver.carIdx < f2Count) {
            driver.gapToLeader = f2Times[driver.carIdx] - f2Times[leader->carIdx];
            
            if (player && player->carIdx < f2Count) {
                driver.gapToPlayer = f2Times[driver.carIdx] - f2Times[player->carIdx];
            }
        }
    }
}

void RelativeCalculator::calculateiRatingProjections() {
    if (m_allDrivers.empty()) return;
    
    int totalDrivers = static_cast<int>(m_allDrivers.size());
    
    for (auto& driver : m_allDrivers) {
        driver.iRatingProjection = iRatingCalculator::calculateDelta(
            driver.iRating,
            m_sof,
            driver.position,
            totalDrivers
        );
    }
}

std::vector<Driver> RelativeCalculator::getRelative(int ahead, int behind) {
    std::vector<Driver> relative;
    
    if (m_allDrivers.empty()) return relative;
    
    // Find player position
    int playerPos = -1;
    for (size_t i = 0; i < m_allDrivers.size(); i++) {
        if (m_allDrivers[i].isPlayer) {
            playerPos = static_cast<int>(i);
            break;
        }
    }
    
    if (playerPos < 0) return relative;
    
    // Get range (4 ahead, 4 behind)
    int start = std::max(0, playerPos - behind);
    int end = std::min(static_cast<int>(m_allDrivers.size()), playerPos + ahead + 1);
    
    for (int i = start; i < end; i++) {
        relative.push_back(m_allDrivers[i]);
    }
    
    return relative;
}

std::string RelativeCalculator::getSeriesName() const {
    // TODO: Parse from session info YAML
    return m_seriesName;
}

std::string RelativeCalculator::getLapInfo() const {
    std::ostringstream oss;
    
    // Try to determine if it's lap-based or time-based
    if (m_totalLaps > 0 && m_totalLaps < 999) {
        // Lap-based race
        oss << m_lapsComplete << "/" << m_totalLaps;
    } else {
        // Time-based race
        int minutes = static_cast<int>(m_sessionTimeRemain / 60.0f);
        int seconds = static_cast<int>(m_sessionTimeRemain) % 60;
        oss << minutes << ":" << (seconds < 10 ? "0" : "") << seconds << " remain";
    }
    
    return oss.str();
}

int RelativeCalculator::getSOF() const {
    return m_sof;
}

std::string RelativeCalculator::getCarBrand(const std::string& carPath) {
    static const std::map<std::string, std::string> brands = {
        {"bmw", "bmw"},
        {"mercedes", "mercedes"},
        {"audi", "audi"},
        {"porsche", "porsche"},
        {"ferrari", "ferrari"},
        {"lamborghini", "lamborghini"},
        {"aston", "aston_martin"},
        {"mclaren", "mclaren"},
        {"ford", "ford"},
        {"chevrolet", "chevrolet"}
    };
    
    std::string lower = carPath;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    for (const auto& [key, brand] : brands) {
        if (lower.find(key) != std::string::npos) {
            return brand;
        }
    }
    
    return "unknown";
}

} // namespace iracing
