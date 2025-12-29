#pragma once
#include <GameEngine/GamePlayer.h>
#include <Raphael/History.h>
#include <Raphael/Killers.h>
#include <Raphael/Transposition.h>
#include <Raphael/nnue.h>

#include <chess.hpp>
#include <chrono>
#include <string>



namespace Raphael {
class RaphaelNNUE: public cge::GamePlayer {
public:
    static std::string version;

    struct EngineOptions {
        uint32_t tablesize = DEF_TABLE_SIZE;  // number of entries in tt
    };

    struct SearchOptions {
        int64_t maxnodes = -1;
        int maxdepth = -1;
        int movetime = -1;
        int movestogo = 0;
        bool infinite = false;
    };

protected:
    struct RaphaelParams {
        RaphaelParams();

        // logic
        static constexpr int ASPIRATION_WINDOW = 50;  // size of aspiration window
        static constexpr int MAX_EXTENSIONS = 16;  // max number of times we can extend the search
        static constexpr int REDUCTION_FROM = 5;   // from which move to apply late move reduction
        static constexpr int PV_STABLE_COUNT = 6;  // consecutive bestmoves to consider pv as stable
        static constexpr int MIN_SKIP_EVAL = 200;  // minimum eval to halt early if pv is stable

        // move ordering
        static constexpr int GOOD_NOISY_FLOOR = 30000;  // good captures/promotions <=30805
        static constexpr int KILLER_FLOOR = 21000;      // killer moves
        static constexpr int BAD_NOISY_FLOOR = -20000;  // bad captures/promotions <=-19195
    };

    // search
    chess::Move pvtable[MAX_DEPTH][MAX_DEPTH] = {{chess::Move::NO_MOVE}};
    int pvlens[MAX_DEPTH] = {0};
    SearchOptions searchopt;  // limit depth, nodes, or movetime
    RaphaelParams params;     // search parameters
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
    // timing
    std::chrono::time_point<std::chrono::high_resolution_clock> start_t;  // search start time
    int64_t search_t;                                                     // search duration (ms)



public:
    /** Initializes Raphael
     *
     * \param name_in player name
     */
    RaphaelNNUE(std::string name_in);

    /** Initializes Raphael
     *
     * \param name_in player name
     * \param options engine options, such as transposition table size
     */
    RaphaelNNUE(std::string name_in, EngineOptions options);


    /** Sets Raphael's engine options
     *
     * \param options options to set to
     */
    void set_options(EngineOptions options);

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


    /** Returns the PV line stored in the transposition table
     *
     * \returns the PV line of the board of length <= depth
     */
    std::string get_pv_line() const;


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


    /** Recursively searches for the best move and eval of the current position assuming optimal
     * play by both us and the opponent
     *
     * \param board current board
     * \param depth depth to search for
     * \param ply current distance from root
     * \param alpha lower bound eval of current position
     * \param beta upper bound eval of current position
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns eval of current board
     */
    int negamax(
        chess::Board& board,
        const int depth,
        const int ply,
        const int ext,
        int alpha,
        int beta,
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
