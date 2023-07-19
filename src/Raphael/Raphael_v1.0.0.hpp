#pragma once
#define NDEBUG  // disables assert inside chess.hpp
#include "consts.hpp"
#include "Transposition.hpp"
#include "../GameEngine/GamePlayer.hpp"
#include "../GameEngine/chess.hpp"
#include <SFML/Graphics.hpp>
#include <string>



namespace Raphael {
class v1_0_0: public cge::GamePlayer {
// class variables
private:
    // return type of negamax search
    struct Searchres {
        chess::Move move;
        int score;
    };
    TranspositionTable tt;



// methods
public:
    // Initializes Raphael with a name
    v1_0_0(std::string name_in = "Raphael v1.0.0"): GamePlayer(name_in), tt(TABLE_SIZE) {
        PST::init_pst();
    }


    // Uses Negamax to return the best move. Should return immediately if halt becomes true
    chess::Move get_move(chess::Board board, int t_remain, sf::Event& event, bool& halt) {
        tt.clear();
        return negamax(board, 4, -INT_MAX, INT_MAX, halt).move;
    }

private:
    // The Negamax search algorithm to search for the best move
    Searchres negamax(chess::Board& board, unsigned int depth, int alpha, int beta, bool& halt) {
        // timeout
        if (halt)
            return {0, 0};
        
        // transposition lookup
        int alphaorig = alpha;
        int ttkey = board.zobrist();
        auto entry = tt.get(ttkey);
        if (tt.valid(entry, depth)) {
            if (entry.flag == tt.EXACT)
                return {entry.move, entry.score};
            else if (entry.flag == tt.LOWER)
                alpha = std::max(alpha, entry.score);
            else
                beta = std::min(beta, entry.score);
            
            // prune
            if (alpha >= beta)
                return {entry.move, entry.score};
        }

        // checkmate/draw
        auto result = board.isGameOver().second;
        if (result == chess::GameResult::DRAW)
            return {0, 0};
        else if (result == chess::GameResult::LOSE)
            return {0, -INT_MAX + 1000*board.fullMoveNumber()};  // reward faster checkmate
        
        // terminal depth
        if (depth == 0)
            return {0, quiescence(board, alpha, beta, halt)};
        
        // search
        chess::Movelist movelist;
        order_moves(movelist, board);
        Searchres res = {0, -INT_MAX};

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

        // store transposition
        TranspositionTable::Flag flag;
        if (res.score <= alphaorig)
            flag = tt.UPPER;
        else if (res.score >= beta)
            flag = tt.LOWER;
        else
            flag = tt.EXACT;
        tt.set(ttkey, {depth, flag, res.score, res.move});

        return res;
    }


    // Quiescence search for all captures
    int quiescence(chess::Board& board, int alpha, int beta, bool& halt) const {
        int score = evaluate(board);

        // timeout
        if (halt)
            return score;

        // prune
        alpha = std::max(alpha, score);
        if (alpha >= beta)
            return alpha;
        
        // search
        chess::Movelist movelist;
        order_cap_moves(movelist, board);
        
        for (auto& move : movelist) {
            board.makeMove(move);
            int score = -quiescence(board, -beta, -alpha, halt);
            board.unmakeMove(move);

            // prune
            alpha = std::max(alpha, score);
            if (alpha >= beta)
                break;
        }
        
        return alpha;
    }


    // Modifies movelist to contain a list of moves, ordered from best to worst
    void order_moves(chess::Movelist& movelist, const chess::Board& board) const {
        chess::movegen::legalmoves(movelist, board);
        for (auto& move : movelist)
            score_move(move, board);
        movelist.sort();
    }


    // order_moves but for only capture moves
    void order_cap_moves(chess::Movelist& movelist, const chess::Board& board) const {
        chess::Movelist all_movelist;
        chess::movegen::legalmoves(all_movelist, board);
        for (auto& move : all_movelist) {
            int to = (int)board.at(move.to());
            // enemy piece captured
            if (to!=12 && whiteturn==(to/6)) {
                score_move(move, board);
                movelist.add(move);
            }
        }
        movelist.sort();
    }


    // Assigns a score to the given move
    void score_move(chess::Move& move, const chess::Board& board) const {
        int score = 0;
        // calculate score
        int from = (int)board.at(move.from());
        int to = (int)board.at(move.to());

        // enemy piece captured
        if (to!=12 && whiteturn==(to/6))
            score += abs(PVAL::VALS[to]) - abs(PVAL::VALS[from]) + 13;  // small bias to encourage trades
        
        // promotion
        if (move.typeOf()==chess::Move::PROMOTION)
            score += abs(PVAL::VALS[(int)move.promotionType()]);

        move.setScore(score);
    }


    // Evaluates the current position (from the current player's perspective)
    int evaluate(const chess::Board& board) const {
        int16_t score = 0;
        int n_pieces_left = chess::builtin::popcount(board.occ());
        double eg_weight = std::min(1.0, double(32-n_pieces_left)/(32-N_PIECES_END));
        int wkr, bkr, wkf, bkf;

        // count pieces and added their values (material + pst)
        for (int sqi=0; sqi<64; sqi++) {
            auto sq = (chess::Square)sqi;
            int piece = (int)board.at(sq);

            // non-empty
            if (piece != 12) {
                // add material score
                score += PVAL::VALS[piece];
                // add positional score
                score += PST::MID[piece][sqi] + eg_weight*(PST::END[piece][sqi] - PST::MID[piece][sqi]);
            }

            // King proximity
            if (piece == 5) {
                wkr = (int)chess::utils::squareRank(sq);
                wkf = (int)chess::utils::squareFile(sq);
            } else if (piece == 11) {
                bkr = (int)chess::utils::squareRank(sq);
                bkf = (int)chess::utils::squareFile(sq);
            }
        }

        // King Proximity
        int kingdist = abs(wkr-bkr) + abs(wkf-bkf);
        score += (14 - kingdist) * KING_DIST_WEIGHT * eg_weight;

        // convert perspective
        if (!whiteturn)
            score *= -1;
        return score;
    }
};  // Raphael
}   // namespace Raphael