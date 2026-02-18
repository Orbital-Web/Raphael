#include <GameEngine/HumanPlayer.h>
#include <GameEngine/utils.h>

using namespace cge;
using std::string;



HumanPlayer::HumanPlayer(const string& name_in): name(name_in) {}


void HumanPlayer::set_board(const chess::Board& board) {
    board_ = board;
    movelist_.clear();
    chess::Movegen::generate_legals(movelist_, board_);

    move_ = chess::Move::NO_MOVE;
    sq_from_ = chess::Square::NONE;
    sq_to_ = chess::Square::NONE;
}


chess::Move HumanPlayer::try_get_move(const MouseInfo& mouse) {
    // onclick (or drag)
    if (mouse.event == MouseEvent::LMBDOWN || mouse.event == MouseEvent::LMBUP) {
        const i32 x = mouse.x;
        const i32 y = mouse.y;

        // board clicked
        if (x > 50 && x < 850 && y > 70 && y < 870) {
            const auto sq = get_square(x, y);
            const auto piece = board_.at(sq);

            // own pieces clicked
            if (piece != chess::Piece::NONE && piece.color() == board_.stm()) {
                sq_from_ = sq;
                sq_to_ = chess::Square::NONE;
            }

            // destination clicked
            if (sq_from_ != chess::Square::NONE && sq_from_ != sq) {
                sq_to_ = sq;
                return move_if_valid();
            }
        }
    } else if (mouse.event == MouseEvent::RMBDOWN) {
        sq_from_ = chess::Square::NONE;
        sq_to_ = chess::Square::NONE;
    }

    return chess::Move::NO_MOVE;
}


chess::Move HumanPlayer::move_if_valid() {
    const auto piece = board_.at(sq_from_);

    // castling
    if (piece == chess::Piece::WHITEKING || piece == chess::Piece::BLACKKING) {
        const auto col = piece.color();

        constexpr auto kingside = chess::Board::CastlingRights::Side::KING_SIDE;
        constexpr auto queenside = chess::Board::CastlingRights::Side::QUEEN_SIDE;

        if (col == chess::Color::WHITE) {
            if (sq_to_ == chess::Square::G1 && board_.castle_rights().has(col, kingside))
                sq_to_ = chess::Square::H1;
            else if (sq_to_ == chess::Square::C1 && board_.castle_rights().has(col, queenside))
                sq_to_ = chess::Square::A1;
        } else {
            if (sq_to_ == chess::Square::G8 && board_.castle_rights().has(col, queenside))
                sq_to_ = chess::Square::H8;
            else if (sq_to_ == chess::Square::C8 && board_.castle_rights().has(col, queenside))
                sq_to_ = chess::Square::A8;
        }
    }

    for (const auto& smove : movelist_)
        if (smove.move.from() == sq_from_ && smove.move.to() == sq_to_) return smove.move;
    return chess::Move::NO_MOVE;
}
