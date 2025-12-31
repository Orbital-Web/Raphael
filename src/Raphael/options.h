#pragma once
#include <cstdint>
#include <string>

struct SpinOption {
    std::string name;
    uint32_t min;
    uint32_t max;
    uint32_t def;
    uint32_t value;

    /** Returns the UCI option info string
     *
     * \returns stringified option info
     */
    std::string to_string() const {
        return "option name " + name + " type spin default " + std::to_string(def) + " min "
               + std::to_string(min) + " max " + std::to_string(max) + "\n";
    }

    /** Sets the value of the option
     *
     * \param val value to set to
     */
    void set(const int32_t val) { value = val; }

    operator int32_t() const { return value; }
};

struct SetSpinOption {
    std::string name;
    uint32_t value;
};
