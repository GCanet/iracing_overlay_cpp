#include "data/relative_calc.h"
#include "data/irsdk_manager.h"
#include "data/irating_calc.h"
#include "utils/yaml_parser.h"
#include <algorithm>
#include <map>
#include <sstream>
#include <cmath>
#include <iostream>

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

    // Update session info when it changes
    int currentUpdate = m_sdk->getSessionInfoUpdate();
    if (currentUpdate != m_lastSessionInfoUpdate) {
        updateSessionInfo();
        m_lastSessionInfoUpdate = currentUpdate;
    }

    m_allDrivers.clear();
    m_allDrivers.reserve(64);
    
    m_playerCarIdx = m_sdk->getInt("PlayerCarIdx", -1);

    m_lapsComplete = m_sdk->getInt("Lap", 0);
    m_sessionTime = m_sdk->getFloat("SessionTime", 0.0f);
    m_sessionTimeRemain = m_sdk->getFloat("SessionTimeRemain", 0.0f);

    // ────────────────────────────────────────────────
    // Get arrays from SDK
    // ────────────────────────────────────────────────
    int lapCount = 0;
    const int* carLap = m_sdk->getIntArray("CarIdxLap", lapCount);
    const int* carLapCompleted = m_sdk->getIntArray("CarIdxLapCompleted", lapCount);

    int posCount = 0;
    const int* positions = m_sdk->getIntArray("CarIdxPosition", posCount);

    int distCount = 0;
    const float* lapDistPct = m_sdk->getFloatArray("CarIdxLapDistPct", distCount);

    int f2Count = 0;
    const float* f2Times = m_sdk->getFloatArray("CarIdxF2Time", f2Count);

    int lapTimeCount = 0;
    const float* lastLapTime = m_sdk->getFloatArray("CarIdxLastLapTime", lapTimeCount);

    int pitCount = 0;
    const int* onPitRoad = m_sdk->getIntArray("CarIdxOnPitRoad", pitCount);

    int surfaceCount = 0;
    const int* trackSurface = m_sdk->getIntArray("CarIdxTrackSurface", surfaceCount);

    // Need at least lap distance to work
    if (!lapDistPct) {
        return;
    }

    std::vector<int> allIRatings;
    allIRatings.reserve(64);

    // ────────────────────────────────────────────────
    // Process each car
    // FIX: The old filter `positions[i] <= 0` was too strict.
    //      In practice/qualifying, position is 0 for drivers who
    //      haven't completed a lap yet, which filtered out EVERYONE
    //      including the player. Now we use CarIdxTrackSurface as the
    //      primary activity check (surface >= 0 = on track/pit).
    //      CarIdxLapDistPct validity is the secondary check.
    // ────────────────────────────────────────────────
    int maxIdx = posCount;
    if (distCount > 0 && distCount < maxIdx) maxIdx = distCount;
    if (maxIdx > 64) maxIdx = 64;

    for (int i = 0; i < maxIdx; ++i) {
        // Primary filter: track surface. -1 = not in world/spectating
        // Values >= 0 mean the car exists on track (even in pits)
        if (trackSurface && i < surfaceCount && trackSurface[i] < 0) continue;

        // Secondary filter: valid lap distance percentage
        if (i < distCount) {
            float dist = lapDistPct[i];
            // -1 means car not on track at all
            if (dist < -0.5f) continue;
        }

        // Also skip if we have positions and the position is negative
        // (but DO allow position == 0, that just means no official position yet)
        if (positions && i < posCount && positions[i] < 0) continue;

        Driver driver{};
        driver.carIdx = i;
        driver.position = (positions && i < posCount) ? positions[i] : 0;
        driver.lapDistPct = (i < distCount) ? lapDistPct[i] : 0.0f;
        
        // Clamp lapDistPct to valid range (sometimes iRacing gives slightly out-of-range values)
        if (driver.lapDistPct < 0.0f) driver.lapDistPct = 0.0f;
        if (driver.lapDistPct > 1.0f) driver.lapDistPct = 1.0f;

        driver.isOnPit = (onPitRoad && i < pitCount && onPitRoad[i] != 0);
        driver.isPlayer = (i == m_playerCarIdx);

        // Laps (critical for correct ordering)
        driver.lap = (carLap && i < lapCount) ? carLap[i] : 0;
        driver.lapCompleted = (carLapCompleted && i < lapCount) ? carLapCompleted[i] : 0;
        
        // Last lap time
        driver.lastLapTime = (lastLapTime && i < lapTimeCount) ? lastLapTime[i] : 0.0f;
        // Filter invalid lap times (iRacing uses large negative or zero for no-time)
        if (driver.lastLapTime <= 0.0f) driver.lastLapTime = -1.0f;

        // Driver info from session string
        auto it = m_driverInfoMap.find(i);
        if (it != m_driverInfoMap.end()) {
            const auto& di = it->second;
            driver.carNumber = di.carNumber.empty() ? std::to_string(i + 1) : di.carNumber;
            driver.driverName = di.userName.empty() ? "Unknown" : di.userName;
            driver.iRating = di.iRating;

            // ────────────────────────────────────────────────
            // FIX: Safety rating calculation
            // Old code: `di.licenseLevel / 4.0f` → WRONG
            // LicSubLevel is SR * 100 (e.g. 349 = B 3.49)
            // LicLevel maps to license class but not directly to SR value
            // ────────────────────────────────────────────────
            if (di.licSubLevel > 0) {
                // Best source: LicSubLevel / 100 gives exact SR
                driver.safetyRating = static_cast<float>(di.licSubLevel) / 100.0f;
            } else if (!di.licString.empty()) {
                // Fallback: parse LicString like "A 4.99" or "B3.21"
                driver.safetyRating = parseSafetyRatingFromLicString(di.licString);
            } else {
                // Last resort: rough estimate from license level
                // LicLevel: 1-4=R, 5-8=D, 9-12=C, 13-16=B, 17-20=A
                if (di.licenseLevel >= 1 && di.licenseLevel <= 20) {
                    int classBase = ((di.licenseLevel - 1) / 4);  // 0=R, 1=D, 2=C, 3=B, 4=A
                    int sublevel = ((di.licenseLevel - 1) % 4);   // 0-3 within class
                    driver.safetyRating = static_cast<float>(classBase) + sublevel * 0.25f;
                } else {
                    driver.safetyRating = 2.5f;
                }
            }

            driver.carBrand = getCarBrand(di.carPath);
            driver.carClass = di.carClassShortName.empty() ? "???" : di.carClassShortName;
        } else {
            driver.carNumber = std::to_string(i + 1);
            driver.driverName = "Driver " + std::to_string(i);
            driver.iRating = 1500;
            driver.safetyRating = 2.5f;
            driver.carBrand = "unknown";
            driver.carClass = "Unknown";
        }

        allIRatings.push_back(driver.iRating);
        m_allDrivers.push_back(std::move(driver));
    }

    if (!allIRatings.empty()) {
        m_sof = iRatingCalculator::calculateSOF(allIRatings);
    }

    // ────────────────────────────────────────────────
    // Robust sorting (key for reliable relative)
    // 1. Laps completed (more laps → ahead)
    // 2. Lap distance percentage (further → ahead)
    // 3. SDK position as tiebreaker
    // ────────────────────────────────────────────────
    std::stable_sort(m_allDrivers.begin(), m_allDrivers.end(),
        [](const Driver& a, const Driver& b) -> bool {
            // First by laps completed (descending)
            if (a.lapCompleted != b.lapCompleted) {
                return a.lapCompleted > b.lapCompleted;
            }
            // If equal, by lap distance percentage (descending)
            if (std::abs(a.lapDistPct - b.lapDistPct) > 0.001f) {
                return a.lapDistPct > b.lapDistPct;
            }
            // Tiebreak: official SDK position (ascending, 0 goes last)
            int posA = (a.position > 0) ? a.position : 9999;
            int posB = (b.position > 0) ? b.position : 9999;
            return posA < posB;
        }
    );

    // Assign calculated relative position (1 = leader)
    for (size_t i = 0; i < m_allDrivers.size(); ++i) {
        m_allDrivers[i].relativePosition = static_cast<int>(i + 1);
    }

    // Calculate gaps
    calculateGaps(f2Times, f2Count);

    // Calculate iRating projections
    calculateiRatingProjections();
}

void RelativeCalculator::updateSessionInfo() {
    const char* yaml = m_sdk->getSessionInfo();
    if (!yaml) return;

    utils::YAMLParser::SessionInfo info = utils::YAMLParser::parse(yaml);

    m_seriesName = info.seriesName;
    m_totalLaps = info.sessionLaps;

    m_driverInfoMap.clear();
    for (const auto& driverInfo : info.drivers) {
        if (driverInfo.carIdx >= 0) {
            m_driverInfoMap[driverInfo.carIdx] = driverInfo;
        }
    }
}

void RelativeCalculator::calculateGaps(const float* f2Times, int f2Count) {
    if (m_allDrivers.empty()) return;

    Driver* leader = nullptr;
    Driver* player = nullptr;

    // Find leader and player
    for (auto& driver : m_allDrivers) {
        if (driver.relativePosition == 1) leader = &driver;
        if (driver.isPlayer) player = &driver;
    }

    if (!leader) return;

    // Calculate gaps for each driver
    for (auto& driver : m_allDrivers) {
        float gapToLeader = 0.0f;
        float gapToPlayer = 0.0f;

        // Gap to leader using F2Time (more precise)
        bool validF2 = (f2Times && 
                       driver.carIdx < f2Count && 
                       leader->carIdx < f2Count &&
                       f2Times[driver.carIdx] > 0.01f &&
                       f2Times[leader->carIdx] > 0.01f);

        if (validF2) {
            gapToLeader = f2Times[driver.carIdx] - f2Times[leader->carIdx];
        } else {
            // Fallback: approximation by laps
            int lapDiff = leader->lapCompleted - driver.lapCompleted;
            float distDiff = leader->lapDistPct - driver.lapDistPct;
            gapToLeader = static_cast<float>(lapDiff) + distDiff;
        }

        driver.gapToLeader = gapToLeader;

        // Gap to player
        if (player) {
            bool validPlayerF2 = (f2Times &&
                                 player->carIdx < f2Count &&
                                 f2Times[player->carIdx] > 0.01f);

            if (validF2 && validPlayerF2) {
                gapToPlayer = f2Times[driver.carIdx] - f2Times[player->carIdx];
            } else {
                int lapDiffP = driver.lapCompleted - player->lapCompleted;
                float distDiffP = driver.lapDistPct - player->lapDistPct;
                gapToPlayer = static_cast<float>(lapDiffP) + distDiffP;
            }
            driver.gapToPlayer = gapToPlayer;
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
            driver.relativePosition,
            totalDrivers
        );
    }
}

std::vector<Driver> RelativeCalculator::getRelative(int ahead, int behind) const {
    std::vector<Driver> relative;

    if (m_allDrivers.empty()) {
        return relative;
    }

    // Find player's relative position
    int playerRelPos = -1;
    for (const auto& d : m_allDrivers) {
        if (d.isPlayer) {
            playerRelPos = d.relativePosition;
            break;
        }
    }

    if (playerRelPos < 1) {
        // Player not found - show top drivers
        int count = std::min(static_cast<int>(m_allDrivers.size()), ahead + behind + 1);
        for (int i = 0; i < count; i++) {
            relative.push_back(m_allDrivers[i]);
        }
        return relative;
    }

    // Calculate range of drivers to show
    int totalDrivers = static_cast<int>(m_allDrivers.size());

    // Smart adjustment like irdashies:
    // If player is near the front (P1-P3), show more behind
    // If player is near the back, show more ahead
    int showAhead = behind;  // positions ahead = lower numbers
    int showBehind = ahead;  // positions behind = higher numbers

    int startPos = std::max(1, playerRelPos - showAhead);
    int endPos = std::min(totalDrivers, playerRelPos + showBehind);

    // If we hit the top, extend the bottom
    if (startPos == 1 && endPos < totalDrivers) {
        endPos = std::min(totalDrivers, 1 + showAhead + showBehind);
    }
    // If we hit the bottom, extend the top
    if (endPos == totalDrivers && startPos > 1) {
        startPos = std::max(1, totalDrivers - showAhead - showBehind);
    }

    // Copy drivers in range
    for (const auto& drv : m_allDrivers) {
        if (drv.relativePosition >= startPos && drv.relativePosition <= endPos) {
            relative.push_back(drv);
        }
    }

    return relative;
}

std::string RelativeCalculator::getSeriesName() const {
    return m_seriesName;
}

std::string RelativeCalculator::getLapInfo() const {
    std::ostringstream oss;

    if (m_totalLaps > 0 && m_totalLaps < 999) {
        // Lap race
        oss << m_lapsComplete << "/" << m_totalLaps;
    } else {
        // Timed race
        int minutes = static_cast<int>(m_sessionTimeRemain / 60.0f);
        int seconds = static_cast<int>(m_sessionTimeRemain) % 60;
        oss << minutes << ":" << (seconds < 10 ? "0" : "") << seconds << " remain";
    }

    return oss.str();
}

int RelativeCalculator::getSOF() const {
    return m_sof;
}

float RelativeCalculator::parseSafetyRatingFromLicString(const std::string& licString) {
    // Parse strings like "A 4.99", "B3.21", "D 1.50", "R 0.85"
    // The letter maps to: R=0, D=1, C=2, B=3, A=4
    // The number is the decimal part within that class
    
    if (licString.empty()) return 2.5f;
    
    float classBase = 2.0f;  // default C
    char letter = licString[0];
    switch (letter) {
        case 'R': classBase = 0.0f; break;
        case 'D': classBase = 1.0f; break;
        case 'C': classBase = 2.0f; break;
        case 'B': classBase = 3.0f; break;
        case 'A': classBase = 4.0f; break;
    }
    
    // Find the numeric part
    size_t numStart = licString.find_first_of("0123456789.");
    if (numStart != std::string::npos) {
        try {
            float numPart = std::stof(licString.substr(numStart));
            // The numeric part is already the sub-level (0.00-4.99)
            // But we want classBase + fractional part
            return classBase + (numPart - static_cast<float>(static_cast<int>(numPart)));
        } catch (...) {
            return classBase + 0.5f;
        }
    }
    
    return classBase + 0.5f;
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
