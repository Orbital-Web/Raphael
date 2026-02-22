#include <Raphael/position.h>

using namespace raphael;
using std::vector;



Position::Position() {
    boards_.reserve(256);
    net_.set_board(current_);
}

Position::Position(const chess::Board& board) {
    boards_.reserve(256);
    set_board(board);
}

void Position::set_board(const chess::Board& board) {
    current_ = board;
    net_.set_board(current_);
    boards_.clear();
    net_.reset();
}


const chess::Board Position::board() const { return current_; }


bool Position::is_repetition(i32 count) const {
    const i32 size = boards_.size();

    u8 c = 0;
    for (i32 i = size - 2; i >= 0 && i >= size - current_.halfmoves() - 1; i -= 2) {
        if (boards_[i].hash() == current_.hash()) c++;
        if (c == count) return true;
    }
    return false;
}


i32 Position::evaluate() { return net_.evaluate(current_.stm()); }


void Position::make_move(chess::Move move) {
    boards_.push_back(current_);
    net_.make_move(current_, move);
    current_.make_move(move);
}

void Position::make_nullmove() {
    boards_.push_back(current_);
    current_.make_nullmove();
}

void Position::unmake_move() {
    current_ = boards_.back();
    boards_.pop_back();
    net_.unmake_move();
}

void Position::unmake_nullmove() {
    current_ = boards_.back();
    boards_.pop_back();
}
