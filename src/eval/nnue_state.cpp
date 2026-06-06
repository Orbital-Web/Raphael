#ifdef EVAL_NNUE
#include <eval/nnue_state.h>

#include <array>
#include <bit>

using namespace raphael::nnue;
using std::array;
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

void NnueState::make_move(const chess::Board& board, chess::Move move) {
    assert(idx_ < MAX_DEPTH - 1);
    idx_++;

    const auto stm = board.stm();
    const auto from_sq = move.from();
    const auto to_sq = move.to();
    const auto from_piece = board.at(from_sq);
    const auto to_piece = board.at(to_sq);
    auto new_king_sq = move.to();  // assuming from_piece == KING
    assert(from_piece != chess::Piece::NONE);

    accumulators_[idx_].prepare_updates();

    if (move.type() == chess::Move::CASTLING) {
        // castling, encoded as king captures own rook
        assert(from_piece.type() == chess::PieceType::KING);
        assert(to_piece.type() == chess::PieceType::ROOK);

        const bool is_king_side = to_sq > from_sq;
        new_king_sq = chess::Square::castling_king_dest(is_king_side, stm);
        const auto new_rook_sq = chess::Square::castling_rook_dest(is_king_side, stm);
        rem_piece(board, from_piece, from_sq);
        rem_piece(board, to_piece, to_sq);
        add_piece(board, from_piece, new_king_sq);
        add_piece(board, to_piece, new_rook_sq);
    } else if (to_piece != chess::Piece::NONE) {
        // captures
        rem_piece(board, from_piece, from_sq);
        if (move.type() == chess::Move::PROMOTION)
            mutate_piece(board, to_piece, chess::Piece(move.promotion_type(), stm), to_sq);
        else
            mutate_piece(board, to_piece, from_piece, to_sq);
    } else {
        // non-captures or ep
        if (move.type() == chess::Move::ENPASSANT) {
            assert(from_piece.type() == chess::PieceType::PAWN);

            const auto ep_pawn = from_piece.color_flipped();
            const auto ep_sq = to_sq.ep_square();
            rem_piece(board, ep_pawn, ep_sq);
        }

        if (move.type() == chess::Move::PROMOTION)
            move_piece(board, from_piece, chess::Piece(move.promotion_type(), stm), from_sq, to_sq);
        else
            move_piece(board, from_piece, from_piece, from_sq, to_sq);

        if (from_piece.type() == chess::PieceType::KING) new_king_sq = to_sq;
    }

    // need psq refresh if previous accumulator needs refresh or we change mirroring/bucket
    if (accumulators_[idx_ - 1].get_psq_state(stm) == NnueAccumulator::AccState::REFRESH
        || (from_piece.type() == chess::PieceType::KING
            && ((needs_mirroring(from_sq) != needs_mirroring(new_king_sq))
                || (king_bucket(from_sq, stm) != king_bucket(new_king_sq, stm)))))
        accumulators_[idx_].set_psq_state(stm, NnueAccumulator::AccState::REFRESH);

    // need ti refresh if previous accumulator needs refresh or we change mirroring
    if (accumulators_[idx_ - 1].get_ti_state(stm) == NnueAccumulator::AccState::REFRESH
        || (from_piece.type() == chess::PieceType::KING
            && needs_mirroring(from_sq) != needs_mirroring(new_king_sq)))
        accumulators_[idx_].set_ti_state(stm, NnueAccumulator::AccState::REFRESH);
}

void NnueState::unmake_move() {
    assert(idx_ > 0);
    idx_--;
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

void NnueState::add_piece(const chess::Board& board, chess::Piece piece, chess::Square sq) {
    accumulators_[idx_].add_psq(piece, sq);
    update_threats_on_change<true>(board, piece, sq);
}

void NnueState::rem_piece(const chess::Board& board, chess::Piece piece, chess::Square sq) {
    accumulators_[idx_].rem_psq(piece, sq);
    update_threats_on_change<false>(board, piece, sq);
}

void NnueState::move_piece(
    const chess::Board& board,
    chess::Piece from_piece,
    chess::Piece to_piece,
    chess::Square from_sq,
    chess::Square to_sq
) {
    accumulators_[idx_].rem_psq(from_piece, from_sq);
    accumulators_[idx_].add_psq(to_piece, to_sq);

    // find all threats relative to from and to squares
    const auto src_perm = geometry::permutation_for(from_sq);
    const auto dst_perm = geometry::permutation_for(to_sq);
    const auto [src_rays, src_bits] = geometry::permute_mailbox(src_perm, board.mailbox(), to_sq);
    const auto [dst_rays, dst_bits] = geometry::permute_mailbox(dst_perm, board.mailbox());

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
}

void NnueState::mutate_piece(
    const chess::Board& board, chess::Piece old_piece, chess::Piece new_piece, chess::Square sq
) {
    accumulators_[idx_].rem_psq(old_piece, sq);
    accumulators_[idx_].add_psq(new_piece, sq);

    // find all threats relative to the focus square
    const auto perm = geometry::permutation_for(sq);
    const auto [rays, bits] = geometry::permute_mailbox(perm, board.mailbox());

    const auto closest = geometry::closest_occupied(bits);
    const auto old_outgoing = geometry::outgoing_threats(old_piece, closest);
    const auto new_outgoing = geometry::outgoing_threats(new_piece, closest);
    const auto incoming_attackers = geometry::incoming_attackers(bits, closest);

    // push focus square-relative threats, no discovered threat changes
    push_focus_threats<false, true>(perm.indices, rays, old_outgoing, old_piece, sq);
    push_focus_threats<false, false>(perm.indices, rays, incoming_attackers, old_piece, sq);
    push_focus_threats<true, true>(perm.indices, rays, new_outgoing, new_piece, sq);
    push_focus_threats<true, false>(perm.indices, rays, incoming_attackers, new_piece, sq);
}

template <bool add>
void NnueState::update_threats_on_change(
    const chess::Board& board, chess::Piece piece, chess::Square sq
) {
    // find all threats relative to the focus square
    const auto perm = geometry::permutation_for(sq);
    const auto [rays, bits] = geometry::permute_mailbox(perm, board.mailbox());

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
}

template <bool add, bool outgoing>
void NnueState::push_focus_threats(
    geometry::Vector indices,
    geometry::Vector rays,
    geometry::BitRays br,
    chess::Piece piece,
    chess::Square sq
) {
#ifdef __AVX512VBMI2__
        // TODO:
#else
    alignas(64) chess::Piece pieces[64];
    alignas(64) chess::Square squares[64];
    rays.store(pieces);
    indices.store(squares);

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
#ifdef __AVX512VBMI2__
        // TODO:
#else
    alignas(64) chess::Piece pieces[64];
    alignas(64) chess::Square squares[64];
    rays.store(pieces);
    indices.store(squares);

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
