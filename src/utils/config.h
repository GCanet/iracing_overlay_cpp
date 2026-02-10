#pragma once
#include <string>

namespace utils {

class Config {
public:
    static void load(const std::string& filename = "config.ini");
    static void save(const std::string& filename = "config.ini");
};

} // namespace utils