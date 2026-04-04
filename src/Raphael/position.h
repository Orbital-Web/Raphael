#pragma once
#include <Raphael/nnue.h>
#include <Raphael/tunable.h>

#include <cstddef>
#include <type_traits>



namespace raphael {
template <bool include_net>
class Position {
private:
    chess::Board current_;
    std::vector<chess::Board> boards_;
    std::vector<chess::PieceMove> moves_;

    using NetType = std::conditional_t<include_net, Nnue, std::nullptr_t>;
    NetType net_;

public:
    /** Initializes the position to startpos */
    Position() {
        boards_.reserve(256);
        moves_.reserve(256);
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
        moves_ = position.moves_;
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
        moves_.clear();
        if constexpr (include_net) {
            net_.set_board(current_);
        }
    }


    /** Returns the current board
     *
     * \returns current board
     */
    const chess::Board board() const { return current_; }


    /** Returns a move n plies ago, or NO_MOVE, Piece::NONE if ply > number of moves played
     *
     * \param ply number of plies to go back
     * \returns the move and moved piece
     */
    const chess::PieceMove prev_move(i32 ply) const {
        assert(ply > 0);
        if (ply > (i32)moves_.size())
            return {.move = chess::Move::NO_MOVE, .moving = chess::Piece::NONE};
        return moves_[moves_.size() - ply];
    }


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
     * \param do_scaling whether to apply material scaling
     * \returns the NNUE evaluation of the board in centipawns
     */
    i32 evaluate(bool do_scaling)
        requires(include_net)
    {
        i32 static_eval = net_.evaluate(current_);
        if (do_scaling) {
            const i32 material_scale
                = MAT_SCALE_BASE + current_.occ(chess::PieceType::KNIGHT).count() * MAT_SCALE_KNIGHT
                  + current_.occ(chess::PieceType::BISHOP).count() * MAT_SCALE_BISHOP
                  + current_.occ(chess::PieceType::ROOK).count() * MAT_SCALE_ROOK
                  + current_.occ(chess::PieceType::QUEEN).count() * MAT_SCALE_QUEEN;
            static_eval = static_eval * material_scale / 32768;
        }
        return std::clamp(static_eval, -MATE_SCORE + 1, MATE_SCORE - 1);
    }


    /** Plays a move
     *
     * \param move move to play
     */
    void make_move(chess::Move move) {
        if constexpr (include_net) {
            net_.make_move(current_, move);
        }
        boards_.push_back(current_);
        moves_.push_back({.move = move, .moving = current_.at(move.from())});
        current_.make_move(move);
    }

    /** Plays a nullmove */
    void make_nullmove() {
        boards_.push_back(current_);
        moves_.push_back({.move = chess::Move::NULL_MOVE, .moving = chess::Piece::NONE});
        current_.make_nullmove();
    }

    /** Unmakes the last move */
    void unmake_move() {
        if constexpr (include_net) {
            net_.unmake_move();
        }
        current_ = boards_.back();
        boards_.pop_back();
        moves_.pop_back();
    }

    /** Unmakes a nullmove */
    void unmake_nullmove() {
        current_ = boards_.back();
        boards_.pop_back();
        moves_.pop_back();
    }

public:
    friend class Position<true>;
};
}  // namespace raphael
