#pragma once
#include <GameEngine/GamePlayer.h>
#include <GameEngine/consts.h>
#include <GameEngine/utils.h>
#include <Raphael/Transposition.h>
#include <Raphael/consts.h>
#include <math.h>

#include <chess.hpp>
#include <future>
#include <iomanip>
#include <iostream>

using std::cout;
using std::fixed;
using std::flush;
using std::max;
using std::min;
using std::setprecision;
using std::string;



namespace Raphael {
class v1_0: public cge::GamePlayer {
    // Raphael vars
public:
    struct EngineOptions {
        uint32_t tablesize = DEF_TABLE_SIZE;  // number of entries in tt
    };

private:
    TranspositionTable tt;
    chess::Move toPlay;    // overall best move
    chess::Move itermove;  // best move from previous iteration



    // Raphael methods
public:
    // Initializes Raphael with a name
    v1_0(string name_in): GamePlayer(name_in), tt(DEF_TABLE_SIZE) { PST::init_pst(); }
    // and with options
    v1_0(string name_in, EngineOptions options): GamePlayer(name_in), tt(options.tablesize) {
        PST::init_pst();
    }


    // Set options
    void set_options(EngineOptions options) { tt = TranspositionTable(options.tablesize); }


    // Uses iterative deepening on Negamax to find best move
    // Should return immediately if halt becomes true
    chess::Move get_move(
        chess::Board board,
        const int t_remain,
        const int t_inc,
        volatile sf::Event& event,
        volatile bool& halt
    ) {
        tt.clear();
        int depth = 1;
        int eval = 0;
        toPlay = chess::Move::NO_MOVE;
        itermove = chess::Move::NO_MOVE;

        // stop search after an appropriate duration
        int duration = search_time(board, t_remain, t_inc);
        auto _ = std::async(manage_time, std::ref(halt), duration);

        // begin iterative deepening
        while (!halt) {
            int itereval = negamax(board, depth, 0, -INT_MAX, INT_MAX, halt);

            // not timeout
            if (!halt) {
                toPlay = itermove;
                eval = itereval;
            }

            // checkmate, no need to continue
            if (tt.isMate(eval)) {
#ifndef UCI
    #ifndef MUTEEVAL
                // get absolute evaluation (i.e, set to white's perspective)
                {
                    lock_guard<mutex> lock(cout_mutex);
                    if (whiteturn == (eval > 0))
                        cout << "Eval: #\n" << flush;
                    else
                        cout << "Eval: -#\n" << flush;
                }
    #endif
#endif
                halt = true;
                return toPlay;
            }
            depth++;
        }
#ifdef UCI
        {
            lock_guard<mutex> lock(cout_mutex);
            cout << "bestmove " << chess::uci::moveToUci(itermove) << "\n" << flush;
        }
#else
    #ifndef MUTEEVAL
        // get absolute evaluation (i.e, set to white's perspective)
        if (!whiteturn) eval *= -1;
        {
            lock_guard<mutex> lock(cout_mutex);
            cout << "Eval: " << fixed << setprecision(2) << eval / 100.0f
                 << "\tDepth: " << depth - 1 << "\n"
                 << flush;
        }
    #endif
#endif
        return toPlay;
    }


    // Resets the player
    void reset() { tt.clear(); }

private:
    // Estimates the time (ms) it should spend on searching a move
    int search_time(const chess::Board& board, const int t_remain, const int t_inc) {
        // ratio: a function within [0, 1]
        // uses 0.5~4% of the remaining time (max at 11 pieces left)
        float n = chess::builtin::popcount(board.occ());
        float ratio = 0.0138f * (32 - n) * (n / 32) * pow(2.5f - n / 32, 3);
        // use 0.5~4% of the remaining time based on the ratio + buffered increment
        int duration = t_remain * (0.005f + 0.035f * ratio) + max(t_inc - 30, 0);
        return min(duration, t_remain);
    }


    // Sets halt to true if duration (ms) passes
    // Must be called asynchronously
    static void manage_time(volatile bool& halt, const int duration) {
        auto start = std::chrono::high_resolution_clock::now();
        while (!halt) {
            auto now = std::chrono::high_resolution_clock::now();
            auto dtime = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            if (dtime >= duration) halt = true;
        }
    }


    // The Negamax search algorithm to search for the best move
    int negamax(chess::Board& board, int depth, int ply, int alpha, int beta, volatile bool& halt) {
        // timeout
        if (halt) return 0;

        // prevent draw in winning positions
        if (ply)
            if (board.isRepetition(1) || board.isHalfMoveDraw()) return 0;

        // transposition lookup
        int alphaorig = alpha;
        auto ttkey = board.zobrist();
        auto entry = tt.get(ttkey, ply);
        if (tt.valid(entry, ttkey, depth)) {
            if (entry.flag == tt.EXACT) {
                if (!ply) itermove = entry.move;
                return entry.eval;
            } else if (entry.flag == tt.LOWER)
                alpha = max(alpha, entry.eval);
            else
                beta = min(beta, entry.eval);

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
        if (depth == 0) return quiescence(board, alpha, beta, halt);

        // search
        chess::Movelist movelist;
        order_moves(movelist, board);
        chess::Move bestmove = movelist[0];  // best move in this position

        for (auto& move : movelist) {
            board.makeMove(move);
            int eval = -negamax(board, depth - 1, ply + 1, -beta, -alpha, halt);
            board.unmakeMove(move);

            // timeout
            if (halt) return 0;

            // update eval
            if (eval > alpha) {
                alpha = eval;
                bestmove = move;
                if (!ply) itermove = move;
            }

            // prune
            if (alpha >= beta) break;
        }

        // store transposition
        TranspositionTable::Flag flag;
        if (alpha <= alphaorig)
            flag = tt.UPPER;
        else if (alpha >= beta)
            flag = tt.LOWER;
        else
            flag = tt.EXACT;
        tt.set({ttkey, depth, flag, bestmove, alpha}, ply);

        return alpha;
    }


    // Quiescence search for all captures
    int quiescence(chess::Board& board, int alpha, int beta, volatile bool& halt) const {
        int eval = evaluate(board);

        // timeout
        if (halt) return eval;

        // prune
        alpha = max(alpha, eval);
        if (alpha >= beta) return alpha;

        // search
        chess::Movelist movelist;
        order_cap_moves(movelist, board);

        for (auto& move : movelist) {
            board.makeMove(move);
            eval = -quiescence(board, -beta, -alpha, halt);
            board.unmakeMove(move);

            // prune
            alpha = max(alpha, eval);
            if (alpha >= beta) break;
        }

        return alpha;
    }


    // Modifies movelist to contain a list of moves, ordered from best to worst
    void order_moves(chess::Movelist& movelist, const chess::Board& board) const {
        chess::movegen::legalmoves(movelist, board);
        for (auto& move : movelist) score_move(move, board);
        movelist.sort();
    }


    // order_moves but for only capture moves
    void order_cap_moves(chess::Movelist& movelist, const chess::Board& board) const {
        chess::Movelist all_movelist;
        chess::movegen::legalmoves(all_movelist, board);
        for (auto& move : all_movelist) {
            // enemy piece captured
            if (board.isCapture(move)) {
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
        int16_t score = 0;
        int from = (int)board.at(move.from());
        int to = (int)board.at(move.to());

        // enemy piece captured
        if (board.isCapture(move))
            score += abs(PVAL::VALS[to]) - abs(PVAL::VALS[from])
                     + 13;  // small bias to encourage trades

        // promotion
        if (move.typeOf() == chess::Move::PROMOTION)
            score += abs(PVAL::VALS[(int)move.promotionType()]);

        move.setScore(score);
    }


    // Evaluates the current position (from the current player's perspective)
    int evaluate(const chess::Board& board) const {
        int eval = 0;
        int n_pieces_left = chess::builtin::popcount(board.occ());
        double eg_weight = min(1.0, double(32 - n_pieces_left) / (32 - N_PIECES_END));
        int wkr = 0, bkr = 0, wkf = 0, bkf = 0;

        // count pieces and added their values (material + pst)
        for (int sqi = 0; sqi < 64; sqi++) {
            auto sq = (chess::Square)sqi;
            int piece = (int)board.at(sq);

            // non-empty
            if (piece != 12) {
                // add material value
                eval += PVAL::VALS[piece];
                // add positional value
                eval += PST::MID[piece][sqi]
                        + eg_weight * (PST::END[piece][sqi] - PST::MID[piece][sqi]);
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
        if (!whiteturn) eval *= -1;

        // King proximity (if winning)
        if (eval >= 0) {
            int kingdist = abs(wkr - bkr) + abs(wkf - bkf);
            eval += (14 - kingdist) * KING_DIST_WEIGHT * eg_weight;
        }

        return eval;
    }
};  // Raphael
}  // namespace Raphael
