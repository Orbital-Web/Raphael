#include <GameEngine/consts.h>
#include <Raphael/Raphaelv1.0.h>
#include <Raphael/consts.h>
#include <math.h>

#include <chess.hpp>
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

extern const bool UCI;



v1_0::RaphaelParams::RaphaelParams() { init_pst(); }

void v1_0::RaphaelParams::init_pst() {
    // https://www.chessprogramming.org/PeSTO%27s_Evaluation_Function
    constexpr int PAWN_MID[64] = {
        0,   0,   0,   0,   0,   0,   0,  0,    //
        98,  134, 61,  95,  68,  126, 34, -11,  //
        -6,  7,   26,  31,  65,  56,  25, -20,  //
        -14, 13,  6,   21,  23,  12,  17, -23,  //
        -27, -2,  -5,  12,  17,  6,   10, -25,  //
        -26, -4,  -4,  -10, 3,   3,   33, -12,  //
        -35, -1,  -20, -23, -15, 24,  38, -22,  //
        0,   0,   0,   0,   0,   0,   0,  0,    //
    };
    constexpr int PAWN_END[64] = {
        0,   0,   0,   0,   0,   0,   0,   0,    //
        178, 173, 158, 134, 147, 132, 165, 187,  //
        94,  100, 85,  67,  56,  53,  82,  84,   //
        32,  24,  13,  5,   -2,  4,   17,  17,   //
        13,  9,   -3,  -7,  -7,  -8,  3,   -1,   //
        4,   7,   -6,  1,   0,   -5,  -1,  -8,   //
        13,  8,   8,   10,  13,  0,   2,   -7,   //
        0,   0,   0,   0,   0,   0,   0,   0,    //
    };
    constexpr int KNIGHT_MID[64] = {
        -167, -89, -34, -49, 61,  -97, -15, -107,  //
        -73,  -41, 72,  36,  23,  62,  7,   -17,   //
        -47,  60,  37,  65,  84,  129, 73,  44,    //
        -9,   17,  19,  53,  37,  69,  18,  22,    //
        -13,  4,   16,  13,  28,  19,  21,  -8,    //
        -23,  -9,  12,  10,  19,  17,  25,  -16,   //
        -29,  -53, -12, -3,  -1,  18,  -14, -19,   //
        -105, -21, -58, -33, -17, -28, -19, -23,   //
    };
    constexpr int KNIGHT_END[64] = {
        -58, -38, -13, -28, -31, -27, -63, -99,  //
        -25, -8,  -25, -2,  -9,  -25, -24, -52,  //
        -24, -20, 10,  9,   -1,  -9,  -19, -41,  //
        -17, 3,   22,  22,  22,  11,  8,   -18,  //
        -18, -6,  16,  25,  16,  17,  4,   -18,  //
        -23, -3,  -1,  15,  10,  -3,  -20, -22,  //
        -42, -20, -10, -5,  -2,  -20, -23, -44,  //
        -29, -51, -23, -15, -22, -18, -50, -64,  //
    };
    constexpr int BISHOP_MID[64] = {
        -29, 4,  -82, -37, -25, -42, 7,   -8,   //
        -26, 16, -18, -13, 30,  59,  18,  -47,  //
        -16, 37, 43,  40,  35,  50,  37,  -2,   //
        -4,  5,  19,  50,  37,  37,  7,   -2,   //
        -6,  13, 13,  26,  34,  12,  10,  4,    //
        0,   15, 15,  15,  14,  27,  18,  10,   //
        4,   15, 16,  0,   7,   21,  33,  1,    //
        -33, -3, -14, -21, -13, -12, -39, -21,  //
    };
    constexpr int BISHOP_END[64] = {
        -14, -21, -11, -8,  -7, -9,  -17, -24,  //
        -8,  -4,  7,   -12, -3, -13, -4,  -14,  //
        2,   -8,  0,   -1,  -2, 6,   0,   4,    //
        -3,  9,   12,  9,   14, 10,  3,   2,    //
        -6,  3,   13,  19,  7,  10,  -3,  -9,   //
        -12, -3,  8,   10,  13, 3,   -7,  -15,  //
        -14, -18, -7,  -1,  4,  -9,  -15, -27,  //
        -23, -9,  -23, -5,  -9, -16, -5,  -17,  //
    };
    constexpr int ROOK_MID[64] = {
        32,  42,  32,  51,  63, 9,  31,  43,   //
        27,  32,  58,  62,  80, 67, 26,  44,   //
        -5,  19,  26,  36,  17, 45, 61,  16,   //
        -24, -11, 7,   26,  24, 35, -8,  -20,  //
        -36, -26, -12, -1,  9,  -7, 6,   -23,  //
        -45, -25, -16, -17, 3,  0,  -5,  -33,  //
        -44, -16, -20, -9,  -1, 11, -6,  -71,  //
        -19, -13, 1,   17,  16, 7,  -37, -26,  //
    };
    constexpr int ROOK_END[64] = {
        13, 10, 18, 15, 12, 12,  8,   5,    //
        11, 13, 13, 11, -3, 3,   8,   3,    //
        7,  7,  7,  5,  4,  -3,  -5,  -3,   //
        4,  3,  13, 1,  2,  1,   -1,  2,    //
        3,  5,  8,  4,  -5, -6,  -8,  -11,  //
        -4, 0,  -5, -1, -7, -12, -8,  -16,  //
        -6, -6, 0,  2,  -9, -9,  -11, -3,   //
        -9, 2,  3,  -1, -5, -13, 4,   -20,  //
    };
    constexpr int QUEEN_MID[64] = {
        -28, 0,   29,  12,  59,  44,  43,  45,   //
        -24, -39, -5,  1,   -16, 57,  28,  54,   //
        -13, -17, 7,   8,   29,  56,  47,  57,   //
        -27, -27, -16, -16, -1,  17,  -2,  1,    //
        -9,  -26, -9,  -10, -2,  -4,  3,   -3,   //
        -14, 2,   -11, -2,  -5,  2,   14,  5,    //
        -35, -8,  11,  2,   8,   15,  -3,  1,    //
        -1,  -18, -9,  10,  -15, -25, -31, -50,  //
    };
    constexpr int QUEEN_END[64] = {
        -9,  22,  22,  27,  27,  19,  10,  20,   //
        -17, 20,  32,  41,  58,  25,  30,  0,    //
        -20, 6,   9,   49,  47,  35,  19,  9,    //
        3,   22,  24,  45,  57,  40,  57,  36,   //
        -18, 28,  19,  47,  31,  34,  39,  23,   //
        -16, -27, 15,  6,   9,   17,  10,  5,    //
        -22, -23, -30, -16, -16, -23, -36, -32,  //
        -33, -28, -22, -43, -5,  -32, -20, -41,  //
    };
    constexpr int KING_MID[64] = {
        -65, 23,  16,  -15, -56, -34, 2,   13,   //
        29,  -1,  -20, -7,  -8,  -4,  -38, -29,  //
        -9,  24,  2,   -16, -20, 6,   22,  -22,  //
        -17, -20, -12, -27, -30, -25, -14, -36,  //
        -49, -1,  -27, -39, -46, -44, -33, -51,  //
        -14, -14, -22, -46, -44, -30, -15, -27,  //
        1,   7,   -8,  -64, -43, -16, 9,   8,    //
        -15, 36,  12,  -54, 8,   -28, 24,  14,   //
    };
    constexpr int KING_END[64] = {
        -74, -35, -18, -18, -11, 15,  4,   -17,  // A8, B8, ...
        -12, 17,  14,  17,  17,  38,  23,  11,   //
        10,  17,  23,  15,  20,  45,  44,  13,   //
        -8,  22,  24,  27,  26,  33,  26,  3,    //
        -18, -4,  21,  24,  27,  23,  9,   -11,  //
        -19, -3,  11,  21,  23,  16,  7,   -9,   //
        -27, -11, 4,   13,  14,  4,   -5,  -17,  //
        -53, -34, -21, -11, -28, -14, -24, -43,  // A1, B1, ...
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


v1_0::v1_0(string name_in): GamePlayer(name_in), tt(DEF_TABLE_SIZE) {}
v1_0::v1_0(string name_in, EngineOptions options): GamePlayer(name_in), tt(options.tablesize) {}


void v1_0::set_options(EngineOptions options) { tt = TranspositionTable(options.tablesize); }


chess::Move v1_0::get_move(
    chess::Board board,
    const int t_remain,
    const int t_inc,
    volatile cge::MouseInfo& mouse,
    volatile bool& halt
) {
    tt.clear();
    int depth = 1;
    int eval = 0;
    toPlay = chess::Move::NO_MOVE;
    itermove = chess::Move::NO_MOVE;

    // stop search after an appropriate duration
    int duration = search_time(board, t_remain, t_inc);
    auto _ = async(manage_time, ref(halt), duration);

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
#ifndef MUTEEVAL
            if (!UCI) {
                // get absolute evaluation (i.e, set to white's perspective)
                lock_guard<mutex> lock(cout_mutex);
                if (whiteturn == (eval > 0))
                    cout << "Eval: #\n" << flush;
                else
                    cout << "Eval: -#\n" << flush;
            }
#endif
            halt = true;
            return toPlay;
        }
        depth++;
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
             << "\n"
             << flush;
    }
#endif
    return toPlay;
}


void v1_0::reset() { tt.clear(); }


int v1_0::search_time(const chess::Board& board, const int t_remain, const int t_inc) {
    // ratio: a function within [0, 1]
    // uses 0.5~4% of the remaining time (max at 11 pieces left)
    float n = chess::builtin::popcount(board.occ());
    float ratio = 0.0138f * (32 - n) * (n / 32) * pow(2.5f - n / 32, 3);
    // use 0.5~4% of the remaining time based on the ratio + buffered increment
    int duration = t_remain * (0.005f + 0.035f * ratio) + max(t_inc - 30, 1);
    return min(duration, t_remain);
}

void v1_0::manage_time(volatile bool& halt, const int duration) {
    auto start = ch::high_resolution_clock::now();
    while (!halt) {
        auto now = ch::high_resolution_clock::now();
        auto dtime = ch::duration_cast<ch::milliseconds>(now - start).count();
        if (dtime >= duration) halt = true;
    }
}


int v1_0::negamax(
    chess::Board& board, int depth, int ply, int alpha, int beta, volatile bool& halt
) {
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

int v1_0::quiescence(chess::Board& board, int alpha, int beta, volatile bool& halt) const {
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


void v1_0::order_moves(chess::Movelist& movelist, const chess::Board& board) const {
    chess::movegen::legalmoves(movelist, board);
    for (auto& move : movelist) score_move(move, board);
    movelist.sort();
}

void v1_0::order_cap_moves(chess::Movelist& movelist, const chess::Board& board) const {
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

void v1_0::score_move(chess::Move& move, const chess::Board& board) const {
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
        score += abs(params.PVAL[to]) - abs(params.PVAL[from]) + 13;  // bias encourage trades

    // promotion
    if (move.typeOf() == chess::Move::PROMOTION)
        score += abs(params.PVAL[(int)move.promotionType()]);

    move.setScore(score);
}

int v1_0::evaluate(const chess::Board& board) const {
    int eval = 0;
    int n_pieces_left = chess::builtin::popcount(board.occ());
    double eg_weight = min(1.0, double(32 - n_pieces_left) / (32 - params.N_PIECES_END));
    int wkr = 0, bkr = 0, wkf = 0, bkf = 0;

    // count pieces and added their values (material + pst)
    for (int sqi = 0; sqi < 64; sqi++) {
        auto sq = (chess::Square)sqi;
        int piece = (int)board.at(sq);

        // non-empty
        if (piece != 12) {
            // add material value
            eval += params.PVAL[piece];
            // add positional value
            eval += params.PST[piece][sqi][0]
                    + eg_weight * (params.PST[piece][sqi][1] - params.PST[piece][sqi][0]);
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
        eval += (14 - kingdist) * params.KING_DIST_WEIGHT * eg_weight;
    }

    return eval;
}
