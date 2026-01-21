#pragma once
#include <chess/types.h>



namespace chess {
class Move {
private:
    u16 move_;

public:
    constexpr Move(): move_(0) {}
    constexpr Move(u16 move): move_(move) {}

    [[nodiscard]] constexpr Square from() const { return Square((move_ >> 6) & 0x3F); }
    [[nodiscard]] constexpr Square to() const { return Square(move_ & 0x3F); }

    [[nodiscard]] constexpr u16 type() const { return move_ & (3 << 14); }

    [[nodiscard]] constexpr PieceType promotion_type() const {
        return PieceType(((move_ >> 12) & 3) + PieceType::KNIGHT);
    }

    /** Creates a move from source and target square
     * https://github.com/Disservin/chess-library/blob/master/src/move.hpp
     *
     * \tparam MoveType (e.g., castling, promotion, etc.)
     * \param source from square
     * \param target to square
     * \param pt promotion type
     * \returns the generated move object
     */
    template <u16 MoveType = 0>
    [[nodiscard]] static constexpr Move make(
        Square source, Square target, PieceType pt = PieceType::KNIGHT
    ) {
        assert(pt >= PieceType::KNIGHT && pt <= PieceType::QUEEN);

        u16 bits_promotion = pt - PieceType::KNIGHT;
        return Move(MoveType + (bits_promotion << 12) + (source << 6) + target);
    }

    static constexpr u16 NO_MOVE = 0;
    static constexpr u16 NULL_MOVE = 65;
    static constexpr u16 NORMAL = 0;
    static constexpr u16 PROMOTION = 1 << 14;
    static constexpr u16 ENPASSANT = 2 << 14;
    static constexpr u16 CASTLING = 3 << 14;
};

struct ScoredMove {
    u32 score;
    u16 move;
    bool is_quiet;
};
}  // namespace chess
