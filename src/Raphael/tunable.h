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



static constexpr i32 DEPTH_SCALE = 128;

// time management
Tunable(TIME_FACTOR, 59, 35, 85, true);
Tunable(INC_FACTOR, 804, 750, 850, true);

Tunable(HARD_TIME_FACTOR, 1983, 1750, 2250, true);
Tunable(SOFT_TIME_FACTOR, 727, 500, 1000, true);

Tunable(MV_STAB_TM_MIN_DEPTH, 5, 3, 10, false);
Tunable(MV_STAB_TM_BASE, 1169, 1000, 1400, true);
Tunable(MV_STAB_TM_MUL, 37, 0, 150, true);
Tunable(MV_STAB_TM_MIN, 798, 600, 1000, true);

Tunable(SCORE_STAB_MARGIN, 10, 1, 50, false);
Tunable(SCORE_STAB_TM_MIN_DEPTH, 5, 3, 10, false);
Tunable(SCORE_STAB_TM_BASE, 1179, 1000, 1400, true);
Tunable(SCORE_STAB_TM_MUL, 50, 0, 150, true);
Tunable(SCORE_STAB_TM_MIN, 779, 600, 1000, true);

Tunable(NODE_TM_MIN_DEPTH, 5, 3, 10, false);
Tunable(NODE_TM_BASE, 1998, 1750, 2250, true);
Tunable(NODE_TM_MUL, 1510, 1250, 1750, true);

// search
Tunable(ASP_MIN_DEPTH, 3, 2, 5, false);
Tunable(ASP_INIT_SIZE, 23, 5, 100, true);
Tunable(ASP_WIDENING_FACTOR, 80, 16, 192, true);
Tunable(ASP_RED, 132, 64, 256, true);
Tunable(ASP_MAX_RED, 378, 128, 640, true);

Tunable(TT_REPL_DEPTH_MARGIN, 512, 0, 1024, false);
Tunable(TT_REPL_PV_MARGIN, 128, 0, 768, false);
Tunable(TT_VALUE_DEPTH_WEIGHT, 128, 0, 512, false);
Tunable(TT_VALUE_AGE_WEIGHT, 256, 0, 512, false);

// negamax
Tunable(IIR_MIN_DEPTH, 404, 384, 768, true);
Tunable(IIR_RED, 127, 64, 256, true);
Tunable(HINDSIGHT_MIN_RED, 411, 384, 768, true);
Tunable(HINDSIGHT_EXT, 123, 64, 256, true);

Tunable(RFP_MAX_DEPTH, 921, 128, 1280, true);
Tunable(RFP_MARGIN_DEPTH_MUL, 44, 16, 128, true);
Tunable(RFP_MARGIN_IMPROVING, 29, 16, 128, true);
Tunable(RFP_MARGIN_CORRPLEXITY, 128, 32, 384, true);

Tunable(RAZOR_MAX_DEPTH, 602, 128, 1280, true);
Tunable(RAZOR_MARGIN_DEPTH_MUL, 243, 32, 384, true);
Tunable(RAZOR_MARGIN_BASE, 308, 32, 384, true);

Tunable(NMP_MIN_DEPTH, 226, 128, 1280, true);
Tunable(NMP_MARGIN_DEPTH_MUL, 1045, 1024, 4096, true);
Tunable(NMP_MARGIN_BASE, 110, 32, 384, true);
Tunable(NMP_RED_BASE, 426, 256, 1024, true);
Tunable(NMP_RED_DEPTH_MUL, 200, 64, 512, false);
Tunable(NMP_RED_EVAL_MUL, 82, 16, 128, false);
Tunable(NMP_RED_EVAL_MAX, 384, 128, 512, false);
Tunable(NMP_VERIF_MIN_DEPTH, 1920, 1280, 2560, false);
Tunable(NMP_VERIF_DEPTH_FACTOR, 96, 32, 128, false);

inline MultiArray<i32, 2, 256> LMP_TABLE;  // lmp moves threshold[improving][depth]
TunableCallback(LMP_THRESH_BASE, 3, 1, 12, update_lmp_table, false);

Tunable(FP_MAX_DEPTH, 892, 512, 1536, true);
Tunable(FP_MARGIN_DEPTH_MUL, 66, 32, 384, true);
Tunable(FP_MARGIN_BASE, 101, 32, 384, true);
Tunable(FP_MARGIN_HIST_MUL, 344, 64, 512, true);

Tunable(SEE_QUIET_DEPTH_MUL, -36, -128, -16, true);
Tunable(SEE_NOISY_DEPTH_MUL, -108, -256, -32, true);

Tunable(SE_MIN_DEPTH, 739, 512, 1536, true);
Tunable(SE_MIN_DEPTH_TTPV, 128, 32, 384, true);
Tunable(SE_MIN_TT_DEPTH, 384, 384, 768, false);
Tunable(SE_MARGIN_DEPTH_MUL, 128, 64, 512, false);
Tunable(DE_MARGIN_BASE, 30, 8, 64, false);
Tunable(DE_MARGIN_PV, 250, 32, 384, false);
Tunable(TE_MARGIN_BASE, 100, 32, 128, false);
Tunable(TE_MARGIN_PV, 700, 256, 1024, false);
Tunable(SE_EXT, 152, 64, 256, true);
Tunable(DE_EXT, 138, 64, 256, true);
Tunable(TE_EXT, 106, 64, 256, true);
Tunable(NE_RED, 136, 64, 256, true);
Tunable(CUTNODE_NE_RED, 128, 64, 256, true);
Tunable(LDSE_MAX_DEPTH, 768, 256, 1024, true);
Tunable(LDSE_MARGIN, 30, 8, 64, false);
Tunable(LDSE_EXT, 128, 64, 256, true);

Tunable(LMR_MIN_DEPTH, 372, 128, 640, true);
Tunable(LMR_FROMMOVE, 5, 2, 8, false);
inline MultiArray<i32, 2, 256, 256> LMR_TABLE;  // lmr reduction[quiet][ply][move_searched]
TunableCallback(LMR_QUIET_BASE, 173, 32, 384, update_lmr_table, true);
TunableCallback(LMR_NOISY_BASE, -29, -128, 128, update_lmr_table, true);
TunableCallback(LMR_QUIET_DIV, 337, 128, 512, update_lmr_table, true);
TunableCallback(LMR_NOISY_DIV, 460, 128, 512, update_lmr_table, true);
Tunable(LMR_NONPV, 84, 32, 384, true);
Tunable(LMR_CUTNODE, 163, 32, 384, true);
Tunable(LMR_IMPROVING, 84, 32, 384, true);
Tunable(LMR_CHECK, 154, 32, 384, true);
Tunable(LMR_QUIET_HIST_DIV, 11520, 4096, 16384, true);
Tunable(LMR_NOISY_HIST_DIV, 9709, 4096, 16384, true);
Tunable(LMR_CORRPLEXITY_DIV, 300, 128, 1024, true);

Tunable(DO_DEEPER_BASE, 40, 0, 128, false);
Tunable(DO_DEEPER_DEPTH_MUL, 6, 1, 12, false);
Tunable(DO_SHALLOWER_BASE, 0, 0, 128, false);
Tunable(DO_SHALLOWER_DEPTH_MUL, 1, 1, 12, false);
Tunable(DO_DEEPER_EXT, 157, 64, 256, true);
Tunable(DO_SHALLOWER_RED, 136, 64, 256, true);

// quiescence
Tunable(QS_MAX_MOVES, 3, 1, 5, false);
Tunable(QS_FP_MARGIN, 158, 32, 384, true);
Tunable(QS_SEE_THRESH, -182, -384, 32, true);

// SEE
inline MultiArray<i32, 13> SEE_TABLE;
TunableCallback(SEE_PAWN_VAL, 100, 100, 100, update_see_table, false);
TunableCallback(SEE_KNIGHT_VAL, 463, 300, 600, update_see_table, true);
TunableCallback(SEE_BISHOP_VAL, 415, 300, 600, update_see_table, true);
TunableCallback(SEE_ROOK_VAL, 681, 500, 800, update_see_table, true);
TunableCallback(SEE_QUEEN_VAL, 1339, 900, 1500, update_see_table, true);

// move ordering
static constexpr i32 HISTORY_MAX = 16384;
static constexpr i32 CAPTHIST_DIV = 8;

Tunable(GOOD_NOISY_SEE_BASE, -28, -128, 128, true);
Tunable(GOOD_NOISY_SEE_MUL, 307, 16, 2048, true);

Tunable(DIRECT_CHECK_BONUS, 5240, 1024, 8192, true);

Tunable(HISTORY_BONUS_DEPTH_MUL, 160, 32, 384, true);
Tunable(HISTORY_BONUS_BASE, 106, 32, 384, true);
Tunable(HISTORY_BONUS_MAX, 1803, 1024, 4096, true);
Tunable(HISTORY_PENALTY_DEPTH_MUL, 92, 32, 384, true);
Tunable(HISTORY_PENALTY_BASE, 105, 32, 384, true);
Tunable(HISTORY_PENALTY_MAX, 1739, 1024, 4096, true);

Tunable(CONTHIST1_WEIGHT, 136, 32, 256, true);
Tunable(CONTHIST2_WEIGHT, 116, 32, 256, true);
Tunable(CONTHIST4_WEIGHT, 69, 32, 256, true);

Tunable(CAPTHIST_BONUS_DEPTH_MUL, 56, 32, 384, true);
Tunable(CAPTHIST_BONUS_BASE, 159, 32, 384, true);
Tunable(CAPTHIST_BONUS_MAX, 2246, 1024, 4096, true);
Tunable(CAPTHIST_PENALTY_DEPTH_MUL, 167, 32, 384, true);
Tunable(CAPTHIST_PENALTY_BASE, 79, 32, 384, true);
Tunable(CAPTHIST_PENALTY_MAX, 1414, 1024, 4096, true);

// corrections
static constexpr i32 CORRHIST_SIZE = 16384;
static constexpr i32 CORRHIST_MAX = 1024;
static constexpr i32 CORRHIST_BONUS_DEPTH_DIV = 8;
static constexpr i32 CORRHIST_BONUS_MAX = 256;

Tunable(PAWN_CORRHIST_WEIGHT, 50, 32, 384, true);
Tunable(MAJOR_CORRHIST_WEIGHT, 41, 32, 384, true);
Tunable(NONPAWN_CORRHIST_WEIGHT, 39, 32, 384, true);
Tunable(CONT1_CORRHIST_WEIGHT, 63, 32, 384, true);
Tunable(CONT2_CORRHIST_WEIGHT, 48, 32, 384, true);

// eval scaling
Tunable(MAT_SCALE_BASE, 25100, 20000, 30000, false);
Tunable(MAT_SCALE_PAWN, 110, 0, 200, false);
Tunable(MAT_SCALE_KNIGHT, 340, 200, 600, false);
Tunable(MAT_SCALE_BISHOP, 340, 200, 600, false);
Tunable(MAT_SCALE_ROOK, 590, 400, 900, false);
Tunable(MAT_SCALE_QUEEN, 970, 800, 1600, false);

// commands
#ifndef MEASURE_SPARSITY
static constexpr i32 BENCH_DEPTH = 14;
#else
static constexpr i32 BENCH_DEPTH = 15;
#endif

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
