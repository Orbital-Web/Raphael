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
    static_assert(n_chunks % 8 == 0);

    for (i32 i = 0; i < n_chunks; i += 8) {
        // copy bias
        VecI16 acc0 = load_i16(&values[(i + 0) * regw]);
        VecI16 acc1 = load_i16(&values[(i + 1) * regw]);
        VecI16 acc2 = load_i16(&values[(i + 2) * regw]);
        VecI16 acc3 = load_i16(&values[(i + 3) * regw]);
        VecI16 acc4 = load_i16(&values[(i + 4) * regw]);
        VecI16 acc5 = load_i16(&values[(i + 5) * regw]);
        VecI16 acc6 = load_i16(&values[(i + 6) * regw]);
        VecI16 acc7 = load_i16(&values[(i + 7) * regw]);

        // add features
        for (i32 f = 0; f < n_adds; f++) {
            const auto fidx = adds[f];

            acc0 = adds_i16(acc0, load_i16(&weights[fidx][(i + 0) * regw]));
            acc1 = adds_i16(acc1, load_i16(&weights[fidx][(i + 1) * regw]));
            acc2 = adds_i16(acc2, load_i16(&weights[fidx][(i + 2) * regw]));
            acc3 = adds_i16(acc3, load_i16(&weights[fidx][(i + 3) * regw]));
            acc4 = adds_i16(acc4, load_i16(&weights[fidx][(i + 4) * regw]));
            acc5 = adds_i16(acc5, load_i16(&weights[fidx][(i + 5) * regw]));
            acc6 = adds_i16(acc6, load_i16(&weights[fidx][(i + 6) * regw]));
            acc7 = adds_i16(acc7, load_i16(&weights[fidx][(i + 7) * regw]));
        }

        // rem features
        for (i32 f = 0; f < n_subs; f++) {
            const auto fidx = subs[f];

            acc0 = subs_i16(acc0, load_i16(&weights[fidx][(i + 0) * regw]));
            acc1 = subs_i16(acc1, load_i16(&weights[fidx][(i + 1) * regw]));
            acc2 = subs_i16(acc2, load_i16(&weights[fidx][(i + 2) * regw]));
            acc3 = subs_i16(acc3, load_i16(&weights[fidx][(i + 3) * regw]));
            acc4 = subs_i16(acc4, load_i16(&weights[fidx][(i + 4) * regw]));
            acc5 = subs_i16(acc5, load_i16(&weights[fidx][(i + 5) * regw]));
            acc6 = subs_i16(acc6, load_i16(&weights[fidx][(i + 6) * regw]));
            acc7 = subs_i16(acc7, load_i16(&weights[fidx][(i + 7) * regw]));
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

        acc0 = subs_i16(acc0, load_i16(&weights[sub1][(i + 0) * regw]));
        acc1 = subs_i16(acc1, load_i16(&weights[sub1][(i + 1) * regw]));
        acc2 = subs_i16(acc2, load_i16(&weights[sub1][(i + 2) * regw]));
        acc3 = subs_i16(acc3, load_i16(&weights[sub1][(i + 3) * regw]));
        acc4 = subs_i16(acc4, load_i16(&weights[sub1][(i + 4) * regw]));
        acc5 = subs_i16(acc5, load_i16(&weights[sub1][(i + 5) * regw]));
        acc6 = subs_i16(acc6, load_i16(&weights[sub1][(i + 6) * regw]));
        acc7 = subs_i16(acc7, load_i16(&weights[sub1][(i + 7) * regw]));

        if (n_subs > 1) {
            acc0 = subs_i16(acc0, load_i16(&weights[sub2][(i + 0) * regw]));
            acc1 = subs_i16(acc1, load_i16(&weights[sub2][(i + 1) * regw]));
            acc2 = subs_i16(acc2, load_i16(&weights[sub2][(i + 2) * regw]));
            acc3 = subs_i16(acc3, load_i16(&weights[sub2][(i + 3) * regw]));
            acc4 = subs_i16(acc4, load_i16(&weights[sub2][(i + 4) * regw]));
            acc5 = subs_i16(acc5, load_i16(&weights[sub2][(i + 5) * regw]));
            acc6 = subs_i16(acc6, load_i16(&weights[sub2][(i + 6) * regw]));
            acc7 = subs_i16(acc7, load_i16(&weights[sub2][(i + 7) * regw]));
        }

        acc0 = adds_i16(acc0, load_i16(&weights[add1][(i + 0) * regw]));
        acc1 = adds_i16(acc1, load_i16(&weights[add1][(i + 1) * regw]));
        acc2 = adds_i16(acc2, load_i16(&weights[add1][(i + 2) * regw]));
        acc3 = adds_i16(acc3, load_i16(&weights[add1][(i + 3) * regw]));
        acc4 = adds_i16(acc4, load_i16(&weights[add1][(i + 4) * regw]));
        acc5 = adds_i16(acc5, load_i16(&weights[add1][(i + 5) * regw]));
        acc6 = adds_i16(acc6, load_i16(&weights[add1][(i + 6) * regw]));
        acc7 = adds_i16(acc7, load_i16(&weights[add1][(i + 7) * regw]));

        if (n_adds > 1) {
            acc0 = adds_i16(acc0, load_i16(&weights[add2][(i + 0) * regw]));
            acc1 = adds_i16(acc1, load_i16(&weights[add2][(i + 1) * regw]));
            acc2 = adds_i16(acc2, load_i16(&weights[add2][(i + 2) * regw]));
            acc3 = adds_i16(acc3, load_i16(&weights[add2][(i + 3) * regw]));
            acc4 = adds_i16(acc4, load_i16(&weights[add2][(i + 4) * regw]));
            acc5 = adds_i16(acc5, load_i16(&weights[add2][(i + 5) * regw]));
            acc6 = adds_i16(acc6, load_i16(&weights[add2][(i + 6) * regw]));
            acc7 = adds_i16(acc7, load_i16(&weights[add2][(i + 7) * regw]));
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
    const auto us_acc = accumulators[idx_][board.stm()];
    const auto them_acc = accumulators[idx_][~board.stm()];
    assert(!us_acc.dirty());
    assert(!them_acc.dirty());

    // get address to weights and biases
    constexpr i32 bucket_div = (32 + N_OUTBUCKETS - 1) / N_OUTBUCKETS;
    const i32 bucket_idx = (board.occ().count() - 2) / bucket_div;
    const auto us_w_base = params->W1[bucket_idx];
    const auto them_w_base = us_w_base + N_HIDDEN;
    const auto bias = params->b1[bucket_idx];

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

    i32 eval = QA * bias + hadd_i32(sum);
#else
    i32 eval = QA * bias;

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

    // if an accumulator needs refresh, refresh at idx_ since we don't know the bucket and mirror
    // states for the in between accumulators, only the current one
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
