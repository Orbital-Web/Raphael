#pragma once
#include <GameEngine/GamePlayer.h>
#include <Raphael/Transposition.h>

#include <string>



namespace Raphael {
class v1_0: public cge::GamePlayer {
public:
    struct EngineOptions {
        uint32_t tablesize = DEF_TABLE_SIZE;  // number of entries in tt
    };

protected:
    struct RaphaelParams {
        RaphaelParams();

        // logic
        static constexpr int N_PIECES_END = 8;  // number of pieces remaining for max endgame scale

        // evaluation[midgame, endgame]
        static constexpr int KING_DIST_WEIGHT = 20;  // weight scaling for closer kings
        static constexpr int PVAL[12] = {
            // WHITE
            100,  // PAWN
            316,  // KNIGHT
            328,  // BISHOP
            493,  // ROOK
            983,  // QUEEN
            0,
            // BLACK
            -100,  // PAWN
            -316,  // KNIGHT
            -328,  // BISHOP
            -493,  // ROOK
            -983,  // QUEEN
            0,
        };  // value of each piece
        int PST[12][64][2];  // piece square table for piece, square, and phase

        void init_pst();
    };

    TranspositionTable tt;
    chess::Move toPlay;    // overall best move
    chess::Move itermove;  // best move from previous iteration
    RaphaelParams params;  // search parameters



public:
    // Initializes Raphael with a name
    v1_0(std::string name_in);
    // and with options
    v1_0(std::string name_in, EngineOptions options);


    // Set options
    void set_options(EngineOptions options);


    // Uses iterative deepening on Negamax to find best move
    // Should return immediately if halt becomes true
    chess::Move get_move(
        chess::Board board,
        const int t_remain,
        const int t_inc,
        volatile sf::Event& event,
        volatile bool& halt
    );


    // Resets the player
    void reset();

protected:
    // Estimates the time (ms) it should spend on searching a move
    int search_time(const chess::Board& board, const int t_remain, const int t_inc);

    // Sets halt to true if duration (ms) passes
    // Must be called asynchronously
    static void manage_time(volatile bool& halt, const int duration);


    // The Negamax search algorithm to search for the best move
    int negamax(chess::Board& board, int depth, int ply, int alpha, int beta, volatile bool& halt);

    // Quiescence search for all captures
    int quiescence(chess::Board& board, int alpha, int beta, volatile bool& halt) const;


    // Modifies movelist to contain a list of moves, ordered from best to worst
    void order_moves(chess::Movelist& movelist, const chess::Board& board) const;

    // order_moves but for only capture moves
    void order_cap_moves(chess::Movelist& movelist, const chess::Board& board) const;

    // Assigns a score to the given move
    void score_move(chess::Move& move, const chess::Board& board) const;

    // Evaluates the current position (from the current player's perspective)
    int evaluate(const chess::Board& board) const;
};
}  // namespace Raphael
