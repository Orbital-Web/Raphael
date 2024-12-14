#pragma once
#include <GameEngine/GamePlayer.h>
#include <GameEngine/consts.h>
#include <GameEngine/utils.h>
#include <Raphael/History.h>
#include <Raphael/Killers.h>
#include <Raphael/SEE.h>
#include <Raphael/Transposition.h>
#include <Raphael/consts.h>
#include <math.h>

#include <chess.hpp>
#include <chrono>
#include <iomanip>
#include <iostream>

using std::cout;
using std::fixed;
using std::setprecision;



namespace Raphael {
class v1_7: public cge::GamePlayer {
    // Raphael vars
public:
    struct EngineOptions {
        uint32_t tablesize = DEF_TABLE_SIZE;  // number of entries in tt
    };

private:
    // search
    chess::Move itermove;  // current iteration's bestmove
    chess::Move prevPlay;  // previous iteration's bestmove
    int consecutives;      // number of consecutive bestmoves
    // ponder
    uint64_t ponderkey = 0;  // hash after opponent's best response
    int pondereval = 0;      // eval we got during ponder
    int ponderdepth = 1;     // depth we searched to during ponder
    // storage
    TranspositionTable tt;  // table with position, eval, and bestmove
    Killers killers;        // 2 killer moves at each ply
    History history;        // history score for each move
    // info
    uint64_t nodes;  // number of nodes visited
    // timing
    std::chrono::system_clock::time_point start_t;  // search start time
    int64_t search_t;                               // search duration (ms)

    // Raphael methods
public:
    // Initializes Raphael with a name
    v1_7(std::string name_in): GamePlayer(name_in), tt(DEF_TABLE_SIZE) {
        PST::init_pst();
        PMASK::init_pawnmask();
    }
    // and with options
    v1_7(std::string name_in, EngineOptions options): GamePlayer(name_in), tt(options.tablesize) {
        PST::init_pst();
        PMASK::init_pawnmask();
    }

    // Set options
    void set_options(EngineOptions options) { tt = TranspositionTable(options.tablesize); }

    // Uses iterative deepening on Negamax to find best move
    // Should return immediately if halt becomes true
    chess::Move get_move(
        chess::Board board, const int t_remain, const int t_inc, sf::Event& event, bool& halt
    ) {
        int depth = 1;
        int eval = 0;
        int alpha = -INT_MAX;
        int beta = INT_MAX;
        history.clear();

        // if ponderhit, start with ponder result and depth
        if (board.hash() != ponderkey) {
            itermove = chess::Move::NO_MOVE;
            prevPlay = chess::Move::NO_MOVE;
            consecutives = 1;
            nodes = 0;
        } else {
            depth = ponderdepth;
            eval = pondereval;
            alpha = eval - ASPIRATION_WINDOW;
            beta = eval + ASPIRATION_WINDOW;
        }

        // stop search after an appropriate duration
        startSearchTimer(board, t_remain, t_inc);

        // begin iterative deepening
        while (!halt && depth <= MAX_DEPTH) {
            // stable pv, skip
            if (consecutives >= PV_STABLE_COUNT) halt = true;
            int itereval = negamax(board, depth, 0, MAX_EXTENSIONS, alpha, beta, halt);

            // not timeout
            if (!halt) {
                eval = itereval;

                // re-search required
                if ((eval <= alpha) || (eval >= beta)) {
                    alpha = -INT_MAX;
                    beta = INT_MAX;
                    continue;
                }

                // narrow window
                alpha = eval - ASPIRATION_WINDOW;
                beta = eval + ASPIRATION_WINDOW;
                depth++;

                // count consecutive bestmove
                if (itermove == prevPlay)
                    consecutives++;
                else {
                    prevPlay = itermove;
                    consecutives = 1;
                }
            }

            // checkmate, no need to continue
            if (tt.isMate(eval)) {
#ifndef UCI
    #ifndef MUTEEVAL
                // get absolute evaluation (i.e, set to white's perspective)
                {
                    lock_guard<mutex> lock(cout_mutex);
                    char sign = (whiteturn == (eval > 0)) ? '\0' : '-';
                    cout << "Eval: " << sign << "#" << MATE_EVAL - abs(eval) << "\tNodes: " << nodes
                         << "\n";
                }
    #endif
#else
                auto now = std::chrono::high_resolution_clock::now();
                auto dtime
                    = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_t).count();
                auto nps = (dtime) ? nodes * 1000 / dtime : 0;
                char sign = (eval >= 0) ? '\0' : '-';
                {
                    lock_guard<mutex> lock(cout_mutex);
                    cout << "info depth " << depth - 1 << " time " << dtime << " nodes " << nodes
                         << " score mate " << sign << MATE_EVAL - abs(eval) << " nps " << nps
                         << " pv " << get_pv_line(board, depth - 1) << "\n";
                    cout << "bestmove " << chess::uci::moveToUci(itermove) << "\n";
                }
#endif
                halt = true;
                return itermove;
            } else {
#ifdef UCI
                auto now = std::chrono::high_resolution_clock::now();
                auto dtime
                    = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_t).count();
                auto nps = (dtime) ? nodes * 1000 / dtime : 0;
                {
                    lock_guard<mutex> lock(cout_mutex);
                    cout << "info depth " << depth - 1 << " time " << dtime << " nodes " << nodes
                         << " score cp " << eval << " nps " << nps << " pv "
                         << get_pv_line(board, depth - 1) << "\n";
                }
#endif
            }
        }
#ifdef UCI
        {
            lock_guard<mutex> lock(cout_mutex);
            cout << "bestmove " << chess::uci::moveToUci(itermove) << "\n";
        }
#else
    #ifndef MUTEEVAL
        // get absolute evaluation (i.e, set to white's perspective)
        if (!whiteturn) eval *= -1;
        {
            lock_guard<mutex> lock(cout_mutex);
            cout << "Eval: " << fixed << setprecision(2) << eval / 100.0f
                 << "\tDepth: " << depth - 1 << "\tNodes: " << nodes << "\n";
        }
    #endif
#endif
        return itermove;
    }

    // Think during opponent's turn. Should return immediately if halt becomes true
    void ponder(chess::Board board, bool& halt) {
        ponderdepth = 1;
        pondereval = 0;
        itermove = chess::Move::NO_MOVE;
        search_t = 0;  // infinite time

        // predict opponent's move from pv
        auto ttkey = board.hash();
        auto ttentry = tt.get(ttkey, 0);

        // no valid response in pv or timeout
        if (halt || !tt.valid(ttentry, ttkey, 0)) {
            consecutives = 1;
            return;
        }

        // play opponent's move and store key to check for ponderhit
        board.makeMove(ttentry.move);
        ponderkey = board.hash();
        history.clear();

        int alpha = -INT_MAX;
        int beta = INT_MAX;
        nodes = 0;
        consecutives = 1;

        // begin iterative deepening for our best response
        while (!halt && ponderdepth <= MAX_DEPTH) {
            int itereval = negamax(board, ponderdepth, 0, MAX_EXTENSIONS, alpha, beta, halt);

            if (!halt) {
                pondereval = itereval;

                // re-search required
                if ((pondereval <= alpha) || (pondereval >= beta)) {
                    alpha = -INT_MAX;
                    beta = INT_MAX;
                    continue;
                }

                // narrow window
                alpha = pondereval - ASPIRATION_WINDOW;
                beta = pondereval + ASPIRATION_WINDOW;
                ponderdepth++;

                // count consecutive bestmove
                if (itermove == prevPlay)
                    consecutives++;
                else {
                    prevPlay = itermove;
                    consecutives = 1;
                }
            }

            // checkmate, no need to continue (but don't edit halt)
            if (tt.isMate(pondereval)) break;
        }
    }

    // Returns the PV from
    std::string get_pv_line(chess::Board board, int depth) {
        // get first move
        auto ttkey = board.hash();
        auto ttentry = tt.get(ttkey, 0);
        chess::Move pvmove;

        std::string pvline = "";

        while (depth && tt.valid(ttentry, ttkey, 0)) {
            pvmove = ttentry.move;
            pvline += chess::uci::moveToUci(pvmove) + " ";
            board.makeMove(pvmove);
            ttkey = board.hash();
            ttentry = tt.get(ttkey, 0);
            depth--;
        }
        return pvline;
    }

    // Resets the player
    void reset() {
        tt.clear();
        killers.clear();
        history.clear();
        itermove = chess::Move::NO_MOVE;
        prevPlay = chess::Move::NO_MOVE;
        consecutives = 0;
    }

private:
    // Estimates the time (ms) it should spend on searching a move
    // Call this at the start before using isTimeOver
    void startSearchTimer(const chess::Board& board, const int t_remain, const int t_inc) {
        float n = chess::builtin::popcount(board.occ());
        // 0~1, higher the more time it uses (max at 20 pieces left)
        float ratio = 0.0044f * (n - 32) * (-n / 32) * pow(2.5f + n / 32, 3);
        // use 1~5% of the remaining time based on the ratio + buffered increment
        int duration = t_remain * (0.01f + 0.04f * ratio) + std::max(t_inc - 30, 0);
        search_t = std::min(duration, t_remain);
        start_t = std::chrono::high_resolution_clock::now();
    }

    // Checks if duration (ms) has passed and modifies halt
    // Runs infinitely if search_t is 0
    bool isTimeOver(bool& halt) const {
        // check every 2048 nodes
        if (search_t && !(nodes & 2047)) {
            auto now = std::chrono::high_resolution_clock::now();
            auto dtime
                = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_t).count();
            if (dtime >= search_t) halt = true;
        }
        return halt;
    }

    // The Negamax search algorithm to search for the best move
    int negamax(
        chess::Board& board,
        const int depth,
        const int ply,
        const int ext,
        int alpha,
        int beta,
        bool& halt
    ) {
        // timeout
        if (isTimeOver(halt)) return 0;
        nodes++;

        if (ply) {
            // prevent draw in winning positions
            // technically this ignores checkmate on the 50th move
            if (board.isRepetition(1) || board.isHalfMoveDraw()) return 0;

            // mate distance pruning
            alpha = std::max(alpha, -MATE_EVAL + ply);
            beta = std::min(beta, MATE_EVAL - ply);
            if (alpha >= beta) return alpha;
        }

        // transposition lookup
        int alphaorig = alpha;
        auto ttkey = board.hash();
        auto entry = tt.get(ttkey, ply);
        if (tt.valid(entry, ttkey, depth)) {
            if (entry.flag == tt.EXACT) {
                if (!ply) itermove = entry.move;
                return entry.eval;
            } else if (entry.flag == tt.LOWER)
                alpha = std::max(alpha, entry.eval);
            else
                beta = std::min(beta, entry.eval);

            // prune
            if (alpha >= beta) {
                if (!ply) itermove = entry.move;
                return entry.eval;
            }
        }

        // terminal analysis
        if (board.isInsufficientMaterial()) return 0;
        chess::Movelist movelist;
        chess::movegen::legalmoves<chess::MoveGenType::ALL>(movelist, board);
        if (movelist.empty()) {
            if (board.inCheck()) return -MATE_EVAL + ply;  // reward faster checkmate
            return 0;
        }

        // terminal depth
        if (depth <= 0) return quiescence(board, alpha, beta, halt);

        // one reply extension
        int extension = 0;
        if (movelist.size() > 1)
            order_moves(movelist, board, ply);
        else if (ext > 0)
            extension++;

        // search
        chess::Move bestmove = movelist[0];  // best move in this position
        if (!ply) itermove = bestmove;       // set itermove in case time runs out here
        int movei = 0;

        for (const auto& move : movelist) {
            bool istactical = board.isCapture(move) || move.typeOf() == chess::Move::PROMOTION;
            board.makeMove(move);
            // check and passed pawn extension
            if (ext > extension) {
                if (board.inCheck())
                    extension++;
                else {
                    auto sqrank = chess::utils::squareRank(move.to());
                    auto piece = board.at(move.to());
                    if ((sqrank == chess::Rank::RANK_2 && piece == chess::Piece::BLACKPAWN)
                        || (sqrank == chess::Rank::RANK_7 && piece == chess::Piece::WHITEPAWN))
                        extension++;
                }
            }
            // late move reduction with zero window for certain quiet moves
            bool fullwindow = true;
            int eval;
            if (extension == 0 && depth >= 3 && movei >= REDUCTION_FROM && !istactical) {
                eval = -negamax(board, depth - 2, ply + 1, ext, -alpha - 1, -alpha, halt);
                fullwindow = eval > alpha;
            }
            if (fullwindow)
                eval = -negamax(
                    board, depth - 1 + extension, ply + 1, ext - extension, -beta, -alpha, halt
                );
            extension = 0;
            movei++;
            board.unmakeMove(move);

            // timeout
            if (halt) return 0;

            // prune
            if (eval >= beta) {
                // store killer move (ignore captures)
                if (!board.isCapture(move)) {
                    killers.put(move, ply);
                    history.update(move, depth, whiteturn);
                }
                // update transposition
                tt.set({ttkey, depth, tt.LOWER, alpha, move}, ply);
                return beta;
            }

            // update eval
            if (eval > alpha) {
                alpha = eval;
                bestmove = move;
                if (!ply) itermove = move;
            }
        }

        // update transposition
        auto flag = (alpha <= alphaorig) ? tt.UPPER : tt.EXACT;
        tt.set({ttkey, depth, flag, alpha, bestmove}, ply);
        return alpha;
    }

    // Quiescence search for all captures
    int quiescence(chess::Board& board, int alpha, int beta, bool& halt) {
        // timeout
        if (isTimeOver(halt)) return 0;
        nodes++;

        // prune with standing pat
        int eval = evaluate(board);
        if (eval >= beta) return beta;
        alpha = std::max(alpha, eval);

        // search
        chess::Movelist movelist;
        chess::movegen::legalmoves<chess::MoveGenType::CAPTURE>(movelist, board);
        order_moves(movelist, board, 0);

        for (const auto& move : movelist) {
            board.makeMove(move);
            // SEE pruning for losing captures
            if (move.score() < GOOD_CAPTURE_WEIGHT && !board.inCheck()) {
                board.unmakeMove(move);
                continue;
            }
            eval = -quiescence(board, -beta, -alpha, halt);
            board.unmakeMove(move);

            // prune
            if (eval >= beta) return beta;
            alpha = std::max(alpha, eval);
        }

        return alpha;
    }

    // Sorts movelist from best to worst using score_move as its heuristic
    void order_moves(chess::Movelist& movelist, const chess::Board& board, const int ply) const {
        for (auto& move : movelist) score_move(move, board, ply);
        movelist.sort();
    }

    // Assigns a score to the given move
    void score_move(chess::Move& move, const chess::Board& board, const int ply) const {
        // prioritize best move from previous iteraton
        if (move == tt.get(board.hash(), 0).move) {
            move.setScore(INT16_MAX);
            return;
        }

        int16_t score = 0;

        // calculate other scores
        int from = (int)board.at(move.from());
        int to = (int)board.at(move.to());

        // enemy piece captured
        if (board.isCapture(move)) {
            score += abs(PVAL::VALS[to]) - (from % 6);  // MVV/LVA
            score += SEE::goodCapture(move, board, -12) * GOOD_CAPTURE_WEIGHT;
        } else {
            // killer move
            if (ply > 0 && killers.isKiller(move, ply)) score += KILLER_WEIGHT;
            // history
            score += history.get(move, whiteturn);
        }

        // promotion
        if (move.typeOf() == chess::Move::PROMOTION)
            score += abs(PVAL::VALS[(int)move.promotionType()]);

        move.setScore(score);
    }

    // Evaluates the current position (from the current player's perspective)
    static int evaluate(const chess::Board& board) {
        int eval = 0;
        auto pieces = board.occ();
        int n_pieces_left = chess::builtin::popcount(pieces);
        float eg_weight = std::min(
            1.0f, float(32 - n_pieces_left) / (32 - N_PIECES_END)
        );  // 0~1 as pieces left decreases

        // mobility
        int krd = 0, kfd = 0;  // king rank and file distance
        int bishmob = 0, rookmob = 0;
        auto wbishx
            = pieces & ~board.pieces(chess::PieceType::QUEEN, chess::Color::WHITE);  // occ - wqueen
        auto bbishx
            = pieces & ~board.pieces(chess::PieceType::QUEEN, chess::Color::BLACK);  // occ - bqueen
        auto wrookx = wbishx
                      & ~board.pieces(
                          chess::PieceType::ROOK, chess::Color::WHITE
                      );  // occ - (wqueen | wrook)
        auto brookx = bbishx
                      & ~board.pieces(
                          chess::PieceType::ROOK, chess::Color::BLACK
                      );  // occ - (bqueen | brook)
        auto wpawns = board.pieces(chess::PieceType::PAWN, chess::Color::WHITE);
        auto bpawns = board.pieces(chess::PieceType::PAWN, chess::Color::BLACK);

        // loop through all pieces
        while (pieces) {
            auto sq = chess::builtin::poplsb(pieces);
            int sqi = (int)sq;
            int piece = (int)board.at(sq);

            // add material value
            eval += PVAL::VALS[piece];
            // add positional value
            eval
                += PST::MID[piece][sqi] + eg_weight * (PST::END[piece][sqi] - PST::MID[piece][sqi]);

            switch (piece) {
                // pawn structure
                case 0:
                    // passed (+ for white) (more important in endgame)
                    if ((PMASK::WPASSED[sqi] & bpawns) == 0)
                        eval += PMASK::PASSEDBONUS[7 - (sqi / 8)] * eg_weight;
                    // isolated (- for white)
                    if ((PMASK::ISOLATED[sqi] & wpawns) == 0) eval -= PMASK::ISOLATION_WEIGHT;
                    break;
                case 6:
                    // passed (- for white) (more important in endgame)
                    if ((PMASK::BPASSED[sqi] & wpawns) == 0)
                        eval -= PMASK::PASSEDBONUS[(sqi / 8)] * eg_weight;
                    // isolated (+ for white)
                    if ((PMASK::ISOLATED[sqi] & bpawns) == 0) eval += PMASK::ISOLATION_WEIGHT;
                    break;

                // bishop mobility (xrays queens)
                case 2:
                    bishmob += chess::builtin::popcount(chess::attacks::bishop(sq, wbishx));
                    break;
                case 8:
                    bishmob -= chess::builtin::popcount(chess::attacks::bishop(sq, bbishx));
                    break;

                // rook mobility (xrays rooks and queens)
                case 3:
                    rookmob += chess::builtin::popcount(chess::attacks::rook(sq, wrookx));
                    break;
                case 9:
                    rookmob -= chess::builtin::popcount(chess::attacks::rook(sq, brookx));
                    break;

                // king proximity
                case 5:
                    krd += (int)chess::utils::squareRank(sq);
                    kfd += (int)chess::utils::squareFile(sq);
                    break;
                case 11:
                    krd -= (int)chess::utils::squareRank(sq);
                    kfd -= (int)chess::utils::squareFile(sq);
                    break;
            }
        }

        // mobility
        eval += bishmob
                * (MOBILITY::BISH_MID + eg_weight * (MOBILITY::BISH_END - MOBILITY::BISH_MID));
        eval += rookmob
                * (MOBILITY::ROOK_MID + eg_weight * (MOBILITY::ROOK_END - MOBILITY::ROOK_MID));

        // convert perspective
        if (!whiteturn) eval *= -1;

        // King proximity bonus (if winning)
        if (eval >= 0) eval += (14 - abs(krd) - abs(kfd)) * KING_DIST_WEIGHT * eg_weight;

        return eval;
    }
};  // Raphael
}  // namespace Raphael
