#pragma once
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
    TranspositionTable tt;
    chess::Move toPlay;     // overall best move
    chess::Move itermove;   // best move from previous iteration



// methods
public:
    // Initializes Raphael with a name
    v1_1_0(std::string name_in): GamePlayer(name_in), tt(TABLE_SIZE) {
        PST::init_pst();
    }


    // Uses Negamax to return the best move. Should return immediately if halt becomes true
    chess::Move get_move(chess::Board board, float t_remain, sf::Event& event, bool& halt) {
        //tt.clear();
        return iterative_deepening(board, t_remain, halt);
    }


    // Resets the player
    void reset() {
        tt.clear();
    }

private:
    // Uses iterative deepening on Negamax to find best move
    chess::Move iterative_deepening(chess::Board& board, const float t_remain, bool& halt) {
        int depth = 1;
        int eval = 0;
        toPlay = chess::Move::NO_MOVE;
        itermove = chess::Move::NO_MOVE;

        // stop search after an appropriate duration
        int duration = search_time(board, t_remain);
        auto _ = std::async(manage_time, std::ref(halt), duration);

        // begin iterative deepening
        while (!halt) {
            int itereval = negamax(board, depth, -INT_MAX, INT_MAX, halt);

            // not timeout
            if (!halt)
                eval = itereval;
            if (itermove != chess::Move::NO_MOVE)
                toPlay = itermove;
            
            // checkmate, no need to continue
            if (abs(eval)>=1073641824) {
                #ifndef NDEBUG
                // get absolute evaluation (i.e, set to white's perspective)
                if (!whiteturn == (eval > 0))
                    depth *= -1;
                printf("Eval: #%d\n", depth);
                #endif
                halt = true;
                return toPlay;
            }
            depth++;
        }
        #ifndef NDEBUG
        // get absolute evaluation (i.e, set to white's perspective)
        if (!whiteturn)
            eval *= -1;
        printf("Eval: %.2f\tDepth: %d\n", eval/100.0, depth-1);
        #endif
        return toPlay;
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
    int negamax(chess::Board& board, unsigned int depth, int alpha, int beta, bool& halt, bool root = true) {
        // timeout
        if (halt)
            return 0;
        
        // transposition lookup
        int alphaorig = alpha;
        auto ttkey = board.zobrist();
        auto entry = tt.get(ttkey);
        if (tt.valid(entry, depth)) {
            if (entry.flag == tt.EXACT) {
                if (root) itermove = entry.move;
                return entry.eval;
            }
            else if (entry.flag == tt.LOWER)
                alpha = std::max(alpha, entry.eval);
            else
                beta = std::min(beta, entry.eval);
            
            // prune
            if (alpha >= beta)
                return entry.eval;
        }

        // checkmate/draw
        auto result = board.isGameOver().second;
        if (result == chess::GameResult::DRAW)
            return 0;
        else if (result == chess::GameResult::LOSE)
            return -INT_MAX + 1000*board.fullMoveNumber();  // reward faster checkmate
        
        // terminal depth
        if (depth == 0)
            return quiescence(board, alpha, beta, halt);
        
        // search
        chess::Movelist movelist;
        order_moves(movelist, board);
        chess::Move bestmove = chess::Move::NO_MOVE;    // best move in this position

        for (auto& move : movelist) {
            board.makeMove(move);
            int eval = -negamax(board, depth-1, -beta, -alpha, halt, false);
            board.unmakeMove(move);

            // timeout
            if (halt)
                return 0;

            // update eval
            if (eval > alpha) {
                alpha = eval;
                bestmove = move;
                if (root) itermove = move;
            }

            // prune
            if (alpha >= beta)
                break;
        }

        // store transposition
        TranspositionTable::Flag flag;
        if (alpha <= alphaorig)
            flag = tt.UPPER;
        else if (alpha >= beta)
            flag = tt.LOWER;
        else
            flag = tt.EXACT;
        tt.set(ttkey, {depth, flag, alpha, bestmove});

        return alpha;
    }


    // Quiescence search for all captures
    int quiescence(chess::Board& board, int alpha, int beta, bool& halt) const {
        int eval = evaluate(board);

        // timeout
        if (halt)
            return eval;

        // prune
        alpha = std::max(alpha, eval);
        if (alpha >= beta)
            return alpha;
        
        // search
        chess::Movelist movelist;
        order_cap_moves(movelist, board);
        
        for (auto& move : movelist) {
            board.makeMove(move);
            eval = -quiescence(board, -beta, -alpha, halt);
            board.unmakeMove(move);

            // prune
            alpha = std::max(alpha, eval);
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
        int eval = 0;
        int n_pieces_left = chess::builtin::popcount(board.occ());
        double eg_weight = std::min(1.0, double(32-n_pieces_left)/(32-N_PIECES_END));
        int wkr, bkr, wkf, bkf;

        // count pieces and added their values (material + pst)
        for (int sqi=0; sqi<64; sqi++) {
            auto sq = (chess::Square)sqi;
            int piece = (int)board.at(sq);

            // non-empty
            if (piece != 12) {
                // add material value
                eval += PVAL::VALS[piece];
                // add positional value
                eval += PST::MID[piece][sqi] + eg_weight*(PST::END[piece][sqi] - PST::MID[piece][sqi]);
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
        eval += (14 - kingdist) * KING_DIST_WEIGHT * eg_weight;

        // convert perspective
        if (!whiteturn)
            eval *= -1;
        return eval;
    }
};  // Raphael
}   // namespace Raphael