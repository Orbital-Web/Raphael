#pragma once
#define NDEBUG  // disables assert inside chess.hpp
#include "consts.hpp"
#include "GamePlayer.hpp"
#include "chess.hpp"
#include <SFML/Graphics.hpp>
#include <string>



namespace Raphael {
class v1_0_0: public cge::GamePlayer {
// methods
public:

    // Initializes Raphael with a name
    v1_0_0(std::string name_in = "Raphael v1.0.0"): GamePlayer(name_in) {
        PST::init_pst();
    }


    // Uses Negamax algorithm to return the best move. Should return immediately if halt becomes true
    chess::Move get_move(chess::Board board, int t_remain, sf::Event& event, bool& halt) {

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
};  // Raphael
}   // namespace Raphael