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
    if (!m_sdk || !m_sdk->isConnected()) {
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
    const int* carClassID = m_sdk->getIntArray("CarIdxClassPosition", classCount);
    
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
    
    // Sort by position
    std::sort(m_allDrivers.begin(), m_allDrivers.end(),
        [](const Driver& a, const Driver& b) {
            return a.position < b.position;
        });
    
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
    
    // Encontrar posición del jugador
    int playerPos = -1;
    for (size_t i = 0; i < m_allDrivers.size(); i++) {
        if (m_allDrivers[i].isPlayer) {
            playerPos = static_cast<int>(i);
            break;
        }
    }
    
    if (playerPos < 0) return relative;
    
    // Siempre intentar mostrar (ahead + 1 + behind) coches total
    const int totalDrivers = static_cast<int>(m_allDrivers.size());
    
    // Calcular rango ideal
    int idealStart = playerPos - behind;
    int idealEnd = playerPos + ahead + 1;
    
    // Ajustar si nos salimos por arriba (vas en primeras posiciones)
    if (idealStart < 0) {
        int deficit = -idealStart;
        idealStart = 0;
        idealEnd = std::min(totalDrivers, idealEnd + deficit);
    }
    
    // Ajustar si nos salimos por abajo (vas en últimas posiciones)
    if (idealEnd > totalDrivers) {
        int excess = idealEnd - totalDrivers;
        idealEnd = totalDrivers;
        idealStart = std::max(0, idealStart - excess);
    }
    
    // Copiar el rango
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
