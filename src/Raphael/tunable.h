#pragma once
#include <chess/types.h>

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
    i32 step;
    SpinOptionCB callback;

    /** Initializes a SpinOption
     *
     * \param name name of the option
     * \param value default value of the option
     * \param min minimum value of the option
     * \param max maximum value of the option
     * \param step step size of the option when tuning
     * \param callback function to call when the option is set
     */
    SpinOption(
        const std::string& name,
        i32 value,
        i32 min,
        i32 max,
        i32 step = 0,
        SpinOptionCB callback = nullptr
    );

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

    /** Returns the OB SPSA tuning input string
     *
     * \returns stringified option tuning info
     */
    std::string ob() const {
        return name + ", int, " + std::to_string(def) + ", " + std::to_string(min) + ", "
               + std::to_string(max) + ", " + std::to_string(step) + ", 0.002\n";
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

    #define Tunable(name, value, min, max, step, tunable) \
        inline raphael::SpinOption<tunable> name { #name, value, min, max, step, nullptr }

    #define TunableCallback(name, value, min, max, step, callback, tunable) \
        inline raphael::SpinOption<tunable> name { #name, value, min, max, step, callback }
#else
    #define Tunable(name, value, min, max, step, tunable) static constexpr i32 name = value

    #define TunableCallback(name, value, min, max, step, callback, tunable) \
        static constexpr i32 name = value
#endif

template <bool tunable>
inline SpinOption<tunable>::SpinOption(
    const std::string& name, i32 value, i32 min, i32 max, i32 step, SpinOptionCB callback
)
    : name(name), value(value), def(value), min(min), max(max), step(step), callback(callback) {
#ifdef TUNE
    if constexpr (tunable) tunables.push_back(this);
#endif
}


/** Updates the lmp table */
void update_lmp_table();

/** Updates the lmr table */
void update_lmr_table();

/** Updates the see table */
void update_see_table();

/** Initializes the tunable dependent parameters */
void init_tunables();



// search
Tunable(ASPIRATION_WINDOW, 50, 5, 100, 5, true);

// negamax
Tunable(RFP_DEPTH, 6, 1, 10, 1, true);           // max depth to apply rfp from
Tunable(RFP_DEPTH_SCALE, 77, 25, 150, 8, true);  // margin depth scale for rfp
Tunable(RFP_IMPROV_SCALE, 0, 0, 100, 8, true);   // margin improving scale for rfp

Tunable(RAZORING_DEPTH, 4, 1, 10, 1, true);              // max depth to apply razoring from
Tunable(RAZORING_DEPTH_SCALE, 250, 100, 350, 40, true);  // margin depth scale for razoring
Tunable(RAZORING_MARGIN_BASE, 300, 0, 400, 40, true);    // base margin for razoring

Tunable(NMP_DEPTH, 3, 1, 10, 1, true);      // depth to apply nmp from
Tunable(NMP_REDUCTION, 4, 1, 10, 1, true);  // depth reduction for nmp

inline MultiArray<i32, 2, 256> LMP_TABLE;  // lmp threshold[improving][depth]
TunableCallback(LMP_THRESH_BASE, 3, 1, 12, 1, update_lmp_table, true);

Tunable(FP_DEPTH, 7, 4, 12, 1, true);            // max depth to apply fp from
Tunable(FP_DEPTH_SCALE, 80, 40, 200, 10, true);  // margin depth scale for fp
Tunable(FP_MARGIN_BASE, 100, 0, 400, 20, true);  // base margin for fp

Tunable(SEE_QUIET_DEPTH_SCALE, -30, -128, -1, 12, true);   // depth scale for quiet SEE pruning
Tunable(SEE_NOISY_DEPTH_SCALE, -90, -128, -30, 20, true);  // depth scale for noisy SEE pruning

Tunable(LMR_DEPTH, 3, 1, 5, 1, true);           // depth to apply lmr from
Tunable(LMR_FROMMOVE, 5, 2, 8, 1, true);        // movei to apply lmr from
inline MultiArray<i32, 2, 256, 256> LMR_TABLE;  // lmr reduction[quiet][ply][move_searched]
TunableCallback(LMR_QUIET_BASE, 182, 0, 384, 20, update_lmr_table, true);
TunableCallback(LMR_NOISY_BASE, 25, 0, 384, 20, update_lmr_table, true);
TunableCallback(LMR_QUIET_DIVISOR, 354, 32, 512, 12, update_lmr_table, true);
TunableCallback(LMR_NOISY_DIVISOR, 414, 32, 512, 12, update_lmr_table, true);
TunableCallback(LMR_NONPV, 130, 0, 384, 20, update_lmr_table, true);

// quiescence
Tunable(QS_FUTILITY_MARGIN, 150, 50, 400, 20, true);  // margin for qs futility pruning
Tunable(QS_SEE_THRESH, -100, -500, 200, 30, true);    // SEE threshold for qs SEE pruning

// SEE
inline MultiArray<i32, 13> SEE_TABLE;
TunableCallback(SEE_PAWN_VAL, 100, 100, 100, 0, update_see_table, false);
TunableCallback(SEE_KNIGHT_VAL, 422, 200, 600, 25, update_see_table, true);
TunableCallback(SEE_BISHOP_VAL, 437, 200, 600, 25, update_see_table, true);
TunableCallback(SEE_ROOK_VAL, 694, 300, 800, 30, update_see_table, true);
TunableCallback(SEE_QUEEN_VAL, 1313, 600, 1800, 40, update_see_table, true);

// move ordering
static constexpr i32 HISTORY_MAX = 16384;
static constexpr i32 CAPTHIST_DIVISOR = 8;

Tunable(GOOD_NOISY_SEE_BASE, -15, -200, 200, 30, true);  // SEE threshold for good tacticals
Tunable(GOOD_NOISY_SEE_SCALE, 16, 0, 128, 8, true);      // SEE score scale for good tacticals

Tunable(HISTORY_BONUS_DEPTH_SCALE, 100, 128, 512, 32, true);
Tunable(HISTORY_BONUS_OFFSET, 100, 128, 768, 64, true);
Tunable(HISTORY_BONUS_MAX, 2000, 1024, 4096, 256, true);
Tunable(HISTORY_PENALTY_DEPTH_SCALE, 100, 128, 512, 32, true);
Tunable(HISTORY_PENALTY_OFFSET, 100, 128, 768, 64, true);
Tunable(HISTORY_PENALTY_MAX, 2000, 1024, 4096, 256, true);

Tunable(CAPTHIST_BONUS_DEPTH_SCALE, 100, 128, 512, 32, true);
Tunable(CAPTHIST_BONUS_OFFSET, 100, 128, 768, 64, true);
Tunable(CAPTHIST_BONUS_MAX, 2000, 1024, 4096, 256, true);
Tunable(CAPTHIST_PENALTY_DEPTH_SCALE, 100, 128, 512, 32, true);
Tunable(CAPTHIST_PENALTY_OFFSET, 100, 128, 768, 64, true);
Tunable(CAPTHIST_PENALTY_MAX, 2000, 1024, 4096, 256, true);

// misc
static constexpr i32 BENCH_DEPTH = 14;


#undef Tunable
#undef TunableCallback
}  // namespace raphael
