#pragma once
#include <chess/types.h>

#include <cassert>
#include <string>



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

[[nodiscard]] constexpr Color::underlying operator~(Color::underlying color) {
    return (color == Color::underlying::WHITE)
               ? Color::underlying::BLACK
               : ((color == Color::underlying::BLACK) ? Color::underlying::WHITE
                                                      : Color::underlying::NONE);
}

enum class Direction : i8 {
    NORTH = 8,
    SOUTH = -8,
    WEST = -1,
    EAST = 1,
    NORTH_WEST = 7,
    NORTH_EAST = 9,
    SOUTH_WEST = -9,
    SOUTH_EAST = -7
};

[[nodiscard]] constexpr Direction relative_direction(Direction dir, Color c) {
    return (c == Color::WHITE) ? dir : static_cast<Direction>(-static_cast<i8>(dir));
}

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

    constexpr File& operator++(i32) {
        file_ = static_cast<underlying>(file_ + 1);
        return *this;
    }
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

    constexpr Rank& operator++(i32) {
        rank_ = static_cast<underlying>(rank_ + 1);
        return *this;
    }

    [[nodiscard]] constexpr bool is_back_rank(Color color) const {
        return rank_ == static_cast<underlying>(color * 7);
    }

    [[nodiscard]] constexpr Rank relative(Color color) const { return Rank(rank_ ^ (color * 7)); }
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

    [[nodiscard]] constexpr Square operator+(Direction dir) const {
        return Square(sq_ + static_cast<i8>(dir));
    }

    [[nodiscard]] constexpr Square operator+(i32 offset) const { return Square(sq_ + offset); }
    [[nodiscard]] constexpr Square operator-(i32 offset) const { return Square(sq_ - offset); }

    constexpr Square& operator++() {
        sq_ = static_cast<underlying>(sq_ + 1);
        return *this;
    }
    constexpr Square& operator--() {
        sq_ = static_cast<underlying>(sq_ - 1);
        return *this;
    }

    constexpr Square operator++(i32) {
        const auto sq = sq_;
        sq_ = static_cast<underlying>(sq_ + 1);
        return Square(sq);
    }
    constexpr Square operator--(i32) {
        const auto sq = sq_;
        sq_ = static_cast<underlying>(sq_ - 1);
        return Square(sq);
    }

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

    [[nodiscard]] constexpr Square mirrored() const { return Square(sq_ ^ 7); }
    constexpr Square& mirror() {
        sq_ = static_cast<underlying>(sq_ ^ 7);
        return *this;
    }

    [[nodiscard]] constexpr Square relative(Color color) const {
        return Square(sq_ ^ (color * 56));
    }

    [[nodiscard]] constexpr bool is_back_rank(Color color) const {
        return rank().is_back_rank(color);
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

    [[nodiscard]] static i32 value_distance(Square sq1, Square sq2) {
        return std::abs(static_cast<i32>(sq1) - static_cast<i32>(sq2));
    }
};
}  // namespace chess
