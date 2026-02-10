#ifndef UTILS_YAML_PARSER_H
#define UTILS_YAML_PARSER_H

#include <string>
#include <map>
#include <vector>

namespace utils {

// Simple YAML parser for iRacing SessionInfo
// Doesn't handle all YAML features, just what iRacing uses
class YAMLParser {
public:
    struct DriverInfo {
        int carIdx;
        std::string userName;
        std::string carNumber;
        int iRating;
        int licenseLevel;
        std::string carPath;
        std::string carClassShortName;
    };
    
    struct SessionInfo {
        std::string seriesName;
        std::string trackName;
        int sessionLaps;
        float sessionTime;
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

#endif // YAML_PARSER_H
