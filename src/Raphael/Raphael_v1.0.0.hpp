#pragma once
#define NDEBUG  // disables assert inside chess.hpp
#include "consts.hpp"
#include "../GameEngine/GamePlayer.hpp"
#include "../GameEngine/chess.hpp"
#include <SFML/Graphics.hpp>
#include <string>



namespace Raphael {
class v1_0_0: public cge::GamePlayer {
// class variables
private:
    const int N_PIECES_END = 8;
    int n_pieces_left;



// methods
public:

    // Initializes Raphael with a name
    v1_0_0(std::string name_in = "Raphael v1.0.0"): GamePlayer(name_in) {
        PST::init_pst();
    }


    // Uses Negamax algorithm to return the best move. Should return immediately if halt becomes true
    chess::Move get_move(chess::Board board, int t_remain, sf::Event& event, bool& halt) {
        n_pieces_left = chess::builtin::popcount(board.occ());
        printf("Eval: %d\n", evaluate(board));

        chess::Movelist movelist;
        order_moves(movelist, board);
        return movelist[0];
    }

private:
    // Modifies movelist to contain a list of moves, ordered from best to worst
    void order_moves(chess::Movelist& movelist, const chess::Board& board) {
        chess::movegen::legalmoves(movelist, board);
        for (auto move : movelist)
            score_move(move);
        movelist.sort();
    }


    // Assigns a score to the given move
    void score_move(chess::Move& move) {
        int16_t score = 0;
        // calculate score
        move.setScore(score);
    }


    // Evaluates the current position (from white's perspective)
    int16_t evaluate(const chess::Board& board) {
        // checkmate/draw
        auto result = board.isGameOver().second;
        if (result == chess::GameResult::DRAW)
            return 0;
        else if (result == chess::GameResult::LOSE)
            return (board.sideToMove() == chess::Color::WHITE) ? INT16_MIN : INT16_MAX;

        int16_t score = 0;

        // count pieces and added their values (material + pst)
        for (int sqi=0; sqi<64; sqi++) {
            int piece = (int)board.at((chess::Square)sqi);

            // non-empty
            if (piece != 12) {
                // add material score
                score += PVAL::VALS[piece];
                // add positional score
                score += (n_pieces_left < N_PIECES_END) ? PST::END[piece][sqi] : PST::MID[piece][sqi];
            }
        }
        return score;
    }
};  // Raphael
}   // namespace Raphael