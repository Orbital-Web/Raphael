// tuned using https://github.com/GediminasMasaitis/texel-tuner
#include "Raphael.h"

#include <cmath>
#include <iomanip>
#include <vector>

#define TAPERED 1

using namespace Raphael;
using std::cout, std::endl;
using std::fixed, std::setprecision;
using std::max, std::min;
using std::round;
using std::string;
using std::vector;


/* --------------------------- Utils --------------------------- */
int PST_PARAM_INDEX[12][64];

void init_pst() {
    int PAWN_IDX[64];
    int KNIGHT_IDX[64];
    int BISHOP_IDX[64];
    int ROOK_IDX[64];
    int QUEEN_IDX[64];
    int KING_IDX[64];

    int i = RaphaelParams::PST_START;
    for (int j = 0; j < 64; j++) PAWN_IDX[j] = i + j;
    i += 64;
    for (int j = 0; j < 64; j++) KNIGHT_IDX[j] = i + j;
    i += 64;
    for (int j = 0; j < 64; j++) BISHOP_IDX[j] = i + j;
    i += 64;
    for (int j = 0; j < 64; j++) ROOK_IDX[j] = i + j;
    i += 64;
    for (int j = 0; j < 64; j++) QUEEN_IDX[j] = i + j;
    i += 64;
    for (int j = 0; j < 64; j++) KING_IDX[j] = i + j;
    i += 64;

    const int* PST_IDX[6] = {PAWN_IDX, KNIGHT_IDX, BISHOP_IDX, ROOK_IDX, QUEEN_IDX, KING_IDX};

    for (int p = 0; p < 6; p++) {
        for (int sq = 0; sq < 64; sq++) {
            // flip to put sq56 -> A1 and so on
            PST_PARAM_INDEX[p][sq] = PST_IDX[p][sq ^ 56];
            PST_PARAM_INDEX[p + 6][sq] = PST_IDX[p][sq];
        }
    }
}


uint64_t WPASSED[64];   // passed pawn mask bitboard for white (A1...H8)
uint64_t BPASSED[64];   // passed pawn mask bitboard for black
uint64_t ISOLATED[64];  // isolated pawn mask bitboard (A1...H8)

void init_pawnmask() {
    uint64_t filemask = 0x0101010101010101;  // A-file
    uint64_t rankregion = 0xFFFFFFFFFF;      // ranks 1~5

    for (int sq = 0; sq < 64; sq++) {
        ISOLATED[sq] = 0;
        int file = sq % 8;
        int rank = sq / 8;

        // left file of pawn
        if (file > 0) ISOLATED[sq] |= filemask << (file - 1);
        // right file of pawn
        if (file < 7) ISOLATED[sq] |= filemask << (file + 1);

        // middle file of pawn for passed mask
        WPASSED[sq] = filemask << file;
        WPASSED[sq] |= ISOLATED[sq];
        // same for black
        BPASSED[sq] = WPASSED[sq];

        // crop ranks above of pawn for white passed
        WPASSED[sq] &= UINT64_MAX << (8 * (rank + 1));
        // crop ranks below of pawn for black passed
        BPASSED[sq] &= UINT64_MAX >> (8 * (8 - rank));

        // crop ±2 ranks of pawn for isolation
        // even if adjacent files are occupied,
        // the pawns must be close to each other
        if (rank > 1)
            ISOLATED[sq] &= rankregion << (8 * (rank - 2));
        else
            ISOLATED[sq] &= rankregion >> (8 * (2 - rank));
    }
}



/* --------------------------- RaphaelParams --------------------------- */
RaphaelParams::operator parameters_t() const {
    parameters_t parameters;  // [midgame, endgame]
    parameters.push_back(pair_t{(double)KING_DIST_WEIGHT[0], (double)KING_DIST_WEIGHT[1]});

    // PVAL
    parameters.push_back(pair_t{(double)KNIGHT[0], (double)KNIGHT[1]});
    parameters.push_back(pair_t{(double)BISHOP[0], (double)BISHOP[1]});
    parameters.push_back(pair_t{(double)ROOK[0], (double)ROOK[1]});
    parameters.push_back(pair_t{(double)QUEEN[0], (double)QUEEN[1]});

    // PST
    assert(parameters.size() == PST_START);
    for (int i = 0; i < 64; i++)
        parameters.push_back(pair_t{(double)PAWN_MID[i], (double)PAWN_END[i]});
    for (int i = 0; i < 64; i++)
        parameters.push_back(pair_t{(double)KNIGHT_MID[i], (double)KNIGHT_END[i]});
    for (int i = 0; i < 64; i++)
        parameters.push_back(pair_t{(double)BISHOP_END[i], (double)BISHOP_END[i]});
    for (int i = 0; i < 64; i++)
        parameters.push_back(pair_t{(double)ROOK_MID[i], (double)ROOK_END[i]});
    for (int i = 0; i < 64; i++)
        parameters.push_back(pair_t{(double)QUEEN_MID[i], (double)QUEEN_END[i]});
    for (int i = 0; i < 64; i++)
        parameters.push_back(pair_t{(double)KING_MID[i], (double)KING_END[i]});

    // PMASK
    assert(parameters.size() == PMASK_START);
    for (int i = 0; i < 7; i++)
        parameters.push_back(pair_t{(double)PASSEDBONUS[i][0], (double)PASSEDBONUS[i][1]});
    parameters.push_back(pair_t{(double)ISOLATION_WEIGHT[0], (double)ISOLATION_WEIGHT[1]});

    // MOBILITY
    assert(parameters.size() == MOB_START);
    parameters.push_back(pair_t{(double)MOBILITY_BISHOP[0], (double)MOBILITY_BISHOP[1]});
    parameters.push_back(pair_t{(double)MOBILITY_ROOK[0], (double)MOBILITY_ROOK[1]});
    parameters.push_back(pair_t{(double)BISH_PAIR_WEIGHT[0], (double)BISH_PAIR_WEIGHT[1]});
    parameters.push_back(pair_t{(double)BISH_CORNER_WEIGHT[0], (double)BISH_CORNER_WEIGHT[1]});

    assert(parameters.size() == PARAM_SIZE);
    return parameters;
}



void RaphaelParams::repr(parameters_t parameters) {
    int i = 0;
    cout << fixed << setprecision(0);
    cout << "KING_DIST_WEIGHT: " << parameters[i][0] << ", " << parameters[i][1] << endl;
    i++;

    // PVAL
    cout << endl;
    cout << "PAWN: " << 100 << ", " << 100 << endl;
    cout << "KNIGHT: " << parameters[i][0] << ", " << parameters[i][1] << endl;
    i++;
    cout << "BISHOP: " << parameters[i][0] << ", " << parameters[i][1] << endl;
    i++;
    cout << "ROOK: " << parameters[i][0] << ", " << parameters[i][1] << endl;
    i++;
    cout << "QUEEN: " << parameters[i][0] << ", " << parameters[i][1] << endl;
    i++;

    // PST
    assert(i == PST_START);
    cout << endl;
    vector<string> psts = {"PAWN", "KNIGHT", "BISHOP", "ROOK", "QUEEN", "KING"};
    for (string& piece : psts) {
        cout << "PST " << piece << " MID: " << endl << "\t";
        for (int j = 0; j < 64; j++) {
            cout << parameters[i + j][0] << ", ";
            if ((j + 1) % 8 == 0) cout << "//\n\t";
        }
        cout << endl;

        cout << "PST " << piece << " END: " << endl << "\t";
        for (int j = 0; j < 64; j++) {
            cout << parameters[i + j][1] << ", ";
            if ((j + 1) % 8 == 0) cout << "//\n\t";
        }
        cout << endl;
        i += 64;
    }

    // PMASK
    assert(i == PMASK_START);
    cout << "PASSEDBONUS: {" << endl;
    for (int j = 0; j < 7; j++)
        cout << parameters[i + j][0] << ", " << parameters[i + j][1] << ",\n";
    cout << "}" << endl;
    i += 7;
    cout << "ISOLATION_WEIGHT: " << parameters[i][0] << ", " << parameters[i][1] << endl;
    i++;

    // MOBILITY
    assert(i == MOB_START);
    cout << "MOB_BISHOP: " << parameters[i][0] << ", " << parameters[i][1] << endl;
    i++;
    cout << "MOB_ROOK: " << parameters[i][0] << ", " << parameters[i][1] << endl;
    i++;
    cout << "BISH_PAIR: " << parameters[i][0] << ", " << parameters[i][1] << endl;
    i++;
    cout << "BISH_CORNER: " << parameters[i][0] << ", " << parameters[i][1] << endl;
    i++;

    cout << "#####################################################################" << endl << endl;

    assert(i == parameters.size());
    assert(i == PARAM_SIZE);
}



/* --------------------------- RaphaelEval --------------------------- */
parameters_t RaphaelEval::get_initial_parameters() {
    // initialize consts
    init_pst();
    init_pawnmask();

    // get default parameters
    RaphaelParams param;
    return parameters_t(param);
}



EvalResult RaphaelEval::get_fen_eval_result(const string& fen, const parameters_t& parameters) {
    chess::Board board(fen);
    return get_external_eval_result(board, parameters);
}



EvalResult RaphaelEval::get_external_eval_result(
    const chess::Board& board, const parameters_t& parameters
) {
    // return white - black coefficients
    EvalResult result;
    result.coefficients = vector<int16_t>(RaphaelParams::PARAM_SIZE, 0);
    result.endgame_scale = 1;

    int add_score = 0;
    auto pieces = board.occ();
    int n_pieces_left = pieces.count();
    int phase = 0;

    // draw evaluation
    int wbish_on_w = 0, wbish_on_b = 0;  // number of white bishop on light and dark tiles
    int bbish_on_w = 0, bbish_on_b = 0;  // number of black bishop on light and dark tiles

    // mobility
    int wkr = 0, bkr = 0;  // king rank
    int wkf = 0, bkf = 0;  // king file
    int bishmob = 0, rookmob = 0;
    auto wbishx = pieces & ~board.pieces(chess::PieceType::QUEEN, chess::Color::WHITE);
    auto bbishx = pieces & ~board.pieces(chess::PieceType::QUEEN, chess::Color::BLACK);
    auto wrookx = wbishx & ~board.pieces(chess::PieceType::ROOK, chess::Color::WHITE);
    auto brookx = bbishx & ~board.pieces(chess::PieceType::ROOK, chess::Color::BLACK);
    auto wpawns = board.pieces(chess::PieceType::PAWN, chess::Color::WHITE);
    auto bpawns = board.pieces(chess::PieceType::PAWN, chess::Color::BLACK);

    // loop through all pieces
    while (pieces) {
        auto sq = pieces.pop();
        int sqi = (int)sq;
        int piece = (int)board.at(sq);

        // add material value (if not King)
        if (piece == 0 || piece == 6)
            add_score += 200 * (piece < 6) - 100;
        else if (piece != 5 || piece != 11)
            result.coefficients[(piece % 6)] += 2 * (piece < 6) - 1;
        // add positional value
        result.coefficients[PST_PARAM_INDEX[piece][sqi]] += 2 * (piece < 6) - 1;

        switch (piece) {
            // pawn structure
            case 0:
                // passed (+ for white) (more important in endgame)
                if ((WPASSED[sqi] & bpawns) == 0)
                    result.coefficients[RaphaelParams::PMASK_START + 7 - (sqi / 8)]++;
                // isolated (- for white)
                if ((ISOLATED[sqi] & wpawns) == 0)
                    result.coefficients[RaphaelParams::PMASK_START + 7]--;
                break;
            case 6:
                // passed (- for white) (more important in endgame)
                if ((BPASSED[sqi] & wpawns) == 0)
                    result.coefficients[RaphaelParams::PMASK_START + (sqi / 8)]--;
                // isolated (+ for white)
                if ((ISOLATED[sqi] & bpawns) == 0)
                    result.coefficients[RaphaelParams::PMASK_START + 7]++;
                break;

            // knight count
            case 1:
            case 7: phase++; break;

            // bishop mobility (xrays queens)
            case 2:
                phase++;
                wbish_on_w += (((sqi >> 3) ^ sqi) & 1);
                wbish_on_b += !(((sqi >> 3) ^ sqi) & 1);
                bishmob += chess::attacks::bishop(sq, wbishx).count();
                break;
            case 8:
                phase++;
                bbish_on_w += (((sqi >> 3) ^ sqi) & 1);
                bbish_on_b += !(((sqi >> 3) ^ sqi) & 1);
                bishmob -= chess::attacks::bishop(sq, bbishx).count();
                break;

            // rook mobility (xrays rooks and queens)
            case 3:
                phase += 2;
                rookmob += chess::attacks::rook(sq, wrookx).count();
                break;
            case 9:
                phase += 2;
                rookmob -= chess::attacks::rook(sq, brookx).count();
                break;

            // queen count
            case 4:
            case 10: phase += 4; break;

            // king proximity
            case 5:
                wkr = (int)(sq >> 3);
                wkf = (int)(sq & 7);
                break;
            case 11:
                bkr = (int)(sq >> 3);
                bkf = (int)(sq & 7);
                break;
        }
    }

    // mobility
    result.coefficients[RaphaelParams::MOB_START + 0] = bishmob;
    result.coefficients[RaphaelParams::MOB_START + 1] = rookmob;

    // bishop pair
    bool wbish_pair = wbish_on_w && wbish_on_b;
    bool bbish_pair = bbish_on_w && bbish_on_b;
    if (wbish_pair) result.coefficients[RaphaelParams::MOB_START + 2]++;
    if (bbish_pair) result.coefficients[RaphaelParams::MOB_START + 2]--;

    // get score
    int eval_mid = add_score;
    int eval_end = add_score;
    for (int i = 0; i < parameters.size(); i++) {
        eval_mid += result.coefficients[i] * parameters[i][0];
        eval_end += result.coefficients[i] * parameters[i][1];
    }

    // convert perspective
    if (board.sideToMove() != chess::Color::WHITE) {
        eval_mid *= -1;
        eval_end *= -1;
    }

    // King proximity bonus (if winning)
    int king_dist = abs(wkr - bkr) + abs(wkf - bkf);
    if (eval_mid >= 0) {
        result.coefficients[0] = 14 - king_dist;
        eval_mid += parameters[0][0] * (14 - king_dist);
    }
    if (eval_end >= 0) {
        result.coefficients[0] = 14 - king_dist;
        eval_end += parameters[0][1] * (14 - king_dist);
    }

    // Bishop corner (if winning)
    int ourbish_on_w = (board.sideToMove() == chess::Color::WHITE) ? wbish_on_w : bbish_on_w;
    int ourbish_on_b = (board.sideToMove() == chess::Color::WHITE) ? wbish_on_b : bbish_on_b;
    int ekr = (board.sideToMove() == chess::Color::WHITE) ? bkr : wkr;
    int ekf = (board.sideToMove() == chess::Color::WHITE) ? bkf : wkf;
    int wtile_dist = min(ekf + (7 - ekr), (7 - ekf) + ekr);  // to A8 and H1
    int btile_dist = min(ekf + ekr, (7 - ekf) + (7 - ekr));  // to A1 and H8
    bool wadd = false, badd = false;
    if (eval_mid >= 0) {
        if (ourbish_on_w) {
            wadd = true;
            eval_mid += parameters[RaphaelParams::MOB_START + 3][0] * (7 - wtile_dist);
        }
        if (ourbish_on_b) {
            badd = true;
            eval_mid += parameters[RaphaelParams::MOB_START + 3][0] * (7 - btile_dist);
        }
    }
    if (eval_end >= 0) {
        if (ourbish_on_w) {
            wadd = true;
            eval_end += parameters[RaphaelParams::MOB_START + 3][1] * (7 - wtile_dist);
        }
        if (ourbish_on_b) {
            badd = true;
            eval_end += parameters[RaphaelParams::MOB_START + 3][1] * (7 - btile_dist);
        }
    }
    if (wadd) result.coefficients[RaphaelParams::MOB_START + 3] += 7 - wtile_dist;
    if (badd) result.coefficients[RaphaelParams::MOB_START + 3] += 7 - btile_dist;

    // apply phase
    int eg_weight = 256 * max(0, 24 - phase) / 24;
    result.score = ((256 - eg_weight) * eval_mid + eg_weight * eval_end) / 256;
    if (board.sideToMove() != chess::Color::WHITE) result.score *= -1;  // readjust to get abs

    return result;
}



void RaphaelEval::print_parameters(const parameters_t& parameters) {
    RaphaelParams::repr(parameters);
}
