#pragma once
#include <GameEngine/GamePlayer.h>
#include <Raphael/History.h>
#include <Raphael/Transposition.h>
#include <Raphael/nnue.h>
#include <Raphael/tunable.h>

#include <chess.hpp>
#include <chrono>
#include <string>



namespace Raphael {
class RaphaelNNUE: public cge::GamePlayer {
public:
    static const std::string version;

    struct EngineOptions {
        // uci options
        SpinOption<false> hash;

        // other options
        CheckOption softnodes;
        SpinOption<false> softhardmult;
    };
    static const EngineOptions default_params;

    struct SearchOptions {
        int64_t maxnodes = -1;
        int maxdepth = -1;
        int movetime = -1;
        int movestogo = 0;
        bool infinite = false;
    };

private:
    // search
    EngineOptions params;
    SearchOptions searchopt;  // limit depth, nodes, or movetime
    // ponder FIXME:
    // uint64_t ponderkey = 0;  // hash after opponent's best response
    // int pondereval = 0;      // eval we got during ponder
    // int ponderdepth = 1;     // depth we searched to during ponder
    // storage
    TranspositionTable tt;  // table with position, eval, and bestmove
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
        chess::Move killer = chess::Move::NO_MOVE;
        int static_eval = 0;
    };



public:
    /** Initializes Raphael
     *
     * \param name_in player name
     */
    RaphaelNNUE(std::string name_in);


    /** Sets Raphael's engine options
     *
     * \param name name of option to set
     * \param value value to set to
     */
    void set_option(const std::string& name, const int value);
    void set_option(const std::string& name, const bool value);

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
     * \returns the best move and its evaluation
     */
    MoveEval get_move(
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

private:
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


    /** Prints out the uci info
     *
     * \param depth current depth
     * \param eval evaluation to print
     * \param search stack at current ply
     */
    void print_uci_info(const int depth, const int eval, const SearchStack* ss) const;

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


    /** Assigns scores to a list of moves
     *
     * \param movelist movelist to score
     * \param ttmove transposition table move
     * \param board current board
     * \param ss search stack at current ply
     */
    void score_moves(
        chess::Movelist& movelist,
        const chess::Move& ttmove,
        const chess::Board& board,
        const SearchStack* ss
    ) const;

    /** Assigns scores to a list of quiet moves
     *
     * \param movelist movelist to score
     * \param board current board
     */
    void score_moves(chess::Movelist& movelist, const chess::Board& board) const;

    /** Picks the movei'th best move in the movelist
     *
     * \param movei index to pick
     * \param movelist movelist to pick from
     * \returns the chosen move in the movelist
     */
    chess::Move pick_move(const int movei, chess::Movelist& movelist);
};
};  // namespace Raphael
