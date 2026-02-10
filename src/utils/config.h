#ifndef CONFIG_H
#define CONFIG_H

#include <string>

namespace utils {

struct OverlayConfig {
    // Window
    int windowWidth = 1920;
    int windowHeight = 1080;
    
    // Relative widget
    int relativeAhead = 5;
    int relativeBehind = 5;
    bool showIRating = true;
    bool showSafetyRating = true;
    
    // Telemetry
    bool showTelemetry = true;
    int telemetryHistorySeconds = 3;
    
    // Paths
    std::string assetsPath = "./assets";
};

class Config {
public:
    static OverlayConfig& get() {
        static OverlayConfig config;
        return config;
    }
    
    static bool load(const std::string& filepath);
    static bool save(const std::string& filepath);
};

} // namespace utils

#endif // CONFIG_H
