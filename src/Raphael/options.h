#pragma once
#include <cstdint>
#include <string>

struct SpinOption {
    std::string name;
    int min;
    int max;
    int def;
    int value;

    /** Returns the UCI option info string
     *
     * \returns stringified option info
     */
    std::string to_string() const {
        return "option name " + name + " type spin default " + std::to_string(def) + " min "
               + std::to_string(min) + " max " + std::to_string(max) + "\n";
    }

    /** Returns the error info string
     *
     * \returns error info string
     */
    std::string error_string() const {
        return "info string error: option '" + name + "' value must be within min "
               + std::to_string(min) + " max " + std::to_string(max) + "\n";
    }

    /** Sets the value of the option
     *
     * \param val value to set to
     */
    void set(const int val) { value = val; }

    operator int() const { return value; }
};

struct SetSpinOption {
    std::string name;
    int value;
};



struct CheckOption {
    std::string name;
    bool def;
    bool value;

    /** Returns the UCI option info string
     *
     * \returns stringified option info
     */
    std::string to_string() const {
        return "option name " + name + " type check default " + ((def) ? "true" : "false") + "\n";
    }

    /** Sets the value of the option
     *
     * \param val value to set to
     */
    void set(const bool val) { value = val; }

    operator bool() const { return value; }
};

struct SetCheckOption {
    std::string name;
    bool value;
};
