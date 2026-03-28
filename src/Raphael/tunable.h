#pragma once
#include <Raphael/utils.h>

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
    i32 min_val;
    i32 max_val;
    SpinOptionCB callback;

    /** Initializes a SpinOption
     *
     * \param name name of the option
     * \param value default value of the option
     * \param min_val minimum value of the option
     * \param max_val maximum value of the option
     * \param callback function to call when the option is set
     */
    SpinOption(
        const std::string& name,
        i32 value,
        i32 min_val,
        i32 max_val,
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

    /** Returns the step size (i.e., C_end in OB)
     *
     * \returns step size
     */
    f32 step_size() const { return f32(max_val - min_val) / 20; }

    /** Returns the learning rate (i.e., R_end in OB)
     *
     * \returns learning rate
     */
    f32 learning_rate() const { return 0.002 / (std::min(0.5f, step_size()) / 0.5); }


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
               + std::to_string(min_val) + " max " + std::to_string(max_val) + "\n";
    }

    /** Returns the OB SPSA tuning input string
     *
     * \returns stringified option tuning info
     */
    std::string ob() const {
        return name + ", int, " + std::to_string(def) + ", " + std::to_string(min_val) + ", "
               + std::to_string(max_val) + ", " + std::to_string(step_size()) + ", "
               + std::to_string(learning_rate()) + "\n";
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
        if (utils::is_case_insensitive_equals(tunable->name, name)) {
            // assume value is valid
            tunable->set(value);
            return true;
        }
    }
    return false;
}

    #define Tunable(name, value, min_val, max_val, tunable)      \
        static_assert((min_val <= value) && (value <= max_val)); \
        inline raphael::SpinOption<tunable> name { #name, value, min_val, max_val, nullptr }

    #define TunableCallback(name, value, min_val, max_val, callback, tunable) \
        static_assert((min_val <= value) && (value <= max_val));              \
        inline raphael::SpinOption<tunable> name { #name, value, min_val, max_val, callback }
#else
    #define Tunable(name, value, min_val, max_val, tunable)      \
        static_assert((min_val <= value) && (value <= max_val)); \
        static constexpr i32 name = value

    #define TunableCallback(name, value, min_val, max_val, callback, tunable) \
        static_assert((min_val <= value) && (value <= max_val));              \
        static constexpr i32 name = value
#endif

template <bool tunable>
inline SpinOption<tunable>::SpinOption(
    const std::string& name, i32 value, i32 min_val, i32 max_val, SpinOptionCB callback
)
    : name(name), value(value), def(value), min_val(min_val), max_val(max_val), callback(callback) {
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



// time management
Tunable(TIME_FACTOR, 6, 2, 10, true);    // percentage of remaining time to use as base time
Tunable(INC_FACTOR, 80, 50, 100, true);  // percentage of increment to use use as base time

Tunable(HARD_TIME_FACTOR, 200, 150, 250, true);  // percentage of base time to use as hard time
Tunable(SOFT_TIME_FACTOR, 70, 50, 100, true);    // percentage of base time to use as soft time

Tunable(MOVE_STABILITY_TM_DEPTH, 5, 3, 10, true);      // min depth for move stability tm
Tunable(MOVE_STABILITY_TM_BASE, 120, 100, 300, true);  // base soft limit for move stability tm
Tunable(MOVE_STABILITY_TM_SCALE, 5, 0, 50, true);      // soft limit scale for move stability tm
Tunable(MOVE_STABILITY_TM_MIN, 80, 50, 100, true);     // min soft limit for move stability tm

Tunable(SCORE_STABILITY_MARGIN, 10, 1, 50, true);       // distance to last score to raise stability
Tunable(SCORE_STABILITY_TM_DEPTH, 5, 3, 10, true);      // min depth for score stability tm
Tunable(SCORE_STABILITY_TM_BASE, 120, 100, 300, true);  // base soft limit for score stability tm
Tunable(SCORE_STABILITY_TM_SCALE, 5, 0, 50, true);      // soft limit scale for score stability tm
Tunable(SCORE_STABILITY_TM_MIN, 80, 50, 100, true);     // min soft limit for score stability tm

Tunable(SCORE_TREND_TM_DEPTH, 5, 3, 10, true);     // min depth for score trend tm
Tunable(SCORE_TREND_TM_BASE, 100, 80, 120, true);  // base soft limit for score trend tm
Tunable(SCORE_TREND_TM_SCALE, 4, 0, 50, true);     // soft limit scale for score trend tm
Tunable(SCORE_TREND_MAX, 5, 3, 10, true);          // max abs value of score trend

Tunable(NODE_TM_DEPTH, 5, 3, 10, true);       // min depth for node tm
Tunable(NODE_TM_BASE, 200, 100, 300, true);   // base soft limit for node tm
Tunable(NODE_TM_SCALE, 150, 100, 200, true);  // soft limit scale for node tm

// search
Tunable(ASPIRATION_DEPTH, 3, 2, 5, true);              // min depth to apply aspiration windows
Tunable(ASPIRATION_INIT_SIZE, 50, 5, 100, true);       // initial window size
Tunable(ASPIRATION_WIDENING_FACTOR, 12, 2, 24, true);  // window scale factor (1 + k/16)

Tunable(TT_REPLACEMENT_DEPTH_OFFSET, 4, 0, 8, true);  // offset to replace tt from

// negamax
Tunable(IIR_DEPTH, 3, 3, 6, true);  // min depth to apply iir from

Tunable(RFP_DEPTH, 6, 1, 10, true);           // max depth to apply rfp from
Tunable(RFP_DEPTH_SCALE, 77, 25, 150, true);  // margin depth scale for rfp
Tunable(RFP_IMPROV_SCALE, 40, 0, 100, true);  // margin improving scale for rfp

Tunable(RAZORING_DEPTH, 4, 1, 10, true);             // max depth to apply razoring from
Tunable(RAZORING_DEPTH_SCALE, 250, 100, 350, true);  // margin depth scale for razoring
Tunable(RAZORING_MARGIN_BASE, 300, 0, 400, true);    // base margin for razoring

Tunable(NMP_DEPTH, 3, 1, 10, true);      // depth to apply nmp from
Tunable(NMP_REDUCTION, 4, 1, 10, true);  // depth reduction for nmp

inline MultiArray<i32, 2, 256> LMP_TABLE;  // lmp threshold[improving][depth]
TunableCallback(LMP_THRESH_BASE, 3, 1, 12, update_lmp_table, true);

Tunable(FP_DEPTH, 7, 4, 12, true);           // max depth to apply fp from
Tunable(FP_DEPTH_SCALE, 80, 40, 200, true);  // margin depth scale for fp
Tunable(FP_MARGIN_BASE, 100, 0, 400, true);  // base margin for fp

Tunable(SEE_QUIET_DEPTH_SCALE, -30, -128, -1, true);   // depth scale for quiet SEE pruning
Tunable(SEE_NOISY_DEPTH_SCALE, -90, -128, -30, true);  // depth scale for noisy SEE pruning

Tunable(SE_DEPTH, 8, 6, 12, true);          // min depth to apply singular extensions from
Tunable(SE_TT_DEPTH, 3, 3, 6, true);        // min tt depth margin for singular extensions
Tunable(SE_DEPTH_MARGIN, 32, 4, 64, true);  // depth margin for singular extension beta
Tunable(DE_MARGIN, 30, 4, 64, true);        // score margin for double extensions

Tunable(LMR_DEPTH, 3, 1, 5, true);              // depth to apply lmr from
Tunable(LMR_FROMMOVE, 5, 2, 8, true);           // movei to apply lmr from
inline MultiArray<i32, 2, 256, 256> LMR_TABLE;  // lmr reduction[quiet][ply][move_searched]
TunableCallback(LMR_QUIET_BASE, 182, 32, 384, update_lmr_table, true);
TunableCallback(LMR_NOISY_BASE, -30, -192, 192, update_lmr_table, true);
TunableCallback(LMR_QUIET_DIVISOR, 354, 32, 512, update_lmr_table, true);
TunableCallback(LMR_NOISY_DIVISOR, 414, 32, 512, update_lmr_table, true);
Tunable(LMR_NONPV, 130, 32, 384, true);
Tunable(LMR_CUTNODE, 128, 32, 384, true);
Tunable(LMR_IMPROVING, 128, 32, 384, true);
Tunable(LMR_CHECK, 128, 32, 384, true);
Tunable(LMR_QUIET_HIST_DIVISOR, 12000, 4096, 16384, true);
Tunable(LMR_NOISY_HIST_DIVISOR, 12000, 4096, 16384, true);

// quiescence
Tunable(QS_MAX_MOVES, 3, 1, 5, true);             // max moves to search in qsearch
Tunable(QS_FUTILITY_MARGIN, 150, 50, 400, true);  // margin for qs futility pruning
Tunable(QS_SEE_THRESH, -100, -500, 200, true);    // SEE threshold for qs SEE pruning

// SEE
inline MultiArray<i32, 13> SEE_TABLE;
TunableCallback(SEE_PAWN_VAL, 100, 100, 100, update_see_table, false);
TunableCallback(SEE_KNIGHT_VAL, 422, 200, 600, update_see_table, true);
TunableCallback(SEE_BISHOP_VAL, 437, 200, 600, update_see_table, true);
TunableCallback(SEE_ROOK_VAL, 694, 300, 800, update_see_table, true);
TunableCallback(SEE_QUEEN_VAL, 1313, 600, 1800, update_see_table, true);

// move ordering
static constexpr i32 HISTORY_MAX = 16384;
static constexpr i32 CAPTHIST_DIVISOR = 8;

Tunable(GOOD_NOISY_SEE_BASE, -15, -200, 200, true);  // SEE threshold for good tacticals
Tunable(GOOD_NOISY_SEE_SCALE, 16, 0, 128, true);     // SEE score scale for good tacticals

Tunable(HISTORY_BONUS_DEPTH_SCALE, 100, 32, 512, true);
Tunable(HISTORY_BONUS_OFFSET, 100, 32, 768, true);
Tunable(HISTORY_BONUS_MAX, 2000, 1024, 4096, true);
Tunable(HISTORY_PENALTY_DEPTH_SCALE, 100, 32, 512, true);
Tunable(HISTORY_PENALTY_OFFSET, 100, 32, 768, true);
Tunable(HISTORY_PENALTY_MAX, 2000, 1024, 4096, true);

Tunable(CONTHIST1_WEIGHT, 128, 16, 256, true);
Tunable(CONTHIST2_WEIGHT, 128, 16, 256, true);
Tunable(CONTHIST4_WEIGHT, 64, 16, 256, true);

Tunable(CAPTHIST_BONUS_DEPTH_SCALE, 100, 32, 512, true);
Tunable(CAPTHIST_BONUS_OFFSET, 100, 32, 768, true);
Tunable(CAPTHIST_BONUS_MAX, 2000, 1024, 4096, true);
Tunable(CAPTHIST_PENALTY_DEPTH_SCALE, 100, 32, 512, true);
Tunable(CAPTHIST_PENALTY_OFFSET, 100, 32, 768, true);
Tunable(CAPTHIST_PENALTY_MAX, 2000, 1024, 4096, true);

// corrections
static constexpr i32 CORRHIST_SIZE = 16384;
static constexpr i32 CORRHIST_MAX = 1024;
static constexpr i32 CORRHIST_BONUS_DEPTH_DIVISOR = 8;
static constexpr i32 CORRHIST_BONUS_MAX = 256;

Tunable(PAWN_CORRHIST_WEIGHT, 64, 32, 384, true);
Tunable(MAJOR_CORRHIST_WEIGHT, 64, 32, 384, true);
Tunable(NONPAWN_CORRHIST_WEIGHT, 64, 32, 384, true);

// commands
static constexpr i32 BENCH_DEPTH = 14;

static constexpr i32 GENFENS_MAX_NODES = 1000;
static constexpr i32 GENFENS_MAX_SCORE = 1000;

static constexpr i32 DATAGEN_WIN_ADJ_MVCNT = 5;
static constexpr i32 DATAGEN_WIN_ADJ_SCORE = 2000;
static constexpr i32 DATAGEN_DRAW_ADJ_MVNUM = 32;
static constexpr i32 DATAGEN_DRAW_ADJ_MVCNT = 6;
static constexpr i32 DATAGEN_DRAW_ADJ_SCORE = 15;
static constexpr i32 DATAGEN_HASH = 16;
static constexpr i32 DATAGEN_BATCH_SIZE = 32;

static constexpr f64 DEF_TARGET_ABS_MEAN = 491.0081;  // average for lichess-big3-resolved


#undef Tunable
#undef TunableCallback
}  // namespace raphael
