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
Tunable(TIME_FACTOR, 61, 35, 85, true);
Tunable(INC_FACTOR, 804, 750, 850, true);

Tunable(HARD_TIME_FACTOR, 1985, 1750, 2250, true);
Tunable(SOFT_TIME_FACTOR, 748, 500, 1000, true);

Tunable(MV_STAB_TM_MIN_DEPTH, 5, 3, 10, false);
Tunable(MV_STAB_TM_BASE, 1181, 1000, 1500, true);
Tunable(MV_STAB_TM_MUL, 39, 0, 150, true);
Tunable(MV_STAB_TM_MIN, 780, 500, 1000, true);

Tunable(SCORE_STAB_MARGIN, 10, 1, 50, false);
Tunable(SCORE_STAB_TM_MIN_DEPTH, 5, 3, 10, false);
Tunable(SCORE_STAB_TM_BASE, 1159, 1000, 1500, true);
Tunable(SCORE_STAB_TM_MUL, 51, 0, 150, true);
Tunable(SCORE_STAB_TM_MIN, 813, 500, 1000, true);

Tunable(NODE_TM_MIN_DEPTH, 5, 3, 10, false);
Tunable(NODE_TM_BASE, 2024, 1750, 2250, true);
Tunable(NODE_TM_MUL, 1483, 1250, 1750, true);

// search
Tunable(ASP_MIN_DEPTH, 3, 2, 5, false);
Tunable(ASP_INIT_SIZE, 12, 5, 50, true);
Tunable(ASP_WIDENING_FACTOR, 87, 16, 128, true);
Tunable(ASP_RED, 148, 64, 256, true);
Tunable(ASP_MAX_RED, 404, 128, 640, true);

Tunable(TT_REPL_DEPTH_MARGIN, 512, 0, 1024, false);
Tunable(TT_REPL_PV_MARGIN, 128, 0, 768, false);
Tunable(TT_VALUE_DEPTH_WEIGHT, 128, 0, 512, false);
Tunable(TT_VALUE_AGE_WEIGHT, 256, 0, 512, false);

// negamax
Tunable(IIR_MIN_DEPTH, 396, 384, 768, true);
Tunable(IIR_RED, 118, 64, 256, true);
Tunable(HINDSIGHT_MIN_RED, 396, 384, 768, true);
Tunable(HINDSIGHT_EXT, 123, 64, 256, true);

Tunable(RFP_MAX_DEPTH, 923, 128, 1280, true);
Tunable(RFP_MARGIN_DEPTH_MUL, 46, 0, 128, true);
Tunable(RFP_MARGIN_IMPROVING, 35, 0, 64, true);
Tunable(RFP_MARGIN_OPP_WORSENING, 18, 0, 64, true);
Tunable(RFP_MARGIN_CORRPLEXITY, 113, 32, 384, true);

Tunable(RAZOR_MAX_DEPTH, 652, 128, 1280, true);
Tunable(RAZOR_MARGIN_DEPTH_MUL, 241, 32, 384, true);
Tunable(RAZOR_MARGIN_BASE, 279, 32, 384, true);

Tunable(NMP_MIN_DEPTH, 345, 128, 1280, true);
Tunable(NMP_MARGIN_DEPTH_MUL, 1417, 768, 1536, true);
Tunable(NMP_MARGIN_BASE, 122, 32, 384, true);
Tunable(NMP_RED_BASE, 399, 256, 1024, true);
Tunable(NMP_RED_DEPTH_MUL, 200, 64, 512, false);
Tunable(NMP_RED_EVAL_MUL, 82, 16, 128, false);
Tunable(NMP_RED_EVAL_MAX, 384, 128, 512, false);
Tunable(NMP_VERIF_MIN_DEPTH, 1920, 1280, 2560, false);
Tunable(NMP_VERIF_DEPTH_FACTOR, 96, 32, 128, false);

Tunable(PC_MIN_DEPTH, 814, 512, 1024, true);
Tunable(PC_MARGIN, 302, 128, 512, true);
Tunable(PC_RED, 433, 128, 512, true);
Tunable(PC_SEE_FACTOR, 135, 64, 256, true);

inline MultiArray<i32, 2, 256> LMP_TABLE;  // lmp moves threshold[improving][depth]
TunableCallback(LMP_THRESH_BASE, 3, 1, 12, update_lmp_table, false);

Tunable(FP_MAX_DEPTH, 868, 512, 1536, true);
Tunable(FP_MARGIN_DEPTH_MUL, 46, 32, 384, true);
Tunable(FP_MARGIN_BASE, 74, 32, 384, true);
Tunable(FP_MARGIN_HIST_MUL, 365, 64, 512, true);

Tunable(SEE_QUIET_DEPTH_MUL, -37, -128, -16, true);
Tunable(SEE_NOISY_DEPTH_MUL, -93, -256, -32, true);

Tunable(SE_MIN_DEPTH, 686, 512, 1536, true);
Tunable(SE_MIN_DEPTH_TTPV, 99, 32, 384, true);
Tunable(SE_MIN_TT_DEPTH, 394, 128, 512, true);
Tunable(SE_MARGIN_DEPTH_MUL, 91, 64, 512, true);
Tunable(DE_MARGIN_BASE, 33, 0, 50, true);
Tunable(DE_MARGIN_PV, 245, 200, 300, true);
Tunable(TE_MARGIN_BASE, 104, 50, 150, true);
Tunable(TE_MARGIN_PV, 720, 600, 800, true);
Tunable(SE_EXT, 170, 64, 256, true);
Tunable(DE_EXT, 143, 64, 256, true);
Tunable(TE_EXT, 86, 64, 256, true);
Tunable(NE_RED, 118, 64, 256, true);
Tunable(CUTNODE_NE_RED, 140, 64, 256, true);
Tunable(LDSE_MAX_DEPTH, 721, 256, 1024, true);
Tunable(LDSE_MARGIN_BASE, 28, 0, 50, true);
Tunable(LDSE_MARGIN_CORRPLEXITY_MUL, 96, 32, 128, true);
Tunable(LDSE_EXT, 124, 64, 256, true);

Tunable(LMR_MIN_DEPTH, 393, 128, 640, true);
Tunable(LMR_FROMMOVE, 5, 2, 8, false);
inline MultiArray<i32, 2, 256, 256> LMR_TABLE;  // lmr reduction[quiet][ply][move_searched]
TunableCallback(LMR_QUIET_BASE, 179, 32, 384, update_lmr_table, true);
TunableCallback(LMR_NOISY_BASE, -21, -128, 128, update_lmr_table, true);
TunableCallback(LMR_QUIET_DIV, 358, 128, 512, update_lmr_table, true);
TunableCallback(LMR_NOISY_DIV, 463, 128, 512, update_lmr_table, true);
Tunable(LMR_NONPV, 73, 32, 384, true);
Tunable(LMR_CUTNODE, 180, 32, 384, true);
Tunable(LMR_IMPROVING, 71, 32, 384, true);
Tunable(LMR_CHECK, 151, 32, 384, true);
Tunable(LMR_QUIET_HIST_DIV, 12282, 8192, 16384, true);
Tunable(LMR_NOISY_HIST_DIV, 11332, 8192, 16384, true);
Tunable(LMR_CORRPLEXITY_DIV, 178, 128, 1024, true);

Tunable(DO_DEEPER_BASE, 40, 0, 128, false);
Tunable(DO_DEEPER_DEPTH_MUL, 6, 1, 12, false);
Tunable(DO_SHALLOWER_BASE, 0, 0, 128, false);
Tunable(DO_SHALLOWER_DEPTH_MUL, 1, 1, 12, false);
Tunable(DO_DEEPER_EXT, 170, 64, 256, true);
Tunable(DO_SHALLOWER_RED, 125, 64, 256, true);

// quiescence
Tunable(QS_MAX_MOVES, 3, 1, 5, false);
Tunable(QS_FP_MARGIN, 165, 32, 384, true);
Tunable(QS_SEE_THRESH, -203, -384, 32, true);

// SEE
inline MultiArray<i32, 13> SEE_TABLE;
TunableCallback(SEE_PAWN_VAL, 100, 100, 100, update_see_table, false);
TunableCallback(SEE_KNIGHT_VAL, 461, 300, 600, update_see_table, true);
TunableCallback(SEE_BISHOP_VAL, 437, 300, 600, update_see_table, true);
TunableCallback(SEE_ROOK_VAL, 670, 500, 800, update_see_table, true);
TunableCallback(SEE_QUEEN_VAL, 1299, 900, 1500, update_see_table, true);

// move ordering
static constexpr i32 HISTORY_MAX = 16384;
static constexpr i32 CAPTHIST_DIV = 8;

Tunable(GOOD_NOISY_SEE_BASE, 13, -128, 128, true);
Tunable(GOOD_NOISY_SEE_MUL, 312, 16, 2048, true);

Tunable(DIRECT_CHECK_BONUS, 4647, 1024, 8192, true);

Tunable(HISTORY_BONUS_DEPTH_MUL, 155, 32, 384, true);
Tunable(HISTORY_BONUS_BASE, 94, 32, 384, true);
Tunable(HISTORY_BONUS_MAX, 1572, 1024, 4096, true);
Tunable(HISTORY_PENALTY_DEPTH_MUL, 103, 32, 384, true);
Tunable(HISTORY_PENALTY_BASE, 118, 32, 384, true);
Tunable(HISTORY_PENALTY_MAX, 1893, 1024, 4096, true);

Tunable(CONTHIST1_WEIGHT, 124, 32, 256, true);
Tunable(CONTHIST2_WEIGHT, 127, 32, 256, true);
Tunable(CONTHIST4_WEIGHT, 77, 32, 256, true);

Tunable(CAPTHIST_BONUS_DEPTH_MUL, 55, 32, 384, true);
Tunable(CAPTHIST_BONUS_BASE, 190, 32, 384, true);
Tunable(CAPTHIST_BONUS_MAX, 2501, 1024, 4096, true);
Tunable(CAPTHIST_PENALTY_DEPTH_MUL, 183, 32, 384, true);
Tunable(CAPTHIST_PENALTY_BASE, 93, 32, 384, true);
Tunable(CAPTHIST_PENALTY_MAX, 1292, 1024, 4096, true);

// corrections
static constexpr i32 CORRHIST_SIZE = 16384;
static constexpr i32 CORRHIST_MAX = 1024;
static constexpr i32 CORRHIST_BONUS_DEPTH_DIV = 8;
static constexpr i32 CORRHIST_BONUS_MAX = 256;

Tunable(PAWN_CORRHIST_WEIGHT, 41, 16, 128, true);
Tunable(MAJOR_CORRHIST_WEIGHT, 68, 16, 128, true);
Tunable(NONPAWN_CORRHIST_WEIGHT, 62, 16, 128, true);
Tunable(CONT1_CORRHIST_WEIGHT, 62, 16, 128, true);
Tunable(CONT2_CORRHIST_WEIGHT, 66, 16, 128, true);

// eval scaling
Tunable(MAT_SCALE_BASE, 25100, 20000, 30000, false);
Tunable(MAT_SCALE_PAWN, 110, 50, 200, false);
Tunable(MAT_SCALE_KNIGHT, 340, 200, 500, false);
Tunable(MAT_SCALE_BISHOP, 340, 200, 500, false);
Tunable(MAT_SCALE_ROOK, 590, 400, 800, false);
Tunable(MAT_SCALE_QUEEN, 970, 800, 1400, false);

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
