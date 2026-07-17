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
    [[nodiscard]] operator i32() const { return value; }

    /** Returns the step size (i.e., C_end in OB)
     *
     * \returns step size
     */
    [[nodiscard]] f32 step_size() const { return f32(max_val - min_val) / 20; }

    /** Returns the learning rate (i.e., R_end in OB)
     *
     * \returns learning rate
     */
    [[nodiscard]] f32 learning_rate() const { return 0.002 / (std::min(0.5f, step_size()) / 0.5); }


    /** Sets a callback for the option
     *
     * \param cb function to call when the option is set
     */
    void set_callback(SpinOptionCB cb) { callback = cb; }


    /** Returns the UCI option info string
     *
     * \returns stringified option info
     */
    [[nodiscard]] std::string uci() const {
        return "option name " + name + " type spin default " + std::to_string(def) + " min "
               + std::to_string(min_val) + " max " + std::to_string(max_val) + "\n";
    }

    /** Returns the OB SPSA tuning input string
     *
     * \returns stringified option tuning info
     */
    [[nodiscard]] std::string ob() const {
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
    [[nodiscard]] operator bool() const { return value; }

    /** Returns the UCI option info string
     *
     * \returns stringified option info
     */
    [[nodiscard]] std::string uci() const {
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
[[nodiscard]] inline bool set_tunable(const std::string& name, i32 value) {
    for (const auto& tunable : tunables) {
        if (utils::is_case_insensitive_equals(tunable->name, name)) {
            // assume value is valid
            tunable->set(value);
            return true;
        }
    }
    return false;
}

// clang-format off
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
// clang-format on
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
Tunable(TIME_FACTOR, 66, 35, 85, true);
Tunable(INC_FACTOR, 803, 750, 850, true);

Tunable(HARD_TIME_FACTOR, 2028, 1750, 2250, true);
Tunable(SOFT_TIME_FACTOR, 818, 500, 1000, true);

Tunable(MV_STAB_TM_MIN_DEPTH, 5, 3, 10, false);
Tunable(MV_STAB_TM_BASE, 1236, 1000, 1500, true);
Tunable(MV_STAB_TM_MUL, 42, 0, 150, true);
Tunable(MV_STAB_TM_MIN, 790, 500, 1000, true);

Tunable(SCORE_STAB_MARGIN, 10, 1, 50, false);
Tunable(SCORE_STAB_TM_MIN_DEPTH, 5, 3, 10, false);
Tunable(SCORE_STAB_TM_BASE, 1143, 1000, 1500, true);
Tunable(SCORE_STAB_TM_MUL, 63, 0, 150, true);
Tunable(SCORE_STAB_TM_MIN, 769, 500, 1000, true);

Tunable(NODE_TM_MIN_DEPTH, 5, 3, 10, false);
Tunable(NODE_TM_BASE, 2041, 1750, 2250, true);
Tunable(NODE_TM_MUL, 1513, 1250, 1750, true);

// search
Tunable(BM_SCORE_OFFSET, 10, 0, 20, false);

Tunable(ASP_MIN_DEPTH, 3, 2, 5, false);
Tunable(ASP_INIT_SIZE, 13, 5, 50, true);
Tunable(ASP_WIDENING_FACTOR, 94, 16, 128, true);
Tunable(ASP_RED, 161, 64, 256, true);
Tunable(ASP_MAX_RED, 383, 128, 640, true);

Tunable(TT_REPL_DEPTH_MARGIN, 512, 0, 1024, false);
Tunable(TT_REPL_PV_MARGIN, 128, 0, 768, false);
Tunable(TT_VALUE_DEPTH_WEIGHT, 128, 0, 512, false);
Tunable(TT_VALUE_AGE_WEIGHT, 256, 0, 512, false);

// negamax
Tunable(IIR_MIN_DEPTH, 398, 384, 768, true);
Tunable(IIR_RED, 151, 64, 256, true);
Tunable(HINDSIGHT_MIN_RED, 423, 384, 768, true);
Tunable(HINDSIGHT_EXT, 149, 64, 256, true);

Tunable(RFP_MAX_DEPTH, 983, 128, 1280, true);
Tunable(RFP_MARGIN_DEPTH_MUL, 36, 0, 128, true);
Tunable(RFP_MARGIN_IMPROVING, 35, 0, 64, true);
Tunable(RFP_MARGIN_OPP_WORSENING, 18, 0, 64, true);
Tunable(RFP_MARGIN_CORRPLEXITY, 103, 32, 384, true);

Tunable(RAZOR_MAX_DEPTH, 684, 128, 1280, true);
Tunable(RAZOR_MARGIN_DEPTH_MUL, 217, 32, 384, true);
Tunable(RAZOR_MARGIN_BASE, 287, 32, 384, true);

Tunable(NMP_MIN_DEPTH, 272, 128, 1280, true);
Tunable(NMP_MARGIN_DEPTH_MUL, 1476, 768, 1536, true);
Tunable(NMP_MARGIN_BASE, 129, 32, 384, true);
Tunable(NMP_RED_BASE, 424, 256, 1024, true);
Tunable(NMP_RED_DEPTH_MUL, 200, 64, 512, false);
Tunable(NMP_RED_EVAL_MUL, 82, 16, 128, false);
Tunable(NMP_RED_EVAL_MAX, 384, 128, 512, false);
Tunable(NMP_VERIF_MIN_DEPTH, 1920, 1280, 2560, false);
Tunable(NMP_VERIF_DEPTH_FACTOR, 96, 32, 128, false);

Tunable(PC_MIN_DEPTH, 841, 512, 1024, true);
Tunable(PC_MARGIN, 263, 128, 512, true);
Tunable(PC_RED, 435, 128, 512, true);
Tunable(PC_SEE_FACTOR, 130, 64, 256, true);

inline MultiArray<i32, 2, 256> LMP_TABLE;  // lmp moves threshold[improving][depth]
TunableCallback(LMP_THRESH_BASE, 3, 1, 12, update_lmp_table, false);
Tunable(LMP_HIST_MUL, 527, 256, 1024, true);

Tunable(FP_MAX_DEPTH, 892, 512, 1536, true);
Tunable(FP_MARGIN_DEPTH_MUL, 36, 32, 384, true);
Tunable(FP_MARGIN_BASE, 70, 32, 384, true);
Tunable(FP_MARGIN_HIST_MUL, 407, 64, 512, true);

Tunable(SEE_QUIET_DEPTH_MUL, -25, -128, -16, true);
Tunable(SEE_NOISY_DEPTH_MUL, -104, -256, -32, true);

Tunable(SE_MIN_DEPTH, 606, 512, 1536, true);
Tunable(SE_MIN_DEPTH_TTPV, 81, 32, 384, true);
Tunable(SE_MIN_TT_DEPTH, 356, 128, 512, true);
Tunable(SE_MARGIN_DEPTH_MUL, 93, 64, 512, true);
Tunable(DE_MARGIN_BASE, 27, 0, 50, true);
Tunable(DE_MARGIN_PV, 247, 200, 300, true);
Tunable(TE_MARGIN_BASE, 106, 50, 150, true);
Tunable(TE_MARGIN_PV, 730, 600, 800, true);
Tunable(SE_EXT, 194, 64, 256, true);
Tunable(DE_EXT, 138, 64, 256, true);
Tunable(TE_EXT, 82, 64, 256, true);
Tunable(NE_RED, 119, 64, 256, true);
Tunable(CUTNODE_NE_RED, 136, 64, 256, true);
Tunable(LDSE_MAX_DEPTH, 698, 256, 1024, true);
Tunable(LDSE_MARGIN_BASE, 23, 0, 50, true);
Tunable(LDSE_MARGIN_CORRPLEXITY_MUL, 105, 32, 128, true);
Tunable(LDSE_EXT, 126, 64, 256, true);

Tunable(LMR_MIN_DEPTH, 426, 128, 640, true);
Tunable(LMR_FROMMOVE, 5, 2, 8, false);
inline MultiArray<i32, 2, 256, 256> LMR_TABLE;  // lmr reduction[quiet][ply][move_searched]
TunableCallback(LMR_QUIET_BASE, 150, 32, 384, update_lmr_table, true);
TunableCallback(LMR_NOISY_BASE, 3, -128, 128, update_lmr_table, true);
TunableCallback(LMR_QUIET_DIV, 364, 128, 512, update_lmr_table, true);
TunableCallback(LMR_NOISY_DIV, 508, 128, 512, update_lmr_table, true);
Tunable(LMR_NONPV, 39, 32, 384, true);
Tunable(LMR_CUTNODE, 211, 32, 384, true);
Tunable(LMR_IMPROVING, 123, 32, 384, true);
Tunable(LMR_CHECK, 167, 32, 384, true);
Tunable(LMR_QUIET_HIST_DIV, 12175, 8192, 16384, true);
Tunable(LMR_NOISY_HIST_DIV, 11077, 8192, 16384, true);
Tunable(LMR_CORRPLEXITY_DIV, 245, 128, 1024, true);

Tunable(DO_DEEPER_BASE, 40, 0, 128, false);
Tunable(DO_DEEPER_DEPTH_MUL, 6, 1, 12, false);
Tunable(DO_SHALLOWER_BASE, 0, 0, 128, false);
Tunable(DO_SHALLOWER_DEPTH_MUL, 1, 1, 12, false);
Tunable(DO_DEEPER_EXT, 157, 64, 256, true);
Tunable(DO_SHALLOWER_RED, 135, 64, 256, true);

// quiescence
Tunable(QS_MAX_MOVES, 3, 1, 5, false);
Tunable(QS_FP_MARGIN, 150, 32, 384, true);
Tunable(QS_SEE_THRESH, -167, -384, 32, true);

// SEE
inline MultiArray<i32, 13> SEE_TABLE;
TunableCallback(SEE_PAWN_VAL, 100, 100, 100, update_see_table, false);
TunableCallback(SEE_KNIGHT_VAL, 472, 300, 600, update_see_table, true);
TunableCallback(SEE_BISHOP_VAL, 435, 300, 600, update_see_table, true);
TunableCallback(SEE_ROOK_VAL, 682, 500, 800, update_see_table, true);
TunableCallback(SEE_QUEEN_VAL, 1276, 900, 1500, update_see_table, true);

// move ordering
static constexpr i32 HISTORY_MAX = 16384;
static constexpr i32 PAWNHIST_SIZE = 512;
static constexpr i32 CAPTHIST_DIV = 8;

Tunable(GOOD_NOISY_SEE_BASE, 3, -128, 128, true);
Tunable(GOOD_NOISY_SEE_MUL, 389, 16, 2048, true);

Tunable(DIRECT_CHECK_BONUS, 5257, 1024, 8192, true);

Tunable(HISTORY_BONUS_DEPTH_MUL, 159, 32, 384, true);
Tunable(HISTORY_BONUS_BASE, 101, 32, 384, true);
Tunable(HISTORY_BONUS_MAX, 1818, 1024, 4096, true);
Tunable(HISTORY_PENALTY_DEPTH_MUL, 112, 32, 384, true);
Tunable(HISTORY_PENALTY_BASE, 155, 32, 384, true);
Tunable(HISTORY_PENALTY_MAX, 1710, 1024, 4096, true);

Tunable(BUTTERFLY_WEIGHT, 120, 32, 256, true);
Tunable(PAWNHIST_WEIGHT, 72, 32, 256, true);
Tunable(CONTHIST1_WEIGHT, 127, 32, 256, true);
Tunable(CONTHIST2_WEIGHT, 121, 32, 256, true);
Tunable(CONTHIST4_WEIGHT, 89, 32, 256, true);

Tunable(CAPTHIST_BONUS_DEPTH_MUL, 60, 32, 384, true);
Tunable(CAPTHIST_BONUS_BASE, 204, 32, 384, true);
Tunable(CAPTHIST_BONUS_MAX, 2282, 1024, 4096, true);
Tunable(CAPTHIST_PENALTY_DEPTH_MUL, 169, 32, 384, true);
Tunable(CAPTHIST_PENALTY_BASE, 57, 32, 384, true);
Tunable(CAPTHIST_PENALTY_MAX, 1368, 1024, 4096, true);

// corrections
static constexpr i32 CORRHIST_SIZE = 16384;
static constexpr i32 CORRHIST_MAX = 1024;
static constexpr i32 CORRHIST_BONUS_DEPTH_DIV = 8;
static constexpr i32 CORRHIST_BONUS_MAX = 256;

Tunable(PAWN_CORRHIST_WEIGHT, 41, 16, 128, true);
Tunable(MAJOR_CORRHIST_WEIGHT, 59, 16, 128, true);
Tunable(NONPAWN_CORRHIST_WEIGHT, 61, 16, 128, true);
Tunable(CONT1_CORRHIST_WEIGHT, 71, 16, 128, true);
Tunable(CONT2_CORRHIST_WEIGHT, 56, 16, 128, true);

// eval scaling
Tunable(MAT_SCALE_BASE, 25100, 20000, 30000, false);
Tunable(MAT_SCALE_PAWN, 110, 50, 200, false);
Tunable(MAT_SCALE_KNIGHT, 340, 200, 500, false);
Tunable(MAT_SCALE_BISHOP, 340, 200, 500, false);
Tunable(MAT_SCALE_ROOK, 590, 400, 800, false);
Tunable(MAT_SCALE_QUEEN, 970, 800, 1400, false);

Tunable(OPT_SCALE_BASE, 2000, 1000, 4000, false);
Tunable(OPT_MAX_BONUS, 150, 75, 300, false);
Tunable(OPT_STRETCH, 100, 50, 200, false);

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
