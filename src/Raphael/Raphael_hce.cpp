#include <GameEngine/consts.h>
#include <Raphael/Raphael_hce.h>
#include <Raphael/SEE.h>
#include <Raphael/consts.h>
#include <math.h>

#include <chess.hpp>
#include <climits>
#include <future>
#include <iomanip>

using namespace Raphael;
using std::async, std::ref;
using std::cout, std::flush;
using std::fixed, std::setprecision;
using std::max, std::min;
using std::mutex, std::lock_guard;
using std::string;
namespace ch = std::chrono;

#define whiteturn (board.sideToMove() == chess::Color::WHITE)
#define lighttile(sqi) (((sq >> 3) ^ sq) & 1)

extern const bool UCI;



string RaphaelHCE::version = "1.8.1.0";

const RaphaelHCE::EngineOptions RaphaelHCE::default_options{
    .hash = {
             .name = "Hash",
             .min = 1,
             .max = TranspositionTable::MAX_TABLE_SIZE * TranspositionTable::ENTRY_SIZE >> 20,
             .value = TranspositionTable::DEF_TABLE_SIZE * TranspositionTable::ENTRY_SIZE >> 20,
             },
};

RaphaelHCE::RaphaelParams::RaphaelParams() {
    init_pst();
    PMASK::init_pawnmask();
}

void RaphaelHCE::RaphaelParams::init_pst() {
    constexpr int PAWN_MID[64] = {
        0,  0,   0,   0,   0,   0,   0,   0,     //
        43, 144, 79,  113, 29,  152, -31, -198,  //
        65, 89,  111, 127, 171, 228, 143, 82,    //
        25, 61,  50,  91,  103, 78,  80,  32,    //
        -3, 7,   44,  67,  74,  61,  47,  7,     //
        10, 11,  38,  37,  62,  59,  90,  42,    //
        5,  21,  5,   20,  35,  91,  99,  31,    //
        0,  0,   0,   0,   0,   0,   0,   0,     //
    };
    constexpr int PAWN_END[64] = {
        0,   0,   0,   0,  0,   0,   0,   0,    //
        183, 160, 123, 83, 123, 100, 163, 203,  //
        85,  70,  46,  1,  -11, 11,  49,  61,   //
        56,  38,  26,  1,  5,   11,  29,  34,   //
        37,  30,  12,  2,  5,   7,   14,  16,   //
        23,  23,  11,  13, 14,  11,  5,   5,    //
        29,  21,  29,  16, 24,  15,  9,   3,    //
        0,   0,   0,   0,  0,   0,   0,   0,    //
    };
    constexpr int KNIGHT_MID[64] = {
        -78, -24, 109, 152, 287, -12, 157, -21,  //
        56,  113, 284, 256, 276, 287, 150, 156,  //
        103, 295, 262, 306, 396, 428, 309, 281,  //
        196, 219, 234, 283, 249, 307, 233, 239,  //
        174, 213, 228, 216, 242, 225, 250, 192,  //
        153, 181, 211, 214, 242, 229, 225, 166,  //
        143, 101, 168, 204, 205, 224, 153, 191,  //
        -17, 173, 139, 159, 195, 197, 173, 154,  //
    };
    constexpr int KNIGHT_END[64] = {
        94,  126, 136, 115, 112, 128, 60,  35,   //
        131, 155, 130, 157, 139, 123, 132, 96,   //
        133, 132, 173, 171, 139, 142, 136, 106,  //
        134, 168, 196, 187, 192, 176, 175, 134,  //
        136, 157, 185, 194, 185, 189, 165, 136,  //
        125, 154, 160, 182, 173, 152, 140, 130,  //
        118, 148, 149, 153, 157, 133, 142, 100,  //
        142, 99,  134, 144, 127, 120, 91,  73,   //
    };
    constexpr int BISHOP_MID[64] = {
        69,  98,  -96, -34, -13, 29,  11,  74,   //
        49,  92,  46,  59,  143, 132, 91,  21,   //
        70,  123, 122, 135, 153, 212, 141, 113,  //
        75,  93,  102, 147, 114, 133, 97,  96,   //
        104, 125, 88,  111, 118, 76,  88,  136,  //
        108, 130, 108, 87,  95,  114, 120, 115,  //
        140, 119, 122, 90,  102, 136, 147, 112,  //
        40,  131, 121, 111, 144, 100, 58,  85,   //
    };
    constexpr int BISHOP_END[64] = {
        93,  95,  110, 111, 119, 109, 97,  80,   //
        107, 115, 123, 103, 105, 106, 109, 105,  //
        121, 101, 112, 101, 98,  102, 118, 113,  //
        107, 124, 120, 118, 115, 117, 113, 115,  //
        108, 105, 126, 125, 114, 112, 111, 99,   //
        104, 112, 118, 122, 120, 111, 104, 102,  //
        95,  89,  103, 106, 117, 98,  98,  84,   //
        97,  106, 86,  107, 100, 100, 111, 102,  //

    };
    constexpr int ROOK_MID[64] = {
        238, 234, 228, 275, 284, 190, 180, 252,  //
        214, 206, 257, 279, 307, 320, 239, 269,  //
        150, 228, 214, 258, 211, 286, 356, 220,  //
        145, 156, 191, 191, 210, 230, 193, 186,  //
        112, 132, 145, 155, 176, 145, 210, 136,  //
        101, 129, 147, 147, 172, 169, 183, 143,  //
        110, 149, 147, 169, 183, 181, 195, 85,   //
        148, 146, 152, 166, 176, 171, 147, 173,  //
    };
    constexpr int ROOK_END[64] = {
        291, 291, 300, 295, 290, 295, 296, 284,  //
        298, 299, 295, 288, 272, 269, 288, 280,  //
        302, 290, 289, 283, 285, 268, 264, 280,  //
        293, 294, 296, 284, 281, 283, 283, 289,  //
        295, 291, 295, 288, 277, 277, 265, 279,  //
        284, 287, 275, 281, 272, 263, 272, 268,  //
        280, 275, 284, 282, 269, 265, 262, 279,  //
        274, 285, 289, 282, 275, 269, 278, 246,  //
    };
    constexpr int QUEEN_MID[64] = {
        613, 599, 613, 621, 778, 814, 757, 662,  //
        578, 541, 618, 618, 617, 745, 646, 718,  //
        601, 591, 613, 619, 688, 765, 752, 714,  //
        578, 587, 588, 590, 609, 631, 627, 627,  //
        604, 566, 596, 595, 607, 608, 633, 621,  //
        595, 628, 609, 611, 601, 626, 626, 628,  //
        555, 612, 639, 631, 631, 647, 615, 671,  //
        618, 593, 607, 640, 600, 584, 539, 549,  //
    };
    constexpr int QUEEN_END[64] = {
        583, 643, 660, 651, 593, 578, 569, 629,  //
        594, 645, 654, 677, 682, 622, 648, 594,  //
        590, 630, 626, 670, 668, 645, 614, 615,  //
        613, 632, 653, 683, 685, 658, 687, 671,  //
        574, 641, 628, 672, 643, 655, 640, 629,  //
        573, 557, 616, 594, 623, 604, 619, 604,  //
        576, 562, 546, 567, 575, 553, 545, 518,  //
        548, 552, 564, 522, 585, 539, 578, 521,  //
    };
    constexpr int KING_MID[64] = {
        -49, 336, 374, 89,   -138, -87,  135,  -25,   //
        283, 40,  -92, 141,  59,   -137, -120, -207,  //
        45,  -31, 5,   -6,   -28,  142,  52,   -31,   //
        6,   -83, -53, -163, -108, -100, -42,  -188,  //
        -97, 17,  -85, -165, -145, -153, -132, -184,  //
        27,  18,  -55, -107, -113, -105, -35,  -75,   //
        55,  17,  -45, -119, -86,  -40,  32,   53,    //
        -35, 71,  50,  -113, 30,   -40,  92,   64,    //
    };
    constexpr int KING_END[64] = {
        -108, -106, -90, -41, 1,   19,  0,   -41,  // A8, B8, ...
        -55,  29,   32,  22,  35,  62,  56,  33,   //
        15,   35,   39,  31,  39,  58,  64,  25,   //
        -4,   42,   46,  55,  45,  56,  46,  17,   //
        -14,  1,    41,  47,  49,  46,  24,  4,    //
        -30,  0,    24,  38,  43,  33,  15,  -4,   //
        -45,  -11,  17,  28,  28,  14,  -10, -37,  //
        -77,  -51,  -33, -9,  -44, -18, -53, -88,  // A1, B1, ...
    };
    const int* PST_MID[6] = {
        PAWN_MID,
        KNIGHT_MID,
        BISHOP_MID,
        ROOK_MID,
        QUEEN_MID,
        KING_MID,
    };
    const int* PST_END[6] = {
        PAWN_END,
        KNIGHT_END,
        BISHOP_END,
        ROOK_END,
        QUEEN_END,
        KING_END,
    };

    for (int p = 0; p < 6; p++) {
        for (int sq = 0; sq < 64; sq++) {
            // flip to put sq56 -> A1 and so on
            PST[p][sq][0] = PST_MID[p][sq ^ 56];
            PST[p][sq][1] = PST_END[p][sq ^ 56];
            // flip for black
            PST[p + 6][sq][0] = -PST_MID[p][sq];
            PST[p + 6][sq][1] = -PST_END[p][sq];
        }
    }
}


RaphaelHCE::RaphaelHCE(string name_in): GamePlayer(name_in), tt(default_options.hash.value) {}


void RaphaelHCE::set_option(SetSpinOption option) {
    if (option.name == "Hash") tt = TranspositionTable(option.value);
}

void RaphaelHCE::set_searchoptions(SearchOptions options) { searchopt = options; }


chess::Move RaphaelHCE::get_move(
    chess::Board board,
    const int t_remain,
    const int t_inc,
    volatile cge::MouseInfo& mouse,
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
    start_search_timer(board, t_remain, t_inc);

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
            if (UCI) {
                auto now = ch::high_resolution_clock::now();
                auto dtime = ch::duration_cast<ch::milliseconds>(now - start_t).count();
                auto nps = (dtime) ? nodes * 1000 / dtime : 0;
                const char* sign = (eval >= 0) ? "" : "-";
                lock_guard<mutex> lock(cout_mutex);
                cout << "info depth " << depth - 1 << " time " << dtime << " nodes " << nodes
                     << " score mate " << sign << MATE_EVAL - abs(eval) << " nps " << nps << " pv "
                     << get_pv_line(board, depth - 1) << "\n";
                cout << "bestmove " << chess::uci::moveToUci(itermove) << "\n" << flush;
            }
#ifndef MUTEEVAL
            else {
                // get absolute evaluation (i.e, set to white's perspective)
                const char* sign = (eval >= 0) ? "" : "-";
                lock_guard<mutex> lock(cout_mutex);
                cout << "Eval: " << sign << "#" << MATE_EVAL - abs(eval) << "\tNodes: " << nodes
                     << "\n"
                     << flush;
            }
#endif
            halt = true;
            return itermove;
        } else if (UCI) {
            auto now = ch::high_resolution_clock::now();
            auto dtime = ch::duration_cast<ch::milliseconds>(now - start_t).count();
            auto nps = (dtime) ? nodes * 1000 / dtime : 0;
            lock_guard<mutex> lock(cout_mutex);
            cout << "info depth " << depth - 1 << " time " << dtime << " nodes " << nodes
                 << " score cp " << eval << " nps " << nps << " pv "
                 << get_pv_line(board, depth - 1) << "\n"
                 << flush;
        }
    }

    if (UCI) {
        lock_guard<mutex> lock(cout_mutex);
        cout << "bestmove " << chess::uci::moveToUci(itermove) << "\n" << flush;
    }
#ifndef MUTEEVAL
    else {
        // get absolute evaluation (i.e, set to white's perspective)
        if (!whiteturn) eval *= -1;
        lock_guard<mutex> lock(cout_mutex);
        cout << "Eval: " << fixed << setprecision(2) << eval / 100.0f << "\tDepth: " << depth - 1
             << "\tNodes: " << nodes << "\n"
             << flush;
    }
#endif
    return itermove;
}

void RaphaelHCE::ponder(chess::Board board, volatile bool& halt) {
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


string RaphaelHCE::get_pv_line(chess::Board board, int depth) const {
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


void RaphaelHCE::reset() {
    tt.clear();
    killers.clear();
    history.clear();
    itermove = chess::Move::NO_MOVE;
    prevPlay = chess::Move::NO_MOVE;
    consecutives = 0;
    searchopt = SearchOptions();
}


void RaphaelHCE::start_search_timer(
    const chess::Board& board, const int t_remain, const int t_inc
) {
    // if movetime is specified, use that instead
    if (searchopt.movetime != -1) {
        search_t = searchopt.movetime;
        start_t = ch::high_resolution_clock::now();
        return;
    }

    // set to infinite if other searchoptions are specified
    if (searchopt.maxdepth != -1 || searchopt.maxnodes != -1 || searchopt.infinite) {
        search_t = 0;
        start_t = ch::high_resolution_clock::now();
        return;
    }

    float n = chess::builtin::popcount(board.occ());
    // 0~1, higher the more time it uses (max at 20 pieces left)
    float ratio = 0.0044f * (n - 32) * (-n / 32) * pow(2.5f + n / 32, 3);
    // use 1~5% of the remaining time based on the ratio + buffered increment
    int duration = t_remain * (0.01f + 0.04f * ratio) + max(t_inc - 30, 1);
    // try to use all of our time if timer resets after movestogo (unless it's 1, then be fast)
    if (searchopt.movestogo > 1) duration += (t_remain - duration) / searchopt.movestogo;
    search_t = min(duration, t_remain);
    start_t = ch::high_resolution_clock::now();
}

bool RaphaelHCE::is_time_over(volatile bool& halt) const {
    // if max nodes is specified, check that instead
    if (searchopt.maxnodes != -1) {
        if (nodes >= searchopt.maxnodes) halt = true;

        // otherwise, check timeover every 2048 nodes
    } else if (search_t && !(nodes & 2047)) {
        auto now = ch::high_resolution_clock::now();
        auto dtime = ch::duration_cast<ch::milliseconds>(now - start_t).count();
        if (dtime >= search_t) halt = true;
    }
    return halt;
}


int RaphaelHCE::negamax(
    chess::Board& board,
    const int depth,
    const int ply,
    const int ext,
    int alpha,
    int beta,
    volatile bool& halt
) {
    // timeout
    if (is_time_over(halt)) return 0;
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
    chess::Movelist quietlist;
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
        bool is_quiet = !board.isCapture(move) && move.typeOf() != chess::Move::PROMOTION;
        if (is_quiet) quietlist.add(move);

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
        if (extension == 0 && depth >= 3 && movei >= params.REDUCTION_FROM && is_quiet) {
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
            // store killer move and update history
            if (is_quiet) {
                killers.put(move, ply);
                history.update(move, quietlist, depth, whiteturn);
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

int RaphaelHCE::quiescence(chess::Board& board, int alpha, int beta, volatile bool& halt) {
    // timeout
    if (is_time_over(halt)) return 0;
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


void RaphaelHCE::order_moves(
    chess::Movelist& movelist, const chess::Board& board, const int ply
) const {
    for (auto& move : movelist) score_move(move, board, ply);
    movelist.sort();
}

void RaphaelHCE::score_move(chess::Move& move, const chess::Board& board, const int ply) const {
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
        if (ply > 0 && killers.is_killer(move, ply)) score += params.KILLER_WEIGHT;
        // history
        score += history.get(move, whiteturn);
    }

    // promotion
    if (move.typeOf() == chess::Move::PROMOTION)
        score += abs(params.PVAL[(int)move.promotionType()][1]);

    move.setScore(score);
}

int RaphaelHCE::evaluate(const chess::Board& board) const {
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
