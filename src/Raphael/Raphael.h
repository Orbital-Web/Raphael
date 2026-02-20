#pragma once
#include <Raphael/History.h>
#include <Raphael/Transposition.h>
#include <Raphael/nnue.h>
#include <Raphael/tm.h>
#include <Raphael/tunable.h>

#include <atomic>
#include <string>



namespace raphael {
class Raphael {
public:
    static const std::string version;
    std::string name;

    struct EngineOptions {
        // uci options
        SpinOption<false> hash;
        SpinOption<false> threads;
        SpinOption<false> moveoverhead;

        // other options
        CheckOption datagen;
        CheckOption softnodes;
        SpinOption<false> softhardmult;
    };
    static const EngineOptions& default_params();

    enum class UciInfoLevel : u8 {
        NONE = 0,
        MINIMAL = 1,
        ALL = 2,
    };

    struct MoveScore {
        chess::Move move;
        i32 score;
        bool is_mate;
        u64 nodes = 0;
    };


private:
    // search
    EngineOptions params_;
    TimeManager::SearchOptions searchopt_;
    // storage
    TranspositionTable tt_;
    History history_;
    // position
    chess::Board board_;
    Nnue net_;
    // info
    UciInfoLevel ucilevel_ = UciInfoLevel::NONE;
    i32 seldepth_;  // maximum search depth reached in PV nodes
    TimeManager tm_;

    struct PVList {
        chess::Move moves[MAX_DEPTH] = {chess::Move::NO_MOVE};
        i32 length = 0;

        /** Updates the PV
         *
         * \param move move to add
         * \param child child PV to append
         */
        void update(const chess::Move move, const PVList& child);
    };

    struct SearchStack {
        PVList pv;
        i32 static_eval = 0;
        chess::Move move = chess::Move::NO_MOVE;
        chess::Move killer = chess::Move::NO_MOVE;
        chess::Move excluded = chess::Move::NO_MOVE;
    };

    struct MoveStack {
        chess::MoveList<chess::ScoredMove> movelist;
        chess::MoveList<chess::Move> quietlist;
        chess::MoveList<chess::Move> noisylist;
    };
    MoveStack movestack_[2 * MAX_DEPTH];



public:
    /** Initializes Raphael
     *
     * \param name_in player name
     */
    Raphael(const std::string& name_in);


    /** Sets Raphael's engine options
     *
     * \param name name of option to set
     * \param value value to set to
     */
    void set_option(const std::string& name, i32 value);
    void set_option(const std::string& name, bool value);

    /** Sets Raphael's search options
     *
     * \param options options to set to
     */
    void set_searchoptions(TimeManager::SearchOptions options);

    /** Sets Raphael's UCI info level
     *
     * \param level level to set to
     */
    void set_uciinfolevel(UciInfoLevel level);


    /** Sets the position to search on
     *
     * \param board current board
     */
    void set_board(const chess::Board& board);

    /** Returns the best move found by Raphael from the set position. Returns immediately if halt
     * becomes true. Will print out bestmove and search statistics if UCI is true.
     *
     * \param t_remain time remaining in ms
     * \param t_inc increment after move in ms
     * \param mouse unused
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns the best move and its score
     */
    MoveScore get_move(const i32 t_remain, const i32 t_inc, std::atomic<bool>& halt);

    /** Ponders for best move during opponent's turn. Returns immediately if halt becomes true
     *
     * \param halt bool reference which will turn false to indicate search should stop
     */
    void ponder(std::atomic<bool>& halt);


    /** Returns the static eval of the set position */
    i32 static_eval();


    /** Resets Raphael */
    void reset();

private:
    /** Prints out the uci info
     *
     * \param depth current depth
     * \param score score to print
     * \param search stack at current ply
     */
    void print_uci_info(i32 depth, i32 score, const SearchStack* ss) const;

    /** Returns the stringified PV line
     *
     * \param pv the PV to stringify
     * \returns the stringified PV line of the board
     */
    std::string get_pv_line(const PVList& pv) const;


    /** Recursively searches for the best move and score of the current position assuming optimal
     * play by both us and the opponent
     *
     * \tparam is_PV whether the current position is a PV node
     * \param depth depth to search for
     * \param ply current distance from root
     * \param mvidx movestack index
     * \param alpha lower bound score of current position
     * \param beta upper bound score of current position
     * \param cutnode whether the current position is a cutnode
     * \param ss search stack at current ply
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns score of current position
     */
    template <bool is_PV>
    i32 negamax(
        i32 depth,
        const i32 ply,
        const i32 mvidx,
        i32 alpha,
        i32 beta,
        bool cutnode,
        SearchStack* ss,
        std::atomic<bool>& halt
    );

    /** Evaluates the board after all noisy moves are played out
     *
     * \tparam is_PV whether the current position is a PV node
     * \param ply current distance from root
     * \param mvidx movestack index
     * \param alpha lower bound score of current position
     * \param beta upper bound score of current position
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns score of current board
     */
    template <bool is_PV>
    i32 quiescence(const i32 ply, const i32 mvidx, i32 alpha, i32 beta, std::atomic<bool>& halt);
};
}  // namespace raphael
