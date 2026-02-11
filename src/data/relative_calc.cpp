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
    , m_playerIncidents(0)
    , m_playerLastLap(-1.0f)
    , m_playerBestLap(-1.0f)
    , m_lastSessionInfoUpdate(-1)
{
}

void RelativeCalculator::update() {
    if (!m_sdk || !m_sdk->isSessionActive()) {
        m_allDrivers.clear();
        return;
    }

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

    // Player stats
    m_playerIncidents = m_sdk->getInt("PlayerCarMyIncidentCount", 0);
    float curLast = m_sdk->getFloat("LapLastLapTime", -1.0f);
    if (curLast > 0.0f) m_playerLastLap = curLast;
    float curBest = m_sdk->getFloat("LapBestLapTime", -1.0f);
    if (curBest > 0.0f) m_playerBestLap = curBest;

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

    if (!lapDistPct) return;

    std::vector<int> allIRatings;
    allIRatings.reserve(64);

    int maxIdx = posCount;
    if (distCount > 0 && distCount < maxIdx) maxIdx = distCount;
    if (maxIdx > 64) maxIdx = 64;

    for (int i = 0; i < maxIdx; ++i) {
        if (trackSurface && i < surfaceCount && trackSurface[i] < 0) continue;
        if (i < distCount && lapDistPct[i] < -0.5f) continue;
        if (positions && i < posCount && positions[i] < 0) continue;

        Driver driver;
        driver.carIdx = i;
        driver.position = (positions && i < posCount) ? positions[i] : 0;
        driver.lapDistPct = (i < distCount) ? lapDistPct[i] : 0.0f;
        if (driver.lapDistPct < 0.0f) driver.lapDistPct = 0.0f;
        if (driver.lapDistPct > 1.0f) driver.lapDistPct = 1.0f;
        driver.isOnPit = (onPitRoad && i < pitCount && onPitRoad[i] != 0);
        driver.isPlayer = (i == m_playerCarIdx);
        driver.lap = (carLap && i < lapCount) ? carLap[i] : 0;
        driver.lapCompleted = (carLapCompleted && i < lapCount) ? carLapCompleted[i] : 0;
        driver.lastLapTime = (lastLapTime && i < lapTimeCount) ? lastLapTime[i] : 0.0f;
        if (driver.lastLapTime <= 0.0f) driver.lastLapTime = -1.0f;

        auto it = m_driverInfoMap.find(i);
        if (it != m_driverInfoMap.end()) {
            const auto& di = it->second;
            driver.carNumber = di.carNumber.empty() ? std::to_string(i + 1) : di.carNumber;
            driver.driverName = di.userName.empty() ? "Unknown" : di.userName;
            driver.iRating = di.iRating;
            driver.countryCode = di.countryCode;

            if (di.licSubLevel > 0) {
                driver.safetyRating = static_cast<float>(di.licSubLevel) / 100.0f;
            } else if (!di.licString.empty()) {
                driver.safetyRating = parseSafetyRatingFromLicString(di.licString);
            } else {
                if (di.licenseLevel >= 1 && di.licenseLevel <= 20) {
                    int cb = ((di.licenseLevel - 1) / 4);
                    int sl = ((di.licenseLevel - 1) % 4);
                    driver.safetyRating = (float)cb + sl * 0.25f;
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

    if (!allIRatings.empty()) m_sof = iRatingCalculator::calculateSOF(allIRatings);

    std::stable_sort(m_allDrivers.begin(), m_allDrivers.end(),
        [](const Driver& a, const Driver& b) {
            if (a.lapCompleted != b.lapCompleted) return a.lapCompleted > b.lapCompleted;
            if (std::abs(a.lapDistPct - b.lapDistPct) > 0.001f) return a.lapDistPct > b.lapDistPct;
            int pa = (a.position > 0) ? a.position : 9999;
            int pb = (b.position > 0) ? b.position : 9999;
            return pa < pb;
        });

    for (size_t i = 0; i < m_allDrivers.size(); ++i)
        m_allDrivers[i].relativePosition = (int)(i + 1);

    calculateGaps(f2Times, f2Count);
    calculateiRatingProjections();
}

void RelativeCalculator::updateSessionInfo() {
    const char* yaml = m_sdk->getSessionInfo();
    if (!yaml) return;
    auto info = utils::YAMLParser::parse(yaml);
    m_seriesName = info.seriesName;
    m_totalLaps = info.sessionLaps;
    m_driverInfoMap.clear();
    for (const auto& di : info.drivers) {
        if (di.carIdx >= 0) m_driverInfoMap[di.carIdx] = di;
    }
}

void RelativeCalculator::calculateGaps(const float* f2Times, int f2Count) {
    if (m_allDrivers.empty()) return;
    Driver* leader = nullptr;
    Driver* player = nullptr;
    for (auto& d : m_allDrivers) {
        if (d.relativePosition == 1) leader = &d;
        if (d.isPlayer) player = &d;
    }
    if (!leader) return;

    for (auto& d : m_allDrivers) {
        float gl = 0.0f, gp = 0.0f;
        bool vf = (f2Times && d.carIdx < f2Count && leader->carIdx < f2Count &&
                   f2Times[d.carIdx] > 0.01f && f2Times[leader->carIdx] > 0.01f);
        if (vf) {
            gl = f2Times[d.carIdx] - f2Times[leader->carIdx];
        } else {
            int ld = leader->lapCompleted - d.lapCompleted;
            float dd = leader->lapDistPct - d.lapDistPct;
            gl = (float)ld + dd;
        }
        d.gapToLeader = gl;

        if (player) {
            bool vpf = (f2Times && player->carIdx < f2Count && f2Times[player->carIdx] > 0.01f);
            if (vf && vpf) {
                gp = f2Times[d.carIdx] - f2Times[player->carIdx];
            } else {
                int ld = d.lapCompleted - player->lapCompleted;
                float dd = d.lapDistPct - player->lapDistPct;
                gp = (float)ld + dd;
            }
            d.gapToPlayer = gp;
        }
    }
}

void RelativeCalculator::calculateiRatingProjections() {
    if (m_allDrivers.empty()) return;
    int total = (int)m_allDrivers.size();
    for (auto& d : m_allDrivers) {
        d.iRatingProjection = iRatingCalculator::calculateDelta(d.iRating, m_sof, d.relativePosition, total);
    }
}

std::vector<Driver> RelativeCalculator::getRelative(int ahead, int behind) const {
    std::vector<Driver> rel;
    if (m_allDrivers.empty()) return rel;
    int playerPos = -1;
    for (auto& d : m_allDrivers) { if (d.isPlayer) { playerPos = d.relativePosition; break; } }
    if (playerPos < 1) {
        int c = std::min((int)m_allDrivers.size(), ahead + behind + 1);
        for (int i = 0; i < c; i++) rel.push_back(m_allDrivers[i]);
        return rel;
    }
    int total = (int)m_allDrivers.size();
    int sa = behind, sb = ahead;
    int sp = std::max(1, playerPos - sa);
    int ep = std::min(total, playerPos + sb);
    if (sp == 1 && ep < total) ep = std::min(total, 1 + sa + sb);
    if (ep == total && sp > 1) sp = std::max(1, total - sa - sb);
    for (auto& d : m_allDrivers) {
        if (d.relativePosition >= sp && d.relativePosition <= ep) rel.push_back(d);
    }
    return rel;
}

std::string RelativeCalculator::getSeriesName() const { return m_seriesName; }

std::string RelativeCalculator::getLapInfo() const {
    std::ostringstream oss;
    if (m_totalLaps > 0 && m_totalLaps < 999) {
        oss << m_lapsComplete << "/" << m_totalLaps;
    } else {
        int min = (int)(m_sessionTimeRemain / 60.0f);
        int sec = (int)m_sessionTimeRemain % 60;
        oss << min << ":" << (sec < 10 ? "0" : "") << sec << " remain";
    }
    return oss.str();
}

int RelativeCalculator::getSOF() const { return m_sof; }

float RelativeCalculator::parseSafetyRatingFromLicString(const std::string& ls) {
    if (ls.empty()) return 2.5f;
    float base = 2.0f;
    switch (ls[0]) {
        case 'R': base = 0.0f; break; case 'D': base = 1.0f; break;
        case 'C': base = 2.0f; break; case 'B': base = 3.0f; break;
        case 'A': base = 4.0f; break;
    }
    size_t ns = ls.find_first_of("0123456789.");
    if (ns != std::string::npos) {
        try {
            float n = std::stof(ls.substr(ns));
            return base + (n - (float)(int)n);
        } catch (...) {}
    }
    return base + 0.5f;
}

std::string RelativeCalculator::getCarBrand(const std::string& carPath) {
    static const std::map<std::string, std::string> brands = {
        {"bmw","bmw"},{"mercedes","mercedes"},{"audi","audi"},{"porsche","porsche"},
        {"ferrari","ferrari"},{"lamborghini","lamborghini"},{"aston","aston_martin"},
        {"mclaren","mclaren"},{"ford","ford"},{"chevrolet","chevrolet"},
        {"toyota","toyota"},{"mazda","mazda"}
    };
    std::string lower = carPath;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (auto& [k, v] : brands) { if (lower.find(k) != std::string::npos) return v; }
    return "unknown";
}

} // namespace iracing
