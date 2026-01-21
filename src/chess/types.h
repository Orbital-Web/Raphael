#pragma once
#include <array>
#include <cassert>
#include <cstdint>
#include <string>

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using usize = std::size_t;



namespace internal {
template <typename T, usize kN, usize... kNs>
struct MultiArrayImpl {
    using Type = std::array<typename MultiArrayImpl<T, kNs...>::Type, kN>;
};

template <typename T, usize kN>
struct MultiArrayImpl<T, kN> {
    using Type = std::array<T, kN>;
};
}  // namespace internal

template <typename T, usize... kNs>
using MultiArray = typename internal::MultiArrayImpl<T, kNs...>::Type;



namespace chess {
class Color {
public:
    enum underlying : u8 { WHITE, BLACK, NONE };

private:
    underlying color_;


public:
    constexpr Color(): color_(underlying::NONE) {}

    constexpr Color(underlying color): color_(color) {}
    explicit constexpr Color(i32 color): color_(static_cast<underlying>(color)) {}

    [[nodiscard]] constexpr operator i32() const { return color_; }

    [[nodiscard]] constexpr Color operator~() const {
        assert(color_ != NONE);
        return Color(color_ ^ 1);
    }
};

class Direction {
public:
    enum underlying : i8 {
        NORTH = 8,
        WEST = -1,
        SOUTH = -8,
        EAST = 1,
        NORTH_EAST = 9,
        NORTH_WEST = 7,
        SOUTH_WEST = -9,
        SOUTH_EAST = -7
    };

private:
    underlying dir_;


public:
    Direction() = delete;

    constexpr Direction(underlying dir): dir_(dir) {}

    [[nodiscard]] constexpr operator i32() const { return dir_; }

    [[nodiscard]] constexpr Direction relative(Color color) const {
        return (color == Color::WHITE) ? Direction(dir_)
                                       : Direction(static_cast<underlying>(-dir_));
    }
};

class File {
public:
    enum underlying : u8 { A, B, C, D, E, F, G, H, NONE };

private:
    underlying file_;


public:
    constexpr File(): file_(underlying::NONE) {}

    constexpr File(underlying file): file_(file) {}
    explicit constexpr File(i32 file): file_(static_cast<underlying>(file)) {}
    explicit constexpr File(std::string_view file)
        : file_(static_cast<underlying>((file[0] <= 'Z') ? (file[0] - 'A') : (file[0] - 'a'))) {}

    [[nodiscard]] constexpr operator i32() const { return file_; }
    [[nodiscard]] explicit operator std::string() const { return std::string(1, file_ + 'a'); }
};

class Rank {
public:
    enum underlying : u8 { R1, R2, R3, R4, R5, R6, R7, R8, NONE };

private:
    underlying rank_;


public:
    constexpr Rank(): rank_(underlying::NONE) {}

    constexpr Rank(underlying rank): rank_(rank) {}
    explicit constexpr Rank(i32 rank): rank_(static_cast<underlying>(rank)) {}
    explicit constexpr Rank(std::string_view rank): rank_(static_cast<underlying>(rank[0] - '1')) {}

    [[nodiscard]] constexpr operator i32() const { return rank_; }
    [[nodiscard]] explicit operator std::string() const { return std::string(1, rank_ + '1'); }
};

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
    explicit constexpr Square(i32 sq): sq_(static_cast<underlying>(sq)) {}
    constexpr Square(File file, Rank rank): sq_(static_cast<underlying>(file + rank * 8)) {}
    explicit constexpr Square(std::string_view str)
        : sq_(static_cast<underlying>((str[0] - 'a') + (str[1] - '1') * 8)) {}

    [[nodiscard]] constexpr operator i32() const { return sq_; }
    [[nodiscard]] explicit operator std::string() const {
        return std::string(file()) + std::string(rank());
    }

    [[nodiscard]] constexpr Square operator+(Direction dir) const { return Square(sq_ + dir); }

    [[nodiscard]] constexpr File file() const { return File(sq_ & 7); }
    [[nodiscard]] constexpr Rank rank() const { return Rank(sq_ >> 3); }

    [[nodiscard]] constexpr bool is_light() const { return (file() + rank()) & 1; }
    [[nodiscard]] constexpr bool is_dark() const { return !is_light(); }

    [[nodiscard]] constexpr bool is_valid() const { return sq_ < 64; }

    [[nodiscard]] constexpr Square flipped() const { return Square(sq_ ^ 56); }
    constexpr Square& flip() {
        sq_ = static_cast<underlying>(sq_ ^ 56);
        return *this;
    }

    [[nodiscard]] constexpr Square relative(Color color) const {
        return Square(sq_ ^ (color * 56));
    }

    [[nodiscard]] constexpr Square ep_square() const {
        assert(
            rank() == Rank::R3     // capture
            || rank() == Rank::R4  // push
            || rank() == Rank::R5  // push
            || rank() == Rank::R6  // capture
        );
        return Square(sq_ ^ 8);
    }

    [[nodiscard]] static constexpr Square castling_king_dest(bool is_king_side, Color color) {
        return Square(is_king_side ? Square::G1 : Square::C1).relative(color);
    }

    [[nodiscard]] static constexpr Square castling_rook_dest(bool is_king_side, Color color) {
        return Square(is_king_side ? Square::F1 : Square::D1).relative(color);
    }

    [[nodiscard]] static constexpr bool same_color(Square sq1, Square sq2) {
        return ((9 * (sq1 ^ sq2)) & 8) == 0;
    }
};


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
    explicit constexpr PieceType(i32 pt): pt_(static_cast<underlying>(pt)) {}
    explicit constexpr PieceType(std::string_view pt): pt_(underlying::NONE) {
        char c = pt[0];

        if (c == 'P' || c == 'p')
            pt_ = PAWN;
        else if (c == 'N' || c == 'n')
            pt_ = KNIGHT;
        else if (c == 'B' || c == 'b')
            pt_ = BISHOP;
        else if (c == 'R' || c == 'r')
            pt_ = ROOK;
        else if (c == 'Q' || c == 'q')
            pt_ = QUEEN;
        else if (c == 'K' || c == 'k')
            pt_ = KING;
        else
            pt_ = NONE;
    }

    [[nodiscard]] constexpr operator i32() const { return pt_; }
    [[nodiscard]] explicit operator std::string() const {
        constexpr static const char* ptstr[] = {"p", "n", "b", "r", "q", "k", "."};
        return ptstr[pt_];
    }
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
    explicit constexpr Piece(i32 piece): piece_(static_cast<underlying>(piece)) {}
    constexpr Piece(PieceType pt, Color color): piece_(static_cast<underlying>(pt + color * 6)) {}
    explicit constexpr Piece(std::string_view piece): piece_(underlying::NONE) {
        char c = piece[0];

        if (c == 'P')
            piece_ = WHITEPAWN;
        else if (c == 'p')
            piece_ = BLACKPAWN;
        else if (c == 'N')
            piece_ = WHITEKNIGHT;
        else if (c == 'n')
            piece_ = BLACKKNIGHT;
        else if (c == 'B')
            piece_ = WHITEBISHOP;
        else if (c == 'b')
            piece_ = BLACKBISHOP;
        else if (c == 'R')
            piece_ = WHITEROOK;
        else if (c == 'r')
            piece_ = BLACKROOK;
        else if (c == 'Q')
            piece_ = WHITEQUEEN;
        else if (c == 'q')
            piece_ = BLACKQUEEN;
        else if (c == 'K')
            piece_ = WHITEKING;
        else if (c == 'k')
            piece_ = BLACKKING;
        else
            piece_ = NONE;
    }

    [[nodiscard]] constexpr operator i32() const { return piece_; }
    [[nodiscard]] explicit operator std::string() const {
        constexpr static const char* piecestr[]
            = {"P", "N", "B", "R", "Q", "K", "p", "n", "b", "r", "q", "k", "."};
        return piecestr[piece_];
    }

    [[nodiscard]] constexpr PieceType type() const {
        if (piece_ == Piece::NONE) return PieceType::NONE;
        return PieceType(piece_ % 6);
    }

    [[nodiscard]] constexpr Color color() const {
        if (piece_ == Piece::NONE) return Color::NONE;
        return Color(piece_ / 6);
    }

    [[nodiscard]] constexpr Piece color_flipped() const {
        if (piece_ == Piece::NONE) return Piece::NONE;
        return Piece((piece_ + 6) % 12);
    }
    constexpr Piece& color_flip() {
        if (piece_ == Piece::NONE) return *this;
        piece_ = static_cast<underlying>((piece_ + 6) % 12);
        return *this;
    }
};
}  // namespace chess
