#include <GameEngine/HumanPlayer.h>

using namespace cge;
using std::string;



HumanPlayer::HumanPlayer(string name_in): GamePlayer(name_in) {}


chess::Move HumanPlayer::get_move(
    chess::Board board,
    const int t_remain,
    const int t_inc,
    volatile sf::Event& event,
    volatile bool& halt
) {
    auto sq_from = chess::NO_SQ;
    auto sq_to = chess::NO_SQ;

    // generate movelist
    chess::Movelist movelist;
    chess::movegen::legalmoves(movelist, board);

    // ui controls for move selection
    while (!halt && (sq_to == chess::NO_SQ || sq_from == chess::NO_SQ)) {
        // onclick (or drag)
        if (lmbdown || lmbup) {
            int x = event.mouseButton.x;
            int y = event.mouseButton.y;

            // board clicked
            if (x > 50 && x < 850 && y > 70 && y < 870) {
                auto sq = get_square(x, y);
                int piece = (int)board.at(sq);

                // own pieces clicked
                if (piece != 12 && whiteturn == (piece < 6)) {
                    sq_from = sq;
                    sq_to = chess::NO_SQ;
                }

                // destination clicked
                if (sq_from != chess::NO_SQ && sq_from != sq) {
                    chess::Move move = move_if_valid(sq_from, sq, movelist, board);
                    if (move != chess::Move::NO_MOVE)
                        return move;
                    else
                        sq_from = chess::NO_SQ;
                }
            }
        }
        if (rmbdown) sq_from = chess::NO_SQ;
    }
    return chess::Move::NO_MOVE;
}


chess::Move HumanPlayer::move_if_valid(
    chess::Square sq_from,
    chess::Square sq_to,
    const chess::Movelist& movelist,
    const chess::Board& board
) {
    chess::Piece piece = board.at(sq_from);

    // castling
    if (piece == chess::Piece::WHITEKING || piece == chess::Piece::BLACKKING) {
        chess::Color col = board.color(piece);

        // white
        if (col == chess::Color::WHITE) {
            if (sq_to == chess::SQ_G1 &&  // white king-side
                board.castlingRights().hasCastlingRight(col, chess::CastleSide::KING_SIDE))
                sq_to = chess::SQ_H1;
            else if (sq_to == chess::SQ_C1 &&  // white queen-side
                     board.castlingRights().hasCastlingRight(col, chess::CastleSide::QUEEN_SIDE))
                sq_to = chess::SQ_A1;

            // black
        } else {
            if (sq_to == chess::SQ_G8 &&  // black king-side
                board.castlingRights().hasCastlingRight(col, chess::CastleSide::KING_SIDE))
                sq_to = chess::SQ_H8;
            else if (sq_to == chess::SQ_C8 &&  // black queen-side
                     board.castlingRights().hasCastlingRight(col, chess::CastleSide::QUEEN_SIDE))
                sq_to = chess::SQ_A8;
        }
    }

    for (auto& move : movelist)
        if (move.from() == sq_from && move.to() == sq_to) return move;
    return chess::Move::NO_MOVE;
}
