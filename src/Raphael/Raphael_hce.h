#pragma once
#include <GameEngine/GamePlayer.h>
#include <Raphael/History.h>
#include <Raphael/Killers.h>
#include <Raphael/Transposition.h>

#include <chess.hpp>
#include <chrono>
#include <string>



namespace Raphael {
class RaphaelHCE: public cge::GamePlayer {
public:
    static std::string version;

    struct EngineOptions {
        uint32_t tablesize = DEF_TABLE_SIZE;  // number of entries in tt
    };

    struct SearchOptions {
        int64_t maxnodes = -1;
        int64_t maxnodes_soft = -1;
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
        static constexpr int KILLER_WEIGHT = 50;          // move priority for killer moves
        static constexpr int GOOD_CAPTURE_WEIGHT = 5000;  // move priority for good captures

        // evaluation[midgame, endgame]
        static constexpr int KING_DIST_WEIGHT[2] = {0, 20};  // closer king bonus
        static constexpr int DRAW_DIVIDE_SCALE = 32;         // eval divide scale by for likely draw
        static constexpr int PVAL[12][2] = {
            // WHITE
            {100,   100 }, // PAWN
            {418,   246 }, // KNIGHT
            {449,   274 }, // BISHOP
            {554,   437 }, // ROOK
            {1191,  727 }, // QUEEN
            {0,     0   }, // KING
            // BLACK
            {-100,  -100}, // PAWN
            {-418,  -246}, // KNIGHT
            {-449,  -274}, // BISHOP
            {-554,  -437}, // ROOK
            {-1191, -727}, // QUEEN
            {0,     0   }, // KING
        };  // value of each piece
        int PST[12][64][2];  // piece square table for piece, square, and phase
        static constexpr int PAWN_PASSED_WEIGHT[7][2] = {
            {0,   0  }, // promotion line
            {114, 215},
            {10,  160},
            {4,   77 },
            {-12, 47 },
            {1,   20 },
            {15,  13 },
        };  // bonus for passed pawn based on its rank
        static constexpr int PAWN_ISOLATION_WEIGHT[2] = {29, 21};  // isolated pawn cost
        static constexpr int MOBILITY_BISHOP[2] = {12, 6};     // bishop see/xray square count bonus
        static constexpr int MOBILITY_ROOK[2] = {11, 3};       // rook see/xray square count bonus
        static constexpr int BISH_PAIR_WEIGHT[2] = {39, 72};   // bishop pair bonus
        static constexpr int BISH_CORNER_WEIGHT[2] = {1, 20};  // enemy king to bishop corner bonus

        /** Initializes the PST value for piece, square, and phase (mid/end game) */
        void init_pst();
    };

    // search
    chess::Move itermove;     // current iteration's bestmove
    chess::Move prevPlay;     // previous iteration's bestmove
    int consecutives;         // number of consecutive bestmoves
    SearchOptions searchopt;  // limit depth, nodes, or movetime
    RaphaelParams params;     // search parameters
    // ponder
    uint64_t ponderkey = 0;  // hash after opponent's best response
    int pondereval = 0;      // eval we got during ponder
    int ponderdepth = 1;     // depth we searched to during ponder
    // storage
    TranspositionTable tt;  // table with position, eval, and bestmove
    Killers killers;        // 2 killer moves at each ply
    History history;        // history score for each move
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
    RaphaelHCE(std::string name_in);

    /** Initializes Raphael
     *
     * \param name_in player name
     * \param options engine options, such as transposition table size
     */
    RaphaelHCE(std::string name_in, EngineOptions options);


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
     * \param board board to get PV from
     * \param depth depth of PV
     * \returns the PV line of the board of length <= depth
     */
    std::string get_pv_line(chess::Board board, int depth) const;


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
     * \param ply current ply (half-moves) from the root
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
     * \param alpha lower bound eval of current position
     * \param beta upper bound eval of current position
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns eval of current board
     */
    int quiescence(chess::Board& board, int alpha, int beta, volatile bool& halt);


    /** Sorts the movelist from best to worst
     *
     * \param movelist movelist to sort
     * \param board current board
     * \param ply current ply (half-moves) from the root
     */
    void order_moves(chess::Movelist& movelist, const chess::Board& board, const int ply) const;

    /** Assigns a score to a move
     *
     * \param move move to score
     * \param board current board
     * \param ply current ply (half-moves) from the root
     */
    void score_move(chess::Move& move, const chess::Board& board, const int ply) const;

    /** Statically evaluates the board from the current player's perspective
     *
     * \param board board to evaluate
     * \returns the static evaluation of the board
     */
    int evaluate(const chess::Board& board) const;
};
};  // namespace Raphael
