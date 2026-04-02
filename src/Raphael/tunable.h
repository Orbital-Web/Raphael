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
Tunable(TIME_FACTOR, 6, 1, 15, false);
Tunable(INC_FACTOR, 80, 50, 100, false);

Tunable(HARD_TIME_FACTOR, 200, 150, 250, false);
Tunable(SOFT_TIME_FACTOR, 70, 50, 100, false);

Tunable(MV_STAB_TM_MIN_DEPTH, 5, 3, 10, false);
Tunable(MV_STAB_TM_BASE, 120, 100, 300, false);
Tunable(MV_STAB_TM_MUL, 5, 0, 50, false);
Tunable(MV_STAB_TM_MIN, 80, 50, 100, false);

Tunable(SCORE_STAB_MARGIN, 10, 1, 50, false);
Tunable(SCORE_STAB_TM_MIN_DEPTH, 5, 3, 10, false);
Tunable(SCORE_STAB_TM_BASE, 120, 100, 300, false);
Tunable(SCORE_STAB_TM_MUL, 5, 0, 50, false);
Tunable(SCORE_STAB_TM_MIN, 80, 50, 100, false);

Tunable(NODE_TM_MIN_DEPTH, 5, 3, 10, false);
Tunable(NODE_TM_BASE, 200, 100, 300, false);
Tunable(NODE_TM_MUL, 150, 100, 200, false);

// search
Tunable(ASP_MIN_DEPTH, 3, 2, 5, false);
Tunable(ASP_INIT_SIZE, 51, 5, 100, true);
Tunable(ASP_WIDENING_FACTOR, 10, 2, 24, true);

Tunable(TT_REPL_DEPTH_MARGIN, 4, 0, 8, false);
Tunable(TT_VALUE_DEPTH_WEIGHT, 1, 0, 4, false);
Tunable(TT_VALUE_AGE_WEIGHT, 1, 0, 4, false);

// negamax
Tunable(IIR_MIN_DEPTH, 3, 3, 6, false);

Tunable(RFP_MAX_DEPTH, 6, 1, 10, false);
Tunable(RFP_MARGIN_DEPTH_MUL, 64, 16, 128, true);
Tunable(RFP_MARGIN_IMPROV_MUL, 58, 16, 128, true);

Tunable(RAZOR_MAX_DEPTH, 4, 1, 10, false);
Tunable(RAZOR_MARGIN_DEPTH_MUL, 213, 32, 384, true);
Tunable(RAZOR_MARGIN_BASE, 304, 32, 384, true);

Tunable(NMP_MIN_DEPTH, 3, 1, 10, false);
Tunable(NMP_RED_BASE, 403, 256, 1024, true);
Tunable(NMP_RED_DEPTH_MUL, 23, 8, 64, true);
Tunable(NMP_RED_EVAL_MUL, 52, 16, 128, true);
Tunable(NMP_RED_EVAL_MAX, 326, 128, 512, true);

inline MultiArray<i32, 2, 256> LMP_TABLE;  // lmp moves threshold[improving][depth]
TunableCallback(LMP_THRESH_BASE, 4, 1, 12, update_lmp_table, true);

Tunable(FP_MAX_DEPTH, 7, 4, 12, false);
Tunable(FP_MARGIN_DEPTH_MUL, 78, 32, 384, true);
Tunable(FP_MARGIN_BASE, 66, 32, 384, true);

Tunable(SEE_QUIET_DEPTH_MUL, -43, -128, -16, true);
Tunable(SEE_NOISY_DEPTH_MUL, -113, -256, -32, true);

Tunable(SE_MIN_DEPTH, 8, 6, 12, false);
Tunable(SE_MIN_TT_DEPTH, 3, 3, 6, false);
Tunable(SE_MARGIN_DEPTH_MUL, 15, 8, 64, true);
Tunable(DE_MARGIN, 34, 8, 64, true);

Tunable(LMR_MIN_DEPTH, 3, 1, 5, false);
Tunable(LMR_FROMMOVE, 5, 2, 8, false);
inline MultiArray<i32, 2, 256, 256> LMR_TABLE;  // lmr reduction[quiet][ply][move_searched]
TunableCallback(LMR_QUIET_BASE, 73, 32, 384, update_lmr_table, true);
TunableCallback(LMR_NOISY_BASE, -3, -128, 128, update_lmr_table, true);
TunableCallback(LMR_QUIET_DIV, 257, 128, 512, update_lmr_table, true);
TunableCallback(LMR_NOISY_DIV, 424, 128, 512, update_lmr_table, true);
Tunable(LMR_NONPV, 150, 32, 384, true);
Tunable(LMR_CUTNODE, 124, 32, 384, true);
Tunable(LMR_IMPROVING, 189, 32, 384, true);
Tunable(LMR_CHECK, 162, 32, 384, true);
Tunable(LMR_QUIET_HIST_DIV, 15090, 4096, 16384, true);
Tunable(LMR_NOISY_HIST_DIV, 11673, 4096, 16384, true);

// quiescence
Tunable(QS_MAX_MOVES, 3, 1, 5, false);
Tunable(QS_FP_MARGIN, 174, 32, 384, true);
Tunable(QS_SEE_THRESH, -112, -384, 32, true);

// SEE
inline MultiArray<i32, 13> SEE_TABLE;
TunableCallback(SEE_PAWN_VAL, 100, 100, 100, update_see_table, false);
TunableCallback(SEE_KNIGHT_VAL, 456, 200, 600, update_see_table, true);
TunableCallback(SEE_BISHOP_VAL, 383, 200, 600, update_see_table, true);
TunableCallback(SEE_ROOK_VAL, 714, 500, 900, update_see_table, true);
TunableCallback(SEE_QUEEN_VAL, 1442, 900, 1600, update_see_table, true);

// move ordering
static constexpr i32 HISTORY_MAX = 16384;
static constexpr i32 CAPTHIST_DIV = 8;

Tunable(GOOD_NOISY_SEE_BASE, 3, -128, 128, true);
Tunable(GOOD_NOISY_SEE_MUL, 30, 16, 128, true);

Tunable(HISTORY_BONUS_DEPTH_MUL, 202, 32, 384, true);
Tunable(HISTORY_BONUS_BASE, 114, 32, 384, true);
Tunable(HISTORY_BONUS_MAX, 2062, 1024, 4096, true);
Tunable(HISTORY_PENALTY_DEPTH_MUL, 241, 32, 384, true);
Tunable(HISTORY_PENALTY_BASE, 81, 32, 384, true);
Tunable(HISTORY_PENALTY_MAX, 2098, 1024, 4096, true);

Tunable(CONTHIST1_WEIGHT, 168, 32, 256, true);
Tunable(CONTHIST2_WEIGHT, 153, 32, 256, true);
Tunable(CONTHIST4_WEIGHT, 130, 32, 256, true);

Tunable(CAPTHIST_BONUS_DEPTH_MUL, 109, 32, 384, true);
Tunable(CAPTHIST_BONUS_BASE, 156, 32, 384, true);
Tunable(CAPTHIST_BONUS_MAX, 2437, 1024, 4096, true);
Tunable(CAPTHIST_PENALTY_DEPTH_MUL, 139, 32, 384, true);
Tunable(CAPTHIST_PENALTY_BASE, 214, 32, 384, true);
Tunable(CAPTHIST_PENALTY_MAX, 2403, 1024, 4096, true);

// corrections
static constexpr i32 CORRHIST_SIZE = 16384;
static constexpr i32 CORRHIST_MAX = 1024;
static constexpr i32 CORRHIST_BONUS_DEPTH_DIV = 8;
static constexpr i32 CORRHIST_BONUS_MAX = 256;

Tunable(PAWN_CORRHIST_WEIGHT, 86, 32, 384, true);
Tunable(MAJOR_CORRHIST_WEIGHT, 100, 32, 384, true);
Tunable(NONPAWN_CORRHIST_WEIGHT, 91, 32, 384, true);

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
