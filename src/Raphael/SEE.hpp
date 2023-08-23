#pragma once
#include <chess.hpp>
#include <Raphael/consts.hpp>



namespace Raphael {
// Static Exchange Evaluation
// https://www.chessprogramming.org/SEE_-_The_Swap_Algorithm
// http://mediocrechess.blogspot.com/2007/03/guide-static-exchange-evaluation-see.html
// http://www.talkchess.com/forum3/viewtopic.php?t=40054
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
    int evaluate(const chess::Move& move, const chess::Board& board) {
        auto to = move.to();            // where the exchange happens
        auto victim = move.from();      // capturer becomes next victim
        auto occ = board.occ() ^ (1ULL<<victim);    // remove capturer from occ
        auto color = ~board.sideToMove();
        int gain[24] = {0};             // material change each capture

        // handle enpassant
        if (move.typeOf() == chess::Move::ENPASSANT) {
            gain[0] = VAL[0];   // pawn captured
            auto enpsq = (board.sideToMove()==chess::Color::WHITE) ? to + chess::Direction::SOUTH : to + chess::Direction::NORTH;
            occ ^= (1ULL<<enpsq);
        } else
            gain[0] = pieceval(to, board);

        int n_captures = 1;
        auto attackers = chess::attacks::attackers(board, color, to, occ);

        // first simulate a series of captures on the same square
        while (attackers) {
            auto capturer = lva(attackers, board);

            gain[n_captures] = pieceval(victim, board) - gain[n_captures-1];    // assume defended
            if (std::max(-gain[n_captures-1], gain[n_captures]) < 0) break;
            n_captures++;

            color = ~color;
            victim = capturer;          // capturer becomes next victim
            occ ^= (1ULL<<capturer);    // remove capturer from occ
            attackers = chess::attacks::attackers(board, color, to, occ);
        }

        // evaluate the final material (dis)advantage if we both trade smartly
        while (n_captures--)
            gain[n_captures-1] = -std::max(-gain[n_captures-1], gain[n_captures]);
        return gain[0];
    }


    // A quicker version for computing whether a capture is good or bad
    bool quick(const chess::Move& move, const chess::Board& board, const int threshold) {

    }
}   // namespace SEE
}   // namespace Raphael