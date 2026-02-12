#include "utils/config.h"
#include <fstream>
#include <iostream>
#include <sstream>

namespace utils {

void Config::load(const std::string& filename) {
    Config& config = getInstance();
    
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[Config] File not found: " << filename << " (using defaults)" << std::endl;
        return;
    }

    std::cout << "[Config] Loading: " << filename << std::endl;

    std::string line;
    while (std::getline(file, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        // Skip empty lines and comments
        if (line.empty() || line[0] == ';' || line[0] == '#' || line[0] == '[') {
            continue;
        }

        // Parse key=value
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);

        // Trim whitespace from key and value
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));

        // Parse values
        try {
            if (key == "PosX") config.posX = std::stof(value);
            else if (key == "PosY") config.posY = std::stof(value);
            else if (key == "Width") config.width = std::stof(value);
            else if (key == "Height") config.height = std::stof(value);
            else if (key == "Alpha") config.alpha = std::stof(value);
            else if (key == "Visible") config.visible = (value == "true" || value == "1");
            else if (key == "UILocked") config.uiLocked = (value == "true" || value == "1");
        } catch (...) {
            std::cerr << "[Config] Error parsing: " << key << "=" << value << std::endl;
        }
    }

    file.close();
    std::cout << "[Config] Loaded successfully" << std::endl;
}

void Config::save(const std::string& filename) {
    Config& config = getInstance();
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "[Config] Failed to save: " << filename << std::endl;
        return;
    }

    file << "[Overlay]\n";
    file << "PosX=" << config.posX << "\n";
    file << "PosY=" << config.posY << "\n";
    file << "Width=" << config.width << "\n";
    file << "Height=" << config.height << "\n";
    file << "Alpha=" << config.alpha << "\n";
    file << "Visible=" << (config.visible ? "true" : "false") << "\n";
    file << "UILocked=" << (config.uiLocked ? "true" : "false") << "\n";

    file.close();
    std::cout << "[Config] Saved successfully" << std::endl;
}

} // namespace utils