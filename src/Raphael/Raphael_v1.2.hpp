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
class v1_2: public cge::GamePlayer {
// class variables
private:
    TranspositionTable tt;
    chess::Move itermove;   // best move from previous iteration
    uint64_t ponderkey = 0; // hashed board after ponder move
    int pondereval = 0;     // eval we got during ponder
    int ponderdepth = 1;    // depth we searched to during ponder



// methods
public:
    // Initializes Raphael with a name
    v1_2(std::string name_in): GamePlayer(name_in), tt(TABLE_SIZE) {
        PST::init_pst();
    }


    // Uses iterative deepening on Negamax to find best move
    // Should return immediately if halt becomes true
    chess::Move get_move(chess::Board board, float t_remain, sf::Event& event, bool& halt) {
        int depth = 1;
        int eval = 0;
        chess::Move toPlay = chess::Move::NO_MOVE;  // overall best move

        // if ponderhit, start with ponder result and depth
        if (board.zobrist() != ponderkey)
            itermove = chess::Move::NO_MOVE;
        else {
            depth = ponderdepth;
            eval = pondereval;
        }

        // stop search after an appropriate duration
        int duration = search_time(board, t_remain);
        auto _ = std::async(manage_time, std::ref(halt), duration);

        // begin iterative deepening
        while (!halt) {
            int itereval = negamax(board, depth, 0, -INT_MAX, INT_MAX, halt);

            // not timeout
            if (!halt)
                eval = itereval;
            if (itermove != chess::Move::NO_MOVE)
                toPlay = itermove;
            
            // checkmate, no need to continue
            if (tt.isMate(eval)) {
                #ifndef NDEBUG
                // get absolute evaluation (i.e, set to white's perspective)
                if (whiteturn == (eval > 0))
                    printf("Eval: +#\n", depth);
                else
                    printf("Eval: -#\n", depth);
                #endif
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


    // Think during opponent's turn. Should return immediately if halt becomes true
    void ponder(chess::Board board, float t_remain, sf::Event& event, bool& halt) {
        pondereval = 0;
        ponderdepth = 1;
        int depth = 1;
        itermove = chess::Move::NO_MOVE;    // opponent's best move

        // begin iterative deepening up to depth 4 for opponent's best move
        while (!halt && depth <= 4) {
            int eval = negamax(board, depth, 0, -INT_MAX, INT_MAX, halt);
            
            // checkmate, no need to continue
            if (tt.isMate(eval))
                break;
            depth++;
        }

        // not enough time to continue
        if (halt) return;

        // store move to check for ponderhit on our turn
        board.makeMove(itermove);
        ponderkey = board.zobrist();
        chess::Move toPlay = chess::Move::NO_MOVE;  // our best response
        itermove = chess::Move::NO_MOVE;

        // begin iterative deepening for our best response
        while (!halt) {
            int eval = negamax(board, ponderdepth, 0, -INT_MAX, INT_MAX, halt);

            // store into toPlay to prevent NO_MOVE
            if (itermove != chess::Move::NO_MOVE)
                toPlay = itermove;
            
            if (!halt) {
                pondereval = eval;
                ponderdepth++;
            }
            
            // checkmate, no need to continue
            if (tt.isMate(eval))
                break;
        }

        // override in case of NO_MOVE
        itermove = toPlay;
    }


    // Resets the player
    void reset() {
        tt.clear();
        itermove = chess::Move::NO_MOVE;
    }

private:
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
    int negamax(chess::Board& board, unsigned int depth, int ply, int alpha, int beta, bool& halt) {
        // timeout
        if (halt)
            return 0;
        
        // transposition lookup
        int alphaorig = alpha;
        auto ttkey = board.zobrist();
        auto entry = tt.get(ttkey, ply);
        if (tt.valid(entry, ttkey, depth)) {
            if (entry.flag == tt.EXACT) {
                if (!ply) itermove = entry.move;
                return entry.eval;
            }
            else if (entry.flag == tt.LOWER)
                alpha = std::max(alpha, entry.eval);
            else
                beta = std::min(beta, entry.eval);
            
            // prune
            if (alpha >= beta) {
                if (!ply) itermove = entry.move;
                return entry.eval;
            }
        }

        // checkmate/draw
        auto result = board.isGameOver().second;
        if (result == chess::GameResult::DRAW)
            return 0;
        else if (result == chess::GameResult::LOSE)
            return -MATE_EVAL + ply;  // reward faster checkmate
        
        // terminal depth
        if (depth == 0)
            return quiescence(board, alpha, beta, halt);
        
        // search
        chess::Movelist movelist;
        order_moves(movelist, board);
        chess::Move bestmove = chess::Move::NO_MOVE;    // best move in this position

        for (auto& move : movelist) {
            board.makeMove(move);
            int eval = -negamax(board, depth-1, ply+1, -beta, -alpha, halt);
            board.unmakeMove(move);

            // timeout
            if (halt)
                return 0;

            // update eval
            if (eval > alpha) {
                alpha = eval;
                bestmove = move;
                if (!ply) itermove = move;
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
        tt.set(ttkey, {ttkey, depth, flag, alpha, bestmove}, ply);

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
        
        // convert perspective
        if (!whiteturn)
            eval *= -1;

        // King proximity (if winning)
        if (eval>=0) {
            int kingdist = abs(wkr-bkr) + abs(wkf-bkf);
            eval += (14 - kingdist) * KING_DIST_WEIGHT * eg_weight;
        }
        
        return eval;
    }
};  // Raphael
}   // namespace Raphael