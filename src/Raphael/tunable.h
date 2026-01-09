#pragma once
#include <Raphael/utils.h>

#include <functional>
#include <string>
#include <vector>



namespace Raphael {

// inspired by https://github.com/Quinniboi10/Lazarus/blob/main/src/tunable.h and
// https://github.com/Ciekce/Stormphrax/blob/main/src/tunable.h
template <bool tunable>
struct SpinOption {
    using SpinOptionCB = std::function<void()>;

    std::string name;
    int value;
    int def;
    int min;
    int max;
    SpinOptionCB callback;

    /** Initializes a SpinOption
     *
     * \param name name of the option
     * \param value default value of the option
     * \param min minimum value of the option
     * \param max maximum value of the option
     * \param callback function to call when the option is set
     */
    SpinOption(const std::string& name, int value, int min, int max, SpinOptionCB callback);

    /** Sets the value of the option
     *
     * \param val value to set to
     */
    void set(int val) {
        value = val;
        if (callback) callback();
    }
    operator int() const { return value; }

    /** Sets a callback for the option
     *
     * \param cb function to call when the option is set
     */
    void set_callback(SpinOptionCB cb) { callback = cb; }

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

    /** Initializes a CheckOption
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



// tunable helpers
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

    #define Tunable(name, value, min, max) \
        inline Raphael::SpinOption<true> name { #name, value, min, max, nullptr }

    #define TunableCallback(name, value, min, max, callback) \
        inline Raphael::SpinOption<true> name { #name, value, min, max, callback }
#else
    #define Tunable(name, value, min, max) static constexpr int name = value

    #define TunableCallback(name, value, min, max, callback) static constexpr int name = value
#endif

template <bool tunable>
inline SpinOption<tunable>::SpinOption(
    const std::string& name, int value, int min, int max, SpinOptionCB callback
)
    : name(name), value(value), def(value), min(min), max(max), callback(callback) {
#ifdef TUNE
    if constexpr (tunable) tunables.push_back(this);
#endif
}


/** Updates the lmr table */
void update_lmr_table();

/** Initializes the tunable dependent parameters */
void init_tunables();



// search
Tunable(ASPIRATION_WINDOW, 50, 5, 100);

// negamax
Tunable(RFP_DEPTH, 6, 1, 10);           // max depth to apply rfp from
Tunable(RFP_DEPTH_SCALE, 77, 25, 150);  // depth margin scale for rfp
Tunable(RFP_IMPROV_SCALE, 0, 0, 100);   // improving margin scale for rfp

Tunable(NMP_DEPTH, 3, 1, 8);      // depth to apply nmp from
Tunable(NMP_REDUCTION, 4, 1, 8);  // depth reduction for nmp

Tunable(LMR_DEPTH, 3, 1, 5);                    // depth to apply lmr from
Tunable(LMR_FROMMOVE, 5, 2, 8);                 // movei to apply lmr from
inline MultiArray<int, 2, 256, 256> LMR_TABLE;  // lmr reduction[quiet][ply][move_searched]
TunableCallback(LMR_QUIET_BASE, 1456, 500, 2000, update_lmr_table);
TunableCallback(LMR_NOISY_BASE, 202, -500, 750, update_lmr_table);
TunableCallback(LMR_QUIET_DIVISOR, 2835, 1000, 4000, update_lmr_table);
TunableCallback(LMR_NOISY_DIVISOR, 3319, 2500, 4500, update_lmr_table);
TunableCallback(LMR_NONPV, 1046, 500, 2000, update_lmr_table);

// quiescence
Tunable(QS_FUTILITY_MARGIN, 150, 50, 400);  // margin for qs futility pruning

// move ordering
static constexpr int GOOD_NOISY_FLOOR = 30000;  // good captures/promotions <=30500
static constexpr int KILLER_FLOOR = 21000;      // killer moves
static constexpr int BAD_NOISY_FLOOR = -20000;  // bad captures/promotions <=-19500

Tunable(GOOD_NOISY_SEE_THRESH, -15, -200, 200);  // SEE threshold for good capture/promotion

Tunable(HISTORY_BONUS_SCALE, 100, 5, 500);
Tunable(HISTORY_BONUS_OFFSET, 100, 0, 200);
Tunable(HISTORY_BONUS_MAX, 2000, 500, 4000);
static constexpr int HISTORY_MAX = 16384;


#undef Tunable
#undef TunableCallback
}  // namespace Raphael
