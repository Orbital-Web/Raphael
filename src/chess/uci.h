#pragma once
#include <chess/board.h>



namespace chess {
class uci {
public:
    [[nodiscard]] static std::string from_move(const Move move, bool chess960 = false) {
        Square from = move.from();
        Square to = move.to();

        if (!chess960 && move.type() == Move::CASTLING)
            to = Square(to > from ? File::G : File::C, from.rank());

        std::string movestr = static_cast<std::string>(from);
        movestr += static_cast<std::string>(to);

        if (move.type() == Move::PROMOTION)
            movestr += static_cast<std::string>(move.promotion_type());

        return movestr;
    }

    [[nodiscard]] static Move to_move(const Board& board, std::string_view movestr) {
        if (movestr.length() < 4) return Move::NO_MOVE;

        Square from = Square(movestr.substr(0, 2));
        Square to = Square(movestr.substr(2, 2));

        if (!from.is_valid() || !to.is_valid()) return Move::NO_MOVE;

        const auto pt = board.at(from).type();

        // castling
        if (!board.chess960() && pt == PieceType::KING && Square::value_distance(to, from) == 2) {
            to = Square(to > from ? File::H : File::A, from.rank());
            return Move::make<Move::CASTLING>(from, to);
        } else if (board.chess960() && pt == PieceType::KING
                   && board.at(to) == Piece(PieceType::ROOK, board.stm())) {
            return Move::make<Move::CASTLING>(from, to);
        }

        // en passant
        if (pt == PieceType::PAWN && to == board.enpassant_square())
            return Move::make<Move::ENPASSANT>(from, to);

        // promotion
        if (pt == PieceType::PAWN && movestr.length() == 5 && to.is_back_rank(~board.stm())) {
            const auto promo = PieceType(movestr.substr(4, 1));

            if (promo == PieceType::NONE || promo == PieceType::KING || promo == PieceType::PAWN)
                return Move::NO_MOVE;
            return Move::make<Move::PROMOTION>(from, to, PieceType(movestr.substr(4, 1)));
        }

        return (movestr.length() == 4) ? Move::make<Move::NORMAL>(from, to) : Move::NO_MOVE;
    }
};
};  // namespace chess
