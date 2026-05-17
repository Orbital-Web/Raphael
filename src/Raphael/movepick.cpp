#include <Raphael/SEE.h>
#include <Raphael/movepick.h>
#include <Raphael/tunable.h>

using namespace raphael;
using std::max;
using std::swap;



MoveGenerator MoveGenerator::negamax(
    chess::MoveList<chess::ScoredMove>* movelist,
    const Position<true>* position,
    const History* history,
    chess::Move ttmove
) {
    return MoveGenerator(Stage::TT_MOVE, movelist, position, history, ttmove);
}

MoveGenerator MoveGenerator::quiescence(
    chess::MoveList<chess::ScoredMove>* movelist,
    const Position<true>* position,
    const History* history,
    chess::Move ttmove
) {
    const auto& board = position->board();

    auto generator = MoveGenerator(Stage::QS_TT_MOVE, movelist, position, history, ttmove);
    if (!board.in_check()) generator.skip_quiets();

    return generator;
}

MoveGenerator MoveGenerator::probcut(
    chess::MoveList<chess::ScoredMove>* movelist,
    const Position<true>* position,
    const History* history,
    chess::Move ttmove
) {
    auto generator = MoveGenerator(Stage::PC_TT_MOVE, movelist, position, history, ttmove);
    return generator;
}


chess::Move MoveGenerator::next() {
    const auto& board = position_->board();

    switch (stage_) {
        // negamax stages
        case Stage::TT_MOVE: {
            stage_ = Stage::GEN_NOISY;

            if (ttmove_ && board.is_legal(ttmove_)) return ttmove_;

            [[fallthrough]];
        }

        case Stage::GEN_NOISY: {
            // generate noisy moves
            assert(movelist_->empty());
            chess::Movegen::generate_legals<chess::Movegen::MoveGenType::NOISY>(*movelist_, board);
            end_ = movelist_->size();

            score_noisies();

            stage_ = Stage::GOOD_NOISY;
            [[fallthrough]];
        }

        case Stage::GOOD_NOISY: {
            // find next non-tt, good noisy move
            while (idx_ < end_) {
                const auto idx = select_next();
                const auto& smove = (*movelist_)[idx];

                if (smove.move == ttmove_) continue;

                const auto thresh = GOOD_NOISY_SEE_BASE - (smove.score * GOOD_NOISY_SEE_MUL / 1024);
                if (SEE::see(smove.move, board, thresh))
                    return smove.move;
                else
                    (*movelist_)[bad_noisy_end_++] = smove;
            }

            // good noisies exhausted
            stage_ = Stage::GEN_QUIET;
            [[fallthrough]];
        }

        case Stage::GEN_QUIET: {
            if (!skip_quiets_) {
                // generate (append) quiet moves
                chess::Movegen::generate_legals<chess::Movegen::MoveGenType::QUIET>(
                    *movelist_, board
                );
                end_ = movelist_->size();

                score_quiets();
            }

            stage_ = Stage::QUIET;
            [[fallthrough]];
        }

        case Stage::QUIET: {
            if (!skip_quiets_) {
                // find next non-tt quiet move
                while (idx_ < end_) {
                    const auto idx = select_next();
                    const auto& smove = (*movelist_)[idx];

                    if (smove.move == ttmove_) continue;

                    return smove.move;
                }
            }

            // quiets exhausted
            idx_ = 0;
            end_ = bad_noisy_end_;
            stage_ = Stage::BAD_NOISY;
            [[fallthrough]];
        }

        case Stage::BAD_NOISY: {
            // find remaining non-tt bad noisy moves (already sorted)
            while (idx_ < end_) {
                const auto& smove = (*movelist_)[idx_++];

                if (smove.move == ttmove_) continue;

                return smove.move;
            }

            // all moves exhausted
            return chess::Move::NO_MOVE;
        }


        // quiescence stages
        case Stage::QS_TT_MOVE: {
            stage_ = Stage::QS_GEN_NOISY;

            if (ttmove_ && board.is_legal(ttmove_)) return ttmove_;

            [[fallthrough]];
        }

        case Stage::QS_GEN_NOISY: {
            // generate noisy moves
            assert(movelist_->empty());
            chess::Movegen::generate_legals<chess::Movegen::MoveGenType::NOISY>(*movelist_, board);
            end_ = movelist_->size();

            score_noisies();

            stage_ = Stage::QS_NOISY;
            [[fallthrough]];
        }

        case Stage::QS_NOISY: {
            // find next non-tt noisy move
            while (idx_ < end_) {
                const auto idx = select_next();
                const auto& smove = (*movelist_)[idx];

                if (smove.move == ttmove_) continue;

                return smove.move;
            }

            stage_ = Stage::QS_GEN_QUIET;
            [[fallthrough]];
        }

        case Stage::QS_GEN_QUIET: {
            if (!skip_quiets_) {
                // generate (append) quiet moves
                chess::Movegen::generate_legals<chess::Movegen::MoveGenType::QUIET>(
                    *movelist_, board
                );
                end_ = movelist_->size();

                score_quiets();
            }

            stage_ = Stage::QS_QUIET;
            [[fallthrough]];
        }

        case Stage::QS_QUIET: {
            if (!skip_quiets_) {
                // find first non-tt quiet move
                while (idx_ < end_) {
                    const auto idx = select_next();
                    const auto& smove = (*movelist_)[idx];

                    if (smove.move == ttmove_) continue;

                    idx_ = end_;  // only generate one quiet move
                    return smove.move;
                }
            }

            // all moves exhausted
            return chess::Move::NO_MOVE;
        }

        // probcut stages
        case Stage::PC_TT_MOVE: {
            stage_ = Stage::PC_GEN_NOISY;

            if (ttmove_ && board.is_legal(ttmove_)) return ttmove_;

            [[fallthrough]];
        }

        case Stage::PC_GEN_NOISY: {
            // generate noisy moves
            assert(movelist_->empty());
            chess::Movegen::generate_legals<chess::Movegen::MoveGenType::NOISY>(*movelist_, board);
            end_ = movelist_->size();

            score_noisies();

            stage_ = Stage::PC_NOISY;
            [[fallthrough]];
        }

        case Stage::PC_NOISY: {
            // find next non-tt noisy move
            while (idx_ < end_) {
                const auto idx = select_next();
                const auto& smove = (*movelist_)[idx];

                if (smove.move == ttmove_) continue;

                return smove.move;
            }

            // all moves exhausted
            return chess::Move::NO_MOVE;
        }
    }
    assert(false);
    return chess::Move::NO_MOVE;
}

void MoveGenerator::skip_quiets() { skip_quiets_ = true; }


MoveGenerator::MoveGenerator(
    Stage start_stage,
    chess::MoveList<chess::ScoredMove>* movelist,
    const Position<true>* position,
    const History* history,
    chess::Move ttmove
)
    : stage_(start_stage),
      movelist_(movelist),
      position_(position),
      history_(history),
      ttmove_(ttmove) {
    movelist_->clear();
}


void MoveGenerator::score_noisies() {
    const auto& board = position_->board();

    for (usize i = idx_; i < end_; i++) {
        auto& smove = (*movelist_)[i];
        const auto victim = board.get_captured(smove.move);

        i32 score = 0;
        score += history_->get_noisyscore(smove.move, victim) / CAPTHIST_DIV;
        score += SEE_TABLE[victim];
        if (smove.move.type() == chess::Move::PROMOTION)
            score += SEE_TABLE[smove.move.promotion_type()] - SEE_TABLE[chess::PieceType::PAWN];
        smove.score = score;
    }
}

void MoveGenerator::score_quiets() {
    const auto& board = position_->board();

    for (usize i = idx_; i < end_; i++) {
        auto& smove = (*movelist_)[i];
        smove.score = history_->get_quietscore(smove.move, *position_);
        if (board.gives_direct_check(smove.move)) smove.score += DIRECT_CHECK_BONUS;
    }
}


usize MoveGenerator::select_next() {
    // from https://github.com/Ciekce/Stormphrax/blob/main/src/movepick.h
    // does selection sort while taking advantage of SIMD when vectorized
    const auto to_u64 = [](i32 score) {
        i64 widened = score;
        widened -= INT32_MIN;
        return static_cast<u64>(widened) << 32;
    };

    auto best = to_u64((*movelist_)[idx_].score) | (256 - idx_);
    for (usize i = idx_ + 1; i < end_; i++) {
        const auto curr = to_u64((*movelist_)[i].score) | (256 - i);
        best = max(best, curr);
    }

    const usize besti = 256 - (best & 0xFFFFFFFF);
    if (besti != idx_) swap((*movelist_)[idx_], (*movelist_)[besti]);
    return idx_++;
}
