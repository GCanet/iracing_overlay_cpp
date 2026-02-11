#include "utils/yaml_parser.h"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace utils {

std::string YAMLParser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

std::string YAMLParser::extractValue(const std::string& line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) return "";
    
    std::string value = line.substr(colon + 1);
    value = trim(value);
    
    // Remove quotes if present
    if (!value.empty() && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.length() - 2);
    }
    
    return value;
}

int YAMLParser::extractInt(const std::string& line) {
    std::string value = extractValue(line);
    try {
        return std::stoi(value);
    } catch (...) {
        return 0;
    }
}

float YAMLParser::extractFloat(const std::string& line) {
    std::string value = extractValue(line);
    try {
        return std::stof(value);
    } catch (...) {
        return 0.0f;
    }
}

YAMLParser::SessionInfo YAMLParser::parse(const char* yaml) {
    SessionInfo info;
    info.sessionLaps = 0;
    info.sessionTime = 0.0f;
    info.seriesName = "Unknown Series";
    info.trackName = "Unknown Track";
    
    if (!yaml) return info;
    
    std::istringstream stream(yaml);
    std::string line;
    
    bool inWeekendInfo = false;
    bool inDriverInfo = false;
    bool inDrivers = false;
    bool inSessionInfo = false;
    
    DriverInfo currentDriver;
    bool buildingDriver = false;
    
    while (std::getline(stream, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;
        
        // Count indentation
        int indent = 0;
        for (char c : line) {
            if (c == ' ') indent++;
            else break;
        }
        
        // Top level sections
        if (trimmed.find("WeekendInfo:") == 0) {
            inWeekendInfo = true;
            inDriverInfo = false;
            inSessionInfo = false;
            continue;
        }
        else if (trimmed.find("DriverInfo:") == 0) {
            inWeekendInfo = false;
            inDriverInfo = true;
            inSessionInfo = false;
            continue;
        }
        else if (trimmed.find("SessionInfo:") == 0) {
            inWeekendInfo = false;
            inDriverInfo = false;
            inSessionInfo = true;
            continue;
        }
        
        // WeekendInfo parsing
        if (inWeekendInfo) {
            if (trimmed.find("TrackName:") != std::string::npos) {
                info.trackName = extractValue(trimmed);
            }
            else if (trimmed.find("SeriesName:") != std::string::npos) {
                // Only match "SeriesName:" not "SeriesNameShort:" etc.
                std::string key = trimmed.substr(0, trimmed.find(':'));
                key = trim(key);
                if (key == "SeriesName") {
                    info.seriesName = extractValue(trimmed);
                }
            }
        }
        
        // SessionInfo parsing
        if (inSessionInfo) {
            if (trimmed.find("SessionLaps:") != std::string::npos) {
                std::string value = extractValue(trimmed);
                if (value == "unlimited") {
                    info.sessionLaps = 999999;
                } else {
                    info.sessionLaps = extractInt(trimmed);
                }
            }
            else if (trimmed.find("SessionTime:") != std::string::npos) {
                std::string value = extractValue(trimmed);
                if (value == "unlimited") {
                    info.sessionTime = 999999.0f;
                } else {
                    info.sessionTime = extractFloat(trimmed);
                }
            }
        }
        
        // DriverInfo parsing
        if (inDriverInfo) {
            if (trimmed.find("Drivers:") == 0) {
                inDrivers = true;
                continue;
            }
            
            if (inDrivers) {
                // New driver entry (starts with -)
                if (trimmed[0] == '-') {
                    // Save previous driver
                    if (buildingDriver) {
                        info.drivers.push_back(currentDriver);
                    }
                    
                    // Start new driver
                    currentDriver = DriverInfo();
                    currentDriver.carIdx = -1;
                    currentDriver.iRating = 0;
                    currentDriver.licenseLevel = 0;
                    currentDriver.licSubLevel = 0;   // FIX: init new field
                    buildingDriver = true;
                    
                    // Parse first field on same line
                    size_t colon = trimmed.find(':');
                    if (colon != std::string::npos) {
                        std::string key = trimmed.substr(2, colon - 2); // Skip "- "
                        key = trim(key);
                        
                        if (key == "CarIdx") {
                            currentDriver.carIdx = extractInt(trimmed);
                        }
                    }
                }
                // Driver properties
                else if (buildingDriver && indent >= 4) {
                    if (trimmed.find("UserName:") != std::string::npos) {
                        currentDriver.userName = extractValue(trimmed);
                    }
                    else if (trimmed.find("CarNumber:") != std::string::npos) {
                        currentDriver.carNumber = extractValue(trimmed);
                    }
                    else if (trimmed.find("IRating:") != std::string::npos) {
                        currentDriver.iRating = extractInt(trimmed);
                    }
                    else if (trimmed.find("LicLevel:") != std::string::npos) {
                        currentDriver.licenseLevel = extractInt(trimmed);
                    }
                    // FIX: Parse LicSubLevel (safety rating * 100, e.g. 349 = SR 3.49)
                    else if (trimmed.find("LicSubLevel:") != std::string::npos) {
                        currentDriver.licSubLevel = extractInt(trimmed);
                    }
                    // FIX: Parse LicString as fallback (e.g. "A 4.99")
                    else if (trimmed.find("LicString:") != std::string::npos) {
                        currentDriver.licString = extractValue(trimmed);
                    }
                    else if (trimmed.find("CarPath:") != std::string::npos) {
                        currentDriver.carPath = extractValue(trimmed);
                    }
                    // Car class checks
                    else if (trimmed.find("CarClassShortName:") != std::string::npos) {
                        currentDriver.carClassShortName = extractValue(trimmed);
                    }
                    else if (trimmed.find("CarClass:") != std::string::npos) {
                        // fallback only if ShortName wasn't found earlier
                        if (currentDriver.carClassShortName.empty()) {
                            currentDriver.carClassShortName = extractValue(trimmed);
                        }
                    }
                }
            }
        }
    }
    
    // Don't forget last driver
    if (buildingDriver) {
        info.drivers.push_back(currentDriver);
    }
    
    return info;
}

} // namespace utils
