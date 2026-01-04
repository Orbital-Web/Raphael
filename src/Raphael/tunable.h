#pragma once
#include <string>
#include <vector>



namespace Raphael {

// inspired by https://github.com/Quinniboi10/Lazarus/blob/main/src/tunable.h
template <bool tunable>
struct SpinOption {
    std::string name;
    int value;
    int def;
    int min;
    int max;

    /** Initializes a SpinOption with appropriate default, min, and max values
     *
     * \param name name of the option
     * \param value value to set as the default
     */
    SpinOption(const std::string& name, int value);

    /** Initializes a SpinOption with custom default, min, and max values
     *
     * \param name name of the option
     * \param value default value of the option
     * \param min minimum value of the option
     * \param max maximum value of the option
     */
    SpinOption(const std::string& name, int value, int min, int max)
        : name(name), value(value), def(value), min(min), max(max) {};

    /** Sets the value of the option
     *
     * \param val value to set to
     */
    void set(int val) { value = val; }
    operator int() const { return value; }

    /** Returns the UCI option info string
     *
     * \returns stringified option info
     */
    std::string uci() const {
        return "option name " + name + " type spin default " + std::to_string(def) + " min "
               + std::to_string(min) + " max " + std::to_string(max) + "\n";
    }
};

struct CheckOption {
    std::string name;
    bool value;
    bool def;

    /** Initializes a CheckOption with appropriate default values
     *
     * \param name name of the option
     * \param value value to set as the default
     */
    CheckOption(const std::string& name, bool value): name(name), value(value), def(value) {};

    /** Sets the value of the option
     *
     * \param val value to set to
     */
    void set(bool val) { value = val; }
    operator bool() const { return value; }

    /** Returns the UCI option info string
     *
     * \returns stringified option info
     */
    std::string uci() const {
        return "option name " + name + " type check default " + ((def) ? "true" : "false") + "\n";
    }
};



#ifdef TUNE
inline std::vector<SpinOption<true>*> tunables;

/** Sets a tunable parameter's value
 *
 * \param name name of parameter
 * \param value value to set to
 * \returns whether a parameter value was set or not
 */
inline bool set_tunable(const std::string& name, int value) {
    for (const auto& tunable : tunables) {
        if (tunable->name == name) {
            // assume value is valid
            tunable->set(value);
            return true;
        }
    }
    return false;
}

    #define Tunable(name, value) \
        inline Raphael::SpinOption<true> name { #name, value }
#else
    #define Tunable(name, value) static constexpr int name = value
#endif



template <bool tunable>
inline SpinOption<tunable>::SpinOption(const std::string& name, int value)
    : name(name), value(value), def(value), min(value / 2), max(value * 2) {
#ifdef TUNE
    if constexpr (tunable) tunables.push_back(this);
#endif
}



// search
Tunable(ASPIRATION_WINDOW, 50);

// negamax
Tunable(RFP_DEPTH, 6);       // max depth to apply rfp from
Tunable(RFP_MARGIN, 77);     // depth margin scale for rfp
Tunable(NMP_DEPTH, 3);       // depth to apply nmp from
Tunable(NMP_REDUCTION, 4);   // depth reduction for nmp
Tunable(REDUCTION_FROM, 5);  // movei to apply lmr from

// quiescence
Tunable(DELTA_THRESHOLD, 400);  // safety margin for delta pruning

// move ordering
static constexpr int GOOD_NOISY_FLOOR = 30000;  // good captures/promotions <=30500
static constexpr int KILLER_FLOOR = 21000;      // killer moves
static constexpr int BAD_NOISY_FLOOR = -20000;  // bad captures/promotions <=-19500

Tunable(GOOD_NOISY_THRESH, 15);  // negative SEE threshold for good capture/promotion

Tunable(HISTORY_BONUS_SCALE, 100);
Tunable(HISTORY_BONUS_OFFSET, 100);
Tunable(HISTORY_BONUS_MAX, 2000);
static constexpr int HISTORY_MAX = 16384;
}  // namespace Raphael
