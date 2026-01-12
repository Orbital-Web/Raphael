#pragma once
#include <Raphael/types.h>

#include <functional>
#include <string>
#include <vector>



namespace raphael {

// inspired by https://github.com/Quinniboi10/Lazarus/blob/main/src/tunable.h and
// https://github.com/Ciekce/Stormphrax/blob/main/src/tunable.h
template <bool tunable>
struct SpinOption {
    using SpinOptionCB = std::function<void()>;

    std::string name;
    i32 value;
    i32 def;
    i32 min;
    i32 max;
    SpinOptionCB callback;

    /** Initializes a SpinOption
     *
     * \param name name of the option
     * \param value default value of the option
     * \param min minimum value of the option
     * \param max maximum value of the option
     * \param callback function to call when the option is set
     */
    SpinOption(const std::string& name, i32 value, i32 min, i32 max, SpinOptionCB callback);

    /** Sets the value of the option
     *
     * \param val value to set to
     */
    void set(i32 val) {
        value = val;
        if (callback) callback();
    }
    operator i32() const { return value; }

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
inline bool set_tunable(const std::string& name, i32 value) {
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
        inline raphael::SpinOption<true> name { #name, value, min, max, nullptr }

    #define TunableCallback(name, value, min, max, callback) \
        inline raphael::SpinOption<true> name { #name, value, min, max, callback }
#else
    #define Tunable(name, value, min, max) static constexpr i32 name = value

    #define TunableCallback(name, value, min, max, callback) static constexpr i32 name = value
#endif

template <bool tunable>
inline SpinOption<tunable>::SpinOption(
    const std::string& name, i32 value, i32 min, i32 max, SpinOptionCB callback
)
    : name(name), value(value), def(value), min(min), max(max), callback(callback) {
#ifdef TUNE
    if constexpr (tunable) tunables.push_back(this);
#endif
}


/** Updates the lmp table */
void update_lmp_table();

/** Updates the lmr table */
void update_lmr_table();

/** Initializes the tunable dependent parameters */
void init_tunables();



// search
Tunable(ASPIRATION_WINDOW, 50, 5, 100);

// negamax
Tunable(RFP_DEPTH, 6, 1, 10);           // max depth to apply rfp from
Tunable(RFP_DEPTH_SCALE, 77, 25, 150);  // margin depth scale for rfp
Tunable(RFP_IMPROV_SCALE, 0, 0, 100);   // margin improving scale for rfp

Tunable(NMP_DEPTH, 3, 1, 8);      // depth to apply nmp from
Tunable(NMP_REDUCTION, 4, 1, 8);  // depth reduction for nmp

inline MultiArray<i32, 2, 256> LMP_TABLE;  // lmp threshold[improving][depth]
TunableCallback(LMP_THRESH_BASE, 3, 1, 12, update_lmp_table);

Tunable(FP_DEPTH, 7, 4, 12);           // max depth to apply fp from
Tunable(FP_DEPTH_SCALE, 80, 50, 200);  // margin depth scale for fp
Tunable(FP_MARGIN_BASE, 100, 0, 400);  // base margin for fp

Tunable(SEE_QUIET_DEPTH_SCALE, -30, -128, 0);  // threshold depth scale for quiet SEE pruning
Tunable(SEE_NOISY_DEPTH_SCALE, -90, -128, 0);  // threshold depth scale for noisy SEE pruning

Tunable(LMR_DEPTH, 3, 1, 5);                    // depth to apply lmr from
Tunable(LMR_FROMMOVE, 5, 2, 8);                 // movei to apply lmr from
inline MultiArray<i32, 2, 256, 256> LMR_TABLE;  // lmr reduction[quiet][ply][move_searched]
TunableCallback(LMR_QUIET_BASE, 182, 0, 384, update_lmr_table);
TunableCallback(LMR_NOISY_BASE, 25, 0, 384, update_lmr_table);
TunableCallback(LMR_QUIET_DIVISOR, 354, 32, 512, update_lmr_table);
TunableCallback(LMR_NOISY_DIVISOR, 414, 32, 512, update_lmr_table);
TunableCallback(LMR_NONPV, 130, 0, 384, update_lmr_table);

// quiescence
Tunable(QS_FUTILITY_MARGIN, 150, 50, 400);  // margin for qs futility pruning
Tunable(QS_SEE_THRESH, -100, -500, 200);    // SEE threshold for qs SEE pruning

// move ordering
static constexpr i32 GOOD_NOISY_FLOOR = 30000;  // good captures/promotions <=30500
static constexpr i32 KILLER_FLOOR = 21000;      // killer moves
static constexpr i32 BAD_NOISY_FLOOR = -20000;  // bad captures/promotions <=-19500

Tunable(GOOD_NOISY_SEE_THRESH, -15, -200, 200);  // SEE threshold for good capture/promotion

Tunable(HISTORY_BONUS_SCALE, 100, 5, 500);
Tunable(HISTORY_BONUS_OFFSET, 100, 0, 200);
Tunable(HISTORY_BONUS_MAX, 2000, 500, 4000);
static constexpr i32 HISTORY_MAX = 16384;


#undef Tunable
#undef TunableCallback
}  // namespace raphael
