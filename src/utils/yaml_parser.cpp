#include "utils/yaml_parser.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>

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
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
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
    info.seriesName = "";
    info.trackName = "Unknown Track";
    
    if (!yaml) return info;
    
    std::istringstream stream(yaml);
    std::string line;
    
    // Section tracking — only ONE can be active at a time
    enum Section { NONE, WEEKEND_INFO, DRIVER_INFO, SESSION_INFO };
    Section section = NONE;
    bool inDriversList = false;  // inside the "Drivers:" sub-list
    
    DriverInfo currentDriver;
    bool buildingDriver = false;
    
    while (std::getline(stream, line)) {
        // Count indentation (raw line, before trim)
        int indent = 0;
        for (char c : line) {
            if (c == ' ') indent++;
            else if (c == '\t') indent += 2;
            else break;
        }
        
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;
        
        // ── Top-level section detection (indent == 0, ends with ':') ──
        // Any line at indent 0 that ends with ':' and has no value → new section
        if (indent == 0 && !trimmed.empty()) {
            // Check if this is a top-level section header
            size_t colonPos = trimmed.find(':');
            if (colonPos != std::string::npos) {
                std::string afterColon = trim(trimmed.substr(colonPos + 1));
                // Section header if nothing after colon (or only whitespace)
                if (afterColon.empty()) {
                    // Save any pending driver before switching sections
                    if (buildingDriver && section == DRIVER_INFO) {
                        info.drivers.push_back(currentDriver);
                        buildingDriver = false;
                    }
                    inDriversList = false;
                    
                    std::string sectionName = trim(trimmed.substr(0, colonPos));
                    if (sectionName == "WeekendInfo") {
                        section = WEEKEND_INFO;
                    } else if (sectionName == "DriverInfo") {
                        section = DRIVER_INFO;
                    } else if (sectionName == "SessionInfo") {
                        section = SESSION_INFO;
                    } else {
                        // Any other top-level section → stop parsing driver data
                        section = NONE;
                    }
                    continue;
                }
            }
        }
        
        // ── WeekendInfo parsing ──────────────────────────────
        if (section == WEEKEND_INFO && indent > 0) {
            if (trimmed.find("TrackName:") == 0) {
                info.trackName = extractValue(trimmed);
            }
            // Match exact "SeriesName:" not "SeriesNameShort:" etc.
            else if (trimmed.find("SeriesName:") == 0 && trimmed[10] == ':') {
                info.seriesName = extractValue(trimmed);
            }
        }
        
        // ── DriverInfo parsing ───────────────────────────────
        if (section == DRIVER_INFO) {
            // Detect "Drivers:" sub-section
            if (trimmed == "Drivers:") {
                inDriversList = true;
                continue;
            }
            
            if (inDriversList) {
                // New driver entry: line starts with '-'
                // iRacing format: " - CarIdx: 0"
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
                    currentDriver.licSubLevel = 0;
                    buildingDriver = true;
                    
                    // Parse the field on the same line as '-'
                    // e.g. "- CarIdx: 0" → strip "- " then treat as normal field
                    std::string afterDash = trim(trimmed.substr(1));
                    if (!afterDash.empty() && afterDash.find(':') != std::string::npos) {
                        // Process this field
                        if (afterDash.find("CarIdx:") == 0) {
                            currentDriver.carIdx = extractInt(afterDash);
                        }
                    }
                }
                // Continuation properties of current driver
                else if (buildingDriver && indent >= 2) {
                    // Use starts-with matching to avoid substring false matches
                    if (trimmed.find("UserName:") == 0) {
                        currentDriver.userName = extractValue(trimmed);
                    }
                    else if (trimmed.find("CarNumber:") == 0) {
                        std::string val = extractValue(trimmed);
                        // CarNumber can be quoted: "13" or unquoted: 13
                        currentDriver.carNumber = val;
                    }
                    else if (trimmed.find("IRating:") == 0) {
                        currentDriver.iRating = extractInt(trimmed);
                    }
                    else if (trimmed.find("LicLevel:") == 0) {
                        currentDriver.licenseLevel = extractInt(trimmed);
                    }
                    else if (trimmed.find("LicSubLevel:") == 0) {
                        currentDriver.licSubLevel = extractInt(trimmed);
                    }
                    else if (trimmed.find("LicString:") == 0) {
                        currentDriver.licString = extractValue(trimmed);
                    }
                    else if (trimmed.find("CarPath:") == 0) {
                        currentDriver.carPath = extractValue(trimmed);
                    }
                    else if (trimmed.find("CarClassShortName:") == 0) {
                        currentDriver.carClassShortName = extractValue(trimmed);
                    }
                    else if (trimmed.find("CarScreenName:") == 0) {
                        // Additional fallback for class
                    }
                }
            }
        }
        
        // ── SessionInfo parsing ──────────────────────────────
        if (section == SESSION_INFO && indent > 0) {
            if (trimmed.find("SessionLaps:") == 0) {
                std::string value = extractValue(trimmed);
                if (value == "unlimited") {
                    info.sessionLaps = 999999;
                } else {
                    info.sessionLaps = extractInt(trimmed);
                }
            }
            else if (trimmed.find("SessionTime:") == 0) {
                std::string value = extractValue(trimmed);
                if (value == "unlimited") {
                    info.sessionTime = 999999.0f;
                } else {
                    info.sessionTime = extractFloat(trimmed);
                }
            }
        }
    }
    
    // Don't forget last driver being built
    if (buildingDriver && section == DRIVER_INFO) {
        info.drivers.push_back(currentDriver);
    }

    // ── Debug summary ────────────────────────────────────────
    std::cout << "[YAML] Parsed: series=\"" << info.seriesName 
              << "\" track=\"" << info.trackName
              << "\" drivers=" << info.drivers.size()
              << " laps=" << info.sessionLaps << "\n";
    for (size_t i = 0; i < std::min(info.drivers.size(), (size_t)3); ++i) {
        const auto& d = info.drivers[i];
        std::cout << "[YAML]   [" << d.carIdx << "] \"" << d.userName 
                  << "\" #" << d.carNumber
                  << " iR=" << d.iRating
                  << " licSub=" << d.licSubLevel
                  << " licStr=\"" << d.licString << "\"\n";
    }
    if (info.drivers.size() > 3) {
        std::cout << "[YAML]   ... and " << (info.drivers.size() - 3) << " more\n";
    }
    
    return info;
}

} // namespace utils
