#include "data/relative_calc.h"
#include "data/irsdk_manager.h"
#include "data/irating_calc.h"
#include "utils/yaml_parser.h"
#include <algorithm>
#include <map>
#include <sstream>
#include <cmath>

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

    // Actualizar info de sesión cuando cambie
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
    // Obtener arrays del SDK
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

    if (!positions || !lapDistPct) {
        return;
    }

    std::vector<int> allIRatings;
    allIRatings.reserve(64);

    // ────────────────────────────────────────────────
    // Procesar cada coche
    // ────────────────────────────────────────────────
    for (int i = 0; i < posCount && i < 64; ++i) {
        // Filtros robustos para evitar fantasmas
        if (trackSurface && i < surfaceCount && trackSurface[i] <= -1) continue;
        if (positions[i] <= 0) continue;
        if (lapDistPct[i] < 0.0f || lapDistPct[i] > 1.0f) continue;

        Driver driver{};
        driver.carIdx = i;
        driver.position = positions[i];
        driver.lapDistPct = lapDistPct[i];
        driver.isOnPit = (onPitRoad && i < pitCount && onPitRoad[i] != 0);
        driver.isPlayer = (i == m_playerCarIdx);

        // Laps precisos (crítico para orden correcto)
        driver.lap = (carLap && i < lapCount) ? carLap[i] : 0;
        driver.lapCompleted = (carLapCompleted && i < lapCount) ? carLapCompleted[i] : 0;
        
        // Last lap time
        driver.lastLapTime = (lastLapTime && i < lapTimeCount) ? lastLapTime[i] : 0.0f;

        // Info del driver desde session string
        auto it = m_driverInfoMap.find(i);
        if (it != m_driverInfoMap.end()) {
            const auto& di = it->second;
            driver.carNumber = di.carNumber.empty() ? std::to_string(i + 1) : di.carNumber;
            driver.driverName = di.userName.empty() ? "Unknown" : di.userName;
            driver.iRating = di.iRating;
            driver.safetyRating = static_cast<float>(di.licenseLevel) / 4.0f;
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
    // Ordenación ROBUSTA (clave para relative fiable)
    // 1. Vueltas completadas (más vueltas → delante)
    // 2. Porcentaje de la vuelta actual (más lejos → delante)
    // 3. Posición del SDK como desempate final
    // ────────────────────────────────────────────────
    std::stable_sort(m_allDrivers.begin(), m_allDrivers.end(),
        [](const Driver& a, const Driver& b) -> bool {
            // Primero por vueltas completadas (descendente)
            if (a.lapCompleted != b.lapCompleted) {
                return a.lapCompleted > b.lapCompleted;
            }
            // Si igual, por porcentaje de vuelta actual (descendente)
            if (std::abs(a.lapDistPct - b.lapDistPct) > 0.001f) {
                return a.lapDistPct > b.lapDistPct;
            }
            // Desempate final: posición oficial del SDK (ascendente)
            return a.position < b.position;
        }
    );

    // Asignar posición relativa calculada (1 = líder)
    for (size_t i = 0; i < m_allDrivers.size(); ++i) {
        m_allDrivers[i].relativePosition = static_cast<int>(i + 1);
    }

    // Calcular gaps
    calculateGaps(f2Times, f2Count);

    // Calcular proyecciones de iRating
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

    // Encontrar líder y jugador
    for (auto& driver : m_allDrivers) {
        if (driver.relativePosition == 1) leader = &driver;
        if (driver.isPlayer) player = &driver;
    }

    if (!leader) return;

    // Calcular gaps para cada driver
    for (auto& driver : m_allDrivers) {
        float gapToLeader = 0.0f;
        float gapToPlayer = 0.0f;

        // Gap to leader usando F2Time (más preciso)
        bool validF2 = (f2Times && 
                       driver.carIdx < f2Count && 
                       leader->carIdx < f2Count &&
                       f2Times[driver.carIdx] > 0.01f &&
                       f2Times[leader->carIdx] > 0.01f);

        if (validF2) {
            gapToLeader = f2Times[driver.carIdx] - f2Times[leader->carIdx];
        } else {
            // Fallback: aproximación por vueltas
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

    // Buscar posición relativa del jugador
    int playerRelPos = -1;
    for (const auto& d : m_allDrivers) {
        if (d.isPlayer) {
            playerRelPos = d.relativePosition;
            break;
        }
    }

    if (playerRelPos < 1) {
        // Jugador no encontrado - mostrar top drivers
        int count = std::min(static_cast<int>(m_allDrivers.size()), ahead + behind);
        for (int i = 0; i < count; i++) {
            relative.push_back(m_allDrivers[i]);
        }
        return relative;
    }

    // Calcular rango de drivers a mostrar
    int startPos = std::max(1, playerRelPos - behind);
    int endPos = std::min(static_cast<int>(m_allDrivers.size()), playerRelPos + ahead);

    // Copiar drivers en el rango
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
        // Carrera por vueltas
        oss << m_lapsComplete << "/" << m_totalLaps;
    } else {
        // Carrera por tiempo
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
