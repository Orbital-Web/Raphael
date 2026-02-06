#include <Raphael/SEE.h>
#include <Raphael/movepick.h>
#include <Raphael/tunable.h>

using namespace raphael;
using std::max;
using std::swap;



MoveGenerator MoveGenerator::negamax(
    chess::MoveList<chess::ScoredMove>* movelist,
    const chess::Board* board,
    const History* history,
    chess::Move ttmove,
    chess::Move killer
) {
    return MoveGenerator(Stage::TT_MOVE, movelist, board, history, ttmove, killer);
}

MoveGenerator MoveGenerator::quiescence(
    chess::MoveList<chess::ScoredMove>* movelist, const chess::Board* board, const History* history
) {
    return MoveGenerator(
        Stage::QS_GEN_NOISY, movelist, board, history, chess::Move::NO_MOVE, chess::Move::NO_MOVE
    );
}


chess::Move MoveGenerator::next() {
    switch (stage_) {
        // negamax stages
        case Stage::TT_MOVE: {
            stage_ = Stage::GEN_NOISY;

            if (ttmove_ && board_->is_legal(ttmove_)) return ttmove_;

            [[fallthrough]];
        }

        case Stage::GEN_NOISY: {
            // generate noisy moves
            assert(movelist_->empty());
            chess::Movegen::generate_legals<chess::Movegen::MoveGenType::NOISY>(
                *movelist_, *board_
            );
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

                const auto thresh = GOOD_NOISY_SEE_BASE - (smove.score * GOOD_NOISY_SEE_SCALE / 64);
                if (SEE::see(smove.move, *board_, thresh))
                    return smove.move;
                else
                    (*movelist_)[bad_noisy_end_++] = smove;
            }

            // good noisies exhausted
            stage_ = Stage::KILLER;
            [[fallthrough]];
        }

        case Stage::KILLER: {
            stage_ = Stage::GEN_QUIET;

            if (!skip_quiets_ && killer_ && killer_ != ttmove_ && board_->is_legal(killer_))
                return killer_;

            [[fallthrough]];
        }

        case Stage::GEN_QUIET: {
            if (!skip_quiets_) {
                // generate (append) quiet moves
                chess::Movegen::generate_legals<chess::Movegen::MoveGenType::QUIET>(
                    *movelist_, *board_
                );
                end_ = movelist_->size();

                score_quiets();
            }

            stage_ = Stage::QUIET;
            [[fallthrough]];
        }

        case Stage::QUIET: {
            if (!skip_quiets_) {
                // find next non-tt, non-killer quiet move
                while (idx_ < end_) {
                    const auto idx = select_next();
                    const auto& smove = (*movelist_)[idx];

                    if (smove.move == ttmove_ || smove.move == killer_) continue;

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
        case Stage::QS_GEN_NOISY: {
            // generate noisy moves
            assert(movelist_->empty());
            chess::Movegen::generate_legals<chess::Movegen::MoveGenType::NOISY>(
                *movelist_, *board_
            );
            end_ = movelist_->size();

            score_noisies();

            stage_ = Stage::QS_NOISY;
            [[fallthrough]];
        }

        case Stage::QS_NOISY: {
            // find next noisy move
            while (idx_ < end_) {
                const auto idx = select_next();
                const auto& smove = (*movelist_)[idx];

                return smove.move;
            }

            // noisies exhausted
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
    const chess::Board* board,
    const History* history,
    chess::Move ttmove,
    chess::Move killer
)
    : stage_(start_stage),
      movelist_(movelist),
      board_(board),
      history_(history),
      ttmove_(ttmove),
      killer_(killer) {
    movelist_->clear();
}


void MoveGenerator::score_noisies() {
    for (usize i = idx_; i < end_; i++) {
        auto& smove = (*movelist_)[i];
        const auto victim = board_->get_captured(smove.move);

        i32 score = 0;
        score += history_->get_noisyscore(smove.move, victim) / CAPTHIST_DIVISOR;
        score += SEE_TABLE[victim];
        if (smove.move.type() == chess::Move::PROMOTION)
            score += SEE_TABLE[smove.move.promotion_type()] - SEE_TABLE[chess::PieceType::PAWN];
        smove.score = score;
    }
}

void MoveGenerator::score_quiets() {
    for (usize i = idx_; i < end_; i++) {
        auto& smove = (*movelist_)[i];
        smove.score = history_->get_quietscore(smove.move, board_->stm());
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
