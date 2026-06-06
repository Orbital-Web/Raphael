#pragma once
#include <Raphael/cuckoo.h>
#include <Raphael/tunable.h>
#include <eval/eval.h>

#include <cstddef>
#include <type_traits>



namespace raphael {
template <bool include_net>
class Position {
private:
    chess::Board current_;
    std::vector<chess::Board> boards_;
    std::vector<chess::PieceMove> moves_;

    using NetType = std::conditional_t<include_net, EvalClass, std::nullptr_t>;
    NetType net_;

public:
    /** Initializes the position to startpos */
    Position() {
        boards_.reserve(256);
        moves_.reserve(256);
        if constexpr (include_net) net_.observer().set_board(current_);
    }


    /** Sets the position
     *
     * \param position position to set to
     */
    void set_position(const Position<false>& position) {
        current_ = position.current_;
        boards_ = position.boards_;
        moves_ = position.moves_;
        if constexpr (include_net) net_.observer().set_board(current_);
    }

    /** Sets the position to the board
     *
     * \param board board t oset to
     */
    void set_board(const chess::Board& board) {
        current_ = board;
        boards_.clear();
        moves_.clear();
        if constexpr (include_net) net_.observer().set_board(current_);
    }


    /** Returns the current board
     *
     * \returns current board
     */
    const chess::Board& board() const { return current_; }


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


    /** Checks if the position is in repetition, requiring threefold if the repetition is before
     * the root
     *
     * \param ply distance from root
     * \returns whether the position is in repetition
     */
    bool is_repetition(i32 ply) const {
        const i32 size = boards_.size();
        const i32 end = std::max(0, size - current_.halfmoves() - 1);

        ply -= 4;
        i32 c = 1;
        for (i32 i = size - 4; i >= end; i -= 2, ply -= 2) {
            if (boards_[i].hash() == current_.hash()) c++;
            if (c == 2 + (ply < 0)) return true;
        }
        return false;
    }

    /** Checks if the position is drawn
     *
     * \param ply distance from root
     * \returns whether the position is drawn
     */
    bool is_drawn(i32 ply) const {
        if (current_.is_halfmovedraw()) {
            if (!current_.in_check()) return true;

            // TODO: exit as soon as we find one legal move
            chess::MoveList movelist;
            chess::Movegen::generate_legals(movelist, current_);
            return !movelist.empty();
        }

        if (current_.is_insufficientmaterial()) return true;

        return is_repetition(ply);
    }

    /** Checks if there is an upcoming repetition
     *
     * \param ply distance from root
     * \returns whether there is an upcoming repetition
     */
    bool has_upcoming_repetition(i32 ply) const {
        // https://web.archive.org/web/20201107002606/https://marcelk.net/2013-04-06/paper/upcoming-rep-v2.pdf
        const i32 size = boards_.size();
        const i32 end = std::min(current_.halfmoves(), size);
        if (end < 3) return false;

        const auto S = [&](i32 d) { return boards_[size - d].hash(); };

        const auto occ = current_.occ();
        const u64 S0 = current_.hash();
        auto other = S0 ^ S(1);

        for (i32 d = 3; d <= end; d += 2) {
            const u64 Sd = S(d);
            other ^= S(d - 1) ^ Sd;
            if (other != 0) continue;

            // check if we can play a reversible move that would result in a repetition
            const u64 key_delta = S0 ^ Sd;
            u32 slot = cuckoo.h1(key_delta);
            if (key_delta != cuckoo.keys[slot]) slot = cuckoo.h2(key_delta);
            if (key_delta != cuckoo.keys[slot]) continue;

            const auto move = cuckoo.moves[slot];
            const auto between = chess::Attacks::between(move.from(), move.to())
                                 & ~chess::BitBoard::from_square(move.to());

            // check move is legal
            if ((occ & between).is_empty()) {
                // only need twofold if repetition happens in the tree
                if (ply >= d) return true;

                // otherwise, require threefold
                for (i32 i = d + 4; i <= end; i += 2)
                    if (Sd == S(i)) return true;
            }
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
            // material scaling
            const i32 material_scale
                = MAT_SCALE_BASE + current_.occ(chess::PieceType::PAWN).count() * MAT_SCALE_PAWN
                  + current_.occ(chess::PieceType::KNIGHT).count() * MAT_SCALE_KNIGHT
                  + current_.occ(chess::PieceType::BISHOP).count() * MAT_SCALE_BISHOP
                  + current_.occ(chess::PieceType::ROOK).count() * MAT_SCALE_ROOK
                  + current_.occ(chess::PieceType::QUEEN).count() * MAT_SCALE_QUEEN;
            static_eval = static_eval * material_scale / 32768;
        }
        return static_eval;
    }


    /** Plays a move
     *
     * \param move move to play
     */
    void make_move(chess::Move move) {
        boards_.push_back(current_);
        moves_.push_back({.move = move, .moving = current_.at(move.from())});
        if constexpr (include_net)
            current_.make_move(move, net_.observer());
        else
            current_.make_move(move);
    }

    /** Plays a nullmove */
    void make_nullmove() {
        boards_.push_back(current_);
        moves_.push_back({.move = chess::Move::NO_MOVE, .moving = chess::Piece::NONE});
        current_.make_nullmove();
    }

    /** Unmakes the last move */
    void unmake_move() {
        current_ = boards_.back();
        boards_.pop_back();
        moves_.pop_back();
        if constexpr (include_net) net_.observer().unmake_move();
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
