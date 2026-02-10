#include "utils/config.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace utils {

// Static member initialization
WidgetConfig Config::s_relativeConfig;
WidgetConfig Config::s_telemetryConfig;
float Config::s_fontScale = 1.0f;
bool Config::s_clickThrough = false;
float Config::s_globalAlpha = 0.7f;

std::string Config::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}

std::string Config::extractValue(const std::string& line) {
    size_t eq = line.find('=');
    if (eq == std::string::npos) return "";
    return trim(line.substr(eq + 1));
}

void Config::load(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cout << "[Config] File not found, using defaults: " << filename << std::endl;
        return;
    }
    
    std::cout << "[Config] Loading: " << filename << std::endl;
    
    std::string line;
    std::string currentSection;
    
    while (std::getline(file, line)) {
        line = trim(line);
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        
        // Section header
        if (line[0] == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.length() - 2);
            continue;
        }
        
        // Key = Value
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        
        std::string key = trim(line.substr(0, eq));
        std::string value = extractValue(line);
        
        // Parse based on section
        if (currentSection == "Global") {
            if (key == "FontScale") s_fontScale = std::stof(value);
            else if (key == "ClickThrough") s_clickThrough = (value == "true" || value == "1");
            else if (key == "GlobalAlpha") s_globalAlpha = std::stof(value);
        }
        else if (currentSection == "Relative") {
            if (key == "PosX") s_relativeConfig.posX = std::stof(value);
            else if (key == "PosY") s_relativeConfig.posY = std::stof(value);
            else if (key == "Width") s_relativeConfig.width = std::stof(value);
            else if (key == "Height") s_relativeConfig.height = std::stof(value);
            else if (key == "Alpha") s_relativeConfig.alpha = std::stof(value);
            else if (key == "Visible") s_relativeConfig.visible = (value == "true" || value == "1");
        }
        else if (currentSection == "Telemetry") {
            if (key == "PosX") s_telemetryConfig.posX = std::stof(value);
            else if (key == "PosY") s_telemetryConfig.posY = std::stof(value);
            else if (key == "Width") s_telemetryConfig.width = std::stof(value);
            else if (key == "Height") s_telemetryConfig.height = std::stof(value);
            else if (key == "Alpha") s_telemetryConfig.alpha = std::stof(value);
            else if (key == "Visible") s_telemetryConfig.visible = (value == "true" || value == "1");
        }
    }
    
    file.close();
    std::cout << "[Config] Loaded successfully" << std::endl;
}

void Config::save(const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[Config] Failed to save: " << filename << std::endl;
        return;
    }
    
    std::cout << "[Config] Saving: " << filename << std::endl;
    
    // Header
    file << "; iRacing Overlay Configuration\n";
    file << "; Generated automatically - edit carefully\n\n";
    
    // Global section
    file << "[Global]\n";
    file << "FontScale=" << s_fontScale << "\n";
    file << "ClickThrough=" << (s_clickThrough ? "true" : "false") << "\n";
    file << "GlobalAlpha=" << s_globalAlpha << "\n\n";
    
    // Relative widget
    file << "[Relative]\n";
    file << "PosX=" << s_relativeConfig.posX << "\n";
    file << "PosY=" << s_relativeConfig.posY << "\n";
    file << "Width=" << s_relativeConfig.width << "\n";
    file << "Height=" << s_relativeConfig.height << "\n";
    file << "Alpha=" << s_relativeConfig.alpha << "\n";
    file << "Visible=" << (s_relativeConfig.visible ? "true" : "false") << "\n\n";
    
    // Telemetry widget
    file << "[Telemetry]\n";
    file << "PosX=" << s_telemetryConfig.posX << "\n";
    file << "PosY=" << s_telemetryConfig.posY << "\n";
    file << "Width=" << s_telemetryConfig.width << "\n";
    file << "Height=" << s_telemetryConfig.height << "\n";
    file << "Alpha=" << s_telemetryConfig.alpha << "\n";
    file << "Visible=" << (s_telemetryConfig.visible ? "true" : "false") << "\n";
    
    file.close();
    std::cout << "[Config] Saved successfully" << std::endl;
}

WidgetConfig& Config::getRelativeConfig() {
    return s_relativeConfig;
}

WidgetConfig& Config::getTelemetryConfig() {
    return s_telemetryConfig;
}

float Config::getFontScale() {
    return s_fontScale;
}

void Config::setFontScale(float scale) {
    s_fontScale = scale;
}

bool Config::isClickThrough() {
    return s_clickThrough;
}

void Config::setClickThrough(bool clickThrough) {
    s_clickThrough = clickThrough;
}

float Config::getGlobalAlpha() {
    return s_globalAlpha;
}

void Config::setGlobalAlpha(float alpha) {
    s_globalAlpha = alpha;
}

} // namespace utils
