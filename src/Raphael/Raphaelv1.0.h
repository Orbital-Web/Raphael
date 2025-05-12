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

        /** Initializes the PST value for piece, square, and phase (mid/end game) */
        void init_pst();
    };

    TranspositionTable tt;
    chess::Move toPlay;    // overall best move
    chess::Move itermove;  // best move from previous iteration
    RaphaelParams params;  // search parameters



public:
    /** Initializes Raphael
     *
     * \param name_in player name
     */
    v1_0(std::string name_in);

    /** Initializes Raphael
     *
     * \param name_in player name
     * \param options engine options, such as transposition table size
     */
    v1_0(std::string name_in, EngineOptions options);


    /** Sets Raphael's engine options
     *
     * \param options options to set to
     */
    void set_options(EngineOptions options);


    /** Returns the best move found by Raphael. Returns immediately if halt becomes true. Will print
     * out bestmove and search statistics if UCI is true.
     *
     * \param board current board
     * \param t_remain time remaining in ms
     * \param t_inc increment after move in ms
     * \param event there for the human player implementation, ignored
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns the best move it found
     */
    chess::Move get_move(
        chess::Board board,
        const int t_remain,
        const int t_inc,
        volatile sf::Event& event,
        volatile bool& halt
    );


    /** Resets Raphael */
    void reset();

protected:
    /** Estimates the time in ms Raphael should spent on searching a move
     *
     * \param board current board
     * \param t_remain remaining time in ms
     * \param t_inc increment after move in ms
     * \returns the time in ms Raphael should spend on searching
     */
    int search_time(const chess::Board& board, const int t_remain, const int t_inc);

    /** Checks and sets halt to true if duration ms has passed. Must be called asynchronously to
     * prevent blocking.
     *
     * \param bool reference which will turn false to indicate search should stop
     * \param duration search duration in ms
     */
    static void manage_time(volatile bool& halt, const int duration);


    /** Recursively searches for the best move and eval of the current position assuming optimal
     * play by both us and the opponent
     *
     * \param board current board
     * \param depth depth to search for
     * \param ply current ply (half-move) from the root
     * \param alpha lower bound eval of current position
     * \param beta upper bound eval of current position
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns eval of current board
     */
    int negamax(chess::Board& board, int depth, int ply, int alpha, int beta, volatile bool& halt);

    /** Evaluates the board after all noisy moves are played out
     *
     * \param board current board
     * \param alpha lower bound eval of current position
     * \param beta upper bound eval of current position
     * \param halt bool reference which will turn false to indicate search should stop
     * \returns eval of current board
     */
    int quiescence(chess::Board& board, int alpha, int beta, volatile bool& halt) const;


    /** Populates the movelist with moves, ordered from best to worst
     *
     * \param movelist movelist to populate
     * \param board current board
     */
    void order_moves(chess::Movelist& movelist, const chess::Board& board) const;

    /** Populates the movelist with capture moves, ordered from best to worst
     *
     * \param movelist movelist to populate
     * \param board current board
     */
    void order_cap_moves(chess::Movelist& movelist, const chess::Board& board) const;

    /** Assigns a score to a move
     *
     * \param move move to score
     * \param board current board
     */
    void score_move(chess::Move& move, const chess::Board& board) const;

    /** Statically evaluates the board from the current player's perspective
     *
     * \param board board to evaluate
     * \returns the static evaluation of the board
     */
    int evaluate(const chess::Board& board) const;
};
}  // namespace Raphael
