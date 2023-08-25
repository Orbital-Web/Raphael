#pragma once
#include <chess.hpp>
#include <Raphael/consts.hpp>



namespace Raphael {
// Static Exchange Evaluation
namespace SEE {
    const int VAL[6] = {
        PVAL::PAWN,
        PVAL::KNIGHT,
        PVAL::BISHOP,
        PVAL::ROOK,
        PVAL::QUEEN,
        10000,
    };


    // Returns value of piece on square
    int pieceval(const chess::Square sq, const chess::Board& board) {
        return VAL[(int)board.at<chess::PieceType>(sq)];
    }


    // Returns the square of the least valuable attacker
    chess::Square lva(chess::Bitboard attackers, const chess::Board& board) {
        for (int p=0; p<6; p++) {
            auto attacker_of_type = attackers & board.pieces((chess::PieceType)p);
            if (attacker_of_type)
                return chess::builtin::poplsb(attacker_of_type);
        }
        return chess::NO_SQ;
    }


    // Returns the static exchange evaluation for a capture move
    // https://www.chessprogramming.org/SEE_-_The_Swap_Algorithm
    // http://www.talkchess.com/forum3/viewtopic.php?t=40054
    int evaluate(const chess::Move& move, const chess::Board& board) {
        auto to = move.to();            // where the exchange happens
        auto victim = move.from();      // capturer becomes next victim
        auto occ = board.occ() ^ (1ULL<<victim);    // remove capturer from occ
        auto color = ~board.sideToMove();
        int gain[26] = {0}; // https://puzzling.stackexchange.com/questions/106011/consecutive-captures-on-the-same-square
        int n_captures = 0;

        // handle enpassant
        if (move.typeOf() == chess::Move::ENPASSANT) {
            gain[0] = VAL[0];   // pawn captured
            auto enpsq = (board.sideToMove()==chess::Color::WHITE) ? to + chess::Direction::SOUTH : to + chess::Direction::NORTH;
            occ ^= (1ULL<<enpsq);
        } else
            gain[0] = pieceval(to, board);

        // generate list of all direct attackers
        auto queens = board.pieces(chess::PieceType::QUEEN);
        auto bqs = board.pieces(chess::PieceType::BISHOP) | queens;
        auto rqs = board.pieces(chess::PieceType::ROOK) | queens;
        auto all_attackers = (chess::attacks::pawn(color, to) & board.pieces(chess::PieceType::PAWN, ~color));
        all_attackers |= (chess::attacks::pawn(~color, to) & board.pieces(chess::PieceType::PAWN, color));
        all_attackers |= (chess::attacks::knight(to) & board.pieces(chess::PieceType::KNIGHT));
        all_attackers |= (chess::attacks::bishop(to, occ) & bqs);
        all_attackers |= (chess::attacks::rook(to, occ) & rqs);
        all_attackers |= (chess::attacks::king(to) & board.pieces(chess::PieceType::KING));

        // first simulate a series of captures on the same square
        while (true) {
            n_captures++;
            gain[n_captures] = pieceval(victim, board) - gain[n_captures-1];    // assume defended
            //if (std::max(-gain[n_captures-1], gain[n_captures]) < 0) break;

            all_attackers &= occ;
            auto attackers = all_attackers & board.us(color);
            if (!attackers) break;

            // update virtual board
            color = ~color;
            victim = lva(attackers, board); // capturer becomes next victim
            occ ^= (1ULL<<victim);    // remove capturer from occ
            all_attackers |= chess::attacks::bishop(to, occ) & bqs;
            all_attackers |= chess::attacks::rook(to, occ) & rqs;
        }

        // evaluate the final material (dis)advantage if we both trade smartly
        while (--n_captures)
            gain[n_captures-1] = -std::max(-gain[n_captures-1], gain[n_captures]);
        return gain[0];
    }


    // A quicker version for computing whether a capture is good or bad
    // https://github.com/rafid-dev/rice/blob/main/src/see.cpp#L5
    bool goodCapture(const chess::Move& move, const chess::Board& board, const int threshold) {
        auto to = move.to();            // where the exchange happens
        auto victim = move.from();      // capturer becomes next victim
        auto occ = board.occ() ^ (1ULL<<victim);    // remove capturer from occ
        auto color = ~board.sideToMove();
        int gain;

        // handle enpassant
        if (move.typeOf() == chess::Move::ENPASSANT) {
            gain = VAL[0] - threshold;  // pawn captured
            auto enpsq = (board.sideToMove()==chess::Color::WHITE) ? to + chess::Direction::SOUTH : to + chess::Direction::NORTH;
            occ ^= (1ULL<<enpsq);
        } else
            gain = pieceval(to, board) - threshold;
        if (gain < 0) return false;

        gain -= pieceval(move.from(), board);
        if (gain >= 0) return true;

        // generate list of all direct attackers
        auto queens = board.pieces(chess::PieceType::QUEEN);
        auto bqs = board.pieces(chess::PieceType::BISHOP) | queens;
        auto rqs = board.pieces(chess::PieceType::ROOK) | queens;
        auto all_attackers = (chess::attacks::pawn(color, to) & board.pieces(chess::PieceType::PAWN, ~color));
        all_attackers |= (chess::attacks::pawn(~color, to) & board.pieces(chess::PieceType::PAWN, color));
        all_attackers |= (chess::attacks::knight(to) & board.pieces(chess::PieceType::KNIGHT));
        all_attackers |= (chess::attacks::bishop(to, occ) & bqs);
        all_attackers |= (chess::attacks::rook(to, occ) & rqs);
        all_attackers |= (chess::attacks::king(to) & board.pieces(chess::PieceType::KING));

        // simulate a series of captures on the same square
        while (true) {
            all_attackers &= occ;
            auto attackers = all_attackers & board.us(color);
            if (!attackers) break;

            color = ~color;
            victim = lva(attackers, board); // capturer becomes next victim
            gain = -gain - 1 -pieceval(victim, board);
            if (gain >= 0) {
                if (board.at<chess::PieceType>(victim)==chess::PieceType::KING &&
                    attackers&board.us(color)) color = ~color;
                break;
            }

            // update virtual board
            occ ^= (1ULL<<victim);    // remove capturer from occ
            all_attackers |= chess::attacks::bishop(to, occ) & bqs;
            all_attackers |= chess::attacks::rook(to, occ) & rqs;
        }

        return color != board.sideToMove();
    }
}   // namespace SEE
}   // namespace Raphael