#include "config.h"
#include <fstream>
#include <iostream>

namespace utils {

void Config::load(const std::string& filename) {
    std::cout << "[Config] Loading " << filename << " (stub - no settings yet)" << std::endl;
    // TODO: Later add real INI parsing for window positions, opacity, enabled widgets, etc.
}

void Config::save(const std::string& filename) {
    std::cout << "[Config] Saving " << filename << " (stub)" << std::endl;
}

} // namespace utils