#include <Raphael/nnue.h>

#define INCBIN_PREFIX g_
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#include <thirdparty/incbin.h>

#include <stdexcept>

using namespace raphael;
using std::copy;
using std::max;
using std::min;
using std::runtime_error;
using std::vector;

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

INCBIN(unsigned char, netfile, TOSTRING(NETWORK_FILE));



i32 Nnue::NnueFeature::index(chess::Color perspective, bool mirror) const {
    const auto sq = (mirror) ? square.mirrored() : square;
    return 64 * piece.relative(perspective) + sq.relative(perspective);
}



Nnue::NnueAccumulator::NnueAccumulator() {}


bool Nnue::NnueAccumulator::dirty() const {
    assert((n_adds > 0) == (n_subs > 0));
    return n_adds > 0;
}


void Nnue::NnueAccumulator::add_piece(chess::Piece piece, chess::Square square) {
    assert(n_adds < 2);
    adds[n_adds++] = {.piece = piece, .square = square};
}

void Nnue::NnueAccumulator::rem_piece(chess::Piece piece, chess::Square square) {
    assert(n_subs < 2);
    subs[n_subs++] = {.piece = piece, .square = square};
}

void Nnue::NnueAccumulator::reset_updates() {
    n_adds = 0;
    n_subs = 0;
}


void Nnue::NnueAccumulator::update(
    const NnueAccumulator& old_acc, const i16* weights, chess::Color perspective, bool mirror
) {
    assert(dirty());
    assert(!old_acc.dirty());

    i32 add1 = adds[0].index(perspective, mirror);
    i32 add2 = adds[1].index(perspective, mirror);
    i32 sub1 = subs[0].index(perspective, mirror);
    i32 sub2 = subs[1].index(perspective, mirror);

#ifdef USE_SIMD
    constexpr i32 regw = ALIGNMENT / sizeof(i16);
    constexpr i32 n_chunks = N_HIDDEN / regw;
    static_assert(N_HIDDEN % regw == 0);
    static_assert(n_chunks % 8 == 0);

    for (i32 i = 0; i < n_chunks; i += 8) {
        // copy old_acc
        VecI16 acc0 = load_i16(&old_acc.values[(i + 0) * regw]);
        VecI16 acc1 = load_i16(&old_acc.values[(i + 1) * regw]);
        VecI16 acc2 = load_i16(&old_acc.values[(i + 2) * regw]);
        VecI16 acc3 = load_i16(&old_acc.values[(i + 3) * regw]);
        VecI16 acc4 = load_i16(&old_acc.values[(i + 4) * regw]);
        VecI16 acc5 = load_i16(&old_acc.values[(i + 5) * regw]);
        VecI16 acc6 = load_i16(&old_acc.values[(i + 6) * regw]);
        VecI16 acc7 = load_i16(&old_acc.values[(i + 7) * regw]);

        acc0 = subs_i16(acc0, load_i16(&weights[sub1 * N_HIDDEN + (i + 0) * regw]));
        acc1 = subs_i16(acc1, load_i16(&weights[sub1 * N_HIDDEN + (i + 1) * regw]));
        acc2 = subs_i16(acc2, load_i16(&weights[sub1 * N_HIDDEN + (i + 2) * regw]));
        acc3 = subs_i16(acc3, load_i16(&weights[sub1 * N_HIDDEN + (i + 3) * regw]));
        acc4 = subs_i16(acc4, load_i16(&weights[sub1 * N_HIDDEN + (i + 4) * regw]));
        acc5 = subs_i16(acc5, load_i16(&weights[sub1 * N_HIDDEN + (i + 5) * regw]));
        acc6 = subs_i16(acc6, load_i16(&weights[sub1 * N_HIDDEN + (i + 6) * regw]));
        acc7 = subs_i16(acc7, load_i16(&weights[sub1 * N_HIDDEN + (i + 7) * regw]));

        if (n_subs > 1) {
            acc0 = subs_i16(acc0, load_i16(&weights[sub2 * N_HIDDEN + (i + 0) * regw]));
            acc1 = subs_i16(acc1, load_i16(&weights[sub2 * N_HIDDEN + (i + 1) * regw]));
            acc2 = subs_i16(acc2, load_i16(&weights[sub2 * N_HIDDEN + (i + 2) * regw]));
            acc3 = subs_i16(acc3, load_i16(&weights[sub2 * N_HIDDEN + (i + 3) * regw]));
            acc4 = subs_i16(acc4, load_i16(&weights[sub2 * N_HIDDEN + (i + 4) * regw]));
            acc5 = subs_i16(acc5, load_i16(&weights[sub2 * N_HIDDEN + (i + 5) * regw]));
            acc6 = subs_i16(acc6, load_i16(&weights[sub2 * N_HIDDEN + (i + 6) * regw]));
            acc7 = subs_i16(acc7, load_i16(&weights[sub2 * N_HIDDEN + (i + 7) * regw]));
        }

        acc0 = adds_i16(acc0, load_i16(&weights[add1 * N_HIDDEN + (i + 0) * regw]));
        acc1 = adds_i16(acc1, load_i16(&weights[add1 * N_HIDDEN + (i + 1) * regw]));
        acc2 = adds_i16(acc2, load_i16(&weights[add1 * N_HIDDEN + (i + 2) * regw]));
        acc3 = adds_i16(acc3, load_i16(&weights[add1 * N_HIDDEN + (i + 3) * regw]));
        acc4 = adds_i16(acc4, load_i16(&weights[add1 * N_HIDDEN + (i + 4) * regw]));
        acc5 = adds_i16(acc5, load_i16(&weights[add1 * N_HIDDEN + (i + 5) * regw]));
        acc6 = adds_i16(acc6, load_i16(&weights[add1 * N_HIDDEN + (i + 6) * regw]));
        acc7 = adds_i16(acc7, load_i16(&weights[add1 * N_HIDDEN + (i + 7) * regw]));

        if (n_adds > 1) {
            acc0 = adds_i16(acc0, load_i16(&weights[add2 * N_HIDDEN + (i + 0) * regw]));
            acc1 = adds_i16(acc1, load_i16(&weights[add2 * N_HIDDEN + (i + 1) * regw]));
            acc2 = adds_i16(acc2, load_i16(&weights[add2 * N_HIDDEN + (i + 2) * regw]));
            acc3 = adds_i16(acc3, load_i16(&weights[add2 * N_HIDDEN + (i + 3) * regw]));
            acc4 = adds_i16(acc4, load_i16(&weights[add2 * N_HIDDEN + (i + 4) * regw]));
            acc5 = adds_i16(acc5, load_i16(&weights[add2 * N_HIDDEN + (i + 5) * regw]));
            acc6 = adds_i16(acc6, load_i16(&weights[add2 * N_HIDDEN + (i + 6) * regw]));
            acc7 = adds_i16(acc7, load_i16(&weights[add2 * N_HIDDEN + (i + 7) * regw]));
        }

        // store into self
        store_i16(&values[(i + 0) * regw], acc0);
        store_i16(&values[(i + 1) * regw], acc1);
        store_i16(&values[(i + 2) * regw], acc2);
        store_i16(&values[(i + 3) * regw], acc3);
        store_i16(&values[(i + 4) * regw], acc4);
        store_i16(&values[(i + 5) * regw], acc5);
        store_i16(&values[(i + 6) * regw], acc6);
        store_i16(&values[(i + 7) * regw], acc7);
    }
#else
    for (i32 i = 0; i < N_HIDDEN; i++) {
        values[i] = old_acc.values[i];

        values[i] -= weights[sub1 * N_HIDDEN + i];
        if (n_subs > 1) values[i] -= weights[sub2 * N_HIDDEN + i];
        values[i] += weights[add1 * N_HIDDEN + i];
        if (n_adds > 1) values[i] += weights[add2 * N_HIDDEN + i];
    }
#endif

    reset_updates();
}

void Nnue::NnueAccumulator::refresh(
    const i16* weights, const i16* biases, const chess::Board& board, chess::Color perspective
) {
    NnueFeature features[32];
    i32 n_features;

    // horizontal mirroring
    const bool mirror = board.king_square(perspective).file() > chess::File::D;

    // get features
    auto pieces = board.occ();
    while (pieces) {
        const auto square = static_cast<chess::Square>(pieces.poplsb());
        const auto piece = board.at(square);

        assert(n_features < 32);
        features[n_features++] = {.piece = piece, .square = square};
    }

    // refresh accumulator
#ifdef USE_SIMD
    constexpr i32 regw = ALIGNMENT / sizeof(i16);
    constexpr i32 n_chunks = N_HIDDEN / regw;
    static_assert(N_HIDDEN % regw == 0);
    static_assert(n_chunks % 8 == 0);

    for (i32 i = 0; i < n_chunks; i += 8) {
        // copy bias
        VecI16 acc0 = load_i16(&biases[(i + 0) * regw]);
        VecI16 acc1 = load_i16(&biases[(i + 1) * regw]);
        VecI16 acc2 = load_i16(&biases[(i + 2) * regw]);
        VecI16 acc3 = load_i16(&biases[(i + 3) * regw]);
        VecI16 acc4 = load_i16(&biases[(i + 4) * regw]);
        VecI16 acc5 = load_i16(&biases[(i + 5) * regw]);
        VecI16 acc6 = load_i16(&biases[(i + 6) * regw]);
        VecI16 acc7 = load_i16(&biases[(i + 7) * regw]);

        // add features
        for (i32 f = 0; f < n_features; f++) {
            const auto fidx = features[f].index(perspective, mirror);

            acc0 = adds_i16(acc0, load_i16(&weights[fidx * N_HIDDEN + (i + 0) * regw]));
            acc1 = adds_i16(acc1, load_i16(&weights[fidx * N_HIDDEN + (i + 1) * regw]));
            acc2 = adds_i16(acc2, load_i16(&weights[fidx * N_HIDDEN + (i + 2) * regw]));
            acc3 = adds_i16(acc3, load_i16(&weights[fidx * N_HIDDEN + (i + 3) * regw]));
            acc4 = adds_i16(acc4, load_i16(&weights[fidx * N_HIDDEN + (i + 4) * regw]));
            acc5 = adds_i16(acc5, load_i16(&weights[fidx * N_HIDDEN + (i + 5) * regw]));
            acc6 = adds_i16(acc6, load_i16(&weights[fidx * N_HIDDEN + (i + 6) * regw]));
            acc7 = adds_i16(acc7, load_i16(&weights[fidx * N_HIDDEN + (i + 7) * regw]));
        }

        // store into self
        store_i16(&values[(i + 0) * regw], acc0);
        store_i16(&values[(i + 1) * regw], acc1);
        store_i16(&values[(i + 2) * regw], acc2);
        store_i16(&values[(i + 3) * regw], acc3);
        store_i16(&values[(i + 4) * regw], acc4);
        store_i16(&values[(i + 5) * regw], acc5);
        store_i16(&values[(i + 6) * regw], acc6);
        store_i16(&values[(i + 7) * regw], acc7);
    }
#else
    copy(biases, biases + N_HIDDEN, values);

    for (i32 f = 0; f < n_features; f++) {
        const auto fidx = features[f].index(perspective, mirror);
        for (i32 i = 0; i < N_HIDDEN; i++) values[i] += weights[fidx * N_HIDDEN + i];
    }
#endif

    reset_updates();
}



Nnue::Nnue(): params(load_network()), idx_(0) {}

const Nnue::NnueParams* Nnue::load_network() {
    constexpr usize padded_size = 64 * ((sizeof(NnueParams) + 63) / 64);
    if (g_netfile_size != padded_size)
        throw runtime_error("network file and architecture doesn't match");

    if (reinterpret_cast<uintptr_t>(g_netfile_data) % alignof(NnueParams) != 0)
        throw runtime_error("network file isn't aligned properly");

    return reinterpret_cast<const NnueParams*>(g_netfile_data);
}



i32 Nnue::evaluate(const chess::Board& board) {
    // apply lazy updates
    lazy_update(board, chess::Color::WHITE);
    lazy_update(board, chess::Color::BLACK);

    // get address to accumulators and weights
    const auto us_acc = accumulators[idx_][board.stm()];
    const auto them_acc = accumulators[idx_][~board.stm()];
    const auto us_w_base = params->W1;
    const auto them_w_base = params->W1 + N_HIDDEN;
    assert(!us_acc.dirty());
    assert(!them_acc.dirty());

#ifdef USE_SIMD
    constexpr i32 regw = ALIGNMENT / sizeof(i16);
    constexpr i32 n_chunks = N_HIDDEN / regw;

    VecI32 sum = zero_i16();
    for (i32 i = 0; i < n_chunks; i++) {
        const VecI16 us = load_i16(&us_acc.values[i * regw]);
        const VecI16 them = load_i16(&them_acc.values[i * regw]);
        const VecI16 us_w = load_i16(&us_w_base[i * regw]);
        const VecI16 them_w = load_i16(&them_w_base[i * regw]);

        const VecI16 us_clamped = clamp_i16(us, zero_i16(), full_i16(QA));
        const VecI16 them_clamped = clamp_i16(them, zero_i16(), full_i16(QA));

        const VecI32 us_screlu = madd_i16(mul_i16(us_w, us_clamped), us_clamped);
        const VecI32 them_screlu = madd_i16(mul_i16(them_w, them_clamped), them_clamped);

        sum = add_i32(sum, add_i32(us_screlu, them_screlu));
    }

    i32 eval = QA * params->b1 + hadd_i32(sum);
#else
    i32 eval = QA * params->b1;

    // compute W1 dot SCReLU(acc)
    for (i32 i = 0; i < N_HIDDEN; i++) {
        const i32 us_clamped = min(max((i32)us_acc.values[i], 0), QA);
        const i32 them_clamped = min(max((i32)them_acc.values[i], 0), QA);

        eval += us_w_base[i] * us_clamped * us_clamped;
        eval += them_w_base[i] * them_clamped * them_clamped;
    }
#endif

    eval /= QA;
    eval *= OUTPUT_SCALE;
    eval /= (QA * QB);
    return eval;
}


void Nnue::set_board(const chess::Board& board) {
    idx_ = 0;
    accumulators[idx_][chess::Color::WHITE].refresh(
        params->W0, params->b0, board, chess::Color::WHITE
    );
    accumulators[idx_][chess::Color::BLACK].refresh(
        params->W0, params->b0, board, chess::Color::BLACK
    );
}

void Nnue::make_move(
    const chess::Board& old_board, const chess::Board& new_board, chess::Move move
) {
    assert(idx_ < MAX_DEPTH - 1);
    idx_++;

    const auto from_sq = move.from();
    const auto to_sq = move.to();
    const auto from_piece = old_board.at(from_sq);
    const auto to_piece = old_board.at(to_sq);
    assert(from_piece != chess::Piece::NONE);

    // reset updates so we can overwrite them
    accumulators[idx_][chess::Color::WHITE].reset_updates();
    accumulators[idx_][chess::Color::BLACK].reset_updates();

    // remove moving piece
    accumulators[idx_][chess::Color::WHITE].rem_piece(from_piece, from_sq);
    accumulators[idx_][chess::Color::BLACK].rem_piece(from_piece, from_sq);

    // add moved/promoted piece
    if (move.type() == chess::Move::PROMOTION) {
        const auto promo = chess::Piece(move.promotion_type(), old_board.stm());
        accumulators[idx_][chess::Color::WHITE].add_piece(promo, to_sq);
        accumulators[idx_][chess::Color::BLACK].add_piece(promo, to_sq);
    } else if (move.type() == chess::Move::CASTLING) {
        assert(from_piece.type() == chess::PieceType::KING);
        assert(to_piece.type() == chess::PieceType::ROOK);

        const bool is_king_side = to_sq > from_sq;
        const auto king_sq = chess::Square::castling_king_dest(is_king_side, old_board.stm());
        const auto rook_sq = chess::Square::castling_rook_dest(is_king_side, old_board.stm());
        accumulators[idx_][chess::Color::WHITE].add_piece(from_piece, king_sq);
        accumulators[idx_][chess::Color::BLACK].add_piece(from_piece, king_sq);
        accumulators[idx_][chess::Color::WHITE].add_piece(to_piece, rook_sq);
        accumulators[idx_][chess::Color::BLACK].add_piece(to_piece, rook_sq);
    } else {
        accumulators[idx_][chess::Color::WHITE].add_piece(from_piece, to_sq);
        accumulators[idx_][chess::Color::BLACK].add_piece(from_piece, to_sq);
    }

    // add captured piece/ep pawn/castling rook
    if (to_piece != chess::Piece::NONE) {
        accumulators[idx_][chess::Color::WHITE].rem_piece(to_piece, to_sq);
        accumulators[idx_][chess::Color::BLACK].rem_piece(to_piece, to_sq);
    } else if (move.type() == chess::Move::ENPASSANT) {
        assert(from_piece.type() == chess::PieceType::PAWN);

        const auto ep_pawn = from_piece.color_flipped();
        const auto ep_sq = to_sq.ep_square();
        accumulators[idx_][chess::Color::WHITE].rem_piece(ep_pawn, ep_sq);
        accumulators[idx_][chess::Color::BLACK].rem_piece(ep_pawn, ep_sq);
    }

    // refresh stm on king mirror change
    if (from_piece.type() == chess::PieceType::KING
        && (from_sq.file() > chess::File::D) != (to_sq.file() > chess::File::D))
        accumulators[idx_][old_board.stm()].refresh(
            params->W0, params->b0, new_board, old_board.stm()
        );
}

void Nnue::unmake_move() {
    assert(idx_ > 0);
    idx_--;
}


void Nnue::lazy_update(const chess::Board& board, chess::Color perspective) {
    // find clean accumulator
    i32 clean_idx = idx_;
    while (accumulators[clean_idx][perspective].dirty()) clean_idx--;

    // horizontal mirroring
    const bool mirror = board.king_square(perspective).file() > chess::File::D;

    // update up the stack
    while (clean_idx++ < idx_)
        accumulators[clean_idx][perspective].update(
            accumulators[clean_idx - 1][perspective], params->W0, perspective, mirror
        );
}
