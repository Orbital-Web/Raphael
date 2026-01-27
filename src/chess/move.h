#pragma once
#include <chess/types.h>

#include <type_traits>



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

    [[nodiscard]] explicit constexpr operator u16() const { return move_; }

    [[nodiscard]] constexpr bool operator==(const Move& rhs) const { return move_ == rhs.move_; }
    [[nodiscard]] constexpr bool operator!=(const Move& rhs) const { return move_ != rhs.move_; }

    [[nodiscard]] constexpr bool operator==(u16 rhs) const { return move_ == rhs; }
    [[nodiscard]] constexpr bool operator!=(u16 rhs) const { return move_ != rhs; }

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
    i32 score = 0;
    Move move;
};


template <typename T>
class MoveList {
private:
    static constexpr usize MAX_MOVES = 256;

    std::array<T, MAX_MOVES> list_ = {};
    usize size_ = 0;


public:
    void push(const T& move) {
        assert(size_ < MAX_MOVES);
        list_[size_++] = move;
    }
    void push(T&& move) {
        assert(size_ < MAX_MOVES);
        list_[size_++] = std::move(move);
    }

    T pop() {
        assert(size_ > 0);
        return std::move(list_[--size_]);
    }

    void clear() { size_ = 0; }

    [[nodiscard]] usize size() const { return size_; }

    [[nodiscard]] usize empty() const { return size_ == 0; }

    [[nodiscard]] T& operator[](usize i) {
        assert(i < size_);
        return list_[i];
    }
    [[nodiscard]] const T& operator[](usize i) const {
        assert(i < size_);
        return list_[i];
    }

    [[nodiscard]] auto begin() { return list_.begin(); }
    [[nodiscard]] auto end() { return list_.begin() + size_; }

    [[nodiscard]] auto begin() const { return list_.begin(); }
    [[nodiscard]] auto end() const { return list_.begin() + size_; }

    [[nodiscard]] bool contains(Move move) const {
        for (usize i = 0; i < size_; ++i) {
            if constexpr (std::is_same_v<T, Move>) {
                if (list_[i] == move) return true;
            } else {
                if (list_[i].move == move) return true;
            }
        }
        return false;
    }
};
}  // namespace chess
