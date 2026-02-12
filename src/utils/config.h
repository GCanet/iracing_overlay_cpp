#ifndef CONFIG_H
#define CONFIG_H

#include <string>

namespace utils {

struct WidgetConfig {
    float posX = -1.0f;  // -1 = first time (auto-center)
    float posY = -1.0f;
    float width = -1.0f;
    float height = -1.0f;
    float alpha = 0.7f;
    bool visible = true;
};

class Config {
public:
    static void load(const std::string& filename = "config.ini");
    static void save(const std::string& filename = "config.ini");

    // Widget configs
    static WidgetConfig& getRelativeConfig();
    static WidgetConfig& getTelemetryConfig();

    // Global overlay settings
    static float getFontScale();
    static void setFontScale(float scale);

    static bool isClickThrough();
    static void setClickThrough(bool clickThrough);

    static float getGlobalAlpha();
    static void setGlobalAlpha(float alpha);

private:
    static WidgetConfig s_relativeConfig;
    static WidgetConfig s_telemetryConfig;
    static float s_fontScale;
    static bool s_clickThrough;
    static float s_globalAlpha;
};

} // namespace utils

#endif // CONFIG_H
