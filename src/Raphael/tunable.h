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

/** Updates the see table */
void update_see_table();

/** Initializes the tunable dependent parameters */
void init_tunables();



// search
Tunable(ASPIRATION_WINDOW, 50, 5, 100);

// negamax
Tunable(RFP_DEPTH, 6, 1, 10);           // max depth to apply rfp from
Tunable(RFP_DEPTH_SCALE, 77, 25, 150);  // margin depth scale for rfp
Tunable(RFP_IMPROV_SCALE, 0, 0, 100);   // margin improving scale for rfp

Tunable(RAZORING_DEPTH, 4, 1, 10);             // max depth to apply razoring from
Tunable(RAZORING_DEPTH_SCALE, 250, 100, 800);  // margin depth scale for razoring
Tunable(RAZORING_MARGIN_BASE, 300, 0, 400);    // base margin for razoring

Tunable(NMP_DEPTH, 3, 1, 10);      // depth to apply nmp from
Tunable(NMP_REDUCTION, 4, 1, 10);  // depth reduction for nmp

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

// SEE
inline MultiArray<i32, 13> SEE_TABLE;
TunableCallback(SEE_PAWN_VAL, 100, 100, 100, update_see_table);
TunableCallback(SEE_KNIGHT_VAL, 422, 200, 600, update_see_table);
TunableCallback(SEE_BISHOP_VAL, 437, 200, 600, update_see_table);
TunableCallback(SEE_ROOK_VAL, 694, 300, 800, update_see_table);
TunableCallback(SEE_QUEEN_VAL, 1313, 600, 1800, update_see_table);

// move ordering
static constexpr i32 TT_MOVE_FLOOR = INT16_MAX;  // tt move                 32767
static constexpr i32 GOOD_NOISY_FLOOR = 25000;   // good captures/queening  20000 to 300000
static constexpr i32 KILLER_FLOOR = 17000;       // killer moves            17000
static constexpr i32 HISTORY_MAX = 16384;        // quiet moves            -16384 to 16384
static constexpr i32 BAD_NOISY_FLOOR = -25000;   // bad captures/queening  -30000 to -20000
static constexpr i32 CAPTHIST_DIVISOR = 8;
static_assert(GOOD_NOISY_FLOOR + HISTORY_MAX / CAPTHIST_DIVISOR < TT_MOVE_FLOOR);

Tunable(GOOD_NOISY_SEE_THRESH, -15, -200, 200);  // SEE threshold for good capture/promotion

Tunable(HISTORY_BONUS_DEPTH_SCALE, 100, 128, 512);
Tunable(HISTORY_BONUS_OFFSET, 100, 128, 768);
Tunable(HISTORY_BONUS_MAX, 2000, 1024, 4096);
Tunable(HISTORY_PENALTY_DEPTH_SCALE, 100, 128, 512);
Tunable(HISTORY_PENALTY_OFFSET, 100, 128, 768);
Tunable(HISTORY_PENALTY_MAX, 2000, 1024, 4096);

Tunable(CAPTHIST_BONUS_DEPTH_SCALE, 100, 128, 512);
Tunable(CAPTHIST_BONUS_OFFSET, 100, 128, 768);
Tunable(CAPTHIST_BONUS_MAX, 2000, 1024, 4096);
Tunable(CAPTHIST_PENALTY_DEPTH_SCALE, 100, 128, 512);
Tunable(CAPTHIST_PENALTY_OFFSET, 100, 128, 768);
Tunable(CAPTHIST_PENALTY_MAX, 2000, 1024, 4096);

// misc
static constexpr i32 BENCH_DEPTH = 14;


#undef Tunable
#undef TunableCallback
}  // namespace raphael
