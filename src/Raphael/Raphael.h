#pragma once
#include <GameEngine/GamePlayer.h>
#include <Raphael/History.h>
#include <Raphael/Killers.h>
#include <Raphael/Transposition.h>
#include <Raphael/nnue.h>
#include <Raphael/options.h>

#include <chess.hpp>
#include <chrono>
#include <string>



namespace Raphael {
class RaphaelNNUE: public cge::GamePlayer {
public:
    static const std::string version;

    struct EngineOptions {
        // uci options
        SpinOption hash;

        // search
        static constexpr int ASPIRATION_WINDOW = 50;  // size of aspiration window

        // negamax
        static constexpr int RFP_DEPTH = 6;       // max depth to apply rfp from
        static constexpr int RFP_MARGIN = 77;     // depth margin scale for rfp
        static constexpr int NMP_DEPTH = 3;       // depth to apply nmp from
        static constexpr int NMP_REDUCTION = 4;   // depth reduction for nmp
        static constexpr int REDUCTION_FROM = 5;  // movei to apply lmr from

        // quiescence
        static constexpr int DELTA_THRESHOLD = 400;  // safety margin for delta pruning

        // move ordering
        static constexpr int GOOD_NOISY_FLOOR = 30000;  // good captures/promotions <=30500
        static constexpr int KILLER_FLOOR = 21000;      // killer moves
        static constexpr int BAD_NOISY_FLOOR = -20000;  // bad captures/promotions <=-19500

        static constexpr int HISTORY_BONUS_SCALE = 100;
        static constexpr int HISTORY_BONUS_OFFSET = 100;
        static constexpr int HISTORY_BONUS_MAX = 2000;
        static constexpr int HISTORY_MAX = 16384;
    };
    static EngineOptions params;

    struct SearchOptions {
        int64_t maxnodes = -1;
        int maxdepth = -1;
        int movetime = -1;
        int movestogo = 0;
        bool infinite = false;
    };

protected:
    // search
    SearchOptions searchopt;  // limit depth, nodes, or movetime
    // ponder FIXME:
    // uint64_t ponderkey = 0;  // hash after opponent's best response
    // int pondereval = 0;      // eval we got during ponder
    // int ponderdepth = 1;     // depth we searched to during ponder
    // storage
    TranspositionTable tt;  // table with position, eval, and bestmove
    Killers killers;        // 2 killer moves at each ply
    History history;        // history score for each move
    // nnue
    Nnue net;
    // info
    int64_t nodes;  // number of nodes visited
    int seldepth;   // maximum search depth reached
    // timing
    std::chrono::time_point<std::chrono::high_resolution_clock> start_t;  // search start time
    int64_t search_t;                                                     // search duration (ms)

    struct PVList {
        chess::Move moves[MAX_DEPTH] = {chess::Move::NO_MOVE};
        int length = 0;

        /** Updates the PV
         *
         * \param move move to add
         * \param child child PV to append
         */
        void update(const chess::Move move, const PVList& child);
    };

    struct SearchStack {
        PVList pv;
        chess::Move move = chess::Move::NO_MOVE;
        int static_eval = 0;
        // TODO: move killers here
    };



public:
    /** Initializes Raphael
     *
     * \param name_in player name
     */
    RaphaelNNUE(std::string name_in);


    /** Sets Raphael's engine options
     *
     * \param option option to set
     */
    void set_option(SetSpinOption option);

    /** Sets Raphael's search options
     *
     * \param options options to set to
     */
    void set_searchoptions(SearchOptions options);


    /** Returns the best move found by Raphael. Returns immediately if halt becomes true. Will print
     * out bestmove and search statistics if UCI is true.
     *
     * \param board current board
     * \param t_remain time remaining in ms
     * \param t_inc increment after move in ms
     * \param mouse unused
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns the best move it found
     */
    chess::Move get_move(
        chess::Board board,
        const int t_remain,
        const int t_inc,
        volatile cge::MouseInfo& mouse,
        volatile bool& halt
    );

    /** Ponders for best move during opponent's turn. Returns immediately if halt becomes true.
     *
     * \param board current board (for opponent)
     * \param halt bool reference which will turn false to indicate search should stop
     */
    void ponder(chess::Board board, volatile bool& halt);


    /** Resets Raphael */
    void reset();

protected:
    /** Estimates the time in ms Raphael should spent on searching a move, and sets search_t. Should
     * be called at the start before using is_time_over.
     *
     * \param board current board
     * \param t_remain remaining time in ms
     * \param t_inc increment after move in ms
     */
    void start_search_timer(const chess::Board& board, const int t_remain, const int t_inc);

    /** Sets and returns halt = true if search_t ms has passed. Will return false indefinetely if
     * search_t = 0.
     *
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns the new value of halt
     */
    bool is_time_over(volatile bool& halt) const;


    /** Returns the stringified PV line
     *
     * \param pv the PV to stringify
     * \returns the stringified PV line of the board
     */
    std::string get_pv_line(const PVList& pv) const;


    /** Recursively searches for the best move and eval of the current position assuming optimal
     * play by both us and the opponent
     *
     * \param board current board
     * \param depth depth to search for
     * \param ply current distance from root
     * \param alpha lower bound eval of current position
     * \param beta upper bound eval of current position
     * \param ss search stack at current ply
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns eval of current board
     */
    template <bool is_PV>
    int negamax(
        chess::Board& board,
        const int depth,
        const int ply,
        int alpha,
        int beta,
        SearchStack* ss,
        volatile bool& halt
    );

    /** Evaluates the board after all noisy moves are played out
     *
     * \param board current board
     * \param ply current distance from root
     * \param alpha lower bound eval of current position
     * \param beta upper bound eval of current position
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns eval of current board
     */
    int quiescence(chess::Board& board, const int ply, int alpha, int beta, volatile bool& halt);


    /** Sorts the movelist from best to worst
     *
     * \param movelist movelist to sort
     * \param ttmove transposition table move
     * \param board current board
     * \param ply current distance from root
     */
    void order_moves(
        chess::Movelist& movelist,
        const chess::Move& ttmove,
        const chess::Board& board,
        const int ply
    ) const;

    /** Assigns a score to a move
     *
     * \param move move to score
     * \param ttmove transposition table move
     * \param board current board
     * \param ply current distance from root
     */
    void score_move(
        chess::Move& move, const chess::Move& ttmove, const chess::Board& board, const int ply
    ) const;
};
};  // namespace Raphael
