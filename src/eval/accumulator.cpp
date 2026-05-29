#ifdef EVAL_NNUE
#include <eval/accumulator.h>

using namespace raphael::nnue;
using std::copy;



i32 PSQFeature::index(chess::Color perspective, bool mirror) const {
    const auto sq = (mirror) ? square.mirrored() : square;
    const auto pc = (piece.type() == chess::PieceType::KING) ? chess::Piece::WHITEKING
                                                             : piece.relative(perspective);
    return 64 * pc + sq.relative(perspective);
}



void NnueFinnyEntry::initialize(const i16 biases[L1_SIZE]) {
    copy(biases, biases + L1_SIZE, values);
}

void NnueFinnyEntry::sync(
    const i16 weights[N_INPUTS][L1_SIZE],
    const chess::Board& board,
    chess::Color perspective,
    bool mirror
) {
    i32 adds[32];
    i32 subs[32];
    i32 n_adds = 0;
    i32 n_subs = 0;

    // compute diff from finny_entry
    for (chess::PieceType pt = chess::PieceType::PAWN; pt <= chess::PieceType::KING; ++pt) {
        for (const auto color : {chess::Color::WHITE, chess::Color::BLACK}) {
            const auto old_occ = occ(pt, color);
            const auto new_occ = board.occ(pt, color);

            auto adds_occ = new_occ & ~old_occ;
            while (adds_occ) {
                const auto sq = chess::Square(adds_occ.poplsb());
                const auto piece = chess::Piece(pt, color);

                assert(n_adds < 32);
                adds[n_adds++] = PSQFeature(piece, sq).index(perspective, mirror);
            }

            auto subs_occ = old_occ & ~new_occ;
            while (subs_occ) {
                const auto sq = chess::Square(subs_occ.poplsb());
                const auto piece = chess::Piece(pt, color);

                assert(n_subs < 32);
                subs[n_subs++] = PSQFeature(piece, sq).index(perspective, mirror);
            }
        }
    }

    // update occupancy bitboards
    for (chess::PieceType pt = chess::PieceType::PAWN; pt <= chess::PieceType::KING; ++pt)
        pieces_[pt] = board.occ(pt);
    for (const auto color : {chess::Color::WHITE, chess::Color::BLACK})
        occ_[color] = board.occ(color);

#ifdef USE_SIMD
    constexpr i32 regw = ALIGNMENT / sizeof(i16);
    constexpr i32 n_chunks = L1_SIZE / regw;
    static_assert(L1_SIZE % regw == 0);
    static_assert(n_chunks % 8 == 0);
    VecI16 accs[8];

    for (i32 i = 0; i < n_chunks; i += 8) {
        #pragma GCC unroll 32
        for (i32 r = 0; r < 8; r++) accs[r] = load_i16(&values[(i + r) * regw]);

        // add features
        for (i32 f = 0; f < n_adds; f++) {
            const auto fidx = adds[f];

            #pragma GCC unroll 32
            for (i32 r = 0; r < 8; r++)
                accs[r] = add_i16(accs[r], load_i16(&weights[fidx][(i + r) * regw]));
        }

        // rem features
        for (i32 f = 0; f < n_subs; f++) {
            const auto fidx = subs[f];

            #pragma GCC unroll 32
            for (i32 r = 0; r < 8; r++)
                accs[r] = sub_i16(accs[r], load_i16(&weights[fidx][(i + r) * regw]));
        }

        #pragma GCC unroll 32
        for (i32 r = 0; r < 8; r++) store_i16(&values[(i + r) * regw], accs[r]);
    }
#else
    for (i32 f = 0; f < n_adds; f++)
        for (i32 i = 0; i < L1_SIZE; i++) values[i] += weights[adds[f]][i];
    for (i32 f = 0; f < n_subs; f++)
        for (i32 i = 0; i < L1_SIZE; i++) values[i] -= weights[subs[f]][i];
#endif
}

chess::BitBoard NnueFinnyEntry::occ(chess::PieceType pt, chess::Color color) const {
    return pieces_[pt] & occ_[color];
}



NnueAccumulator::PSQState NnueAccumulator::get_psq_state(chess::Color perspective) const {
    return psq_state[perspective];
}

void NnueAccumulator::set_psq_state(chess::Color perspective, PSQState state) {
    psq_state[perspective] = state;
}

void NnueAccumulator::add_piece(chess::Piece piece, chess::Square square) {
    psq_adds.push({.piece = piece, .square = square});
}

void NnueAccumulator::rem_piece(chess::Piece piece, chess::Square square) {
    psq_subs.push({.piece = piece, .square = square});
}

void NnueAccumulator::prepare_updates() {
    // reset psq updates and mark as dirty (as we're going to update them immediately after)
    psq_adds.clear();
    psq_subs.clear();
    set_psq_state(chess::Color::WHITE, PSQState::DIRTY);
    set_psq_state(chess::Color::BLACK, PSQState::DIRTY);
}

void NnueAccumulator::apply_updates(
    const NnueAccumulator& old_acc,
    const i16 weights[N_INPUTS][L1_SIZE],
    chess::Color perspective,
    bool mirror
) {
    assert(get_psq_state(perspective) == PSQState::DIRTY);
    assert(old_acc.get_psq_state(perspective) == PSQState::CLEAN);
    assert(psq_adds.size() >= 1);
    assert(psq_subs.size() >= 1);

    const i32 add1 = psq_adds[0].index(perspective, mirror);
    const i32 add2 = (psq_adds.size() > 1) ? psq_adds[1].index(perspective, mirror) : 0;
    const i32 sub1 = psq_subs[0].index(perspective, mirror);
    const i32 sub2 = (psq_subs.size() > 1) ? psq_subs[1].index(perspective, mirror) : 0;

#ifdef USE_SIMD
    constexpr i32 regw = ALIGNMENT / sizeof(i16);
    constexpr i32 n_chunks = L1_SIZE / regw;
    static_assert(L1_SIZE % regw == 0);
    static_assert(n_chunks % 8 == 0);
    VecI16 accs[8];

    for (i32 i = 0; i < n_chunks; i += 8) {
        #pragma GCC unroll 32
        for (i32 r = 0; r < 8; r++)
            accs[r] = load_i16(&old_acc.values[perspective][(i + r) * regw]);

        #pragma GCC unroll 32
        for (i32 r = 0; r < 8; r++)
            accs[r] = sub_i16(accs[r], load_i16(&weights[sub1][(i + r) * regw]));

        if (psq_subs.size() > 1)
            #pragma GCC unroll 32
            for (i32 r = 0; r < 8; r++)
                accs[r] = sub_i16(accs[r], load_i16(&weights[sub2][(i + r) * regw]));

        #pragma GCC unroll 32
        for (i32 r = 0; r < 8; r++)
            accs[r] = add_i16(accs[r], load_i16(&weights[add1][(i + r) * regw]));

        if (psq_adds.size() > 1)
            #pragma GCC unroll 32
            for (i32 r = 0; r < 8; r++)
                accs[r] = add_i16(accs[r], load_i16(&weights[add2][(i + r) * regw]));

        #pragma GCC unroll 32
        for (i32 r = 0; r < 8; r++) store_i16(&values[perspective][(i + r) * regw], accs[r]);
    }
#else
    for (i32 i = 0; i < L1_SIZE; i++) {
        values[perspective][i] = old_acc.values[perspective][i];

        values[perspective][i] -= weights[sub1][i];
        if (psq_subs.size() > 1) values[perspective][i] -= weights[sub2][i];
        values[perspective][i] += weights[add1][i];
        if (psq_adds.size() > 1) values[perspective][i] += weights[add2][i];
    }
#endif

    // mark as clean
    set_psq_state(perspective, PSQState::CLEAN);
}

void NnueAccumulator::refresh_from(const NnueFinnyEntry& finny_entry, chess::Color perspective) {
    copy(finny_entry.values, finny_entry.values + L1_SIZE, values[perspective]);
    set_psq_state(perspective, PSQState::CLEAN);
}
#endif