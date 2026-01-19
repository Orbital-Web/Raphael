#pragma once
#include <array>
#include <cassert>
#include <cstdint>

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;



namespace internal {
template <typename T, std::size_t kN, std::size_t... kNs>
struct MultiArrayImpl {
    using Type = std::array<typename MultiArrayImpl<T, kNs...>::Type, kN>;
};

template <typename T, std::size_t kN>
struct MultiArrayImpl<T, kN> {
    using Type = std::array<T, kN>;
};
}  // namespace internal

template <typename T, std::size_t... kNs>
using MultiArray = typename internal::MultiArrayImpl<T, kNs...>::Type;



namespace chess {
enum File : u8 { A, B, C, D, E, F, G, H, NONE };

enum Rank : u8 { R1, R2, R3, R4, R5, R6, R7, R8, NONE };

class Square {
public:
    enum underlying : u8 {
        // clang-format off
        A1, B1, C1, D1, E1, F1, G1, H1,
        A2, B2, C2, D2, E2, F2, G2, H2,
        A3, B3, C3, D3, E3, F3, G3, H3,
        A4, B4, C4, D4, E4, F4, G4, H4,
        A5, B5, C5, D5, E5, F5, G5, H5,
        A6, B6, C6, D6, E6, F6, G6, H6,
        A7, B7, C7, D7, E7, F7, G7, H7,
        A8, B8, C8, D8, E8, F8, G8, H8,
        NONE
        // clang-format on
    };

private:
    underlying sq_;


public:
    constexpr Square(): sq_(underlying::NONE) {}

    constexpr Square(underlying sq): sq_(sq) {}
    explicit constexpr Square(i32 sq): sq_(underlying(sq)) {}
    constexpr Square(File file, Rank rank): sq_(underlying(file + rank * 8)) {}

    [[nodiscard]] operator i32() const { return sq_; }

    [[nodiscard]] constexpr File file() const { return File(sq_ & 7); }
    [[nodiscard]] constexpr Rank rank() const { return Rank(sq_ >> 3); }

    [[nodiscard]] constexpr bool is_light() const { return (file() + rank()) & 1; }
    [[nodiscard]] constexpr bool is_dark() const { return !is_light(); }

    [[nodiscard]] constexpr Square flipped() const { return Square(sq_ ^ 56); }
    constexpr Square& flip() {
        sq_ = underlying(sq_ ^ 56);
        return *this;
    }

    [[nodiscard]] static constexpr bool same_color(Square sq1, Square sq2) {
        return ((9 * (sq1 ^ sq2)) & 8) == 0;
    }
};



enum Color : u8 { WHITE, BLACK };



class PieceType {
public:
    enum underlying : u8 {
        PAWN,
        KNIGHT,
        BISHOP,
        ROOK,
        QUEEN,
        KING,
        NONE,
    };

private:
    underlying pt_;


public:
    constexpr PieceType(): pt_(underlying::NONE) {}

    constexpr PieceType(underlying pt): pt_(pt) {}
    explicit constexpr PieceType(i32 pt): pt_(underlying(pt)) {}

    [[nodiscard]] constexpr operator i32() const { return pt_; }
};

class Piece {
public:
    enum underlying : u8 {
        WHITEPAWN,
        WHITEKNIGHT,
        WHITEBISHOP,
        WHITEROOK,
        WHITEQUEEN,
        WHITEKING,
        BLACKPAWN,
        BLACKKNIGHT,
        BLACKBISHOP,
        BLACKROOK,
        BLACKQUEEN,
        BLACKKING,
        NONE
    };

private:
    underlying piece_;


public:
    constexpr Piece(): piece_(underlying::NONE) {}

    constexpr Piece(underlying piece): piece_(piece) {}
    explicit constexpr Piece(i32 piece): piece_(underlying(piece)) {}
    constexpr Piece(PieceType pt, Color color): piece_(underlying(pt + color * 6)) {}

    [[nodiscard]] constexpr operator i32() const { return piece_; }

    [[nodiscard]] constexpr PieceType type() const {
        assert(piece_ != NONE);
        return PieceType(piece_ % 6);
    }

    [[nodiscard]] constexpr Color color() const {
        assert(piece_ != NONE);
        return Color(piece_ / 6);
    }

    [[nodiscard]] constexpr Piece color_flipped() const {
        assert(piece_ != NONE);
        return Piece((piece_ + 6) % 12);
    }
    constexpr Piece& color_flip() {
        assert(piece_ != NONE);
        piece_ = underlying((piece_ + 6) % 12);
        return *this;
    }
};
}  // namespace chess
