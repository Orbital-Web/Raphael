#pragma once
#include <GameEngine/GamePlayer.h>
#include <GameEngine/consts.h>
#include <GameEngine/utils.h>
#include <Raphael/History.h>
#include <Raphael/Killers.h>
#include <Raphael/SEE.h>
#include <Raphael/Transposition.h>
#include <Raphael/consts.h>
#include <Raphael/nnue.h>
#include <math.h>

#include <Raphael/RaphaelParam_v2.0.hpp>
#include <chess.hpp>
#include <chrono>
#include <climits>
#include <iomanip>
#include <iostream>

using std::cout, std::flush, std::fixed, std::setprecision;
using std::max, std::min;
using std::mutex, std::lock_guard;
using std::string;



namespace Raphael {
class v2_0: public cge::GamePlayer {
    // Raphael vars
public:
    struct EngineOptions {
        uint32_t tablesize = DEF_TABLE_SIZE;  // number of entries in tt
    };

    struct SearchOptions {
        int64_t maxnodes = -1;
        int maxdepth = -1;
        int movetime = -1;
        int movestogo = 0;
        bool infinite = false;
    };

protected:
    // search
    chess::Move itermove;     // current iteration's bestmove
    chess::Move prevPlay;     // previous iteration's bestmove
    int consecutives;         // number of consecutive bestmoves
    SearchOptions searchopt;  // limit depth, nodes, or movetime
    v2_0_params params;       // search parameters
    // ponder
    uint64_t ponderkey = 0;  // hash after opponent's best response
    int pondereval = 0;      // eval we got during ponder
    int ponderdepth = 1;     // depth we searched to during ponder
    // storage
    TranspositionTable tt;  // table with position, eval, and bestmove
    Killers killers;        // 2 killer moves at each ply
    History history;        // history score for each move
    // nnue
    Nnue net;
    // info
    int64_t nodes;  // number of nodes visited
    // timing
    std::chrono::time_point<std::chrono::high_resolution_clock> start_t;  // search start time
    int64_t search_t;                                                     // search duration (ms)

    // Raphael methods
public:
    // Initializes Raphael with a name
    v2_0(string name_in): GamePlayer(name_in), tt(DEF_TABLE_SIZE), net("net-2.0.0.nnue") {
        PMASK::init_pawnmask();
    }
    // and with engine options
    v2_0(string name_in, EngineOptions options)
        : GamePlayer(name_in), tt(options.tablesize), net("net-2.0.0.nnue") {
        PMASK::init_pawnmask();
    }

    // Set engine options
    void set_options(EngineOptions options) { tt = TranspositionTable(options.tablesize); }

    // Set search options
    void set_searchoptions(SearchOptions options) { searchopt = options; }

    // Uses iterative deepening on Negamax to find best move
    // Should return immediately if halt becomes true
    chess::Move get_move(
        chess::Board board,
        const int t_remain,
        const int t_inc,
        volatile sf::Event& event,
        volatile bool& halt
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
            alpha = eval - params.ASPIRATION_WINDOW;
            beta = eval + params.ASPIRATION_WINDOW;
        }

        // stop search after an appropriate duration
        startSearchTimer(board, t_remain, t_inc);

        // begin iterative deepening
        while (!halt && depth <= MAX_DEPTH) {
            // max depth override
            if (searchopt.maxdepth != -1 && depth > searchopt.maxdepth) break;

            // stable pv, skip
            if (eval >= params.MIN_SKIP_EVAL && consecutives >= params.PV_STABLE_COUNT
                && !searchopt.infinite)
                halt = true;
            int itereval = negamax(board, depth, 0, params.MAX_EXTENSIONS, alpha, beta, halt);

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
                alpha = eval - params.ASPIRATION_WINDOW;
                beta = eval + params.ASPIRATION_WINDOW;
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
#ifdef UCI
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
                    cout << "bestmove " << chess::uci::moveToUci(itermove) << "\n" << flush;
                }
#else
    #ifndef MUTEEVAL
                // get absolute evaluation (i.e, set to white's perspective)
                {
                    lock_guard<mutex> lock(cout_mutex);
                    char sign = (whiteturn == (eval > 0)) ? '\0' : '-';
                    cout << "Eval: " << sign << "#" << MATE_EVAL - abs(eval) << "\tNodes: " << nodes
                         << "\n"
                         << flush;
                }
    #endif
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
                         << get_pv_line(board, depth - 1) << "\n"
                         << flush;
                }
#endif
            }
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
                 << "\tDepth: " << depth - 1 << "\tNodes: " << nodes << "\n"
                 << flush;
        }
    #endif
#endif
        return itermove;
    }

    // Think during opponent's turn. Should return immediately if halt becomes true
    void ponder(chess::Board board, volatile bool& halt) {
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
            int itereval = negamax(board, ponderdepth, 0, params.MAX_EXTENSIONS, alpha, beta, halt);

            if (!halt) {
                pondereval = itereval;

                // re-search required
                if ((pondereval <= alpha) || (pondereval >= beta)) {
                    alpha = -INT_MAX;
                    beta = INT_MAX;
                    continue;
                }

                // narrow window
                alpha = pondereval - params.ASPIRATION_WINDOW;
                beta = pondereval + params.ASPIRATION_WINDOW;
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
    string get_pv_line(chess::Board board, int depth) {
        // get first move
        auto ttkey = board.hash();
        auto ttentry = tt.get(ttkey, 0);
        chess::Move pvmove;

        string pvline = "";

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
        searchopt = SearchOptions();
    }

protected:
    // Estimates the time (ms) it should spend on searching a move
    // Call this at the start before using isTimeOver
    void startSearchTimer(const chess::Board& board, const int t_remain, const int t_inc) {
        // if movetime is specified, use that instead
        if (searchopt.movetime != -1) {
            search_t = searchopt.movetime;
            start_t = std::chrono::high_resolution_clock::now();
            return;
        }

        // set to infinite if other searchoptions are specified
        if (searchopt.maxdepth != -1 || searchopt.maxnodes != -1 || searchopt.infinite) {
            search_t = 0;
            start_t = std::chrono::high_resolution_clock::now();
            return;
        }

        float n = chess::builtin::popcount(board.occ());
        // 0~1, higher the more time it uses (max at 20 pieces left)
        float ratio = 0.0044f * (n - 32) * (-n / 32) * pow(2.5f + n / 32, 3);
        // use 1~5% of the remaining time based on the ratio + buffered increment
        int duration = t_remain * (0.01f + 0.04f * ratio) + max(t_inc - 30, 0);
        // try to use all of our time if timer resets after movestogo (unless it's 1, then be fast)
        if (searchopt.movestogo > 1) duration += (t_remain - duration) / searchopt.movestogo;
        search_t = min(duration, t_remain);
        start_t = std::chrono::high_resolution_clock::now();
    }

    // Checks if duration (ms) has passed and modifies halt
    // Runs infinitely if search_t is 0
    bool isTimeOver(volatile bool& halt) const {
        // if max nodes is specified, check that instead
        if (searchopt.maxnodes != -1) {
            if (nodes >= searchopt.maxnodes) halt = true;

            // otherwise, check timeover every 2048 nodes
        } else if (search_t && !(nodes & 2047)) {
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
        volatile bool& halt
    ) {
        // timeout
        if (isTimeOver(halt)) return 0;
        nodes++;

        if (ply) {
            // prevent draw in winning positions
            // technically this ignores checkmate on the 50th move
            if (board.isRepetition(1) || board.isHalfMoveDraw()) return 0;

            // mate distance pruning
            alpha = max(alpha, -MATE_EVAL + ply);
            beta = min(beta, MATE_EVAL - ply);
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
                alpha = max(alpha, entry.eval);
            else
                beta = min(beta, entry.eval);

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
            if (extension == 0 && depth >= 3 && movei >= params.REDUCTION_FROM && !istactical) {
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
                tt.set({ttkey, depth, tt.LOWER, move, alpha}, ply);
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
        tt.set({ttkey, depth, flag, bestmove, alpha}, ply);
        return alpha;
    }

    // Quiescence search for all captures
    int quiescence(chess::Board& board, int alpha, int beta, volatile bool& halt) {
        // timeout
        if (isTimeOver(halt)) return 0;
        nodes++;

        // prune with standing pat
        int eval = evaluate(board);
        if (eval >= beta) return beta;
        alpha = max(alpha, eval);

        // search
        chess::Movelist movelist;
        chess::movegen::legalmoves<chess::MoveGenType::CAPTURE>(movelist, board);
        order_moves(movelist, board, 0);

        for (const auto& move : movelist) {
            board.makeMove(move);
            // SEE pruning for losing captures
            if (move.score() < params.GOOD_CAPTURE_WEIGHT && !board.inCheck()) {
                board.unmakeMove(move);
                continue;
            }
            eval = -quiescence(board, -beta, -alpha, halt);
            board.unmakeMove(move);

            // prune
            if (eval >= beta) return beta;
            alpha = max(alpha, eval);
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
            score += abs(params.PVAL[to][1]) - (from % 6);  // MVV/LVA
            score += SEE::goodCapture(move, board, -12) * params.GOOD_CAPTURE_WEIGHT;
        } else {
            // killer move
            if (ply > 0 && killers.isKiller(move, ply)) score += params.KILLER_WEIGHT;
            // history
            score += history.get(move, whiteturn);
        }

        // promotion
        if (move.typeOf() == chess::Move::PROMOTION)
            score += abs(params.PVAL[(int)move.promotionType()][1]);

        move.setScore(score);
    }

    // Evaluates the current position (from the current player's perspective)
    int evaluate(const chess::Board& board) const {
        int eval_mid = 0, eval_end = 0;
        int phase = 0;
        auto pieces = board.occ();

        // draw evaluation
        int wbish_on_w = 0, wbish_on_b = 0;  // number of white bishop on light and dark tiles
        int bbish_on_w = 0, bbish_on_b = 0;  // number of black bishop on light and dark tiles
        int wbish = 0, bbish = 0;
        int wknight = 0, bknight = 0;
        bool minor_only = true;

        // mobility
        int wkr = 0, bkr = 0;          // king rank
        int wkf = 0, bkf = 0;          // king file
        int bishmob = 0, rookmob = 0;  // number of squares bishop and rooks see (white - black)
        // xray bitboards
        auto wbishx = pieces & ~board.pieces(chess::PieceType::QUEEN, chess::Color::WHITE);
        auto bbishx = pieces & ~board.pieces(chess::PieceType::QUEEN, chess::Color::BLACK);
        auto wrookx = wbishx & ~board.pieces(chess::PieceType::ROOK, chess::Color::WHITE);
        auto brookx = bbishx & ~board.pieces(chess::PieceType::ROOK, chess::Color::BLACK);
        auto wpawns = board.pieces(chess::PieceType::PAWN, chess::Color::WHITE);
        auto bpawns = board.pieces(chess::PieceType::PAWN, chess::Color::BLACK);

        // loop through all pieces
        while (pieces) {
            auto sq = chess::builtin::poplsb(pieces);
            int sqi = (int)sq;
            int piece = (int)board.at(sq);

            // add material value
            eval_mid += params.PVAL[piece][0];
            eval_end += params.PVAL[piece][1];
            // add positional value
            eval_mid += params.PST[piece][sqi][0];
            eval_end += params.PST[piece][sqi][1];

            switch (piece) {
                // pawn structure
                case 0:
                    minor_only = false;
                    // passed (+ for white)
                    if ((PMASK::WPASSED[sqi] & bpawns) == 0) {
                        eval_mid += params.PAWN_PASSED_WEIGHT[7 - (sqi / 8)][0];
                        eval_end += params.PAWN_PASSED_WEIGHT[7 - (sqi / 8)][1];
                    }
                    // isolated (- for white)
                    if ((PMASK::ISOLATED[sqi] & wpawns) == 0) {
                        eval_mid -= params.PAWN_ISOLATION_WEIGHT[0];
                        eval_end -= params.PAWN_ISOLATION_WEIGHT[1];
                    }
                    break;
                case 6:
                    minor_only = false;
                    // passed (- for white)
                    if ((PMASK::BPASSED[sqi] & wpawns) == 0) {
                        eval_mid -= params.PAWN_PASSED_WEIGHT[sqi / 8][0];
                        eval_end -= params.PAWN_PASSED_WEIGHT[sqi / 8][1];
                    }
                    // isolated (+ for white)
                    if ((PMASK::ISOLATED[sqi] & bpawns) == 0) {
                        eval_mid += params.PAWN_ISOLATION_WEIGHT[0];
                        eval_end += params.PAWN_ISOLATION_WEIGHT[1];
                    }
                    break;

                // knight count
                case 1:
                    phase++;
                    wknight++;
                    break;
                case 7:
                    phase++;
                    bknight++;
                    break;

                // bishop mobility (xrays queens)
                case 2:
                    phase++;
                    wbish++;
                    wbish_on_w += lighttile(sqi);
                    wbish_on_b += !lighttile(sqi);
                    bishmob += chess::builtin::popcount(chess::attacks::bishop(sq, wbishx));
                    break;
                case 8:
                    phase++;
                    bbish++;
                    bbish_on_w += lighttile(sqi);
                    bbish_on_b += !lighttile(sqi);
                    bishmob -= chess::builtin::popcount(chess::attacks::bishop(sq, bbishx));
                    break;

                // rook mobility (xrays rooks and queens)
                case 3:
                    phase += 2;
                    minor_only = false;
                    rookmob += chess::builtin::popcount(chess::attacks::rook(sq, wrookx));
                    break;
                case 9:
                    phase += 2;
                    minor_only = false;
                    rookmob -= chess::builtin::popcount(chess::attacks::rook(sq, brookx));
                    break;

                // queen count
                case 4:
                    phase += 4;
                    minor_only = false;
                    break;
                case 10:
                    phase += 4;
                    minor_only = false;
                    break;

                // king proximity
                case 5:
                    wkr = (int)chess::utils::squareRank(sq);
                    wkf = (int)chess::utils::squareFile(sq);
                    break;
                case 11:
                    bkr = (int)chess::utils::squareRank(sq);
                    bkf = (int)chess::utils::squareFile(sq);
                    break;
            }
        }

        // mobility
        eval_mid += bishmob * params.MOBILITY_BISHOP[0];
        eval_end += bishmob * params.MOBILITY_BISHOP[1];
        eval_mid += rookmob * params.MOBILITY_ROOK[0];
        eval_end += rookmob * params.MOBILITY_ROOK[1];

        // bishop pair
        bool wbish_pair = wbish_on_w && wbish_on_b;
        bool bbish_pair = bbish_on_w && bbish_on_b;
        if (wbish_pair) {
            eval_mid += params.BISH_PAIR_WEIGHT[0];
            eval_end += params.BISH_PAIR_WEIGHT[1];
        }
        if (bbish_pair) {
            eval_mid -= params.BISH_PAIR_WEIGHT[0];
            eval_end -= params.BISH_PAIR_WEIGHT[1];
        }

        // convert perspective
        if (!whiteturn) {
            eval_mid *= -1;
            eval_end *= -1;
        }

        // King proximity bonus (if winning)
        int king_dist = abs(wkr - bkr) + abs(wkf - bkf);
        if (eval_mid >= 0) eval_mid += params.KING_DIST_WEIGHT[0] * (14 - king_dist);
        if (eval_end >= 0) eval_end += params.KING_DIST_WEIGHT[1] * (14 - king_dist);

        // Bishop corner (if winning)
        int ourbish_on_w = (whiteturn) ? wbish_on_w : bbish_on_w;
        int ourbish_on_b = (whiteturn) ? wbish_on_b : bbish_on_b;
        int ekr = (whiteturn) ? bkr : wkr;
        int ekf = (whiteturn) ? bkf : wkf;
        int wtile_dist = min(ekf + (7 - ekr), (7 - ekf) + ekr);  // to A8 and H1
        int btile_dist = min(ekf + ekr, (7 - ekf) + (7 - ekr));  // to A1 and H8
        if (eval_mid >= 0) {
            if (ourbish_on_w) eval_mid += params.BISH_CORNER_WEIGHT[0] * (7 - wtile_dist);
            if (ourbish_on_b) eval_mid += params.BISH_CORNER_WEIGHT[0] * (7 - btile_dist);
        }
        if (eval_end >= 0) {
            if (ourbish_on_w) eval_end += params.BISH_CORNER_WEIGHT[1] * (7 - wtile_dist);
            if (ourbish_on_b) eval_end += params.BISH_CORNER_WEIGHT[1] * (7 - btile_dist);
        }

        // apply phase
        int eg_weight = 256 * max(0, 24 - phase) / 24;
        int eval = ((256 - eg_weight) * eval_mid + eg_weight * eval_end) / 256;

        // draw division
        int wminor = wbish + wknight;
        int bminor = bbish + bknight;
        if (minor_only && wminor <= 2 && bminor <= 2) {
            if ((wminor == 1 && bminor == 1) ||                                      // 1 vs 1
                ((wbish + bbish == 3) && (wminor + bminor == 3)) ||                  // 2B vs B
                ((wknight == 2 && bminor <= 1) || (bknight == 2 && wminor <= 1)) ||  // 2N vs 0:1
                (!wbish_pair && wminor == 2 && bminor == 1) ||  // 2 vs 1, not bishop pair
                (!bbish_pair && bminor == 2 && wminor == 1))
                return eval / params.DRAW_DIVIDE_SCALE;
        }
        return eval;
    }
};  // Raphael
}  // namespace Raphael
