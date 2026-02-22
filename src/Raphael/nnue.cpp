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

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

INCBIN(unsigned char, netfile, TOSTRING(NETWORK_FILE));



i32 Nnue::NnueFeature::index(chess::Color perspective, bool mirror) const {
    const auto sq = (mirror) ? square.mirrored() : square;
    return 64 * piece.relative(perspective) + sq.relative(perspective);
}



Nnue::NnueAccumulator::NnueAccumulator() {}


bool Nnue::NnueAccumulator::is_clean() const { return !(dirty || needs_refresh); }


void Nnue::NnueAccumulator::add_piece(chess::Piece piece, chess::Square square) {
    assert(n_adds < 2);
    adds[n_adds++] = {.piece = piece, .square = square};
}

void Nnue::NnueAccumulator::rem_piece(chess::Piece piece, chess::Square square) {
    assert(n_subs < 2);
    subs[n_subs++] = {.piece = piece, .square = square};
}

void Nnue::NnueAccumulator::move_piece(chess::Piece piece, chess::Square from, chess::Square to) {
    rem_piece(piece, from);
    add_piece(piece, to);
}


void Nnue::NnueAccumulator::update(
    const NnueAccumulator& old_acc, const i16* weights, chess::Color perspective, bool mirror
) {
    assert(dirty);
    assert(!old_acc.dirty);
    assert(!old_acc.needs_refresh);

    i32 add1 = (n_adds >= 1) ? adds[0].index(perspective, mirror) : -1;
    i32 add2 = (n_adds >= 2) ? adds[1].index(perspective, mirror) : -1;
    i32 sub1 = (n_subs >= 1) ? subs[0].index(perspective, mirror) : -1;
    i32 sub2 = (n_subs >= 2) ? subs[1].index(perspective, mirror) : -1;

#ifdef USE_SIMD
    constexpr i32 regw = ALIGNMENT / sizeof(i16);
    static_assert(N_HIDDEN % regw == 0);
    static_assert(N_HIDDEN == 64);

    // TODO: once N_HIDDEN>=128:
    // constexpr i32 n_chunks = N_HIDDEN / regw;
    // static_assert(n_chunks % 8 == 0);
    // for (i32 i = 0; i < n_chunks; n_chunks += 8) { ... }  // loop unroll by 8 instead of 4

    const int i = 0;
    VecI16 acc0 = load_i16(&old_acc.values[(i + 0) * regw]);
    VecI16 acc1 = load_i16(&old_acc.values[(i + 1) * regw]);
    VecI16 acc2 = load_i16(&old_acc.values[(i + 2) * regw]);
    VecI16 acc3 = load_i16(&old_acc.values[(i + 3) * regw]);

    if (n_subs >= 1) {
        acc0 = subs_i16(acc0, load_i16(&weights[sub1 * N_HIDDEN + (i + 0) * regw]));
        acc1 = subs_i16(acc1, load_i16(&weights[sub1 * N_HIDDEN + (i + 1) * regw]));
        acc2 = subs_i16(acc2, load_i16(&weights[sub1 * N_HIDDEN + (i + 2) * regw]));
        acc3 = subs_i16(acc3, load_i16(&weights[sub1 * N_HIDDEN + (i + 3) * regw]));
    }

    if (n_subs >= 2) {
        acc0 = subs_i16(acc0, load_i16(&weights[sub2 * N_HIDDEN + (i + 0) * regw]));
        acc1 = subs_i16(acc1, load_i16(&weights[sub2 * N_HIDDEN + (i + 1) * regw]));
        acc2 = subs_i16(acc2, load_i16(&weights[sub2 * N_HIDDEN + (i + 2) * regw]));
        acc3 = subs_i16(acc3, load_i16(&weights[sub2 * N_HIDDEN + (i + 3) * regw]));
    }

    if (n_adds >= 1) {
        acc0 = adds_i16(acc0, load_i16(&weights[add1 * N_HIDDEN + (i + 0) * regw]));
        acc1 = adds_i16(acc1, load_i16(&weights[add1 * N_HIDDEN + (i + 1) * regw]));
        acc2 = adds_i16(acc2, load_i16(&weights[add1 * N_HIDDEN + (i + 2) * regw]));
        acc3 = adds_i16(acc3, load_i16(&weights[add1 * N_HIDDEN + (i + 3) * regw]));
    }

    if (n_adds >= 2) {
        acc0 = adds_i16(acc0, load_i16(&weights[add2 * N_HIDDEN + (i + 0) * regw]));
        acc1 = adds_i16(acc1, load_i16(&weights[add2 * N_HIDDEN + (i + 1) * regw]));
        acc2 = adds_i16(acc2, load_i16(&weights[add2 * N_HIDDEN + (i + 2) * regw]));
        acc3 = adds_i16(acc3, load_i16(&weights[add2 * N_HIDDEN + (i + 3) * regw]));
    }

    store_i16(&values[(i + 0) * regw], acc0);
    store_i16(&values[(i + 1) * regw], acc1);
    store_i16(&values[(i + 2) * regw], acc2);
    store_i16(&values[(i + 3) * regw], acc3);
#else
    for (i32 i = 0; i < N_HIDDEN; i++) {
        values[i] = old_acc.values[i];
        if (n_subs >= 1) values[i] -= weights[sub1 * N_HIDDEN + i];
        if (n_subs >= 2) values[i] -= weights[sub2 * N_HIDDEN + i];
        if (n_adds >= 1) values[i] += weights[add1 * N_HIDDEN + i];
        if (n_adds >= 2) values[i] += weights[add2 * N_HIDDEN + i];
    }
#endif

    n_adds = 0;
    n_subs = 0;
    dirty = false;
}



Nnue::Nnue(): params(load_network()) {}

const Nnue::NnueParams* Nnue::load_network() {
    constexpr usize padded_size = 64 * ((sizeof(NnueParams) + 63) / 64);
    if (g_netfile_size != padded_size)
        throw runtime_error("network file and architecture doesn't match");

    if (reinterpret_cast<uintptr_t>(g_netfile_data) % alignof(NnueParams) != 0)
        throw runtime_error("network file isn't aligned properly");

    return reinterpret_cast<const NnueParams*>(g_netfile_data);
}


void Nnue::refresh_color(
    NnueAccumulator& acc, const chess::Board& board, chess::Color perspective
) {
    NnueFeature features[32];
    i32 n_features;

    // TODO: horizontal mirroring
    const bool mirror = false;

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
    static_assert(N_HIDDEN % regw == 0);
    // TODO: once N_HIDDEN>=128:
    // constexpr i32 n_chunks = N_HIDDEN / regw;
    // static_assert(n_chunks % 8 == 0);
    // for (i32 i = 0; i < n_chunks; n_chunks += 8) { ... }  // loop unroll by 8 instead of 4

    // copy bias
    const int i = 0;
    VecI16 acc0 = load_i16(&params->b0[(i + 0) * regw]);
    VecI16 acc1 = load_i16(&params->b0[(i + 1) * regw]);
    VecI16 acc2 = load_i16(&params->b0[(i + 2) * regw]);
    VecI16 acc3 = load_i16(&params->b0[(i + 3) * regw]);
    // store if N_HIDDEN>128

    // add features
    for (i32 f = 0; f < n_features; f++) {
        const auto fidx = features[f].index(perspective, mirror);

        // load if N_HIDDEN>128
        acc0 = adds_i16(acc0, load_i16(&params->W0[fidx * N_HIDDEN + (i + 0) * regw]));
        acc1 = adds_i16(acc1, load_i16(&params->W0[fidx * N_HIDDEN + (i + 1) * regw]));
        acc2 = adds_i16(acc2, load_i16(&params->W0[fidx * N_HIDDEN + (i + 2) * regw]));
        acc3 = adds_i16(acc3, load_i16(&params->W0[fidx * N_HIDDEN + (i + 3) * regw]));
        // store if N_HIDDEN>128
    }

    // store (remove if N_HIDDEN>128)
    store_i16(&acc.values[(i + 0) * regw], acc0);
    store_i16(&acc.values[(i + 1) * regw], acc1);
    store_i16(&acc.values[(i + 2) * regw], acc2);
    store_i16(&acc.values[(i + 3) * regw], acc3);
#else
    copy(params->b0, params->b0 + N_HIDDEN, acc.values);

    for (i32 f = 0; f < n_features; f++) {
        const auto fidx = features[f].index(perspective, mirror);
        for (i32 i = 0; i < N_HIDDEN; i++) values[i] += params->W0[fidx * N_HIDDEN + i];
    }
#endif

    acc.needs_refresh = false;
}



i32 Nnue::evaluate(i32 ply, chess::Color color) {
    // lazy update accumulators
    for (const auto perspective : {chess::Color::WHITE, chess::Color::BLACK}) {
        i32 p = ply;
        while (!accumulators[p][perspective].is_clean()) p--;
        if (accumulators[++p][perspective].needs_refresh) {
            refresh_color(accumulators[p][perspective], boar)
        }
    }
    i32 p = ply;
    while (accumulators[p][chess::Color::WHITE].di)

        while (accumulator_states[p].dirty) p--;
    while (p++ < ply) {
        auto& state = accumulator_states[p];
        update_accumulator(
            accumulators[p],
            accumulators[p - 1],
            state.add1[chess::Color::WHITE],
            state.add2[chess::Color::WHITE],
            state.rem1[chess::Color::WHITE],
            state.rem2[chess::Color::WHITE],
            chess::Color::WHITE
        );
        update_accumulator(
            accumulators[p],
            accumulators[p - 1],
            state.add1[chess::Color::BLACK],
            state.add2[chess::Color::BLACK],
            state.rem1[chess::Color::BLACK],
            state.rem2[chess::Color::BLACK],
            chess::Color::BLACK
        );
        state.dirty = false;
    }

    // get address to accumulators and weights
    const auto us_base = accumulators[ply][color];
    const auto them_base = accumulators[ply][~color];
    const auto us_w_base = params->W1;
    const auto them_w_base = params->W1 + N_HIDDEN;

#ifdef USE_SIMD
    constexpr i32 regw = ALIGNMENT / sizeof(i16);
    static_assert(N_HIDDEN % regw == 0);
    constexpr i32 n_chunks = N_HIDDEN / regw;

    VecI32 sum = zeros;
    for (i32 i = 0; i < n_chunks; i++) {
        const VecI16 us = load_i16(&us_base[i * regw]);
        const VecI16 them = load_i16(&them_base[i * regw]);
        const VecI16 us_w = load_i16(&us_w_base[i * regw]);
        const VecI16 them_w = load_i16(&them_w_base[i * regw]);

        const VecI16 us_clamped = clamp_i16(us, zeros, qas);
        const VecI16 them_clamped = clamp_i16(them, zeros, qas);

        const VecI32 us_screlu = madd_i16(mul_i16(us_w, us_clamped), us_clamped);
        const VecI32 them_screlu = madd_i16(mul_i16(them_w, them_clamped), them_clamped);

        sum = add_i32(sum, add_i32(us_screlu, them_screlu));
    }

    const i32 eval = QA * params->b1 + hadd_i32(sum);
    return (eval / QA) * OUTPUT_SCALE / (QA * QB);
#else
    i32 eval = QA * params->b1;

    // compute W1 dot SCReLU(acc)
    for (i32 i = 0; i < N_HIDDEN; i++) {
        const i32 us_clamped = min(max((i32)us_base[i], 0), QA);
        const i32 them_clamped = min(max((i32)them_base[i], 0), QA);

        eval += us_w_base[i] * us_clamped * us_clamped;
        eval += them_w_base[i] * them_clamped * them_clamped;
    }

    return (eval / QA) * OUTPUT_SCALE / (QA * QB);
#endif
}

void Nnue::set_board(const chess::Board& board) {
    refresh_color(accumulators[0][chess::Color::WHITE], board, chess::Color::WHITE);
    refresh_color(accumulators[0][chess::Color::BLACK], board, chess::Color::BLACK);
}

void Nnue::make_move(const chess::Board& board, chess::Move move, i32 ply) {
    // FIXME:
    // assert((ply != 0));

    // auto& state = accumulator_states[ply];
    // state.dirty = true;

    // // nullmove
    // if (move == move.NULL_MOVE) {
    //     state.add1[chess::Color::WHITE] = -1;
    //     state.add2[chess::Color::WHITE] = -1;
    //     state.rem1[chess::Color::WHITE] = -1;
    //     state.rem2[chess::Color::WHITE] = -1;
    //     state.add1[chess::Color::BLACK] = -1;
    //     state.add2[chess::Color::BLACK] = -1;
    //     state.rem1[chess::Color::BLACK] = -1;
    //     state.rem2[chess::Color::BLACK] = -1;
    //     return;
    // }

    // // incremental update
    // const auto move_type = move.type();
    // const auto stm = board.stm();
    // const auto from_sq = move.from();
    // const auto to_sq = move.to();
    // const auto from_piece = board.at(from_sq);
    // const auto to_piece = board.at(to_sq);
    // assert(from_piece != chess::Piece::NONE);

    // // remove moving piece
    // state.add1[chess::Color::WHITE] = -1;
    // state.add2[chess::Color::WHITE] = -1;
    // state.rem1[chess::Color::WHITE] = 64 * from_piece + from_sq;
    // state.rem2[chess::Color::WHITE] = -1;
    // state.add1[chess::Color::BLACK] = -1;
    // state.add2[chess::Color::BLACK] = -1;
    // state.rem1[chess::Color::BLACK] = 64 * from_piece.color_flipped() + from_sq.flipped();
    // state.rem2[chess::Color::BLACK] = -1;

    // // add moved piece to add_features (handle promotion and castling)
    // if (move_type == chess::Move::PROMOTION) {
    //     const auto promo = chess::Piece(move.promotion_type(), stm);
    //     state.add1[chess::Color::WHITE] = 64 * promo + to_sq;
    //     state.add1[chess::Color::BLACK] = 64 * promo.color_flipped() + to_sq.flipped();
    // } else if (move_type == chess::Move::CASTLING) {
    //     assert(from_piece.type() == chess::PieceType::KING);
    //     assert(to_piece.type() == chess::PieceType::ROOK);

    //     bool is_king_side = to_sq > from_sq;
    //     const auto king_dest = chess::Square::castling_king_dest(is_king_side, stm);
    //     const auto rook_dest = chess::Square::castling_rook_dest(is_king_side, stm);
    //     state.add1[chess::Color::WHITE] = 64 * from_piece + king_dest;
    //     state.add2[chess::Color::WHITE] = 64 * to_piece + rook_dest;
    //     state.add1[chess::Color::BLACK] = 64 * from_piece.color_flipped() + king_dest.flipped();
    //     state.add2[chess::Color::BLACK] = 64 * to_piece.color_flipped() + rook_dest.flipped();
    // } else {
    //     state.add1[chess::Color::WHITE] = 64 * from_piece + to_sq;
    //     state.add1[chess::Color::BLACK] = 64 * from_piece.color_flipped() + to_sq.flipped();
    // }

    // // add captured piece (castling is treated as rook capture) to rem_features
    // if (to_piece != chess::Piece::NONE) {
    //     state.rem2[chess::Color::WHITE] = 64 * to_piece + to_sq;
    //     state.rem2[chess::Color::BLACK] = 64 * to_piece.color_flipped() + to_sq.flipped();
    // } else if (move_type == chess::Move::ENPASSANT) {
    //     const auto ep_pawn = chess::Piece(chess::PieceType::PAWN, ~stm);
    //     const auto ep_sq = to_sq.ep_square();
    //     state.rem2[chess::Color::WHITE] = 64 * ep_pawn + ep_sq;
    //     state.rem2[chess::Color::BLACK] = 64 * ep_pawn.color_flipped() + ep_sq.flipped();
    // }

    // // king refresh FIXME:
    // if (false && from_piece.type() == chess::PieceType::KING
    //     && (from_sq.file() > chess::File::D) != (to_sq.file() > chess::File::D)) {
    //     vector<i32> features;

    //     auto pieces = board.occ().unset(from_sq).unset(to_sq);
    //     while (pieces) {
    //         const auto sq = static_cast<chess::Square>(pieces.poplsb());
    //         const auto piece = board.at(sq);
    //         features.push_back(64 * piece.relative(stm) + sq.relative(stm).mirrored());
    //     }

    //     refresh_accumulator(accumulators[ply], features, stm);
    // }
}
