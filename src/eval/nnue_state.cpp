#ifdef EVAL_NNUE
#include <eval/nnue_state.h>

#include <array>
#include <bit>

using namespace raphael::nnue;
using std::array;
using std::popcount;
using std::rotr;



NnueState::NnueState(
    const i16 W0_psq[N_INBUCKETS][N_PSQ][L1_SIZE],
    const i8 W0_ti[N_THREATS][L1_SIZE],
    const i16 b0[L1_SIZE]
)
    : idx_(0), psq_weights_(W0_psq), ti_weights_(W0_ti) {
    // set the finny table entries to the bias
    for (const auto perspective : {chess::Color::WHITE, chess::Color::BLACK})
        for (const auto mirror : {false, true})
            for (i32 bucket = 0; bucket < N_INBUCKETS; bucket++)
                finny_table_[perspective][mirror][bucket].initialize(b0);
}

const NnueAccumulator& NnueState::get_top_accumulator(const chess::Board& board) {
    lazy_update(board, chess::Color::WHITE);
    lazy_update(board, chess::Color::BLACK);

    assert(accumulators_[idx_].get_psq_state(board.stm()) == NnueAccumulator::AccState::CLEAN);
    assert(accumulators_[idx_].get_psq_state(~board.stm()) == NnueAccumulator::AccState::CLEAN);

    return accumulators_[idx_];
}

void NnueState::set_board(const chess::Board& board) {
    idx_ = 0;
    for (const auto perspective : {chess::Color::WHITE, chess::Color::BLACK}) {
        const bool mirror = needs_mirroring(board.king_square(perspective));
        const auto bucket = king_bucket(board.king_square(perspective), perspective);

        // refresh psq accumulator from finny table
        finny_table_[perspective][mirror][bucket].sync(
            psq_weights_[bucket], board, perspective, mirror
        );
        accumulators_[idx_].refresh_psq(finny_table_[perspective][mirror][bucket], perspective);

        // refresh ti accumulator
        accumulators_[idx_].refresh_ti(ti_weights_, board, perspective, mirror);
    }
}

void NnueState::prepare_make_move() {
    assert(idx_ < MAX_DEPTH - 1);
    idx_++;
    accumulators_[idx_].prepare_updates();
}

void NnueState::unmake_move() {
    assert(idx_ > 0);
    idx_--;
}

void NnueState::add_piece(const chess::Board* board, chess::Piece piece, chess::Square sq) {
    accumulators_[idx_].add_psq(piece, sq);
    update_threats_on_change<true>(board, piece, sq);
}

void NnueState::rem_piece(const chess::Board* board, chess::Piece piece, chess::Square sq) {
    accumulators_[idx_].rem_psq(piece, sq);
    update_threats_on_change<false>(board, piece, sq);
}

void NnueState::move_piece(
    const chess::Board* board,
    chess::Piece from_piece,
    chess::Piece to_piece,
    chess::Square from_sq,
    chess::Square to_sq
) {
    accumulators_[idx_].rem_psq(from_piece, from_sq);
    accumulators_[idx_].add_psq(to_piece, to_sq);

#ifdef USE_SIMD
    // find all threats relative to from and to squares
    const auto src_perm = geometry::permutation_for(from_sq);
    const auto dst_perm = geometry::permutation_for(to_sq);
    const auto [src_rays, src_bits] = geometry::permute_mailbox(src_perm, board->mailbox(), to_sq);
    const auto [dst_rays, dst_bits] = geometry::permute_mailbox(dst_perm, board->mailbox());

    const auto src_closest = geometry::closest_occupied(src_bits);
    const auto dst_closest = geometry::closest_occupied(dst_bits);
    const auto src_outgoing = geometry::outgoing_threats(from_piece, src_closest);
    const auto dst_outgoing = geometry::outgoing_threats(to_piece, dst_closest);
    const auto src_incoming_attackers = geometry::incoming_attackers(src_bits, src_closest);
    const auto dst_incoming_attackers = geometry::incoming_attackers(dst_bits, dst_closest);
    const auto src_incoming_sliders = geometry::incoming_sliders(src_bits, src_closest);
    const auto dst_incoming_sliders = geometry::incoming_sliders(dst_bits, dst_closest);

    // push from and to square-relative threats
    push_focus_threats<false, true>(src_perm.indices, src_rays, src_outgoing, from_piece, from_sq);
    push_focus_threats<false, false>(
        src_perm.indices, src_rays, src_incoming_attackers, from_piece, from_sq
    );
    push_focus_threats<true, true>(dst_perm.indices, dst_rays, dst_outgoing, to_piece, to_sq);
    push_focus_threats<true, false>(
        dst_perm.indices, dst_rays, dst_incoming_attackers, to_piece, to_sq
    );

    // update discovered threats
    const auto src_victim_mask = rotr(src_closest & 0xFE'FE'FE'FE'FE'FE'FE'FE, 32);
    const auto dst_victim_mask = rotr(dst_closest & 0xFE'FE'FE'FE'FE'FE'FE'FE, 32);
    const auto src_valid
        = geometry::ray_fill(src_victim_mask) & geometry::ray_fill(src_incoming_sliders);
    const auto dst_valid
        = geometry::ray_fill(dst_victim_mask) & geometry::ray_fill(dst_incoming_sliders);

    push_discovered_threats<false>(
        src_perm.indices, src_rays, src_incoming_sliders & src_valid, src_victim_mask & src_valid
    );
    push_discovered_threats<true>(
        dst_perm.indices, dst_rays, dst_incoming_sliders & dst_valid, dst_victim_mask & dst_valid
    );
#else
    const auto occ = board->occ();
    const auto xrayocc = occ ^ chess::BitBoard::from_square(to_sq);
    const auto mailbox = board->mailbox();

    auto src_outgoing = chess::Attacks::attacks(from_piece, from_sq, xrayocc) & xrayocc;
    auto dst_outgoing = chess::Attacks::attacks(to_piece, to_sq, occ) & occ;

    while (src_outgoing) {
        const auto other_sq = static_cast<chess::Square>(src_outgoing.poplsb());
        const auto other = mailbox[other_sq];
        accumulators_[idx_].rem_ti(from_piece, other, from_sq, other_sq);
    }
    while (dst_outgoing) {
        const auto other_sq = static_cast<chess::Square>(dst_outgoing.poplsb());
        const auto other = mailbox[other_sq];
        accumulators_[idx_].add_ti(to_piece, other, to_sq, other_sq);
    }

    for (const chess::Piece pc :
         {chess::Piece::WHITEPAWN,
          chess::Piece::BLACKPAWN,
          chess::Piece::WHITEKNIGHT,
          chess::Piece::WHITEBISHOP,
          chess::Piece::WHITEROOK})
    {
        bool is_slider = false;
        auto incoming = chess::Attacks::attacks(pc.color_flipped(), from_sq, xrayocc) & xrayocc;
        const auto potential_victims = incoming;
        if (pc.type() == chess::PieceType::PAWN)
            incoming &= board->occ(pc);
        else if (pc.type() == chess::PieceType::KNIGHT)
            incoming &= board->occ(pc.type());
        else {
            incoming &= board->occ(pc.type()) | board->occ(chess::PieceType::QUEEN);
            is_slider = true;
        }

        while (incoming) {
            const auto other_sq = static_cast<chess::Square>(incoming.poplsb());
            const auto other = mailbox[other_sq];
            accumulators_[idx_].rem_ti(other, from_piece, other_sq, from_sq);

            if (!is_slider) continue;

            // check if a victim is in sight from this slider
            auto victims = (potential_victims ^ chess::BitBoard::from_square(other_sq))
                           & chess::Attacks::attacks(pc, other_sq, chess::BitBoard());
            if (victims) {
                // there will be at most one victim per slider
                const auto victim_sq = static_cast<chess::Square>(victims.poplsb());
                const auto victim = mailbox[victim_sq];

                accumulators_[idx_].add_ti(other, victim, other_sq, victim_sq);
            }
        }
    }
    for (const chess::Piece pc :
         {chess::Piece::WHITEPAWN,
          chess::Piece::BLACKPAWN,
          chess::Piece::WHITEKNIGHT,
          chess::Piece::WHITEBISHOP,
          chess::Piece::WHITEROOK})
    {
        bool is_slider = false;
        auto incoming = chess::Attacks::attacks(pc.color_flipped(), to_sq, occ) & occ;
        const auto potential_victims = incoming;
        if (pc.type() == chess::PieceType::PAWN)
            incoming &= board->occ(pc);
        else if (pc.type() == chess::PieceType::KNIGHT)
            incoming &= board->occ(pc.type());
        else {
            incoming &= board->occ(pc.type()) | board->occ(chess::PieceType::QUEEN);
            is_slider = true;
        }

        while (incoming) {
            const auto other_sq = static_cast<chess::Square>(incoming.poplsb());
            const auto other = mailbox[other_sq];
            accumulators_[idx_].add_ti(other, to_piece, other_sq, to_sq);

            if (!is_slider) continue;

            // check if a victim is in sight from this slider
            auto victims = (potential_victims ^ chess::BitBoard::from_square(other_sq))
                           & chess::Attacks::attacks(pc, other_sq, chess::BitBoard());
            if (victims) {
                // there will be at most one victim per slider
                const auto victim_sq = static_cast<chess::Square>(victims.poplsb());
                const auto victim = mailbox[victim_sq];

                accumulators_[idx_].rem_ti(other, victim, other_sq, victim_sq);
            }
        }
    }
#endif
}

void NnueState::mutate_piece(
    const chess::Board* board, chess::Piece old_piece, chess::Piece new_piece, chess::Square sq
) {
    accumulators_[idx_].rem_psq(old_piece, sq);
    accumulators_[idx_].add_psq(new_piece, sq);

#ifdef USE_SIMD
    // find all threats relative to the focus square
    const auto perm = geometry::permutation_for(sq);
    const auto [rays, bits] = geometry::permute_mailbox(perm, board->mailbox());

    const auto closest = geometry::closest_occupied(bits);
    const auto old_outgoing = geometry::outgoing_threats(old_piece, closest);
    const auto new_outgoing = geometry::outgoing_threats(new_piece, closest);
    const auto incoming_attackers = geometry::incoming_attackers(bits, closest);

    // push focus square-relative threats, no discovered threat changes
    push_focus_threats<false, true>(perm.indices, rays, old_outgoing, old_piece, sq);
    push_focus_threats<false, false>(perm.indices, rays, incoming_attackers, old_piece, sq);
    push_focus_threats<true, true>(perm.indices, rays, new_outgoing, new_piece, sq);
    push_focus_threats<true, false>(perm.indices, rays, incoming_attackers, new_piece, sq);
#else
    const auto occ = board->occ();
    const auto mailbox = board->mailbox();

    auto old_outgoing = chess::Attacks::attacks(old_piece, sq, occ) & occ;
    auto new_outgoing = chess::Attacks::attacks(new_piece, sq, occ) & occ;

    while (old_outgoing) {
        const auto other_sq = static_cast<chess::Square>(old_outgoing.poplsb());
        const auto other = mailbox[other_sq];
        accumulators_[idx_].rem_ti(old_piece, other, sq, other_sq);
    }
    while (new_outgoing) {
        const auto other_sq = static_cast<chess::Square>(new_outgoing.poplsb());
        const auto other = mailbox[other_sq];
        accumulators_[idx_].add_ti(new_piece, other, sq, other_sq);
    }

    for (const chess::Piece pc :
         {chess::Piece::WHITEPAWN,
          chess::Piece::BLACKPAWN,
          chess::Piece::WHITEKNIGHT,
          chess::Piece::WHITEBISHOP,
          chess::Piece::WHITEROOK})
    {
        auto incoming = chess::Attacks::attacks(pc.color_flipped(), sq, occ);
        if (pc.type() == chess::PieceType::PAWN)
            incoming &= board->occ(pc);
        else if (pc.type() == chess::PieceType::KNIGHT)
            incoming &= board->occ(pc.type());
        else
            incoming &= board->occ(pc.type()) | board->occ(chess::PieceType::QUEEN);

        while (incoming) {
            const auto other_sq = static_cast<chess::Square>(incoming.poplsb());
            const auto other = mailbox[other_sq];
            accumulators_[idx_].rem_ti(other, old_piece, other_sq, sq);
            accumulators_[idx_].add_ti(other, new_piece, other_sq, sq);
        }
    }
#endif
}

void NnueState::move_king(chess::Color color, chess::Square from_sq, chess::Square to_sq) {
    // need psq refresh if previous accumulator needs refresh or we change mirroring/bucket
    if (accumulators_[idx_ - 1].get_psq_state(color) == NnueAccumulator::AccState::REFRESH
        || needs_mirroring(from_sq) != needs_mirroring(to_sq)
        || king_bucket(from_sq, color) != king_bucket(to_sq, color))
        accumulators_[idx_].set_psq_state(color, NnueAccumulator::AccState::REFRESH);

    // need ti refresh if previous accumulator needs refresh or we change mirroring
    if (accumulators_[idx_ - 1].get_ti_state(color) == NnueAccumulator::AccState::REFRESH
        || needs_mirroring(from_sq) != needs_mirroring(to_sq))
        accumulators_[idx_].set_ti_state(color, NnueAccumulator::AccState::REFRESH);
}



void NnueState::lazy_update(const chess::Board& board, chess::Color perspective) {
    // horizontal mirroring and king bucket
    const bool mirror = needs_mirroring(board.king_square(perspective));
    const auto bucket = king_bucket(board.king_square(perspective), perspective);

    // find first clean/needs_refresh psq accumulator
    i32 clean_idx = idx_;
    while (accumulators_[clean_idx].get_psq_state(perspective) == NnueAccumulator::AccState::DIRTY)
        clean_idx--;

    if (accumulators_[clean_idx].get_psq_state(perspective) == NnueAccumulator::AccState::REFRESH) {
        // if we need to refresh, refresh at idx_ since we don't know the board state at clean_idx
        finny_table_[perspective][mirror][bucket].sync(
            psq_weights_[bucket], board, perspective, mirror
        );
        accumulators_[idx_].refresh_psq(finny_table_[perspective][mirror][bucket], perspective);
    } else
        // otherwise, apply psq updates up the stack
        while (clean_idx++ < idx_)
            accumulators_[clean_idx].apply_psq_updates(
                accumulators_[clean_idx - 1], psq_weights_[bucket], perspective, mirror
            );

    // find first clean/needs_refresh ti accumulator
    clean_idx = idx_;
    while (accumulators_[clean_idx].get_ti_state(perspective) == NnueAccumulator::AccState::DIRTY)
        clean_idx--;

    if (accumulators_[clean_idx].get_ti_state(perspective) == NnueAccumulator::AccState::REFRESH)
        // if we need to refresh, refresh at idx_ since we don't know the board state at clean_idx
        accumulators_[idx_].refresh_ti(ti_weights_, board, perspective, mirror);
    else
        // otherwise, apply ti updates up the stack
        while (clean_idx++ < idx_)
            accumulators_[clean_idx].apply_ti_updates(
                accumulators_[clean_idx - 1], ti_weights_, perspective, mirror
            );
}

bool NnueState::needs_mirroring(chess::Square king_sq) { return king_sq.file() > chess::File::D; }

i32 NnueState::king_bucket(chess::Square king_sq, chess::Color perspective) {
    const bool mirror = needs_mirroring(king_sq);
    const auto sq = king_sq.mirrored(mirror).relative(perspective);
    return BUCKETS[4 * sq.rank() + sq.file()];
}

template <bool add>
void NnueState::update_threats_on_change(
    const chess::Board* board, chess::Piece piece, chess::Square sq
) {
#ifdef USE_SIMD
    // find all threats relative to the focus square
    const auto perm = geometry::permutation_for(sq);
    const auto [rays, bits] = geometry::permute_mailbox(perm, board->mailbox());

    const auto closest = geometry::closest_occupied(bits);
    const auto outgoing = geometry::outgoing_threats(piece, closest);
    const auto incoming_attackers = geometry::incoming_attackers(bits, closest);
    const auto incoming_sliders = geometry::incoming_sliders(bits, closest);

    // push focus square-relative threats
    push_focus_threats<add, true>(perm.indices, rays, outgoing, piece, sq);
    push_focus_threats<add, false>(perm.indices, rays, incoming_attackers, piece, sq);

    // update discovered threats (add if piece remove, retract if piece added)
    const auto victim_mask = rotr(closest & 0xFE'FE'FE'FE'FE'FE'FE'FE, 32);  // flip slider rays
    const auto valid = geometry::ray_fill(victim_mask) & geometry::ray_fill(incoming_sliders);

    push_discovered_threats<add>(perm.indices, rays, incoming_sliders & valid, victim_mask & valid);
#else
    const auto occ = board->occ();
    const auto mailbox = board->mailbox();

    auto outgoing = chess::Attacks::attacks(piece, sq, occ) & occ;

    while (outgoing) {
        const auto other_sq = static_cast<chess::Square>(outgoing.poplsb());
        const auto other = mailbox[other_sq];
        if constexpr (add)
            accumulators_[idx_].add_ti(piece, other, sq, other_sq);
        else
            accumulators_[idx_].rem_ti(piece, other, sq, other_sq);
    }

    for (const chess::Piece pc :
         {chess::Piece::WHITEPAWN,
          chess::Piece::BLACKPAWN,
          chess::Piece::WHITEKNIGHT,
          chess::Piece::WHITEBISHOP,
          chess::Piece::WHITEROOK})
    {
        bool is_slider = false;
        auto incoming = chess::Attacks::attacks(pc.color_flipped(), sq, occ) & occ;
        const auto potential_victims = incoming;
        if (pc.type() == chess::PieceType::PAWN)
            incoming &= board->occ(pc);
        else if (pc.type() == chess::PieceType::KNIGHT)
            incoming &= board->occ(pc.type());
        else {
            incoming &= board->occ(pc.type()) | board->occ(chess::PieceType::QUEEN);
            is_slider = true;
        }

        while (incoming) {
            const auto other_sq = static_cast<chess::Square>(incoming.poplsb());
            const auto other = mailbox[other_sq];
            if constexpr (add)
                accumulators_[idx_].add_ti(other, piece, other_sq, sq);
            else
                accumulators_[idx_].rem_ti(other, piece, other_sq, sq);

            if (!is_slider) continue;

            // check if a victim is in sight from this slider
            auto victims = (potential_victims ^ chess::BitBoard::from_square(other_sq))
                           & chess::Attacks::attacks(pc, other_sq, chess::BitBoard());
            if (victims) {
                // there will be at most one victim per slider
                const auto victim_sq = static_cast<chess::Square>(victims.poplsb());
                const auto victim = mailbox[victim_sq];

                if constexpr (add)
                    accumulators_[idx_].rem_ti(other, victim, other_sq, victim_sq);
                else
                    accumulators_[idx_].add_ti(other, victim, other_sq, victim_sq);
            }
        }
    }
#endif
}

#ifdef USE_SIMD
template <bool add, bool outgoing>
void NnueState::push_focus_threats(
    geometry::Vector indices,
    geometry::Vector rays,
    geometry::BitRays br,
    chess::Piece piece,
    chess::Square sq
) {
#ifdef USE_AVX512
    // create (attacker, attacker_sq, attacked, attacked_sq) tuples and store it

    // clang-format off
    const auto pair2shuffle = _mm512_set_epi8(
        79, 15, 79, 15, 78, 14, 78, 14, 77, 13, 77, 13, 76, 12, 76, 12, 75, 11, 75, 11,
        74, 10, 74, 10, 73, 9, 73, 9, 72, 8, 72, 8, 71, 7, 71, 7, 70, 6, 70, 6, 69, 5,
        69, 5, 68, 4, 68, 4, 67, 3, 67, 3, 66, 2, 66, 2, 65, 1, 65, 1, 64, 0, 64, 0
    );  // clang-format on

    // focus pair
    const auto pair1 = full_i16(static_cast<i16>(piece | (sq << 8)));

    // non-focus pair
    const auto pair2sq = _mm512_maskz_compress_epi8(br, indices.raw);
    const auto pair2piece = _mm512_maskz_compress_epi8(br, rays.raw);
    const auto pair2 = _mm512_permutex2var_epi8(pair2piece, pair2shuffle, pair2sq);

    // select which is the attacker and which is the victim
    constexpr u64 mask = (outgoing) ? 0xCCCCCCCCCCCCCCCC : 0x3333333333333333;
    const auto vector = _mm512_mask_mov_epi8(pair1, mask, pair2);

    ((add) ? accumulators_[idx_].ti_adds : accumulators_[idx_].ti_subs)
        .unsafe_write([&](TIFeature* ptr) {
            _mm512_storeu_si512(ptr, vector);
            return popcount(br);
        });
#else
    alignas(64) chess::Piece pieces[64];
    alignas(64) chess::Square squares[64];
    rays.store_into(pieces);
    indices.store_into(squares);

    for (; br; br &= (br - 1)) {
        const auto i = __builtin_ctzll(br);

        const auto other = pieces[i];
        const auto other_sq = squares[i];

        const auto attacker = (outgoing) ? piece : other;
        const auto attacked = (outgoing) ? other : piece;
        const auto attacker_sq = (outgoing) ? sq : other_sq;
        const auto attacked_sq = (outgoing) ? other_sq : sq;

        if constexpr (add)
            accumulators_[idx_].add_ti(attacker, attacked, attacker_sq, attacked_sq);
        else
            accumulators_[idx_].rem_ti(attacker, attacked, attacker_sq, attacked_sq);
    }
#endif
}

template <bool add>
void NnueState::push_discovered_threats(
    geometry::Vector indices,
    geometry::Vector rays,
    geometry::BitRays sliders,
    geometry::BitRays victims
) {
#ifdef USE_AVX512
    const auto count = popcount(victims);
    assert(popcount(victims) == popcount(sliders));

    // create (attacker, attacker_sq, attacked, attacked_sq) tuples and store it
    const auto p1 = _mm512_castsi512_si128(_mm512_maskz_compress_epi8(sliders, rays.raw));
    const auto sq1 = _mm512_castsi512_si128(_mm512_maskz_compress_epi8(sliders, indices.raw));
    const auto p2 = _mm512_castsi512_si128(_mm512_maskz_compress_epi8(victims, rays.flip().raw));
    const auto sq2
        = _mm512_castsi512_si128(_mm512_maskz_compress_epi8(victims, indices.flip().raw));

    const auto pair1 = _mm_unpacklo_epi8(p1, sq1);
    const auto pair2 = _mm_unpacklo_epi8(p2, sq2);

    const auto tuple1 = _mm_unpacklo_epi16(pair1, pair2);
    const auto tuple2 = _mm_unpackhi_epi16(pair1, pair2);

    ((add) ? accumulators_[idx_].ti_subs : accumulators_[idx_].ti_adds)
        .unsafe_write([&](TIFeature* ptr) {
            _mm_storeu_si128(reinterpret_cast<__m128i*>(ptr) + 0, tuple1);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(ptr) + 1, tuple2);
            return count;
        });
#else
    alignas(64) chess::Piece pieces[64];
    alignas(64) chess::Square squares[64];
    rays.store_into(pieces);
    indices.store_into(squares);

    for (; sliders; sliders &= (sliders - 1), victims &= (victims - 1)) {
        const auto slider = __builtin_ctzll(sliders);
        const auto victim = (__builtin_ctzll(victims) + 32) % 64;

        const auto attacker = pieces[slider];
        const auto attacked = pieces[victim];
        const auto attacker_sq = squares[slider];
        const auto attacked_sq = squares[victim];
        if constexpr (add)
            accumulators_[idx_].rem_ti(attacker, attacked, attacker_sq, attacked_sq);
        else
            accumulators_[idx_].add_ti(attacker, attacked, attacker_sq, attacked_sq);
    }

    assert(!sliders && !victims);
#endif
}
#endif
#endif
