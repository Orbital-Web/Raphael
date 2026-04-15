#include <Raphael/consts.h>
#include <Raphael/tunable.h>

#include <cmath>



namespace raphael {
void update_lmp_table() {
    for (const bool improving : {false, true})
        for (i32 depth = 0; depth <= MAX_DEPTH; depth++)
            LMP_TABLE[improving][depth] = (LMP_THRESH_BASE + depth * depth) / (2 - improving);
}

void update_lmr_table() {
    for (const bool is_quiet : {false, true}) {
        for (i32 depth = 0; depth <= MAX_DEPTH; depth++) {
            for (i32 move_searched = 0; move_searched < 256; move_searched++) {
                if (depth == 0 || move_searched == 0) {
                    LMR_TABLE[is_quiet][depth][move_searched] = 0;
                    continue;
                }

                const double scale = DEPTH_SCALE * 128 * log(depth) * log(move_searched);
                LMR_TABLE[is_quiet][depth][move_searched]
                    = (is_quiet) ? LMR_QUIET_BASE + scale / LMR_QUIET_DIV
                                 : LMR_NOISY_BASE + scale / LMR_NOISY_DIV;
            }
        }
    }
}

void update_see_table() {
    SEE_TABLE[chess::Piece::WHITEPAWN] = SEE_PAWN_VAL;
    SEE_TABLE[chess::Piece::WHITEKNIGHT] = SEE_KNIGHT_VAL;
    SEE_TABLE[chess::Piece::WHITEBISHOP] = SEE_BISHOP_VAL;
    SEE_TABLE[chess::Piece::WHITEROOK] = SEE_ROOK_VAL;
    SEE_TABLE[chess::Piece::WHITEQUEEN] = SEE_QUEEN_VAL;
    SEE_TABLE[chess::Piece::WHITEKING] = 0;
    SEE_TABLE[chess::Piece::BLACKPAWN] = SEE_PAWN_VAL;
    SEE_TABLE[chess::Piece::BLACKKNIGHT] = SEE_KNIGHT_VAL;
    SEE_TABLE[chess::Piece::BLACKBISHOP] = SEE_BISHOP_VAL;
    SEE_TABLE[chess::Piece::BLACKROOK] = SEE_ROOK_VAL;
    SEE_TABLE[chess::Piece::BLACKQUEEN] = SEE_QUEEN_VAL;
    SEE_TABLE[chess::Piece::BLACKKING] = 0;
    SEE_TABLE[chess::Piece::NONE] = 0;
}

void update_escape_table() {
    ESCAPE_TABLE[chess::PieceType::PAWN] = 0;
    ESCAPE_TABLE[chess::PieceType::KNIGHT] = CE_KNIGHT_VAL;
    ESCAPE_TABLE[chess::PieceType::BISHOP] = CE_BISHOP_VAL;
    ESCAPE_TABLE[chess::PieceType::ROOK] = CE_ROOK_VAL;
    ESCAPE_TABLE[chess::PieceType::QUEEN] = CE_QUEEN_VAL;
    ESCAPE_TABLE[chess::PieceType::KING] = 0;
}

void init_tunables() {
    update_lmp_table();
    update_lmr_table();
    update_see_table();
    update_escape_table();
}
}  // namespace raphael
