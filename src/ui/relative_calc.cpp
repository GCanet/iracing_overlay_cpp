#include "data/relative_calc.h"
#include "data/irsdk_manager.h"
#include "data/irating_calc.h"
#include "utils/yaml_parser.h"
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
    , m_lastSessionInfoUpdate(-1)
{
}

void RelativeCalculator::update() {
    if (!m_sdk || !m_sdk->isSessionActive()) {
        m_allDrivers.clear();
        return;
    }
    
    // Check if we need to update session info
    int currentUpdate = m_sdk->getSessionInfoUpdate();
    if (currentUpdate != m_lastSessionInfoUpdate) {
        updateSessionInfo();
        m_lastSessionInfoUpdate = currentUpdate;
    }
    
    m_allDrivers.clear();
    m_allDrivers.reserve(64);
    
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
    
    int classCount = 0;
    const int* carClassPos = m_sdk->getIntArray("CarIdxClassPosition", classCount);
    
    // We need at least lapDistPct to know who is on track
    if (!lapDistPct) return;
    
    // Collect all iRatings for SOF calculation
    std::vector<int> allIRatings;
    allIRatings.reserve(64);
    
    // Process each car
    int maxCars = distCount < 64 ? distCount : 64;
    
    for (int i = 0; i < maxCars; i++) {
        // Skip cars not in the world at all (trackSurface == -1)
        if (trackSurface && i < surfaceCount && trackSurface[i] == -1) continue;
        
        // Use lapDistPct as the primary presence indicator:
        // If a car is on track, its lapDistPct will be >= 0 and < 1.
        // Cars not in the session have lapDistPct of -1 or exactly 0 with no position.
        // This works in practice, quali AND race unlike CarIdxPosition which is 0 
        // until a driver completes a lap.
        if (lapDistPct[i] < 0.0f) continue;
        
        // If we have positions AND the car has no lapDistPct activity, skip
        // (spectators, disconnected slots with stale data)
        if (lapDistPct[i] == 0.0f && positions && i < posCount && positions[i] <= 0) {
            // Double check with trackSurface if available
            if (trackSurface && i < surfaceCount && trackSurface[i] <= 0) continue;
            // If no trackSurface data, still skip if position is 0 and dist is 0
            if (!trackSurface) continue;
        }
        
        Driver driver;
        driver.carIdx = i;
        
        // Use position from SDK; if 0 (no lap completed yet), use a large number
        // so they sort to the back but still appear in the list
        if (positions && i < posCount && positions[i] > 0) {
            driver.position = positions[i];
        } else {
            // No official position yet - assign based on lapDistPct
            // (will be re-sorted below for cars without position)
            driver.position = 999 + i;  // placeholder, sorted at back
        }
        
        driver.lapDistPct = lapDistPct[i];
        driver.lastLapTime = (lastLapTime && i < lapTimeCount) ? lastLapTime[i] : 0.0f;
        driver.isOnPit = (onPitRoad && i < pitCount) ? (onPitRoad[i] != 0) : false;
        driver.isPlayer = (i == m_playerCarIdx);
        driver.gapToLeader = 0.0f;
        driver.gapToPlayer = 0.0f;
        driver.iRatingProjection = 0;
        
        // Get driver info from session data
        auto it = m_driverInfoMap.find(i);
        if (it != m_driverInfoMap.end()) {
            driver.carNumber = it->second.carNumber;
            driver.driverName = it->second.userName;
            driver.iRating = it->second.iRating;
            driver.safetyRating = static_cast<float>(it->second.licenseLevel) / 4.0f;
            driver.carBrand = getCarBrand(it->second.carPath);
            driver.carClass = it->second.carClassShortName.empty() ? "???" : it->second.carClassShortName;
        } else {
            // Fallback if not in session info
            driver.carNumber = std::to_string(i);
            driver.driverName = "Driver " + std::to_string(i);
            driver.iRating = 1500;
            driver.safetyRating = 2.5f;
            driver.carBrand = "unknown";
            driver.carClass = "Unknown";
        }
        
        allIRatings.push_back(driver.iRating);
        m_allDrivers.push_back(driver);
    }
    
    // Calculate SOF
    if (!allIRatings.empty()) {
        m_sof = iRatingCalculator::calculateSOF(allIRatings);
    }
    
    // Sort by position (cars with real positions first, then by lapDistPct for unranked)
    std::sort(m_allDrivers.begin(), m_allDrivers.end(),
        [](const Driver& a, const Driver& b) {
            // Both have real positions
            if (a.position < 999 && b.position < 999) {
                return a.position < b.position;
            }
            // One has real position, other doesn't - real position wins
            if (a.position < 999 && b.position >= 999) return true;
            if (a.position >= 999 && b.position < 999) return false;
            // Neither has real position - sort by lapDistPct (further along = better)
            return a.lapDistPct > b.lapDistPct;
        });
    
    // Re-assign sequential positions for display if some were placeholders
    for (int i = 0; i < static_cast<int>(m_allDrivers.size()); i++) {
        if (m_allDrivers[i].position >= 999) {
            m_allDrivers[i].position = i + 1;
        }
    }
    
    // Calculate gaps and projections
    calculateGaps();
    calculateiRatingProjections();
}

void RelativeCalculator::updateSessionInfo() {
    const char* yaml = m_sdk->getSessionInfo();
    if (!yaml) return;
    
    utils::YAMLParser::SessionInfo info = utils::YAMLParser::parse(yaml);
    
    // Update session data
    m_seriesName = info.seriesName;
    m_totalLaps = info.sessionLaps;
    
    // Update driver info map
    m_driverInfoMap.clear();
    for (const auto& driverInfo : info.drivers) {
        if (driverInfo.carIdx >= 0) {
            m_driverInfoMap[driverInfo.carIdx] = driverInfo;
        }
    }
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
    
    // Find player index in sorted array
    int playerPos = -1;
    for (size_t i = 0; i < m_allDrivers.size(); i++) {
        if (m_allDrivers[i].isPlayer) {
            playerPos = static_cast<int>(i);
            break;
        }
    }
    
    if (playerPos < 0) {
        // Player not found - could be spectating or not yet on track
        // Show the top drivers instead
        int count = std::min(static_cast<int>(m_allDrivers.size()), ahead + behind);
        for (int i = 0; i < count; i++) {
            relative.push_back(m_allDrivers[i]);
        }
        return relative;
    }
    
    // Total drivers to show (ahead + behind, player included in one of them)
    const int totalDrivers = static_cast<int>(m_allDrivers.size());
    
    // Calculate range: 'behind' cars ahead of player, 'ahead' cars behind
    // (in the sorted array, lower index = better position = "ahead" on track)
    int idealStart = playerPos - behind;
    int idealEnd = playerPos + ahead + 1;
    
    // Adjust if we go out of bounds at the top (player near P1)
    if (idealStart < 0) {
        int deficit = -idealStart;
        idealStart = 0;
        idealEnd = std::min(totalDrivers, idealEnd + deficit);
    }
    
    // Adjust if we go out of bounds at the bottom (player near last)
    if (idealEnd > totalDrivers) {
        int excess = idealEnd - totalDrivers;
        idealEnd = totalDrivers;
        idealStart = std::max(0, idealStart - excess);
    }
    
    // Copy the range
    for (int i = idealStart; i < idealEnd; i++) {
        relative.push_back(m_allDrivers[i]);
    }
    
    return relative;
}

std::string RelativeCalculator::getSeriesName() const {
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
        {"chevrolet", "chevrolet"},
        {"toyota", "toyota"},
        {"mazda", "mazda"}
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
