#ifdef EVAL_NNUE
#include <eval/accumulator.h>

#include <utility>

using namespace raphael::nnue;
using std::array;
using std::copy;
using std::pair;



namespace internal {
static constexpr i32 PIECE_TARGET_MAP[6][6] = {
    0,  1,  -1, 2,  -1, -1,  // pawn    BQ excluded as P->B/Q implies B/Q->P
    0,  1,  2,  3,  4,  -1,  // knight  no exclusion as other pieces can't attack N if N->piece
    0,  1,  2,  3,  -1, -1,  // bishop  Q excluded as B->Q implies Q->B
    0,  1,  2,  3,  -1, -1,  // rook    Q excluded as R->Q implies Q->R
    0,  1,  2,  3,  4,  -1,  // queen   no exclusion so threats for other pieces get encoded
    -1, -1, -1, -1, -1, -1   // king    no threats to and from king
};

static constexpr i32 PIECE_TARGET_COUNT[6] = {6, 10, 8, 8, 10, 0};  // num included * 2 colors

static constexpr auto compute_piece_indices(chess::Piece piece) {
    // idx[from][to] = number of squares attacked by piece whose square index is less than to square
    MultiArray<u8, 64, 64> idx{};

    for (chess::Square from = chess::Square::A1; from <= chess::Square::H8; ++from) {
        const auto attacks = chess::Attacks::pseudo_attacks(piece, from);

        for (chess::Square to = chess::Square::A1; to <= chess::Square::H8; ++to) {
            const auto mask = attacks & (chess::BitBoard::from_square(to) - 1);
            idx[from][to] = mask.count();
        }
    }

    return idx;
}

static constexpr auto PIECE_INDICES = [] {
    MultiArray<u8, 12, 64, 64> dst{};

    // white and black pawns have different attacks, other pieces are symmetric
    dst[chess::Piece::WHITEPAWN] = compute_piece_indices(chess::Piece::WHITEPAWN);
    dst[chess::Piece::BLACKPAWN] = compute_piece_indices(chess::Piece::BLACKPAWN);

    for (chess::PieceType pt = chess::PieceType::KNIGHT; pt <= chess::PieceType::KING; ++pt) {
        const auto idx = compute_piece_indices(chess::Piece(pt, chess::Color::WHITE));
        dst[chess::Piece(pt, chess::Color::WHITE)] = idx;
        dst[chess::Piece(pt, chess::Color::BLACK)] = idx;
    }

    return dst;
}();

static constexpr auto OFFSETS = [] {
    // indices[piece] = {total num squares this piece can attack, global piece threat offset}
    // offsets[piece][from] = global offset for attacker, attacker square features
    struct {
        array<pair<i32, i32>, 12> indicies{};
        MultiArray<i32, 12, 64> offsets{};
    } dst{};

    i32 offset = 0;

    for (chess::Piece piece = chess::Piece::WHITEPAWN; piece <= chess::Piece::BLACKKING; ++piece) {
        i32 piece_offset = 0;

        for (chess::Square from = chess::Square::A1; from <= chess::Square::H8; ++from) {
            dst.offsets[piece][from] = piece_offset;

            // pawns cannot be on the back rank
            if (piece.type() != chess::PieceType::PAWN || (!from.rank().is_back_rank())) {
                const auto attacks = chess::Attacks::pseudo_attacks(piece.color_flipped(), from);
                piece_offset += attacks.count();
            }
        }

        dst.indicies[piece] = {piece_offset, offset};
        offset += PIECE_TARGET_COUNT[piece.type()] * piece_offset;
    }

    return dst;
}();

static constexpr auto ATTACK_INDICIES = [] {
    // indicies[attacker][attacked][forwards] = global threat index offset
    MultiArray<i32, 12, 12, 2> dst{};

    for (chess::Piece atk = chess::Piece::WHITEPAWN; atk <= chess::Piece::BLACKKING; ++atk) {
        for (chess::Piece vic = chess::Piece::WHITEPAWN; vic <= chess::Piece::BLACKKING; ++vic) {
            const bool is_enemy = atk.color() != vic.color();
            const auto map = PIECE_TARGET_MAP[atk.type()][vic.type()];

            // attacks between the same piece only gets encoded in one direction to avoid duplicates
            const bool is_semi_excluded
                = atk.type() == vic.type() && (is_enemy || atk.type() != chess::PieceType::PAWN);
            const bool is_excluded = map < 0;

            const auto [piece_offset, offset] = OFFSETS.indicies[atk];

            const auto feature_idx
                = offset + (vic.color() * PIECE_TARGET_COUNT[atk.type()] / 2 + map) * piece_offset;

            dst[atk][vic][0] = (is_excluded) ? N_THREATS : feature_idx;
            dst[atk][vic][1] = (is_excluded || is_semi_excluded) ? N_THREATS : feature_idx;
        }
    }

    return dst;
}();
}  // namespace internal



i32 PSQFeature::index(chess::Color perspective, bool mirror) const {
    const auto sq = square.mirrored(mirror).relative(perspective);
    const auto pc = (piece.type() == chess::PieceType::KING) ? chess::Piece::WHITEKING
                                                             : piece.relative(perspective);
    return 64 * pc + sq;
}

i32 TIFeature::index(chess::Color perspective, bool mirror) const {
    const auto atk = attacker.relative(perspective);
    const auto vic = attacked.relative(perspective);
    const auto atk_sq = attacker_sq.mirrored(mirror).relative(perspective);
    const auto vic_sq = attacked_sq.mirrored(mirror).relative(perspective);

    const bool forwards = atk_sq < vic_sq;
    const auto attack_idx = internal::ATTACK_INDICIES[atk][vic][forwards];
    const auto offset = internal::OFFSETS.offsets[atk][atk_sq];
    const auto piece_idx = internal::PIECE_INDICES[atk][atk_sq][vic_sq];

    return attack_idx + offset + piece_idx;
}



void NnueFinnyEntry::initialize(const i16 biases[L1_SIZE]) {
    copy(biases, biases + L1_SIZE, values);
}

void NnueFinnyEntry::sync(
    const i16 weights[N_PSQ][L1_SIZE],
    const chess::Board& board,
    chess::Color perspective,
    bool mirror
) {
    StaticVector<i32, 32> adds;
    StaticVector<i32, 32> subs;

    // compute diff from finny_entry
    for (chess::PieceType pt = chess::PieceType::PAWN; pt <= chess::PieceType::KING; ++pt) {
        for (const auto color : {chess::Color::WHITE, chess::Color::BLACK}) {
            const auto old_occ = occ(pt, color);
            const auto new_occ = board.occ(pt, color);

            auto adds_occ = new_occ & ~old_occ;
            while (adds_occ) {
                const auto sq = chess::Square(adds_occ.poplsb());
                const auto piece = chess::Piece(pt, color);
                adds.push(PSQFeature(piece, sq).index(perspective, mirror));
            }

            auto subs_occ = old_occ & ~new_occ;
            while (subs_occ) {
                const auto sq = chess::Square(subs_occ.poplsb());
                const auto piece = chess::Piece(pt, color);
                subs.push(PSQFeature(piece, sq).index(perspective, mirror));
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
        for (const i32 fidx : adds) {
            #pragma GCC unroll 32
            for (i32 r = 0; r < 8; r++)
                accs[r] = add_i16(accs[r], load_i16(&weights[fidx][(i + r) * regw]));
        }

        // rem features
        for (const i32 fidx : subs) {
            #pragma GCC unroll 32
            for (i32 r = 0; r < 8; r++)
                accs[r] = sub_i16(accs[r], load_i16(&weights[fidx][(i + r) * regw]));
        }

        #pragma GCC unroll 32
        for (i32 r = 0; r < 8; r++) store_i16(&values[(i + r) * regw], accs[r]);
    }
#else
    for (const i32 fidx : adds)
        for (i32 i = 0; i < L1_SIZE; i++) values[i] += weights[fidx][i];
    for (const i32 fidx : subs)
        for (i32 i = 0; i < L1_SIZE; i++) values[i] -= weights[fidx][i];
#endif
}

chess::BitBoard NnueFinnyEntry::occ(chess::PieceType pt, chess::Color color) const {
    return pieces_[pt] & occ_[color];
}



NnueAccumulator::AccState NnueAccumulator::get_psq_state(chess::Color perspective) const {
    return psq_state[perspective];
}

void NnueAccumulator::set_psq_state(chess::Color perspective, AccState state) {
    psq_state[perspective] = state;
}

NnueAccumulator::AccState NnueAccumulator::get_ti_state(chess::Color perspective) const {
    return ti_state[perspective];
}

void NnueAccumulator::set_ti_state(chess::Color perspective, AccState state) {
    ti_state[perspective] = state;
}

void NnueAccumulator::add_psq(chess::Piece piece, chess::Square square) {
    psq_adds.push({.piece = piece, .square = square});
}

void NnueAccumulator::rem_psq(chess::Piece piece, chess::Square square) {
    psq_subs.push({.piece = piece, .square = square});
}

void NnueAccumulator::add_ti(
    chess::Piece attacker,
    chess::Piece attacked,
    chess::Square attacker_sq,
    chess::Square attacked_sq
) {
    ti_adds.push({
        .attacker = attacker,
        .attacked = attacked,
        .attacker_sq = attacker_sq,
        .attacked_sq = attacked_sq,
    });
}

void NnueAccumulator::rem_ti(
    chess::Piece attacker,
    chess::Piece attacked,
    chess::Square attacker_sq,
    chess::Square attacked_sq
) {
    ti_subs.push({
        .attacker = attacker,
        .attacked = attacked,
        .attacker_sq = attacker_sq,
        .attacked_sq = attacked_sq,
    });
}

void NnueAccumulator::prepare_updates() {
    // reset updates and mark as dirty (as we're going to update them immediately after)
    psq_adds.clear();
    psq_subs.clear();
    ti_adds.clear();
    ti_subs.clear();
    set_psq_state(chess::Color::WHITE, AccState::DIRTY);
    set_psq_state(chess::Color::BLACK, AccState::DIRTY);
    set_ti_state(chess::Color::WHITE, AccState::REFRESH);  // FIXME: dirty after adding ue
    set_ti_state(chess::Color::BLACK, AccState::REFRESH);  // here too
}

void NnueAccumulator::apply_psq_updates(
    const NnueAccumulator& old_acc,
    const i16 weights[N_PSQ][L1_SIZE],
    chess::Color perspective,
    bool mirror
) {
    assert(get_psq_state(perspective) == AccState::DIRTY);
    assert(old_acc.get_psq_state(perspective) == AccState::CLEAN);
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
            accs[r] = load_i16(&old_acc.psq_vals[perspective][(i + r) * regw]);

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
        for (i32 r = 0; r < 8; r++) store_i16(&psq_vals[perspective][(i + r) * regw], accs[r]);
    }
#else
    for (i32 i = 0; i < L1_SIZE; i++) {
        psq_vals[perspective][i] = old_acc.psq_vals[perspective][i];

        psq_vals[perspective][i] -= weights[sub1][i];
        if (psq_subs.size() > 1) psq_vals[perspective][i] -= weights[sub2][i];
        psq_vals[perspective][i] += weights[add1][i];
        if (psq_adds.size() > 1) psq_vals[perspective][i] += weights[add2][i];
    }
#endif

    // mark as clean
    set_psq_state(perspective, AccState::CLEAN);
}

void NnueAccumulator::apply_ti_updates(
    const NnueAccumulator& old_acc,
    const i8 weights[N_THREATS][L1_SIZE],
    chess::Color perspective,
    bool mirror
) {
    // FIXME:
    assert(false);
}

void NnueAccumulator::refresh_psq(const NnueFinnyEntry& finny_entry, chess::Color perspective) {
    copy(finny_entry.values, finny_entry.values + L1_SIZE, psq_vals[perspective]);
    set_psq_state(perspective, AccState::CLEAN);
}

void NnueAccumulator::refresh_ti(
    const i8 weights[N_THREATS][L1_SIZE],
    const chess::Board& board,
    chess::Color perspective,
    bool mirror
) {
    StaticVector<i32, 128> features;

    // skip kings as we don't encode king threats
    const auto occ = board.occ();
    const auto nonkings = occ ^ chess::BitBoard::from_square(board.king_square(perspective));

    auto from_occ = nonkings;
    while (from_occ) {
        const auto from = static_cast<chess::Square>(from_occ.poplsb());
        const auto attacker = board.at(from);

        auto attacked = nonkings & chess::Attacks::attacks(attacker, from, occ);
        while (attacked) {
            const auto to = static_cast<chess::Square>(attacked.poplsb());
            const auto victim = board.at(to);
            const i32 feature = TIFeature(attacker, victim, from, to).index(perspective, mirror);

            if (feature < N_THREATS) features.push(feature);
        }
    }

#ifdef USE_SIMD
    constexpr i32 regw = ALIGNMENT / sizeof(i16);
    constexpr i32 n_chunks = L1_SIZE / regw;
    static_assert(L1_SIZE % regw == 0);
    static_assert(n_chunks % 8 == 0);
    VecI16 accs[8];

    for (i32 i = 0; i < n_chunks; i += 8) {
        #pragma GCC unroll 32
        for (i32 r = 0; r < 8; r++) accs[r] = zero_i16();

        // add features
        for (const i32 fidx : features) {
            #pragma GCC unroll 32
            for (i32 r = 0; r < 4; r++) {
                const VecI8 ws = load_i8(&weights[fidx][(i + 2 * r) * regw]);
                accs[2 * r + 0] = add_i16(accs[2 * r + 0], low_i8_i16(ws));
                accs[2 * r + 1] = add_i16(accs[2 * r + 1], high_i8_i16(ws));
            }
        }

        #pragma GCC unroll 32
        for (i32 r = 0; r < 8; r++) store_i16(&ti_vals[perspective][(i + r) * regw], accs[r]);
    }
#else
    for (i32 i = 0; i < L1_SIZE; i++) ti_vals[perspective][i] = 0;

    for (const i32 fidx : features)
        for (i32 i = 0; i < L1_SIZE; i++) ti_vals[perspective][i] += weights[fidx][i];
#endif

    set_ti_state(perspective, AccState::CLEAN);
}
#endif