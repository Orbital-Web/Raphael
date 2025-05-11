#pragma once
#include <GameEngine/GamePlayer.h>
#include <Raphael/History.h>
#include <Raphael/Killers.h>
#include <Raphael/Transposition.h>

#include <chess.hpp>
#include <chrono>
#include <string>



namespace Raphael {
class v1_8: public cge::GamePlayer {
public:
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
    // Initializes Raphael with a name
    v1_8(std::string name_in);
    // and with engine options
    v1_8(std::string name_in, EngineOptions options);


    // Set engine options
    void set_options(EngineOptions options);

    // Set search options
    void set_searchoptions(SearchOptions options);


    // Uses iterative deepening on Negamax to find best move
    // Should return immediately if halt becomes true
    chess::Move get_move(
        chess::Board board,
        const int t_remain,
        const int t_inc,
        volatile sf::Event& event,
        volatile bool& halt
    );

    // Think during opponent's turn. Should return immediately if halt becomes true
    void ponder(chess::Board board, volatile bool& halt);


    // Returns the PV from
    std::string get_pv_line(chess::Board board, int depth) const;


    // Resets the player
    void reset();

protected:
    // Estimates the time (ms) it should spend on searching a move
    // Call this at the start before using isTimeOver
    void startSearchTimer(const chess::Board& board, const int t_remain, const int t_inc);

    // Checks if duration (ms) has passed and modifies halt
    // Runs infinitely if search_t is 0
    bool isTimeOver(volatile bool& halt) const;


    // The Negamax search algorithm to search for the best move
    int negamax(
        chess::Board& board,
        const int depth,
        const int ply,
        const int ext,
        int alpha,
        int beta,
        volatile bool& halt
    );

    // Quiescence search for all captures
    int quiescence(chess::Board& board, int alpha, int beta, volatile bool& halt);


    // Sorts movelist from best to worst using score_move as its heuristic
    void order_moves(chess::Movelist& movelist, const chess::Board& board, const int ply) const;

    // Assigns a score to the given move
    void score_move(chess::Move& move, const chess::Board& board, const int ply) const;

    // Evaluates the current position (from the current player's perspective)
    int evaluate(const chess::Board& board) const;
};
};  // namespace Raphael
