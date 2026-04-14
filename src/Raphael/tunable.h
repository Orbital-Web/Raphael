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
Tunable(ASP_INIT_SIZE, 49, 5, 100, true);
Tunable(ASP_WIDENING_FACTOR, 13, 2, 24, true);

Tunable(TT_REPL_DEPTH_MARGIN, 4, 0, 8, false);
Tunable(TT_VALUE_DEPTH_WEIGHT, 1, 0, 4, false);
Tunable(TT_VALUE_AGE_WEIGHT, 1, 0, 4, false);

// negamax
Tunable(IIR_MIN_DEPTH, 3, 3, 6, false);
Tunable(HINDSIGHT_MIN_RED, 3, 3, 6, false);

Tunable(RFP_MAX_DEPTH, 6, 1, 10, false);
Tunable(RFP_MARGIN_DEPTH_MUL, 65, 16, 128, true);
Tunable(RFP_MARGIN_IMPROVING, 40, 16, 128, true);

Tunable(RAZOR_MAX_DEPTH, 4, 1, 10, false);
Tunable(RAZOR_MARGIN_DEPTH_MUL, 249, 32, 384, true);
Tunable(RAZOR_MARGIN_BASE, 304, 32, 384, true);

Tunable(NMP_MIN_DEPTH, 3, 1, 10, false);
Tunable(NMP_MARGIN_DEPTH_MUL, 1280, 1024, 4096, false);
Tunable(NMP_MARGIN_BASE, 100, 32, 384, false);
Tunable(NMP_RED_BASE, 531, 256, 1024, true);
Tunable(NMP_RED_DEPTH_MUL, 25, 8, 64, false);
Tunable(NMP_RED_EVAL_MUL, 82, 16, 128, false);
Tunable(NMP_RED_EVAL_MAX, 384, 128, 512, false);
Tunable(NMP_VERIF_MIN_DEPTH, 15, 10, 20, false);
Tunable(NMP_VERIF_DEPTH_FACTOR, 96, 32, 128, false);

inline MultiArray<i32, 2, 256> LMP_TABLE;  // lmp moves threshold[improving][depth]
TunableCallback(LMP_THRESH_BASE, 3, 1, 12, update_lmp_table, false);

Tunable(FP_MAX_DEPTH, 7, 4, 12, false);
Tunable(FP_MARGIN_DEPTH_MUL, 85, 32, 384, true);
Tunable(FP_MARGIN_BASE, 106, 32, 384, true);

Tunable(SEE_QUIET_DEPTH_MUL, -27, -128, -16, true);
Tunable(SEE_NOISY_DEPTH_MUL, -104, -256, -32, true);

Tunable(SE_MIN_DEPTH, 8, 6, 12, false);
Tunable(SE_MIN_TT_DEPTH, 3, 3, 6, false);
Tunable(SE_MARGIN_DEPTH_MUL, 16, 8, 64, false);
Tunable(DE_MARGIN, 30, 8, 64, false);
Tunable(TE_MARGIN, 100, 32, 128, false);

Tunable(LMR_MIN_DEPTH, 3, 1, 5, false);
Tunable(LMR_FROMMOVE, 5, 2, 8, false);
inline MultiArray<i32, 2, 256, 256> LMR_TABLE;  // lmr reduction[quiet][ply][move_searched]
TunableCallback(LMR_QUIET_BASE, 177, 32, 384, update_lmr_table, true);
TunableCallback(LMR_NOISY_BASE, -21, -128, 128, update_lmr_table, true);
TunableCallback(LMR_QUIET_DIV, 353, 128, 512, update_lmr_table, true);
TunableCallback(LMR_NOISY_DIV, 403, 128, 512, update_lmr_table, true);
Tunable(LMR_NONPV, 137, 32, 384, true);
Tunable(LMR_CUTNODE, 144, 32, 384, true);
Tunable(LMR_IMPROVING, 115, 32, 384, true);
Tunable(LMR_CHECK, 163, 32, 384, true);
Tunable(LMR_QUIET_HIST_DIV, 11863, 4096, 16384, true);
Tunable(LMR_NOISY_HIST_DIV, 12298, 4096, 16384, true);
Tunable(DO_DEEPER_BASE, 40, 0, 128, false);
Tunable(DO_DEEPER_DEPTH_MUL, 6, 1, 12, false);
Tunable(DO_SHALLOWER_BASE, 0, 0, 128, false);
Tunable(DO_SHALLOWER_DEPTH_MUL, 1, 1, 12, false);

// quiescence
Tunable(QS_MAX_MOVES, 3, 1, 5, false);
Tunable(QS_FP_MARGIN, 146, 32, 384, true);
Tunable(QS_SEE_THRESH, -115, -384, 32, true);

// SEE
inline MultiArray<i32, 13> SEE_TABLE;
TunableCallback(SEE_PAWN_VAL, 100, 100, 100, update_see_table, false);
TunableCallback(SEE_KNIGHT_VAL, 432, 300, 600, update_see_table, true);
TunableCallback(SEE_BISHOP_VAL, 432, 300, 600, update_see_table, true);
TunableCallback(SEE_ROOK_VAL, 698, 500, 800, update_see_table, true);
TunableCallback(SEE_QUEEN_VAL, 1301, 900, 1500, update_see_table, true);

// move ordering
static constexpr i32 HISTORY_MAX = 16384;
static constexpr i32 CAPTHIST_DIV = 8;

Tunable(GOOD_NOISY_SEE_BASE, -22, -128, 128, true);
Tunable(GOOD_NOISY_SEE_MUL, 16, 16, 128, false);

Tunable(DIRECT_CHECK_BONUS, 4096, 1024, 8192, true);

Tunable(HISTORY_BONUS_DEPTH_MUL, 103, 32, 384, true);
Tunable(HISTORY_BONUS_BASE, 96, 32, 384, true);
Tunable(HISTORY_BONUS_MAX, 1909, 1024, 4096, true);
Tunable(HISTORY_PENALTY_DEPTH_MUL, 79, 32, 384, true);
Tunable(HISTORY_PENALTY_BASE, 111, 32, 384, true);
Tunable(HISTORY_PENALTY_MAX, 1980, 1024, 4096, true);

Tunable(CONTHIST1_WEIGHT, 133, 32, 256, true);
Tunable(CONTHIST2_WEIGHT, 123, 32, 256, true);
Tunable(CONTHIST4_WEIGHT, 65, 32, 256, true);

Tunable(CAPTHIST_BONUS_DEPTH_MUL, 104, 32, 384, true);
Tunable(CAPTHIST_BONUS_BASE, 111, 32, 384, true);
Tunable(CAPTHIST_BONUS_MAX, 2064, 1024, 4096, true);
Tunable(CAPTHIST_PENALTY_DEPTH_MUL, 129, 32, 384, true);
Tunable(CAPTHIST_PENALTY_BASE, 113, 32, 384, true);
Tunable(CAPTHIST_PENALTY_MAX, 1866, 1024, 4096, true);

// corrections
static constexpr i32 CORRHIST_SIZE = 16384;
static constexpr i32 CORRHIST_MAX = 1024;
static constexpr i32 CORRHIST_BONUS_DEPTH_DIV = 8;
static constexpr i32 CORRHIST_BONUS_MAX = 256;

Tunable(PAWN_CORRHIST_WEIGHT, 56, 32, 384, true);
Tunable(MAJOR_CORRHIST_WEIGHT, 48, 32, 384, true);
Tunable(NONPAWN_CORRHIST_WEIGHT, 48, 32, 384, true);
Tunable(CONT1_CORRHIST_WEIGHT, 64, 32, 384, true);

// eval scaling
Tunable(MAT_SCALE_BASE, 25100, 20000, 30000, true);
Tunable(MAT_SCALE_PAWN, 110, 0, 200, true);
Tunable(MAT_SCALE_KNIGHT, 340, 200, 600, true);
Tunable(MAT_SCALE_BISHOP, 340, 200, 600, true);
Tunable(MAT_SCALE_ROOK, 590, 400, 900, true);
Tunable(MAT_SCALE_QUEEN, 970, 800, 1600, true);

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
