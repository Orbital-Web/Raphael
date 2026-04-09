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
    const auto pc = (piece.type() == chess::PieceType::KING) ? chess::Piece::WHITEKING
                                                             : piece.relative(perspective);
    return 64 * pc + sq.relative(perspective);
}



Nnue::NnueFinnyEntry::NnueFinnyEntry() {}

void Nnue::NnueFinnyEntry::initialize(const i16 biases[N_HIDDEN]) {
    copy(biases, biases + N_HIDDEN, values);
}

void Nnue::NnueFinnyEntry::update(
    const i16 weights[N_INPUTS][N_HIDDEN],
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
                adds[n_adds++] = NnueFeature(piece, sq).index(perspective, mirror);
            }

            auto subs_occ = old_occ & ~new_occ;
            while (subs_occ) {
                const auto sq = chess::Square(subs_occ.poplsb());
                const auto piece = chess::Piece(pt, color);

                assert(n_subs < 32);
                subs[n_subs++] = NnueFeature(piece, sq).index(perspective, mirror);
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
    constexpr i32 n_chunks = N_HIDDEN / regw;
    static_assert(N_HIDDEN % regw == 0);
    static_assert(n_chunks % SIMD_UNROLL == 0);
    VecI16 accs[SIMD_UNROLL];

    for (i32 i = 0; i < n_chunks; i += SIMD_UNROLL) {
    // copy bias
        #pragma GCC unroll 32  // fmt: skip
        for (i32 r = 0; r < SIMD_UNROLL; r++) accs[r] = load_i16(&values[(i + r) * regw]);

        // add features
        for (i32 f = 0; f < n_adds; f++) {
            const auto fidx = adds[f];

            #pragma GCC unroll 32  // fmt: skip
            for (i32 r = 0; r < SIMD_UNROLL; r++)
                accs[r] = adds_i16(accs[r], load_i16(&weights[fidx][(i + r) * regw]));
        }

        // rem features
        for (i32 f = 0; f < n_subs; f++) {
            const auto fidx = subs[f];

            #pragma GCC unroll 32  // fmt: skip
            for (i32 r = 0; r < SIMD_UNROLL; r++)
                accs[r] = subs_i16(accs[r], load_i16(&weights[fidx][(i + r) * regw]));
        }

        // store into self
        #pragma GCC unroll 32  // fmt: skip
        for (i32 r = 0; r < SIMD_UNROLL; r++) store_i16(&values[(i + r) * regw], accs[r]);
    }
#else
    for (i32 f = 0; f < n_adds; f++)
        for (i32 i = 0; i < N_HIDDEN; i++) values[i] += weights[adds[f]][i];
    for (i32 f = 0; f < n_subs; f++)
        for (i32 i = 0; i < N_HIDDEN; i++) values[i] -= weights[subs[f]][i];
#endif
}

chess::BitBoard Nnue::NnueFinnyEntry::occ(chess::PieceType pt, chess::Color color) const {
    return pieces_[pt] & occ_[color];
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
    const NnueAccumulator& old_acc,
    const i16 weights[N_INPUTS][N_HIDDEN],
    chess::Color perspective,
    bool mirror
) {
    assert(dirty());
    assert(!old_acc.dirty());
    assert(!old_acc.needs_refresh);

    i32 add1 = adds[0].index(perspective, mirror);
    i32 add2 = adds[1].index(perspective, mirror);
    i32 sub1 = subs[0].index(perspective, mirror);
    i32 sub2 = subs[1].index(perspective, mirror);

#ifdef USE_SIMD
    constexpr i32 regw = ALIGNMENT / sizeof(i16);
    constexpr i32 n_chunks = N_HIDDEN / regw;
    static_assert(N_HIDDEN % regw == 0);
    static_assert(n_chunks % SIMD_UNROLL == 0);
    VecI16 accs[SIMD_UNROLL];

    for (i32 i = 0; i < n_chunks; i += SIMD_UNROLL) {
    // copy old_acc
        #pragma GCC unroll 32  // fmt: skip
        for (i32 r = 0; r < SIMD_UNROLL; r++) accs[r] = load_i16(&old_acc.values[(i + r) * regw]);

        #pragma GCC unroll 32  // fmt: skip
        for (i32 r = 0; r < SIMD_UNROLL; r++)
            accs[r] = subs_i16(accs[r], load_i16(&weights[sub1][(i + r) * regw]));

        if (n_subs > 1)
            #pragma GCC unroll 32  // fmt: skip
            for (i32 r = 0; r < SIMD_UNROLL; r++)
                accs[r] = subs_i16(accs[r], load_i16(&weights[sub2][(i + r) * regw]));

        #pragma GCC unroll 32  // fmt: skip
        for (i32 r = 0; r < SIMD_UNROLL; r++)
            accs[r] = adds_i16(accs[r], load_i16(&weights[add1][(i + r) * regw]));

        if (n_adds > 1)
            #pragma GCC unroll 32  // fmt: skip
            for (i32 r = 0; r < SIMD_UNROLL; r++)
                accs[r] = adds_i16(accs[r], load_i16(&weights[add2][(i + r) * regw]));

        // store into self
        #pragma GCC unroll 32  // fmt: skip
        for (i32 r = 0; r < SIMD_UNROLL; r++) store_i16(&values[(i + r) * regw], accs[r]);
    }
#else
    for (i32 i = 0; i < N_HIDDEN; i++) {
        values[i] = old_acc.values[i];

        values[i] -= weights[sub1][i];
        if (n_subs > 1) values[i] -= weights[sub2][i];
        values[i] += weights[add1][i];
        if (n_adds > 1) values[i] += weights[add2][i];
    }
#endif

    reset_updates();
}

void Nnue::NnueAccumulator::refresh_from(const NnueFinnyEntry& finny_entry) {
    copy(finny_entry.values, finny_entry.values + N_HIDDEN, values);
    reset_updates();
    needs_refresh = false;
}



Nnue::Nnue(): params(load_network()), idx_(0) {
    // set the finny table entries to the bias
    for (const auto perspective : {chess::Color::WHITE, chess::Color::BLACK})
        for (const auto mirror : {false, true})
            for (i32 bucket = 0; bucket < N_INBUCKETS; bucket++)
                finny_table[perspective][mirror][bucket].initialize(params->b0);
}

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

    // get address to accumulators
    const auto stm_acc = accumulators[idx_][board.stm()];
    const auto ntm_acc = accumulators[idx_][~board.stm()];
    assert(!stm_acc.dirty());
    assert(!ntm_acc.dirty());

    // get address to weights and biases
    constexpr i32 bucket_div = (32 + N_OUTBUCKETS - 1) / N_OUTBUCKETS;
    const i32 bucket_idx = (board.occ().count() - 2) / bucket_div;
    const auto stm_w_base = params->W1[bucket_idx];
    const auto ntm_w_base = stm_w_base + N_HIDDEN / 2;
    const auto bias = params->b1[bucket_idx];

#ifdef USE_SIMD
    constexpr i32 regw = ALIGNMENT / sizeof(i16);
    constexpr i32 n_chunks = (N_HIDDEN / 2) / regw;
    static_assert((N_HIDDEN / 2) % regw == 0);

    const VecI16 zs = zero_i16();
    const VecI16 qa = full_i16(QA);

    VecI32 sum = zero_i16();
    for (i32 i = 0; i < n_chunks; i++) {
        const VecI16 stm_v0 = clamp_i16(load_i16(&stm_acc.values[i * regw]), zs, qa);
        const VecI16 stm_v1 = clamp_i16(load_i16(&stm_acc.values[i * regw + N_HIDDEN / 2]), zs, qa);
        const VecI16 ntm_v0 = clamp_i16(load_i16(&ntm_acc.values[i * regw]), zs, qa);
        const VecI16 ntm_v1 = clamp_i16(load_i16(&ntm_acc.values[i * regw + N_HIDDEN / 2]), zs, qa);

        const VecI16 stm_w = load_i16(&stm_w_base[i * regw]);
        const VecI16 ntm_w = load_i16(&ntm_w_base[i * regw]);

        const VecI16 stm_pw = madd_i16(mul_i16(stm_w, stm_v0), stm_v1);
        const VecI16 ntm_pw = madd_i16(mul_i16(ntm_w, ntm_v0), ntm_v1);

        sum = add_i32(sum, add_i32(stm_pw, ntm_pw));
    }

    i32 eval = QA * bias + hadd_i32(sum);
#else
    i32 eval = QA * bias;

    // compute W1 dot SCReLU(acc)
    for (i32 i = 0; i < N_HIDDEN / 2; i++) {
        const i32 stm_v0 = min(max(static_cast<i32>(stm_acc.values[i]), 0), QA);
        const i32 stm_v1 = min(max(static_cast<i32>(stm_acc.values[i + N_HIDDEN / 2]), 0), QA);
        const i32 ntm_v0 = min(max(static_cast<i32>(ntm_acc.values[i]), 0), QA);
        const i32 ntm_v1 = min(max(static_cast<i32>(ntm_acc.values[i + N_HIDDEN / 2]), 0), QA);

        eval += stm_w_base[i] * stm_v0 * stm_v1;
        eval += ntm_w_base[i] * ntm_v0 * ntm_v1;
    }
#endif

    eval /= QA;
    eval *= OUTPUT_SCALE;
    eval /= (QA * QB);
    return eval;
}


void Nnue::set_board(const chess::Board& board) {
    idx_ = 0;
    for (const auto perspective : {chess::Color::WHITE, chess::Color::BLACK}) {
        const bool mirror = needs_mirroring(board.king_square(perspective));
        const auto bucket = king_bucket(board.king_square(perspective), perspective);

        finny_table[perspective][mirror][bucket].update(
            params->W0[bucket], board, perspective, mirror
        );
        accumulators[idx_][perspective].refresh_from(finny_table[perspective][mirror][bucket]);
    }
}

void Nnue::make_move(const chess::Board& board, chess::Move move) {
    assert(idx_ < MAX_DEPTH - 1);
    idx_++;

    const auto stm = board.stm();
    const auto from_sq = move.from();
    const auto to_sq = move.to();
    const auto from_piece = board.at(from_sq);
    const auto to_piece = board.at(to_sq);
    auto new_king_sq = move.to();  // assuming from_piece == KING
    assert(from_piece != chess::Piece::NONE);

    // reset updates so we can overwrite them
    accumulators[idx_][chess::Color::WHITE].reset_updates();
    accumulators[idx_][chess::Color::BLACK].reset_updates();

    // remove moving piece
    accumulators[idx_][chess::Color::WHITE].rem_piece(from_piece, from_sq);
    accumulators[idx_][chess::Color::BLACK].rem_piece(from_piece, from_sq);

    // add moved/promoted piece
    if (move.type() == chess::Move::PROMOTION) {
        const auto promo = chess::Piece(move.promotion_type(), stm);
        accumulators[idx_][chess::Color::WHITE].add_piece(promo, to_sq);
        accumulators[idx_][chess::Color::BLACK].add_piece(promo, to_sq);
    } else if (move.type() == chess::Move::CASTLING) {
        assert(from_piece.type() == chess::PieceType::KING);
        assert(to_piece.type() == chess::PieceType::ROOK);

        const bool is_king_side = to_sq > from_sq;
        new_king_sq = chess::Square::castling_king_dest(is_king_side, stm);
        const auto rook_sq = chess::Square::castling_rook_dest(is_king_side, stm);
        accumulators[idx_][chess::Color::WHITE].add_piece(from_piece, new_king_sq);
        accumulators[idx_][chess::Color::BLACK].add_piece(from_piece, new_king_sq);
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

    // need refresh only if previous accumulator needs refresh or we change mirroring/bucket
    accumulators[idx_][stm].needs_refresh = accumulators[idx_ - 1][stm].needs_refresh;

    if (from_piece.type() == chess::PieceType::KING
        && ((needs_mirroring(from_sq) != needs_mirroring(new_king_sq))
            || (king_bucket(from_sq, stm) != king_bucket(new_king_sq, stm))))
        accumulators[idx_][stm].needs_refresh = true;
}

void Nnue::unmake_move() {
    assert(idx_ > 0);
    idx_--;
}


bool Nnue::needs_mirroring(chess::Square king_sq) { return king_sq.file() > chess::File::D; }

i32 Nnue::king_bucket(chess::Square king_sq, chess::Color perspective) {
    const bool mirror = needs_mirroring(king_sq);
    const auto sq = ((mirror) ? king_sq.mirrored() : king_sq).relative(perspective);
    return BUCKETS[4 * sq.rank() + sq.file()];
}

void Nnue::lazy_update(const chess::Board& board, chess::Color perspective) {
    // find first clean/needs_refresh accumulator
    i32 clean_idx = idx_;
    while (accumulators[clean_idx][perspective].dirty()
           && !accumulators[clean_idx][perspective].needs_refresh)
        clean_idx--;

    // horizontal mirroring and king bucket
    const bool mirror = needs_mirroring(board.king_square(perspective));
    const auto bucket = king_bucket(board.king_square(perspective), perspective);

    // if an accumulator needs refresh, refresh at idx_ since we don't know the board at clean_idx
    if (accumulators[clean_idx][perspective].needs_refresh) {
        finny_table[perspective][mirror][bucket].update(
            params->W0[bucket], board, perspective, mirror
        );
        accumulators[idx_][perspective].refresh_from(finny_table[perspective][mirror][bucket]);
        return;
    }

    // update up the stack
    while (clean_idx++ < idx_)
        accumulators[clean_idx][perspective].update(
            accumulators[clean_idx - 1][perspective], params->W0[bucket], perspective, mirror
        );
}
