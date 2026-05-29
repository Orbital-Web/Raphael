#ifdef EVAL_NNUE
#include <eval/nnue_state.h>

using namespace raphael::nnue;



NnueState::NnueState(const i16 W0[N_INBUCKETS][N_INPUTS][L1_SIZE], const i16 b0[L1_SIZE])
    : idx_(0), weights_(W0) {
    // set the finny table entries to the bias
    for (const auto perspective : {chess::Color::WHITE, chess::Color::BLACK})
        for (const auto mirror : {false, true})
            for (i32 bucket = 0; bucket < N_INBUCKETS; bucket++)
                finny_table_[perspective][mirror][bucket].initialize(b0);
}

const NnueAccumulator& NnueState::get_top_accumulator(const chess::Board& board) {
    lazy_update(board, chess::Color::WHITE);
    lazy_update(board, chess::Color::BLACK);

    assert(accumulators_[idx_].get_psq_state(board.stm()) == NnueAccumulator::PSQState::CLEAN);
    assert(accumulators_[idx_].get_psq_state(~board.stm()) == NnueAccumulator::PSQState::CLEAN);

    return accumulators_[idx_];
}

void NnueState::set_board(const chess::Board& board) {
    idx_ = 0;
    for (const auto perspective : {chess::Color::WHITE, chess::Color::BLACK}) {
        const bool mirror = needs_mirroring(board.king_square(perspective));
        const auto bucket = king_bucket(board.king_square(perspective), perspective);

        finny_table_[perspective][mirror][bucket].sync(
            weights_[bucket], board, perspective, mirror
        );
        accumulators_[idx_].refresh_from(finny_table_[perspective][mirror][bucket], perspective);
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

    // remove moving piece
    accumulators_[idx_].rem_piece(from_piece, from_sq);

    // add moved/promoted piece
    if (move.type() == chess::Move::PROMOTION) {
        const auto promo = chess::Piece(move.promotion_type(), stm);
        accumulators_[idx_].add_piece(promo, to_sq);
    } else if (move.type() == chess::Move::CASTLING) {
        assert(from_piece.type() == chess::PieceType::KING);
        assert(to_piece.type() == chess::PieceType::ROOK);

        const bool is_king_side = to_sq > from_sq;
        new_king_sq = chess::Square::castling_king_dest(is_king_side, stm);
        const auto rook_sq = chess::Square::castling_rook_dest(is_king_side, stm);
        accumulators_[idx_].add_piece(from_piece, new_king_sq);
        accumulators_[idx_].add_piece(to_piece, rook_sq);
    } else
        accumulators_[idx_].add_piece(from_piece, to_sq);

    // add captured piece/ep pawn/castling rook
    if (to_piece != chess::Piece::NONE)
        accumulators_[idx_].rem_piece(to_piece, to_sq);
    else if (move.type() == chess::Move::ENPASSANT) {
        assert(from_piece.type() == chess::PieceType::PAWN);

        const auto ep_pawn = from_piece.color_flipped();
        const auto ep_sq = to_sq.ep_square();
        accumulators_[idx_].rem_piece(ep_pawn, ep_sq);
    }

    // need refresh if previous accumulator needs refresh or we change mirroring/bucket
    if (accumulators_[idx_ - 1].get_psq_state(stm) == NnueAccumulator::PSQState::REFRESH
        || (from_piece.type() == chess::PieceType::KING
            && ((needs_mirroring(from_sq) != needs_mirroring(new_king_sq))
                || (king_bucket(from_sq, stm) != king_bucket(new_king_sq, stm)))))
        accumulators_[idx_].set_psq_state(stm, NnueAccumulator::PSQState::REFRESH);
}

void NnueState::unmake_move() {
    assert(idx_ > 0);
    idx_--;
}



void NnueState::lazy_update(const chess::Board& board, chess::Color perspective) {
    // find first clean/needs_refresh accumulator
    i32 clean_idx = idx_;
    while (accumulators_[clean_idx].get_psq_state(perspective) == NnueAccumulator::PSQState::DIRTY)
        clean_idx--;

    // horizontal mirroring and king bucket
    const bool mirror = needs_mirroring(board.king_square(perspective));
    const auto bucket = king_bucket(board.king_square(perspective), perspective);

    // if an accumulator needs refresh, refresh at idx_ since we don't know the board at clean_idx
    if (accumulators_[clean_idx].get_psq_state(perspective) == NnueAccumulator::PSQState::REFRESH) {
        finny_table_[perspective][mirror][bucket].sync(
            weights_[bucket], board, perspective, mirror
        );
        accumulators_[idx_].refresh_from(finny_table_[perspective][mirror][bucket], perspective);
        return;
    }

    // apply update up the stack
    while (clean_idx++ < idx_)
        accumulators_[clean_idx].apply_updates(
            accumulators_[clean_idx - 1], weights_[bucket], perspective, mirror
        );
}

bool NnueState::needs_mirroring(chess::Square king_sq) { return king_sq.file() > chess::File::D; }

i32 NnueState::king_bucket(chess::Square king_sq, chess::Color perspective) {
    const bool mirror = needs_mirroring(king_sq);
    const auto sq = ((mirror) ? king_sq.mirrored() : king_sq).relative(perspective);
    return BUCKETS[4 * sq.rank() + sq.file()];
}
#endif