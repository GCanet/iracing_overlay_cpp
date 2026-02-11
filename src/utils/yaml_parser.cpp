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
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        value = value.substr(1, value.length() - 2);
    return value;
}

int YAMLParser::extractInt(const std::string& line) {
    std::string v = extractValue(line);
    try { return std::stoi(v); } catch (...) { return 0; }
}

float YAMLParser::extractFloat(const std::string& line) {
    std::string v = extractValue(line);
    try { return std::stof(v); } catch (...) { return 0.0f; }
}

YAMLParser::SessionInfo YAMLParser::parse(const char* yaml) {
    SessionInfo info;
    if (!yaml) return info;

    std::istringstream stream(yaml);
    std::string line;
    enum Section { NONE, WEEKEND, DRIVER_INFO, SESSION_INFO };
    Section section = NONE;
    bool inDriversList = false;
    DriverInfo cur;
    bool building = false;

    while (std::getline(stream, line)) {
        int indent = 0;
        for (char c : line) { if (c == ' ') indent++; else if (c == '\t') indent += 2; else break; }
        std::string t = trim(line);
        if (t.empty()) continue;

        if (indent == 0) {
            size_t cp = t.find(':');
            if (cp != std::string::npos && trim(t.substr(cp + 1)).empty()) {
                if (building && section == DRIVER_INFO) { info.drivers.push_back(cur); building = false; }
                inDriversList = false;
                std::string name = trim(t.substr(0, cp));
                if (name == "WeekendInfo") section = WEEKEND;
                else if (name == "DriverInfo") section = DRIVER_INFO;
                else if (name == "SessionInfo") section = SESSION_INFO;
                else section = NONE;
                continue;
            }
        }

        if (section == WEEKEND && indent > 0) {
            if (t.find("TrackName:") == 0) info.trackName = extractValue(t);
            else if (t.size() > 10 && t.substr(0, 11) == "SeriesName:") info.seriesName = extractValue(t);
        }

        if (section == DRIVER_INFO) {
            if (t == "Drivers:") { inDriversList = true; continue; }
            if (inDriversList) {
                if (t[0] == '-') {
                    if (building) info.drivers.push_back(cur);
                    cur = DriverInfo();
                    building = true;
                    std::string ad = trim(t.substr(1));
                    if (!ad.empty() && ad.find("CarIdx:") == 0) cur.carIdx = extractInt(ad);
                } else if (building && indent >= 2) {
                    if (t.find("UserName:") == 0) cur.userName = extractValue(t);
                    else if (t.find("CarNumber:") == 0) cur.carNumber = extractValue(t);
                    else if (t.find("IRating:") == 0) cur.iRating = extractInt(t);
                    else if (t.find("LicLevel:") == 0) cur.licenseLevel = extractInt(t);
                    else if (t.find("LicSubLevel:") == 0) cur.licSubLevel = extractInt(t);
                    else if (t.find("LicString:") == 0) cur.licString = extractValue(t);
                    else if (t.find("CarPath:") == 0) cur.carPath = extractValue(t);
                    else if (t.find("CarClassShortName:") == 0) cur.carClassShortName = extractValue(t);
                    else if (t.find("ClubName:") == 0) cur.countryCode = extractValue(t);
                }
            }
        }

        if (section == SESSION_INFO && indent > 0) {
            if (t.find("SessionLaps:") == 0) {
                std::string v = extractValue(t);
                info.sessionLaps = (v == "unlimited") ? 999999 : extractInt(t);
            } else if (t.find("SessionTime:") == 0) {
                std::string v = extractValue(t);
                info.sessionTime = (v == "unlimited") ? 999999.0f : extractFloat(t);
            }
        }
    }

    if (building && section == DRIVER_INFO) info.drivers.push_back(cur);

    std::cout << "[YAML] series=\"" << info.seriesName << "\" drivers=" << info.drivers.size() << "\n";
    for (size_t i = 0; i < std::min(info.drivers.size(), (size_t)3); ++i) {
        auto& d = info.drivers[i];
        std::cout << "[YAML]   [" << d.carIdx << "] \"" << d.userName << "\" iR=" << d.iRating
                  << " licSub=" << d.licSubLevel << " club=\"" << d.countryCode << "\"\n";
    }
    return info;
}

} // namespace utils
