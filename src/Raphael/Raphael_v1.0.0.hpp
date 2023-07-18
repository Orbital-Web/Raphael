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
    // return type of negamax search
    struct searchres {
        chess::Move move;
        int score;
    };



// methods
public:

    // Initializes Raphael with a name
    v1_0_0(std::string name_in = "Raphael v1.0.0"): GamePlayer(name_in) {
        PST::init_pst();
    }


    // Uses Negamax to return the best move. Should return immediately if halt becomes true
    chess::Move get_move(chess::Board board, int t_remain, sf::Event& event, bool& halt) {
        n_pieces_left = chess::builtin::popcount(board.occ());
        return negamax(board, 5, -INT_MAX, INT_MAX, halt).move;
    }

private:
    // The Negamax search algorithm to search for the best move
    searchres negamax(chess::Board& board, int depth, int alpha, int beta, bool& halt) {
        // timeout
        if (halt)
            return {0, 0};

        // terminal depth/state
        auto result = board.isGameOver().second;
        if (depth==0 || result!=chess::GameResult::NONE)
            return {0, evaluate(board, result)};
        
        // search
        chess::Movelist movelist;
        order_moves(movelist, board);
        searchres res = {0, -INT_MAX};

        for (auto& move : movelist) {
            board.makeMove(move);
            int score = -negamax(board, depth-1, -beta, -alpha, halt).score;
            board.unmakeMove(move);

            // update score
            if (score > res.score)
                res = {move, score};

            // prune
            alpha = std::max(alpha, score);
            if (alpha >= beta)
                break;
        }

        return res;
    }


    // Modifies movelist to contain a list of moves, ordered from best to worst
    void order_moves(chess::Movelist& movelist, const chess::Board& board) {
        chess::movegen::legalmoves(movelist, board);
        for (auto& move : movelist)
            score_move(move, board);
        movelist.sort();
    }


    // Assigns a score to the given move
    void score_move(chess::Move& move, const chess::Board& board) {
        int16_t score = 0;
        // calculate score
        int from = (int)board.at(move.from());
        int to = (int)board.at(move.to());

        // enemy piece captured
        if (to!=12 && whiteturn==(to/6))
            score += abs(PVAL::VALS[to] + PVAL::VALS[from]);
        
        // promotion
        if (move.typeOf()==chess::Move::PROMOTION)
            score += abs(PVAL::VALS[(int)move.promotionType()]);

        move.setScore(score);
    }


    // Evaluates the current position (from the current player's perspective)
    int evaluate(const chess::Board& board, const chess::GameResult& result) {
        // checkmate/draw
        if (result == chess::GameResult::DRAW)
            return 0;
        else if (result == chess::GameResult::LOSE)
            return (whiteturn) ? -INT_MAX : INT_MAX;

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

        // convert perspective
        if (!whiteturn)
            score *= -1;
        return score;
    }
};  // Raphael
}   // namespace Raphael