#pragma once
#include <chess.hpp>
#include <Raphael/consts.hpp>



namespace Raphael {
// Static Exchange Evaluation
// https://www.chessprogramming.org/Static_Exchange_Evaluation#Implementation
// http://mediocrechess.blogspot.com/2007/03/guide-static-exchange-evaluation-see.html
namespace SEE {
    int evaluate(const chess::Move& move, const chess::Board& board) {

    }

    // Returns the attackers on the specified square of the specified color
    chess::Bitboard attackers(const chess::Board& board, chess::Color color, chess::Square square, chess::Bitboard occupied) {
        const auto queens = board.pieces(chess::PieceType::QUEEN, color);
        const auto bishops = board.pieces(chess::PieceType::BISHOP, color);
        const auto rooks = board.pieces(chess::PieceType::ROOK, color);

        // using the fact that if we can attack PieceType from square, they can attack us back
        auto atks = (chess::movegen::attacks::pawn(~color, square) & board.pieces(chess::PieceType::PAWN, color));
        atks |= (chess::movegen::attacks::knight(square) & board.pieces(chess::PieceType::KNIGHT, color));
        atks |= (chess::movegen::attacks::bishop(square, occupied) & (bishops | queens));
        atks |= (chess::movegen::attacks::rook(square, occupied) & (rooks | queens));
        atks |= (chess::movegen::attacks::king(square) & board.pieces(chess::PieceType::KING, color));

        return atks & occupied;
    }
}   // namespace SEE
}   // namespace Raphael