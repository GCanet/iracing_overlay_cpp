#ifndef UTILS_YAML_PARSER_H
#define UTILS_YAML_PARSER_H

#include <string>
#include <map>
#include <vector>

namespace utils {

class YAMLParser {
public:
    struct DriverInfo {
        int carIdx = -1;
        std::string userName;
        std::string carNumber;
        int iRating = 0;
        int licenseLevel = 0;
        int licSubLevel = 0;
        std::string licString;
        std::string carPath;
        std::string carClassShortName;
        std::string countryCode;   // e.g. "ES", "NL", "US"
    };

    struct SessionInfo {
        std::string seriesName;
        std::string trackName;
        int sessionLaps = 0;
        float sessionTime = 0.0f;
        std::vector<DriverInfo> drivers;
    };

    static SessionInfo parse(const char* yaml);

private:
    static std::string trim(const std::string& str);
    static std::string extractValue(const std::string& line);
    static int extractInt(const std::string& line);
    static float extractFloat(const std::string& line);
};

} // namespace utils

#endif // UTILS_YAML_PARSER_H
