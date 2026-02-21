#pragma once
#include <chess/coords.h>



namespace chess {
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
        constexpr static const char* ptstr[] = {"p", "n", "b", "r", "q", "k", " "};
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
    constexpr Piece(PieceType pt, Color color)
        : piece_((pt == PieceType::NONE) ? Piece::NONE : static_cast<underlying>(pt + color * 6)) {}
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

    [[nodiscard]] constexpr Piece relative(Color color) const {
        if (piece_ == Piece::NONE) return Piece::NONE;
        return Piece((piece_ + (color * 6)) % 12);
    }
};
}  // namespace chess
