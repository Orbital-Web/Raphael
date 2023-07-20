#pragma once
#define NDEBUG  // disables assert inside chess.hpp
#include "consts.hpp"
#include "Transposition.hpp"
#include "../GameEngine/GamePlayer.hpp"
#include "../GameEngine/chess.hpp"
#include <SFML/Graphics.hpp>
#include <string>
#include <future>
#include <chrono>
#include <math.h>



namespace Raphael {
class v1_1_0: public cge::GamePlayer {
// class variables
private:
    // return type of negamax search
    struct Searchres {
        chess::Move move;
        int score;
    };
    TranspositionTable tt;
    chess::Move itermove;   // best move from previous iteration



// methods
public:
    // Initializes Raphael with a name
    v1_1_0(std::string name_in): GamePlayer(name_in), tt(TABLE_SIZE) {
        PST::init_pst();
    }


    // Uses Negamax to return the best move. Should return immediately if halt becomes true
    chess::Move get_move(chess::Board board, float t_remain, sf::Event& event, bool& halt) {
        tt.clear();
        return iterative_deepening(board, t_remain, halt);
    }

private:
    // Uses iterative deepening on Negamax to find best move
    chess::Move iterative_deepening(chess::Board& board, const float t_remain, bool& halt) {
        int depth = 1;
        itermove = chess::Move::NO_MOVE;
        Searchres res = {0, 0};

        // stop search after an appropriate duration
        int duration = search_time(board, t_remain);
        auto _ = std::async(manage_time, std::ref(halt), duration);

        // begin iterative deepening
        while (!halt) {
            auto iterres = negamax(board, depth, -INT_MAX, INT_MAX, halt);

            // not timeout
            if (!halt)
                res = iterres;
            
            // checkmate, no need to continue
            if (abs(res.score)>=1073641824) {
                // get absolute evaluation (i.e, set to white's perspective)
                if (!whiteturn == (res.score > 0))
                    depth *= -1;
                printf("Eval: #%d\n", depth);
                halt = true;
                return res.move;
            }

            itermove = res.move;
            depth++;
        }
        // get absolute evaluation (i.e, set to white's perspective)
        if (!whiteturn)
            res.score *= -1;
        printf("Eval: %.2f\tDepth %d\n", res.score/100.0, depth-1);
        return res.move;
    }


    // Estimates the time (ms) it should spend on searching a move
    int search_time(const chess::Board& board, const float t_remain) {
        // ratio: a function within [0, 1]
        // in this case it's a function at 0 when n_pieces = 0 or 32
        // and 1 when n_pieces is 11
        float n = chess::builtin::popcount(board.occ());
        float ratio = 0.0138*(32-n)*(n/32)*pow(2.5-n/32, 3);
        // use 0.5~4% of the remaining time based on the ratio
        float duration = t_remain * (0.005 + 0.035*ratio);
        return duration*1000;
    }


    // Sets halt to true if duration (ms) passes
    // Must be called asynchronously
    static void manage_time(bool& halt, const int duration) {
        auto start = std::chrono::high_resolution_clock::now();
        while (!halt) {
            auto now = std::chrono::high_resolution_clock::now();
            auto dtime = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            if (dtime >= duration)
                halt = true;
        }
    }


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
        // prioritize best move from previous iteraton
        if (move == itermove) {
            move.setScore(INT16_MAX);
            return;
        }

        // calculate score
        int score = 0;
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