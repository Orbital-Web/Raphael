#include <chess/include.h>

#include <iostream>
#include <tests/doctest/doctest.hpp>

using namespace chess;
using std::cout;
using std::flush;
using std::string;
using std::vector;



class LegalChecker {
private:
    Board board_;
    vector<Move> allmoves_;


public:
    void init_allmoves() {
        // normal moves
        for (Square from = Square::A1; from <= Square::H8; from++) {
            BitBoard to_squares
                = Attacks::knight(from) | Attacks::bishop(from, 0) | Attacks::rook(from, 0);

            while (to_squares) {
                const auto to = static_cast<Square>(to_squares.poplsb());
                allmoves_.emplace_back(Move::make<Move::NORMAL>(from, to));
            }
        }

        // enpassants
        for (const auto color : {Color::WHITE, Color::BLACK}) {
            const auto from_rank = Rank(Rank::R5).relative(color);

            for (File file = File::A; file <= File::H; file++) {
                const auto from = Square(file, from_rank);
                auto to_squares = Attacks::pawn(from, color);

                while (to_squares) {
                    const auto to = static_cast<Square>(to_squares.poplsb());
                    allmoves_.emplace_back(Move::make<Move::ENPASSANT>(from, to));
                }
            }
        }

        // promotions
        for (const auto color : {Color::WHITE, Color::BLACK}) {
            const auto from_rank = Rank(Rank::R7).relative(color);
            const auto to_rank = Rank(Rank::R8).relative(color);

            for (File file = File::A; file <= File::H; file++) {
                const auto from = Square(file, from_rank);
                auto to_squares
                    = BitBoard::from_square(Square(file, to_rank)) | Attacks::pawn(from, color);

                while (to_squares) {
                    const auto to = static_cast<Square>(to_squares.poplsb());
                    allmoves_.emplace_back(
                        Move::make<Move::PROMOTION>(from, to, PieceType::KNIGHT)
                    );
                    allmoves_.emplace_back(
                        Move::make<Move::PROMOTION>(from, to, PieceType::BISHOP)
                    );
                    allmoves_.emplace_back(Move::make<Move::PROMOTION>(from, to, PieceType::ROOK));
                    allmoves_.emplace_back(Move::make<Move::PROMOTION>(from, to, PieceType::QUEEN));
                }
            }
        }

        // castling
        allmoves_.emplace_back(Move::make<Move::CASTLING>(Square::E1, Square::H1));
        allmoves_.emplace_back(Move::make<Move::CASTLING>(Square::E1, Square::A1));
        allmoves_.emplace_back(Move::make<Move::CASTLING>(Square::E8, Square::H8));
        allmoves_.emplace_back(Move::make<Move::CASTLING>(Square::E8, Square::A8));
    }


    void check(i32 depth) {
        if (depth == 0) return;

        MoveList<chess::ScoredMove> legalmoves;
        Movegen::generate_legals(legalmoves, board_);

        for (const auto& move : allmoves_) {
            const bool is_legal = legalmoves.contains(move);
            const bool check_legal = board_.is_legal(move);
            if (check_legal != is_legal) {
                cout << "is_legal failed for position " << board_.get_fen() << " move "
                     << uci::from_move(move) << ((move.type() == Move::ENPASSANT) ? " ep" : "")
                     << " expected " << (is_legal ? "legal" : "illegal") << " got "
                     << (check_legal ? "legal" : "illegal") << "\n"
                     << flush;

                CHECK(false);
            }

            if (is_legal) {
                board_.make_move(move);
                check(depth - 1);
                board_.unmake_move(move);
            }
        }
    }


    void check_all(const string& fen) {
        board_.set_fen(fen);

        check(4);
        CHECK(true);
    }
};

TEST_SUITE("is_legal") {
    TEST_CASE("Standard Chess") {
        const string test_positions[] = {
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - ",
            "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - ",
            "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
            "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
            "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 1",
            "r3k2r/pp1pq1pp/8/4Pp2/b1Q3n1/8/PP1P1PPP/R3K2R w Qkq - 0 1",
            "8/5bk1/8/2Pp4/8/1K6/8/8 w - - 0 1",
            "8/2p3p1/8/q2PKP1P/2N5/8/kp5R/2B5 b - - 0 1",
            "5k2/4N3/8/2Q5/8/q7/8/4K2R w K - 0 1",
            "5rr1/4n2k/4q2P/P1P2n2/3B1p2/4pP2/2N1P3/1RR1K2Q w - - 1 49",
        };

        LegalChecker checker;
        checker.init_allmoves();

        for (const auto& fen : test_positions) checker.check_all(fen);
    }
}
