#pragma once
#include <Raphael/nnue.h>
#include <chess/include.h>

#include <type_traits>



namespace raphael {
template <bool include_net>
class Position {
private:
    chess::Board current_;
    std::vector<chess::Board> boards_;

    using NetType = std::conditional_t<include_net, Nnue, nullptr_t>;
    NetType net_;

public:
    /** Initializes the position to startpos */
    Position() {
        boards_.reserve(256);
        if constexpr (include_net) {
            net_.set_board(current_);
        }
    }


    /** Sets the position
     *
     * \param position position to set to
     */
    void set_position(const Position<false>& position) {
        current_ = position.current_;
        boards_ = position.boards_;
        if constexpr (include_net) {
            net_.set_board(current_);
        }
    }

    /** Sets the position to the board
     *
     * \param board board t oset to
     */
    void set_board(const chess::Board& board) {
        current_ = board;
        boards_.clear();
        if constexpr (include_net) {
            net_.set_board(current_);
        }
    }


    /** Returns the current board
     *
     * \returns current board
     */
    const chess::Board board() const { return current_; }


    /** Checks if the position is in repetition
     *
     * \param count number of times we must see the same move to report a repetition
     * \returns whether the position is in repetition
     */
    bool is_repetition(i32 count = 2) const {
        const i32 size = boards_.size();

        u8 c = 0;
        for (i32 i = size - 2; i >= 0 && i >= size - current_.halfmoves() - 1; i -= 2) {
            if (boards_[i].hash() == current_.hash()) c++;
            if (c == count) return true;
        }
        return false;
    }


    /** Evaluates the current board from the current side to move
     *
     * \returns the NNUE evaluation of the board in centipawns
     */
    i32 evaluate()
        requires(include_net)
    {
        return net_.evaluate(current_.stm());
    }


    /** Plays a move
     *
     * \param move move to play
     */
    void make_move(chess::Move move) {
        boards_.push_back(current_);
        if constexpr (include_net) {
            net_.make_move(current_, move);
        }
        current_.make_move(move);
    }

    /** Plays a nullmove */
    void make_nullmove() {
        boards_.push_back(current_);
        current_.make_nullmove();
    }

    /** Unmakes the last move */
    void unmake_move() {
        current_ = boards_.back();
        boards_.pop_back();
        if constexpr (include_net) {
            net_.unmake_move();
        }
    }

    /** Unmakes a nullmove */
    void unmake_nullmove() {
        current_ = boards_.back();
        boards_.pop_back();
    }

public:
    friend class Position<true>;
};
}  // namespace raphael
