#pragma once
#include <chess/movegen_fwd.h>
#include <chess/utils.h>
#include <chess/zobrist.h>



namespace chess {
class Board {
public:
    static constexpr auto STARTPOS = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    class CastlingRights {
    public:
        enum Side : u8 { KING_SIDE, QUEEN_SIDE };

        constexpr void set(Color color, Side castle, File rook_file) {
            rooks[color][castle] = rook_file;
        }

        constexpr void clear() {
            rooks[0][0] = rooks[0][1] = rooks[1][0] = rooks[1][1] = File::NONE;
        }
        constexpr void clear(Color color) { rooks[color][0] = rooks[color][1] = File::NONE; }
        constexpr i32 clear(Color color, Side castle) {
            rooks[color][castle] = File::NONE;
            return castle + color * 2;
        }

        [[nodiscard]] constexpr bool has(Color color, Side castle) const {
            return rooks[color][castle] != File::NONE;
        }

        [[nodiscard]] constexpr bool has(Color color) const {
            return has(color, Side::KING_SIDE) || has(color, Side::QUEEN_SIDE);
        }

        [[nodiscard]] constexpr File get_rook_file(Color color, Side castle) const {
            return rooks[color][castle];
        }

        [[nodiscard]] constexpr i32 hash_index() const {
            return has(Color::WHITE, Side::KING_SIDE) + 2 * has(Color::WHITE, Side::QUEEN_SIDE)
                   + 4 * has(Color::BLACK, Side::KING_SIDE)
                   + 8 * has(Color::BLACK, Side::QUEEN_SIDE);
        }

        [[nodiscard]] constexpr bool is_empty() const {
            return !has(Color::WHITE) && !has(Color::BLACK);
        }

        [[nodiscard]] static constexpr Side closest_side(File to, File from) {
            return to > from ? Side::KING_SIDE : Side::QUEEN_SIDE;
        }

    private:
        MultiArray<File, 2, 2> rooks = {};
    };

private:
    std::array<BitBoard, 6> pieces_ = {};
    std::array<BitBoard, 2> occ_ = {};
    std::array<Piece, 64> mailbox_ = {};

    MultiArray<BitBoard, 2, 2> castle_path_ = {};
    CastlingRights castle_rights_ = {};

    u64 hash_ = 0;

    u16 plies_ = 1;     // total plies
    u8 halfmoves_ = 0;  // plies since last capture or pawn move
    Color stm_ = Color::WHITE;

    Square enpassant_ = Square::NONE;

    bool chess960_ = false;


public:
    explicit Board(std::string_view fen = STARTPOS, bool chess960 = false): chess960_(chess960) {
        assert(chess960 == false);  // TODO: FRC not supported yet
        set_fen(fen);
    }

    [[nodiscard]] BitBoard occ() const { return occ_[0] | occ_[1]; }
    [[nodiscard]] BitBoard occ(PieceType pt) const { return pieces_[pt]; }
    [[nodiscard]] BitBoard occ(PieceType pt, Color color) const {
        return pieces_[pt] & occ_[color];
    }
    [[nodiscard]] BitBoard occ(Piece piece) const { return occ(piece.type(), piece.color()); }
    [[nodiscard]] BitBoard occ(Color color) const { return occ_[color]; }

    [[nodiscard]] Piece at(Square sq) const { return mailbox_[sq]; }

    [[nodiscard]] CastlingRights castle_rights() const { return castle_rights_; }
    [[nodiscard]] BitBoard castle_path(Color color, bool is_king_side) const {
        return castle_path_[color][is_king_side];
    }
    [[nodiscard]] Square enpassant_square() const { return enpassant_; }

    [[nodiscard]] Color stm() const { return stm_; }
    [[nodiscard]] i32 halfmoves() const { return halfmoves_; }
    [[nodiscard]] i32 fullmoves() const { return 1 + plies_ / 2; }

    [[nodiscard]] u64 hash() const { return hash_; }

    [[nodiscard]] bool chess960() const { return chess960_; }


    [[nodiscard]] Square king_square(Color color) const {
        return Square(occ(PieceType::KING, color).lsb());
    }

    [[nodiscard]] bool is_kingpawn(Color color) const {
        return (occ(color) ^ occ(PieceType::PAWN, color)).count() == 1;
    }


    [[nodiscard]] bool in_check() const { return is_attacked(king_square(stm_), ~stm_); }

    [[nodiscard]] bool is_halfmovedraw() const { return halfmoves_ >= 100; }

    [[nodiscard]] bool is_insufficientmaterial() const {
        const i32 count = occ().count();

        // only kings, draw
        if (count == 2) return true;

        // only one bishop/knight
        if (count == 3) {
            if (occ(PieceType::BISHOP) || occ(PieceType::KNIGHT)) return true;
        }

        // same colored bishops
        else if (count == 4) {
            auto bishops = occ(PieceType::BISHOP);
            if (bishops.count() == 2
                && Square::same_color(Square(bishops.msb()), Square(bishops.lsb())))
                return true;
        }

        return false;
    }


    [[nodiscard]] bool is_capture(Move move) const {
        return (move.type() != Move::CASTLING && at(move.to()) != Piece::NONE)
               || move.type() == Move::ENPASSANT;
    }

    [[nodiscard]] Piece get_captured(Move move) const {
        if (move.type() == Move::ENPASSANT) return at(move.from()).color_flipped();
        if (move.type() != Move::CASTLING) return at(move.to());
        return Piece::NONE;
    }

    [[nodiscard]] bool is_quiet(Move move) const {
        return !(
            (move.type() == Move::PROMOTION && move.promotion_type() == PieceType::QUEEN)
            || is_capture(move)
        );
    }

    [[nodiscard]] bool is_attacked(Square sq, Color color) const {
        // cheap checks first
        if (Attacks::pawn(sq, ~color) & occ(PieceType::PAWN, color)) return true;
        if (Attacks::knight(sq) & occ(PieceType::KNIGHT, color)) return true;
        if (Attacks::king(sq) & occ(PieceType::KING, color)) return true;
        if (Attacks::bishop(sq, occ()) & (occ(PieceType::BISHOP) | occ(PieceType::QUEEN))
            & occ(color))
            return true;
        if (Attacks::rook(sq, occ()) & (occ(PieceType::ROOK) | occ(PieceType::QUEEN)) & occ(color))
            return true;

        return false;
    }


    [[nodiscard]] bool is_legal(Move move) const { return Movegen::is_legal(*this, move); }


    void make_move(Move move) {
        assert((at(move.from()) < Piece::BLACKPAWN) == (stm_ == Color::WHITE));

        const auto captured = at(move.to());
        const auto capture = captured != Piece::NONE && move.type() != Move::CASTLING;
        const auto pt = at(move.from()).type();

        halfmoves_++;
        plies_++;

        if (enpassant_ != Square::NONE) hash_ ^= Zobrist::enpassant(enpassant_.file());
        enpassant_ = Square::NONE;

        if (capture) {
            remove_piece(captured, move.to());

            halfmoves_ = 0;
            hash_ ^= Zobrist::piece(captured, move.to());

            // remove castling rights if rook is captured
            if (captured.type() == PieceType::ROOK && move.to().rank().is_back_rank(~stm_)) {
                const auto king_sq = king_square(~stm_);
                const auto side = CastlingRights::closest_side(move.to().file(), king_sq.file());

                if (castle_rights_.get_rook_file(~stm_, side) == move.to().file())
                    hash_ ^= Zobrist::castle_index(castle_rights_.clear(~stm_, side));
            }
        }

        if (pt == PieceType::KING && castle_rights_.has(stm_)) {
            // remove castling rights if king moves
            hash_ ^= Zobrist::castling(castle_rights_.hash_index());
            castle_rights_.clear(stm_);
            hash_ ^= Zobrist::castling(castle_rights_.hash_index());

        } else if (pt == PieceType::ROOK && move.from().is_back_rank(stm_)) {
            // remove castling rights if rook moves from back rank
            const auto king_sq = king_square(stm_);
            const auto side = CastlingRights::closest_side(move.from().file(), king_sq.file());

            if (castle_rights_.get_rook_file(stm_, side) == move.from().file())
                hash_ ^= Zobrist::castle_index(castle_rights_.clear(stm_, side));

        } else if (pt == PieceType::PAWN) {
            halfmoves_ = 0;

            // double push
            if (Square::value_distance(move.to(), move.from()) == 16) {
                // add enpassant hash if enemy pawns are attacking the square
                const auto ep_mask = Attacks::pawn(move.to().ep_square(), stm_);

                if (ep_mask & occ(PieceType::PAWN, ~stm_)) {
                    assert(at(move.to().ep_square()) == Piece::NONE);
                    enpassant_ = move.to().ep_square();
                    hash_ ^= Zobrist::enpassant(move.to().ep_square().file());
                }
            }
        }

        if (move.type() == Move::CASTLING) {
            const auto king = at(move.from());
            const auto rook = at(move.to());

            assert(king == Piece(PieceType::KING, stm_));
            assert(rook == Piece(PieceType::ROOK, stm_));

            const bool is_king_side = move.to() > move.from();
            const auto kingto = Square::castling_king_dest(is_king_side, stm_);
            const auto rookto = Square::castling_rook_dest(is_king_side, stm_);

            remove_piece(king, move.from());
            remove_piece(rook, move.to());

            place_piece(king, kingto);
            place_piece(rook, rookto);

            hash_ ^= Zobrist::piece(king, move.from()) ^ Zobrist::piece(king, kingto);
            hash_ ^= Zobrist::piece(rook, move.to()) ^ Zobrist::piece(rook, rookto);

        } else if (move.type() == Move::PROMOTION) {
            const auto pawn = at(move.from());
            const auto prom = Piece(move.promotion_type(), stm_);

            assert(pawn == Piece(PieceType::PAWN, stm_));

            remove_piece(pawn, move.from());
            place_piece(prom, move.to());

            hash_ ^= Zobrist::piece(pawn, move.from()) ^ Zobrist::piece(prom, move.to());

        } else {
            assert(at(move.from()) != Piece::NONE);
            assert(at(move.to()) == Piece::NONE);

            const auto piece = at(move.from());

            remove_piece(piece, move.from());
            place_piece(piece, move.to());

            hash_ ^= Zobrist::piece(piece, move.from()) ^ Zobrist::piece(piece, move.to());
        }

        if (move.type() == Move::ENPASSANT) {
            assert(at(move.to().ep_square()).type() == PieceType::PAWN);

            const auto piece = Piece(PieceType::PAWN, ~stm_);

            remove_piece(piece, move.to().ep_square());

            hash_ ^= Zobrist::piece(piece, move.to().ep_square());
        }

        hash_ ^= Zobrist::stm();
        stm_ = ~stm_;
    }

    void make_nullmove() {
        hash_ ^= Zobrist::stm();
        if (enpassant_ != Square::NONE) hash_ ^= Zobrist::enpassant(enpassant_.file());
        enpassant_ = Square::NONE;

        plies_++;
        stm_ = ~stm_;
    }


    /** Returns the zobrist hash of the board after a move
     *
     * \tparam EXACT whether to account for enpassant and castling
     * \param move move to compute hash for, can be a nullmove
     * \returns the new hash after the move
     */
    template <bool EXACT = true>
    [[nodiscard]] u64 hash_after(Move move) const {
        u64 newhash = hash_;

        newhash ^= Zobrist::stm();
        if (EXACT && enpassant_ != Square::NONE) newhash ^= Zobrist::enpassant(enpassant_.file());

        if (move == Move::NULL_MOVE) return newhash;

        const auto captured = at(move.to());
        const auto capture = captured != Piece::NONE && move.type() != Move::CASTLING;
        const auto pt = at(move.from()).type();

        if (capture) {
            newhash ^= Zobrist::piece(captured, move.to());

            // remove castling rights if rook is captured
            if (EXACT && captured.type() == PieceType::ROOK
                && move.to().rank().is_back_rank(~stm_)) {
                const auto king_sq = king_square(~stm_);
                const auto side = CastlingRights::closest_side(move.to().file(), king_sq.file());

                if (castle_rights_.get_rook_file(~stm_, side) == move.to().file())
                    newhash ^= Zobrist::castle_index(~stm_ * 2 + side);
            }
        }

        if constexpr (EXACT) {
            if (pt == PieceType::KING && castle_rights_.has(stm_)) {
                // remove castling rights if king moves
                const auto oldidx = castle_rights_.hash_index();
                const auto newidx = oldidx & ((stm_) ? 3 : 12);

                newhash ^= Zobrist::castling(oldidx);
                newhash ^= Zobrist::castling(newidx);

            } else if (pt == PieceType::ROOK && move.from().is_back_rank(stm_)) {
                // remove castling rights if rook moves from back rank
                const auto king_sq = king_square(stm_);
                const auto side = CastlingRights::closest_side(move.from().file(), king_sq.file());

                if (castle_rights_.get_rook_file(stm_, side) == move.from().file())
                    newhash ^= Zobrist::castle_index(stm_ * 2 + side);

            } else if (pt == PieceType::PAWN) {
                // double push
                if (Square::value_distance(move.to(), move.from()) == 16) {
                    // add enpassant hash if enemy pawns are attacking the square
                    const auto ep_mask = Attacks::pawn(move.to().ep_square(), stm_);

                    if (ep_mask & occ(PieceType::PAWN, ~stm_)) {
                        assert(at(move.to().ep_square()) == Piece::NONE);
                        newhash ^= Zobrist::enpassant(move.to().ep_square().file());
                    }
                }
            }
        }

        if (move.type() != Move::CASTLING && move.type() != Move::PROMOTION) {
            assert(at(move.from()) != Piece::NONE);

            const auto piece = at(move.from());

            newhash ^= Zobrist::piece(piece, move.from()) ^ Zobrist::piece(piece, move.to());

        } else if constexpr (EXACT) {
            if (move.type() == Move::CASTLING) {
                const auto king = at(move.from());
                const auto rook = at(move.to());

                assert(king == Piece(PieceType::KING, stm_));
                assert(rook == Piece(PieceType::ROOK, stm_));

                const bool is_king_side = move.to() > move.from();
                const auto kingto = Square::castling_king_dest(is_king_side, stm_);
                const auto rookto = Square::castling_rook_dest(is_king_side, stm_);

                newhash ^= Zobrist::piece(king, move.from()) ^ Zobrist::piece(king, kingto);
                newhash ^= Zobrist::piece(rook, move.to()) ^ Zobrist::piece(rook, rookto);

            } else {
                const auto pawn = at(move.from());
                const auto prom = Piece(move.promotion_type(), stm_);

                assert(pawn == Piece(PieceType::PAWN, stm_));

                newhash ^= Zobrist::piece(pawn, move.from()) ^ Zobrist::piece(prom, move.to());
            }
        }

        if (EXACT && move.type() == Move::ENPASSANT) {
            assert(at(move.to().ep_square()).type() == PieceType::PAWN);

            const auto piece = Piece(PieceType::PAWN, ~stm_);

            newhash ^= Zobrist::piece(piece, move.to().ep_square());
        }

        return newhash;
    }


    void set_fen(std::string_view fen) {
        reset();

        while (!fen.empty() && fen[0] == ' ') fen.remove_prefix(1);
        assert(!fen.empty());

        const auto params = utils::split_string_view<6>(fen);
        const auto position = params[0].value_or("");
        const auto stm = params[1].value_or("w");
        const auto castling = params[2].value_or("-");
        const auto enpassant = params[3].value_or("-");
        const auto halfmoves = params[4].value_or("0");
        const auto fullmoves = params[5].value_or("1");
        assert(!position.empty());
        assert(stm == "w" || stm == "b");

        // pares fullmove and halfmoves
        halfmoves_ = utils::stringview_to_int(halfmoves).value_or(0);
        plies_ = utils::stringview_to_int(fullmoves).value_or(1);
        plies_ = plies_ * 2 - 2;

        // parse stm
        stm_ = (stm == "w") ? Color::WHITE : Color::BLACK;
        if (stm_ == Color::BLACK) plies_++;

        // parse enpassant square
        if (enpassant != "-") {
            enpassant_ = Square(enpassant);

            assert(enpassant_.is_valid());
            if (!(enpassant_.rank() == Rank::R3 && stm_ == Color::BLACK)
                && !(enpassant_.rank() == Rank::R6 && stm_ == Color::WHITE))
                enpassant_ = Square::NONE;
        }

        // parse pieces
        i32 square = 56;
        for (const char c : position) {
            if (isdigit(c))
                square += (c - '0');
            else if (c == '/')
                square -= 16;
            else {
                const auto piece = Piece(std::string_view(&c, 1));
                assert(piece != Piece::NONE);

                place_piece(piece, Square(square));
                square++;
            }
        }

        // parse castling
        for (const char c : castling) {
            if (c == '-') break;

            const auto king_side = CastlingRights::Side::KING_SIDE;
            const auto queen_side = CastlingRights::Side::QUEEN_SIDE;

            if (!chess960_) {
                if (c == 'K')
                    set_castling_rights(Color::WHITE, king_side, File::H);
                else if (c == 'Q')
                    set_castling_rights(Color::WHITE, queen_side, File::A);
                else if (c == 'k')
                    set_castling_rights(Color::BLACK, king_side, File::H);
                else if (c == 'q')
                    set_castling_rights(Color::BLACK, queen_side, File::A);
                else
                    continue;
            } else {
                // TODO: chess960 castling
            }
        }

        // validate enpassant
        if (enpassant_ != Square::NONE) {
            bool valid;
            if (stm_ == Color::WHITE)
                valid = Movegen::is_ep_valid<Color::WHITE>(*this, enpassant_);
            else
                valid = Movegen::is_ep_valid<Color::BLACK>(*this, enpassant_);

            if (!valid) enpassant_ = Square::NONE;
        }

        // init castling_path
        for (const Color color : {Color::WHITE, Color::BLACK}) {
            const auto king_from = king_square(color);

            for (const auto side :
                 {CastlingRights::Side::KING_SIDE, CastlingRights::Side::QUEEN_SIDE}) {
                if (!castle_rights_.has(color, side)) continue;

                const bool is_king_side = (side == CastlingRights::Side::KING_SIDE);
                const auto rook_from
                    = Square(castle_rights_.get_rook_file(color, side), king_from.rank());
                const auto king_to = Square::castling_king_dest(is_king_side, color);
                const auto rook_to = Square::castling_rook_dest(is_king_side, color);

                castle_path_[color][is_king_side]
                    = (Attacks::between(rook_from, rook_to) | Attacks::between(king_from, king_to))
                      & ~(BitBoard::from_square(king_from) | BitBoard::from_square(rook_from));
            }
        }

        hash_ = compute_hash();
    }

    [[nodiscard]] std::string get_fen() const {
        std::string fen;
        fen.reserve(100);

        // write pieces
        for (i32 rank = 7; rank >= 0; rank--) {
            i32 gap = 0;

            for (i32 file = 0; file < 8; file++) {
                const i32 sq = rank * 8 + file;

                if (Piece piece = at(Square(sq)); piece != Piece::NONE) {
                    if (gap) {
                        fen += std::to_string(gap);
                        gap = 0;
                    }

                    fen += static_cast<std::string>(piece);
                } else
                    gap++;
            }

            if (gap != 0) fen += std::to_string(gap);
            fen += (rank > 0 ? "/" : "");
        }

        // write stm
        fen += ' ';
        fen += (stm_ == Color::WHITE ? 'w' : 'b');

        // write castling
        fen += ' ';
        if (castle_rights_.is_empty())
            fen += '-';
        else {
            if (!chess960_) {
                if (castle_rights_.has(Color::WHITE, CastlingRights::Side::KING_SIDE)) fen += 'K';
                if (castle_rights_.has(Color::WHITE, CastlingRights::Side::QUEEN_SIDE)) fen += 'Q';
                if (castle_rights_.has(Color::BLACK, CastlingRights::Side::KING_SIDE)) fen += 'k';
                if (castle_rights_.has(Color::BLACK, CastlingRights::Side::QUEEN_SIDE)) fen += 'q';
            } else {
                // TODO: chess960 castling
            }
        }

        // write enpassant
        fen += ' ';
        fen += (enpassant_ == Square::NONE ? "-" : static_cast<std::string>(enpassant_));

        // write halfmove and fullmoves
        fen += ' ';
        fen += std::to_string(halfmoves());
        fen += ' ';
        fen += std::to_string(fullmoves());

        return fen;
    }

private:
    void reset() {
        pieces_.fill(0);
        occ_.fill(0);
        mailbox_.fill(Piece::NONE);

        castle_rights_.clear();
        castle_path_ = {};
        enpassant_ = Square::NONE;

        stm_ = Color::WHITE;
        halfmoves_ = 0;
        plies_ = 1;

        hash_ = 0;
    }


    void place_piece(Piece piece, Square sq) {
        assert(mailbox_[sq] == Piece::NONE);

        auto pt = piece.type();
        auto color = piece.color();

        assert(pt != PieceType::NONE);
        assert(color != Color::NONE);

        pieces_[pt].set(sq);
        occ_[color].set(sq);
        mailbox_[sq] = piece;
    }
    void remove_piece(Piece piece, Square sq) {
        assert(mailbox_[sq] == piece && piece != Piece::NONE);

        const auto pt = piece.type();
        const auto color = piece.color();

        assert(pt != PieceType::NONE);
        assert(color != Color::NONE);

        pieces_[pt].unset(sq);
        occ_[color].unset(sq);
        mailbox_[sq] = Piece::NONE;
    }


    [[nodiscard]] u64 compute_hash() const {
        u64 hash_key = 0;

        auto pieces = occ();
        while (pieces) {
            const auto sq = Square(pieces.poplsb());
            hash_key ^= Zobrist::piece(at(sq), sq);
        }

        u64 ep_hash = 0;
        if (enpassant_ != Square::NONE) ep_hash ^= Zobrist::enpassant(enpassant_.file());

        u64 stm_hash = 0;
        if (stm_ == Color::WHITE) stm_hash ^= Zobrist::stm();

        u64 castling_hash = 0;
        castling_hash ^= Zobrist::castling(castle_rights_.hash_index());

        return hash_key ^ ep_hash ^ stm_hash ^ castling_hash;
    }


    void set_castling_rights(Color color, CastlingRights::Side side, File rook_file) {
        // check the rook and king actually exists they're supposed to
        const auto king_sq = king_square(color);
        const auto rook_sq = Square(rook_file, king_sq.rank());
        const auto piece = at(rook_sq);
        if (piece.type() != PieceType::ROOK || piece.color() != color) return;

        castle_rights_.set(color, side, rook_file);
    }
};
}  // namespace chess
