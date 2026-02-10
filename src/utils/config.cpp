#include "utils/config.h"
#include <fstream>

namespace utils {

bool Config::load(const std::string& filepath) {
    // TODO: Implement JSON/INI parsing
    // For now, use defaults
    return true;
}

bool Config::save(const std::string& filepath) {
    // TODO: Implement JSON/INI saving
    return true;
}

} // namespace utils
