#pragma once
#include <cstdint>
#include <string>

struct SpinOption {
    std::string name;
    uint32_t min;
    uint32_t max;
    uint32_t value;

    inline std::string to_string() const {
        return "option name " + name + " type spin default " + std::to_string(value) + " min "
               + std::to_string(min) + " max " + std::to_string(max) + "\n";
    }
};

struct SetSpinOption {
    std::string name;
    uint32_t value;
};
