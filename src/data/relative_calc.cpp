#include "data/relative_calc.h"
#include "data/irsdk_manager.h"
#include "data/irating_calc.h"
#include "utils/yaml_parser.h"
#include <algorithm>
#include <map>
#include <sstream>
#include <cmath>  // para std::abs

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

    // Actualizar info de sesión cuando cambie
    int currentUpdate = m_sdk->getSessionInfoUpdate();
    if (currentUpdate != m_lastSessionInfoUpdate) {
        updateSessionInfo();
        m_lastSessionInfoUpdate = currentUpdate;
    }

    m_allDrivers.clear();
    m_playerCarIdx = m_sdk->getInt("PlayerCarIdx", -1);

    m_lapsComplete     = m_sdk->getInt("Lap", 0);
    m_sessionTime      = m_sdk->getFloat("SessionTime", 0.0f);
    m_sessionTimeRemain = m_sdk->getFloat("SessionTimeRemain", 0.0f);

    // ────────────────────────────────────────────────
    // Variables clave del SDK
    // ────────────────────────────────────────────────
    int lapCount = 0;
    const int* carLap          = m_sdk->getIntArray("CarIdxLap", lapCount);
    const int* carLapCompleted = m_sdk->getIntArray("CarIdxLapCompleted", lapCount);

    int posCount = 0;
    const int* positions       = m_sdk->getIntArray("CarIdxPosition", posCount);

    int distCount = 0;
    const float* lapDistPct    = m_sdk->getFloatArray("CarIdxLapDistPct", distCount);

    int f2Count = 0;
    const float* f2Times       = m_sdk->getFloatArray("CarIdxF2Time", f2Count);

    int pitCount = 0;
    const int* onPitRoad       = m_sdk->getIntArray("CarIdxOnPitRoad", pitCount);

    int surfaceCount = 0;
    const int* trackSurface    = m_sdk->getIntArray("CarIdxTrackSurface", surfaceCount);

    if (!positions || !lapDistPct) {
        return;
    }

    std::vector<int> allIRatings;

    for (int i = 0; i < posCount && i < 64; ++i) {
        // Filtros más estrictos para evitar fantasmas / coches no válidos
        if (!trackSurface || trackSurface[i] <= 0) continue;  // -1 = no en mundo, 0 = off track
        if (positions[i] <= 0) continue;
        if (lapDistPct[i] < 0.0f || lapDistPct[i] > 1.0f) continue;

        Driver driver{};
        driver.carIdx       = i;
        driver.position     = positions[i];
        driver.lapDistPct   = lapDistPct[i];
        driver.isOnPit      = (onPitRoad && onPitRoad[i] != 0);
        driver.isPlayer     = (i == m_playerCarIdx);

        // Laps más precisos (muy importante para orden correcto)
        driver.lap          = (carLap && i < lapCount)          ? carLap[i]          : 0;
        driver.lapCompleted = (carLapCompleted && i < lapCount) ? carLapCompleted[i] : 0;

        // Info del driver desde session string
        auto it = m_driverInfoMap.find(i);
        if (it != m_driverInfoMap.end()) {
            const auto& di = it->second;
            driver.carNumber    = di.carNumber.empty() ? std::to_string(i+1) : di.carNumber;
            driver.driverName   = di.userName.empty()  ? "Unknown" : di.userName;
            driver.iRating      = di.iRating;
            driver.safetyRating = static_cast<float>(di.licenseLevel) / 4.0f;
            driver.carBrand     = getCarBrand(di.carPath);
            driver.carClass     = di.carClassShortName.empty() ? "???" : di.carClassShortName;
        } else {
            driver.carNumber    = std::to_string(i+1);
            driver.driverName   = "Driver " + std::to_string(i);
            driver.iRating      = 1500;
            driver.safetyRating = 2.5f;
            driver.carBrand     = "unknown";
            driver.carClass     = "Unknown";
        }

        allIRatings.push_back(driver.iRating);
        m_allDrivers.push_back(std::move(driver));
    }

    if (!allIRatings.empty()) {
        m_sof = iRatingCalculator::calculateSOF(allIRatings);
    }

    // ────────────────────────────────────────────────
    // Ordenación ROBUSTA (clave para relative fiable)
    // 1. Vueltas completadas (más vueltas → delante)
    // 2. Porcentaje de la vuelta actual (más lejos → delante)
    // 3. Posición oficial como desempate
    // ────────────────────────────────────────────────
    std::sort(m_allDrivers.begin(), m_allDrivers.end(),
        [](const Driver& a, const Driver& b) -> bool {
            if (a.lapCompleted != b.lapCompleted) {
                return a.lapCompleted > b.lapCompleted;
            }
            if (std::abs(a.lapDistPct - b.lapDistPct) > 0.0001f) {
                return a.lapDistPct > b.lapDistPct;
            }
            return a.position < b.position;
        });

    // Asignar posición relativa después de ordenar
    for (size_t i = 0; i < m_allDrivers.size(); ++i) {
        m_allDrivers[i].relativePosition = static_cast<int>(i) + 1;
    }

    // Calcular gaps (con fallback si F2Time no es válido)
    calculateGaps(f2Times, f2Count);
    calculateiRatingProjections();
}

void RelativeCalculator::calculateGaps(const float* f2Times, int f2Count) {
    if (m_allDrivers.empty()) return;

    const Driver* leader = nullptr;
    const Driver* player = nullptr;

    for (const auto& d : m_allDrivers) {
        if (d.relativePosition == 1) leader = &d;
        if (d.isPlayer) player = &d;
    }

    if (!leader) return;

    for (auto& driver : m_allDrivers) {
        float gapToLeader = 0.0f;
        float gapToPlayer = 0.0f;

        bool validF2 = (f2Times &&
                        driver.carIdx < f2Count &&
                        leader->carIdx < f2Count &&
                        f2Times[driver.carIdx] > 0.01f &&
                        f2Times[leader->carIdx] > 0.01f);

        if (validF2) {
            gapToLeader = f2Times[driver.carIdx] - f2Times[leader->carIdx];
        } else {
            // Fallback aproximado en "vueltas" (puedes convertir a segundos si tienes track length)
            int lapDiff = leader->lapCompleted - driver.lapCompleted;
            float distDiff = leader->lapDistPct - driver.lapDistPct;
            gapToLeader = static_cast<float>(lapDiff) + distDiff;
        }

        driver.gapToLeader = gapToLeader;

        if (player) {
            bool validPlayerF2 = (f2Times &&
                                  player->carIdx < f2Count &&
                                  f2Times[player->carIdx] > 0.01f);

            if (validF2 && validPlayerF2) {
                gapToPlayer = f2Times[driver.carIdx] - f2Times[player->carIdx];
            } else if (!validF2) {
                int lapDiffP = driver.lapCompleted - player->lapCompleted;
                float distDiffP = driver.lapDistPct - player->lapDistPct;
                gapToPlayer = static_cast<float>(lapDiffP) + distDiffP;
            }
            driver.gapToPlayer = gapToPlayer;
        }
    }
}

std::vector<Driver> RelativeCalculator::getRelative(int ahead, int behind) const {
    std::vector<Driver> relative;

    if (m_allDrivers.empty()) {
        return relative;
    }

    int playerRelPos = -1;
    for (const auto& d : m_allDrivers) {
        if (d.isPlayer) {
            playerRelPos = d.relativePosition;
            break;
        }
    }

    if (playerRelPos < 1) {
        return relative;
    }

    int startPos = std::max(1, playerRelPos - behind);
    int endPos   = std::min(static_cast<int>(m_allDrivers.size()), playerRelPos + ahead);

    for (const auto& drv : m_allDrivers) {
        if (drv.relativePosition >= startPos && drv.relativePosition <= endPos) {
            relative.push_back(drv);
        }
    }

    return relative;
}

// ────────────────────────────────────────────────
// Resto de funciones sin cambios importantes
// ────────────────────────────────────────────────

void RelativeCalculator::updateSessionInfo() {
    const char* yaml = m_sdk->getSessionInfo();
    if (!yaml) return;

    utils::YAMLParser::SessionInfo info = utils::YAMLParser::parse(yaml);

    m_seriesName = info.seriesName;
    m_totalLaps  = info.sessionLaps;

    m_driverInfoMap.clear();
    for (const auto& driverInfo : info.drivers) {
        if (driverInfo.carIdx >= 0) {
            m_driverInfoMap[driverInfo.carIdx] = driverInfo;
        }
    }
}

void RelativeCalculator::calculateiRatingProjections() {
    if (m_allDrivers.empty()) return;

    int totalDrivers = static_cast<int>(m_allDrivers.size());

    for (auto& driver : m_allDrivers) {
        driver.iRatingProjection = iRatingCalculator::calculateDelta(
            driver.iRating, m_sof, driver.relativePosition, totalDrivers);
    }
}

std::string RelativeCalculator::getSeriesName() const { return m_seriesName; }

std::string RelativeCalculator::getLapInfo() const {
    std::ostringstream oss;

    if (m_totalLaps > 0 && m_totalLaps < 999) {
        oss << m_lapsComplete << "/" << m_totalLaps;
    } else {
        int minutes = static_cast<int>(m_sessionTimeRemain / 60.0f);
        int seconds = static_cast<int>(m_sessionTimeRemain) % 60;
        oss << minutes << ":" << (seconds < 10 ? "0" : "") << seconds << " remain";
    }

    return oss.str();
}

int RelativeCalculator::getSOF() const { return m_sof; }

std::string RelativeCalculator::getCarBrand(const std::string& carPath) {
    // ... sin cambios ...
    static const std::map<std::string, std::string> brands = { /* ... */ };
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
