#include <GameEngine/HumanPlayer.h>
#include <GameEngine/utils.h>

using namespace cge;
using std::string;



HumanPlayer::HumanPlayer(const string& name_in): GamePlayer(name_in) {}


HumanPlayer::MoveScore HumanPlayer::get_move(
    chess::Board board, const i32, const i32, volatile MouseInfo& mouse, volatile bool& halt
) {
    chess::Square sq_from = chess::Square::NONE;
    chess::Square sq_to = chess::Square::NONE;

    // generate moves
    chess::MoveList<chess::ScoredMove> movelist;
    chess::Movegen::generate_legals(movelist, board);

    // ui controls for move selection
    while (!halt && (sq_to == chess::Square::NONE || sq_from == chess::Square::NONE)) {
        // onclick (or drag)
        if (mouse.event == MouseEvent::LMBDOWN || mouse.event == MouseEvent::LMBUP) {
            const i32 x = mouse.x;
            const i32 y = mouse.y;

            // board clicked
            if (x > 50 && x < 850 && y > 70 && y < 870) {
                auto sq = get_square(x, y);
                auto piece = board.at(sq);

                // own pieces clicked
                if (piece != chess::Piece::NONE && piece.color() == board.stm()) {
                    sq_from = sq;
                    sq_to = chess::Square::NONE;
                }

                // destination clicked
                if (sq_from != chess::Square::NONE && sq_from != sq) {
                    chess::Move move = move_if_valid(sq_from, sq, movelist, board);
                    if (move)
                        return {move, 0, false};
                    else
                        sq_from = chess::Square::NONE;
                }
            }
        }
        if (mouse.event == MouseEvent::RMBDOWN) sq_from = chess::Square::NONE;
    }
    return {chess::Move::NO_MOVE, 0, false};
}


chess::Move HumanPlayer::move_if_valid(
    chess::Square sq_from,
    chess::Square sq_to,
    const chess::MoveList<chess::ScoredMove>& movelist,
    const chess::Board& board
) {
    const auto piece = board.at(sq_from);

    // castling
    if (piece == chess::Piece::WHITEKING || piece == chess::Piece::BLACKKING) {
        const auto col = piece.color();

        if (col == chess::Color::WHITE) {
            if (sq_to == chess::Square::G1  // white king-side
                && board.castle_rights().has(col, chess::Board::CastlingRights::Side::KING_SIDE))
                sq_to = chess::Square::H1;
            else if (sq_to == chess::Square::C1 &&  // white queen-side
                     board.castle_rights().has(col, chess::Board::CastlingRights::Side::QUEEN_SIDE))
                sq_to = chess::Square::A1;

        } else {
            if (sq_to == chess::Square::G8 &&  // black king-side
                board.castle_rights().has(col, chess::Board::CastlingRights::Side::KING_SIDE))
                sq_to = chess::Square::H8;
            else if (sq_to == chess::Square::C8 &&  // black queen-side
                     board.castle_rights().has(col, chess::Board::CastlingRights::Side::QUEEN_SIDE))
                sq_to = chess::Square::A8;
        }
    }

    for (const auto& smove : movelist)
        if (smove.move.from() == sq_from && smove.move.to() == sq_to) return smove.move;
    return chess::Move::NO_MOVE;
}
