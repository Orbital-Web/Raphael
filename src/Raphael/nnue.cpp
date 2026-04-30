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

void Nnue::NnueFinnyEntry::initialize(const i16 biases[L1_SIZE]) {
    copy(biases, biases + L1_SIZE, values);
}

void Nnue::NnueFinnyEntry::update(
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
    constexpr i32 n_chunks = L1_SIZE / regw;
    static_assert(L1_SIZE % regw == 0);
    static_assert(n_chunks % SIMD_REGS == 0);
    VecI16 accs[SIMD_REGS];

    for (i32 i = 0; i < n_chunks; i += SIMD_REGS) {
    // copy bias
        #pragma GCC unroll 32  // fmt: skip
        for (i32 r = 0; r < SIMD_REGS; r++) accs[r] = load_i16(&values[(i + r) * regw]);

        // add features
        for (i32 f = 0; f < n_adds; f++) {
            const auto fidx = adds[f];

            #pragma GCC unroll 32  // fmt: skip
            for (i32 r = 0; r < SIMD_REGS; r++)
                accs[r] = add_i16(accs[r], load_i16(&weights[fidx][(i + r) * regw]));
        }

        // rem features
        for (i32 f = 0; f < n_subs; f++) {
            const auto fidx = subs[f];

            #pragma GCC unroll 32  // fmt: skip
            for (i32 r = 0; r < SIMD_REGS; r++)
                accs[r] = sub_i16(accs[r], load_i16(&weights[fidx][(i + r) * regw]));
        }

        // store into self
        #pragma GCC unroll 32  // fmt: skip
        for (i32 r = 0; r < SIMD_REGS; r++) store_i16(&values[(i + r) * regw], accs[r]);
    }
#else
    for (i32 f = 0; f < n_adds; f++)
        for (i32 i = 0; i < L1_SIZE; i++) values[i] += weights[adds[f]][i];
    for (i32 f = 0; f < n_subs; f++)
        for (i32 i = 0; i < L1_SIZE; i++) values[i] -= weights[subs[f]][i];
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
    const i16 weights[N_INPUTS][L1_SIZE],
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
    constexpr i32 n_chunks = L1_SIZE / regw;
    static_assert(L1_SIZE % regw == 0);
    static_assert(n_chunks % SIMD_REGS == 0);
    VecI16 accs[SIMD_REGS];

    for (i32 i = 0; i < n_chunks; i += SIMD_REGS) {
        #pragma GCC unroll 32  // fmt: skip
        for (i32 r = 0; r < SIMD_REGS; r++) accs[r] = load_i16(&old_acc.values[(i + r) * regw]);

        #pragma GCC unroll 32  // fmt: skip
        for (i32 r = 0; r < SIMD_REGS; r++)
            accs[r] = sub_i16(accs[r], load_i16(&weights[sub1][(i + r) * regw]));

        if (n_subs > 1)
            #pragma GCC unroll 32  // fmt: skip
            for (i32 r = 0; r < SIMD_REGS; r++)
                accs[r] = sub_i16(accs[r], load_i16(&weights[sub2][(i + r) * regw]));

        #pragma GCC unroll 32  // fmt: skip
        for (i32 r = 0; r < SIMD_REGS; r++)
            accs[r] = add_i16(accs[r], load_i16(&weights[add1][(i + r) * regw]));

        if (n_adds > 1)
            #pragma GCC unroll 32  // fmt: skip
            for (i32 r = 0; r < SIMD_REGS; r++)
                accs[r] = add_i16(accs[r], load_i16(&weights[add2][(i + r) * regw]));

        #pragma GCC unroll 32  // fmt: skip
        for (i32 r = 0; r < SIMD_REGS; r++) store_i16(&values[(i + r) * regw], accs[r]);
    }
#else
    for (i32 i = 0; i < L1_SIZE; i++) {
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
    copy(finny_entry.values, finny_entry.values + L1_SIZE, values);
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

    alignas(ALIGNMENT) u8 l0_out[L1_SIZE];
    activate_l0(stm_acc, l0_out);
    activate_l0(ntm_acc, l0_out + L1_SIZE / 2);

    constexpr i32 bucket_div = (32 + N_OUTBUCKETS - 1) / N_OUTBUCKETS;
    const i32 bucket_idx = (board.occ().count() - 2) / bucket_div;

    alignas(ALIGNMENT) f32 l1_out[L2_SIZE];
    f32 eval;
    forward_l1(l0_out, l1_out, bucket_idx);
    forward_l2l3(l1_out, eval, bucket_idx);

    return i32(eval * OUTPUT_SCALE);
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

void Nnue::activate_l0(const NnueAccumulator& acc, u8 l0_out[L1_SIZE / 2]) const {
    constexpr i32 n_pairs = L1_SIZE / 2;

#ifdef USE_SIMD
    constexpr i32 regw16 = ALIGNMENT / sizeof(i16);
    static_assert(L1_SIZE % (8 * regw16) == 0);

    constexpr i32 n_chunks = n_pairs / regw16;
    const VecI16 zs = zero_i16();
    const VecI16 qa = full_i16(QA);

    for (i32 i = 0; i < n_chunks; i += 4) {
        // compute 4 * regw16 values of the pairwise mul at once
        const VecI16 acc0_v0 = clamp_i16(load_i16(&acc.values[(i + 0) * regw16]), zs, qa);
        const VecI16 acc1_v0 = clamp_i16(load_i16(&acc.values[(i + 1) * regw16]), zs, qa);
        const VecI16 acc2_v0 = clamp_i16(load_i16(&acc.values[(i + 2) * regw16]), zs, qa);
        const VecI16 acc3_v0 = clamp_i16(load_i16(&acc.values[(i + 3) * regw16]), zs, qa);
        const VecI16 acc0_v1 = clamp_i16(load_i16(&acc.values[(i + 0) * regw16 + n_pairs]), zs, qa);
        const VecI16 acc1_v1 = clamp_i16(load_i16(&acc.values[(i + 1) * regw16 + n_pairs]), zs, qa);
        const VecI16 acc2_v1 = clamp_i16(load_i16(&acc.values[(i + 2) * regw16 + n_pairs]), zs, qa);
        const VecI16 acc3_v1 = clamp_i16(load_i16(&acc.values[(i + 3) * regw16 + n_pairs]), zs, qa);

        const VecI16 pw0 = mulhi_i16(lshift_i16(acc0_v0, 7), acc0_v1);
        const VecI16 pw1 = mulhi_i16(lshift_i16(acc1_v0, 7), acc1_v1);
        const VecI16 pw2 = mulhi_i16(lshift_i16(acc2_v0, 7), acc2_v1);
        const VecI16 pw3 = mulhi_i16(lshift_i16(acc3_v0, 7), acc3_v1);

        // note that packus will interleave every 8 values, thus l1w must be permuted
        const VecU8 out0 = pack_u8_i16(pw0, pw1);
        const VecU8 out1 = pack_u8_i16(pw2, pw3);
        store_u8(&l0_out[(i + 0) * regw16], out0);
        store_u8(&l0_out[(i + 2) * regw16], out1);
    }
#else
    for (i32 i = 0; i < n_pairs; i++) {
        const i32 acc_v0 = min(max(acc.values[i], i16(0)), i16(QA));
        const i32 acc_v1 = min(max(acc.values[i + n_pairs], i16(0)), i16(QA));

        // simulate mulhi, assuming non-permuted weights
        l0_out[i] = ((acc_v0 << 7) * acc_v1) >> 16;
    }
#endif
}

void Nnue::forward_l1(const u8 l0_out[L1_SIZE], f32 l1_out[L2_SIZE], i32 bucket_idx) const {
    constexpr i32 n_tiles = L1_SIZE / 4;
    constexpr f32 scale = 1.0f / (QA * QA * QB / 512.0f);

#ifdef USE_SIMD
    // compute l1 matmul
    constexpr i32 regw32 = ALIGNMENT / sizeof(i32);
    constexpr i32 regw8 = ALIGNMENT / sizeof(i8);
    static_assert(L2_SIZE % regw32 == 0);
    static_assert(L1_SIZE % 4 == 0);

    constexpr i32 n_chunks = L2_SIZE / regw32;
    VecI32 l1_pre[n_chunks];

    #pragma GCC unroll 32  // fmt: skip
    for (i32 r = 0; r < n_chunks; r++) l1_pre[r] = zero_i32();

    for (i32 i = 0; i < n_tiles; i++) {
        // compute l1_pre += W1[:, 4i:4(i+1)] * l0_out[4i:4(i+1)]
        VecU8 inputs = tile_u8(&l0_out[4 * i]);
        VecI8 weights[n_chunks];

        #pragma GCC unroll 32  // fmt: skip
        for (i32 r = 0; r < n_chunks; r++)
            weights[r] = load_i8(&params->W1[bucket_idx][i][r * regw8]);

        #pragma GCC unroll 32  // fmt: skip
        for (i32 r = 0; r < n_chunks; r++) l1_pre[r] = dpbusd_i32(l1_pre[r], inputs, weights[r]);
    }

    // activate l1
    const VecF32 zs = zero_f32();
    const VecF32 os = full_f32(1.0f);
    const VecF32 scales = full_f32(scale);

    for (i32 r = 0; r < n_chunks; r++) {
        const VecF32 bias = load_f32(&params->b1[bucket_idx][r * regw32]);
        const VecF32 pre = fmadd_f32(cvt_i32_f32(l1_pre[r]), scales, bias);
        const VecF32 crelu = clamp_f32(pre, zs, os);
        const VecF32 screlu = mul_f32(crelu, crelu);
        store_f32(&l1_out[r * regw32], screlu);
    }
#else
    i32 l1_pre[L2_SIZE] = {0};

    // compute l1 matmul
    for (i32 i = 0; i < n_tiles; i++)
        for (i32 j = 0; j < L2_SIZE; j++)
            for (i32 k = 0; k < 4; k++)
                l1_pre[j] += l0_out[4 * i + k] * params->W1[bucket_idx][i][4 * j + k];

    // activate l1
    for (i32 i = 0; i < L2_SIZE; i++) {
        const f32 bias = params->b1[bucket_idx][i];
        const f32 crelu = min(max(l1_pre[i] * scale + bias, 0.0f), 1.0f);
        const f32 screlu = crelu * crelu;
        l1_out[i] = screlu;
    }
#endif
}

void Nnue::forward_l2l3(const f32 l1_out[L2_SIZE], f32& l3_out, i32 bucket_idx) const {
#ifdef USE_SIMD
    // compute l2 matmul
    constexpr i32 regw32 = ALIGNMENT / sizeof(f32);
    static_assert(L3_SIZE % regw32 == 0);

    constexpr i32 n_chunks = L3_SIZE / regw32;
    VecF32 l2_pre[n_chunks];

    #pragma GCC unroll 32  // fmt: skip
    for (i32 r = 0; r < n_chunks; r++) l2_pre[r] = load_f32(&params->b2[bucket_idx][r * regw32]);

    for (i32 i = 0; i < L2_SIZE; i++) {
        // compute l1_pre += W2[:, i] * l1_out[i]
        VecF32 input = full_f32(l1_out[i]);
        VecF32 weights[n_chunks];

        #pragma GCC unroll 32  // fmt: skip
        for (i32 r = 0; r < n_chunks; r++)
            weights[r] = load_f32(&params->W2[bucket_idx][i][r * regw32]);

        #pragma GCC unroll 32  // fmt: skip
        for (i32 r = 0; r < n_chunks; r++) l2_pre[r] = fmadd_f32(input, weights[r], l2_pre[r]);
    }

    // activate l2 and compute l3 matmul
    const VecF32 zs = zero_f32();
    const VecF32 os = full_f32(1.0f);

    l3_out = params->b3[bucket_idx];
    VecF32 sums = zero_f32();

    for (i32 r = 0; r < n_chunks; r++) {
        const VecF32 crelu = clamp_f32(l2_pre[r], zs, os);
        const VecF32 screlu = mul_f32(crelu, crelu);
        const VecF32 weights = load_f32(&params->W3[bucket_idx][r * regw32]);
        sums = fmadd_f32(screlu, weights, sums);
    }

    l3_out += hadd_f32(sums);
#else
    f32 l2_out[L3_SIZE];
    for (i32 i = 0; i < L3_SIZE; i++) l2_out[i] = params->b2[bucket_idx][i];

    // compute l2 matmul
    for (i32 i = 0; i < L2_SIZE; i++)
        for (i32 j = 0; j < L3_SIZE; j++) l2_out[j] += l1_out[i] * params->W2[bucket_idx][i][j];

    // activate l2 and compute l3 dotprod
    l3_out = params->b3[bucket_idx];
    for (i32 i = 0; i < L3_SIZE; i++) {
        const f32 crelu = min(max(l2_out[i], 0.0f), 1.0f);
        const f32 screlu = crelu * crelu;
        l3_out += screlu * params->W3[bucket_idx][i];
    }
#endif
}
