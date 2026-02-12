#ifndef CONFIG_H
#define CONFIG_H

#include <string>

namespace utils {

class Config {
public:
    // Singleton access
    static Config& getInstance() {
        static Config instance;
        return instance;
    }

    // Delete copy/move constructors
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    // Configuration data
    float posX = 100.0f;
    float posY = 100.0f;
    float width = 600.0f;
    float height = 400.0f;
    float alpha = 0.9f;
    bool visible = true;
    bool uiLocked = true;  // Start locked

    // Load/Save
    static void load(const std::string& filename = "config.ini");
    static void save(const std::string& filename = "config.ini");

private:
    Config() = default;  // Private constructor for singleton
};

} // namespace utils

#endif // CONFIG_H