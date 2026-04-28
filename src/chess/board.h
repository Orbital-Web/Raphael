#pragma once
#include <chess/movegen_fwd.h>
#include <chess/utils.h>
#include <chess/zobrist.h>



namespace chess {
class Board {
public:
    static constexpr auto STARTPOS = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    class CastlingRights {
    private:
        u16 rooks_ = 0x8888;  // 4-bit values for [color][castle]
        static_assert(static_cast<u32>(File::NONE) == 0x8);

    public:
        enum Side : u8 { KING_SIDE, QUEEN_SIDE };

        constexpr void set(Color color, Side castle, File rook_file) {
            i32 shift = 4 * (castle + 2 * color);
            u32 mask = 0xF << shift;
            rooks_ = (rooks_ & ~mask) | (static_cast<u32>(rook_file) << shift);
        }

        constexpr void clear() { rooks_ = 0x8888; }
        constexpr void clear(Color color) {
            i32 shift = 4 * 2 * color;
            u32 mask = 0xFF << shift;
            rooks_ = (rooks_ & ~mask) | (0x88 << shift);
        }
        constexpr i32 clear(Color color, Side castle) {
            set(color, castle, File::NONE);
            return castle + color * 2;
        }

        [[nodiscard]] constexpr bool has(Color color, Side castle) const {
            return get_rook_file(color, castle) != File::NONE;
        }

        [[nodiscard]] constexpr bool has(Color color) const {
            return has(color, Side::KING_SIDE) || has(color, Side::QUEEN_SIDE);
        }

        [[nodiscard]] constexpr File get_rook_file(Color color, Side castle) const {
            i32 shift = 4 * (castle + 2 * color);
            return File((rooks_ >> shift) & 0xF);
        }

        [[nodiscard]] constexpr Square get_rook_square(Color color, Side castle) const {
            const auto file = get_rook_file(color, castle);
            if (file == File::NONE) return Square::NONE;
            const auto rank = Rank(color * 7);
            return Square(file, rank);
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
    };

private:
    std::array<BitBoard, 6> pieces_ = {};          // [048] 48  bitboard per piece type
    std::array<BitBoard, 2> occ_ = {};             // [064] 16  bitboard per color
    std::array<Piece, 64> mailbox_ = {};           // [128] 64  piece on each square
    BitBoard threats_ = {};                        // [136] 8   attacked sqs by ntm (xrays stm king)
    std::array<BitBoard, 2> pinmask_ = {};         // [152] 16  pin rays per color
    std::array<BitBoard, 4> checkzones_ = {};      // [184] 32  checkzones of ntm king for pnbr
    MultiArray<BitBoard, 2, 2> castle_path_ = {};  // [216] 32  castling path for color and side
    u64 hash_ = 0;                                 // [224] 8   zobrist hash
    u64 pawn_hash_ = 0;                            // [232] 8   zobrist hash of pawns
    u64 major_hash_ = 0;                           // [240] 8   zobrist hash of major pieces
    u64 nonpawn_hash_[2] = {};                     // [256] 16  zobrist hash of non-pawns per color
    CastlingRights castle_rights_ = {};            // [264] 2   allowed castling files
    u16 plies_ = 1;                                // [264] 2   number of plies
    u8 halfmoves_ = 0;                             // [264] 1   plies since last capture/pawn move
    Color stm_ = Color::WHITE;                     // [264] 1   current stm
    Square enpassant_ = Square::NONE;              // [264] 1   enpassant square
    bool chess960_ = false;                        // [264] 1   whether chess960 is enabled


public:
    explicit Board(std::string_view fen = STARTPOS, bool chess960 = false): chess960_(chess960) {
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

    [[nodiscard]] i32 halfmove_bucket() const { return halfmove_bucket(halfmoves_); }
    [[nodiscard]] i32 halfmove_bucket(u8 halfmoves) const {
        return (halfmoves < 8) ? 0 : std::min((halfmoves - 6) / 8, 15);
    }

    [[nodiscard]] u64 hash() const { return hash_ ^ Zobrist::halfmove_clock(halfmove_bucket()); }
    [[nodiscard]] u64 pawn_hash() const { return pawn_hash_; }
    [[nodiscard]] u64 major_hash() const { return major_hash_; }
    [[nodiscard]] u64 nonpawn_hash(Color color) const { return nonpawn_hash_[color]; }

    [[nodiscard]] bool chess960() const { return chess960_; }

    void set960(bool chess960) { chess960_ = chess960; }


    [[nodiscard]] Square king_square(Color color) const {
        assert(occ(PieceType::KING, color).count() == 1);
        return Square(occ(PieceType::KING, color).lsb());
    }

    [[nodiscard]] bool is_kingpawn(Color color) const {
        return (occ(color) ^ occ(PieceType::PAWN, color)).count() == 1;
    }


    [[nodiscard]] i32 material() const {
        return occ(PieceType::PAWN).count() + 3 * occ(PieceType::KNIGHT).count()
               + 3 * occ(PieceType::BISHOP).count() + 5 * occ(PieceType::ROOK).count()
               + 9 * occ(PieceType::QUEEN).count();
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
        else if (count == 4)
        {
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


    [[nodiscard]] BitBoard threats() const { return threats_; }

    [[nodiscard]] BitBoard pinned(Color color) const { return pinmask_[color] & occ(color); }

    [[nodiscard]] BitBoard pinmask(Color color) const { return pinmask_[color]; }

    [[nodiscard]] BitBoard checkzones(PieceType pt) const {
        assert(pt != PieceType::KING);
        return (pt != PieceType::QUEEN)
                   ? checkzones_[pt]
                   : checkzones_[PieceType::BISHOP] | checkzones_[PieceType::ROOK];
    }


    [[nodiscard]] bool is_attacked(Square sq, Color color) const {
        if (color == ~stm_) return threats_.is_set(sq);

        // cheap checks first
        const auto xrayocc = occ() ^ BitBoard::from_square(king_square(stm_));
        if (Attacks::pawn(sq, ~color) & occ(PieceType::PAWN, color)) return true;
        if (Attacks::knight(sq) & occ(PieceType::KNIGHT, color)) return true;
        if (Attacks::king(sq) & occ(PieceType::KING, color)) return true;
        if (Attacks::bishop(sq, xrayocc) & (occ(PieceType::BISHOP) | occ(PieceType::QUEEN))
            & occ(color))
            return true;
        if (Attacks::rook(sq, xrayocc) & (occ(PieceType::ROOK) | occ(PieceType::QUEEN))
            & occ(color))
            return true;

        return false;
    }

    [[nodiscard]] bool gives_direct_check(Move move) const {
        auto pt = at(move.from()).type();
        auto sq = move.to();

        if (move.type() == Move::PROMOTION)
            pt = move.promotion_type();
        else if (move.type() == Move::CASTLING) {
            const bool is_king_side = move.to() > move.from();
            pt = PieceType::ROOK;
            sq = Square::castling_rook_dest(is_king_side, stm_);
        }

        return (pt == PieceType::KING) ? false : checkzones(pt).is_set(sq);
    }

    [[nodiscard]] bool is_legal(Move move) const { return Movegen::is_legal(*this, move); }


    void make_move(Move move) {
        assert((at(move.from()) < Piece::BLACKPAWN) == (stm_ == Color::WHITE));

        const auto captured = at(move.to());
        const auto capture = captured != Piece::NONE && move.type() != Move::CASTLING;
        const auto pt = at(move.from()).type();

        halfmoves_++;
        plies_++;

        if (enpassant_ != Square::NONE) update_ep_hash(enpassant_.file());
        enpassant_ = Square::NONE;

        if (capture) {
            remove_piece(captured, move.to());

            halfmoves_ = 0;

            // remove castling rights if rook is captured
            if (captured.type() == PieceType::ROOK && move.to().rank().is_back_rank(~stm_)) {
                const auto king_sq = king_square(~stm_);
                const auto side = CastlingRights::closest_side(move.to().file(), king_sq.file());

                if (castle_rights_.get_rook_file(~stm_, side) == move.to().file())
                    update_castling_change_hash(castle_rights_.clear(~stm_, side));
            }
        }

        if (pt == PieceType::KING && castle_rights_.has(stm_)) {
            // remove castling rights if king moves
            update_castling_hash(castle_rights_.hash_index());
            castle_rights_.clear(stm_);
            update_castling_hash(castle_rights_.hash_index());

        } else if (pt == PieceType::ROOK && move.from().is_back_rank(stm_)) {
            // remove castling rights if rook moves from back rank
            const auto king_sq = king_square(stm_);
            const auto side = CastlingRights::closest_side(move.from().file(), king_sq.file());

            if (castle_rights_.get_rook_file(stm_, side) == move.from().file())
                update_castling_change_hash(castle_rights_.clear(stm_, side));

        } else if (pt == PieceType::PAWN) {
            halfmoves_ = 0;

            // double push
            if (Square::value_distance(move.to(), move.from()) == 16) {
                // add enpassant hash if enemy pawns are attacking the square
                const auto ep_mask = Attacks::pawn(move.to().ep_square(), stm_);

                if (ep_mask & occ(PieceType::PAWN, ~stm_)) {
                    assert(at(move.to().ep_square()) == Piece::NONE);
                    enpassant_ = move.to().ep_square();
                    update_ep_hash(enpassant_.file());
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
        } else if (move.type() == Move::PROMOTION) {
            const auto pawn = at(move.from());
            const auto prom = Piece(move.promotion_type(), stm_);

            assert(pawn == Piece(PieceType::PAWN, stm_));

            remove_piece(pawn, move.from());
            place_piece(prom, move.to());
        } else {
            assert(at(move.from()) != Piece::NONE);
            assert(at(move.to()) == Piece::NONE);

            const auto piece = at(move.from());

            remove_piece(piece, move.from());
            place_piece(piece, move.to());
        }

        if (move.type() == Move::ENPASSANT) {
            assert(at(move.to().ep_square()).type() == PieceType::PAWN);

            const auto piece = Piece(PieceType::PAWN, ~stm_);

            remove_piece(piece, move.to().ep_square());
        }

        hash_ ^= Zobrist::stm();
        stm_ = ~stm_;
        threats_ = compute_threats();
        pinmask_[Color::WHITE] = compute_pinmask(Color::WHITE);
        pinmask_[Color::BLACK] = compute_pinmask(Color::BLACK);
        update_checkzones();
    }

    void make_nullmove() {
        hash_ ^= Zobrist::stm();
        if (enpassant_ != Square::NONE) update_ep_hash(enpassant_.file());
        enpassant_ = Square::NONE;

        plies_++;
        stm_ = ~stm_;
        threats_ = compute_threats();
        update_checkzones();
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
        u8 new_halfmoves = halfmoves_ + 1;

        newhash ^= Zobrist::stm();
        if (EXACT && enpassant_ != Square::NONE) newhash ^= Zobrist::enpassant(enpassant_.file());

        if (move == Move::NULL_MOVE) return newhash ^ Zobrist::halfmove_clock(halfmove_bucket());

        const auto captured = at(move.to());
        const auto capture = captured != Piece::NONE && move.type() != Move::CASTLING;
        const auto pt = at(move.from()).type();

        if (capture) {
            newhash ^= Zobrist::piece(captured, move.to());
            new_halfmoves = 0;

            // remove castling rights if rook is captured
            if (EXACT && captured.type() == PieceType::ROOK && move.to().rank().is_back_rank(~stm_))
            {
                const auto king_sq = king_square(~stm_);
                const auto side = CastlingRights::closest_side(move.to().file(), king_sq.file());

                if (castle_rights_.get_rook_file(~stm_, side) == move.to().file())
                    newhash ^= Zobrist::castle_index(~stm_ * 2 + side);
            }
        } else if (pt == PieceType::PAWN)
            new_halfmoves = 0;

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

        return newhash ^ Zobrist::halfmove_clock(halfmove_bucket(new_halfmoves));
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
                const auto color = std::isupper(c) ? Color::WHITE : Color::BLACK;
                const auto king_sq = king_square(color);

                if (c == 'K' || c == 'k') {
                    const auto file = find_rook_on_side(color, king_side);
                    if (file != File::NONE) set_castling_rights(color, king_side, file);
                } else if (c == 'Q' || c == 'q') {
                    const auto file = find_rook_on_side(color, queen_side);
                    if (file != File::NONE) set_castling_rights(color, queen_side, file);
                } else {
                    const auto file = File(std::string_view(&c, 1));
                    if (file == File::NONE) continue;
                    const auto side = CastlingRights::closest_side(file, king_sq.file());
                    set_castling_rights(color, side, file);
                }
            }
        }

        threats_ = compute_threats();
        pinmask_[Color::WHITE] = compute_pinmask(Color::WHITE);
        pinmask_[Color::BLACK] = compute_pinmask(Color::BLACK);
        update_checkzones();

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
                 {CastlingRights::Side::KING_SIDE, CastlingRights::Side::QUEEN_SIDE})
            {
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

        recompute_hash();
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
            const auto king_side = CastlingRights::Side::KING_SIDE;
            const auto queen_side = CastlingRights::Side::QUEEN_SIDE;

            if (!chess960_) {
                if (castle_rights_.has(Color::WHITE, king_side)) fen += 'K';
                if (castle_rights_.has(Color::WHITE, queen_side)) fen += 'Q';
                if (castle_rights_.has(Color::BLACK, king_side)) fen += 'k';
                if (castle_rights_.has(Color::BLACK, queen_side)) fen += 'q';
            } else {
                for (const auto color : {Color::WHITE, Color::BLACK}) {
                    for (const auto side : {king_side, queen_side}) {
                        if (castle_rights_.has(color, side)) {
                            const auto rook_file = castle_rights_.get_rook_file(color, side);
                            const auto filestr = static_cast<std::string>(rook_file);
                            fen += (color == Color::WHITE) ? std::toupper(filestr[0]) : filestr[0];
                        }
                    }
                }
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


    [[nodiscard]] std::string pretty_print() const {
        std::string boardstr;
        boardstr.reserve(400);

        for (i32 rank = Rank::R8; rank >= Rank::R1; --rank) {
            boardstr += std::to_string(rank + 1) + " ";
            for (i32 file = File::A; file <= File::H; ++file) {
                const auto piece = at(Square(File(file), Rank(rank)));
                boardstr += static_cast<std::string>(piece) + ' ';
            }

            // show other meta info
            if (rank == Rank::R7) {
                boardstr += " Side: ";
                boardstr += ((stm_ == Color::WHITE) ? "White" : "Black");
            } else if (rank == Rank::R6) {
                boardstr += " Castling:  ";

                if (castle_rights_.is_empty())
                    boardstr += '-';
                else {
                    const auto king_side = CastlingRights::Side::KING_SIDE;
                    const auto queen_side = CastlingRights::Side::QUEEN_SIDE;

                    if (!chess960_) {
                        if (castle_rights_.has(Color::WHITE, king_side)) boardstr += 'K';
                        if (castle_rights_.has(Color::WHITE, queen_side)) boardstr += 'Q';
                        if (castle_rights_.has(Color::BLACK, king_side)) boardstr += 'k';
                        if (castle_rights_.has(Color::BLACK, queen_side)) boardstr += 'q';
                    } else {
                        for (const auto color : {Color::WHITE, Color::BLACK}) {
                            for (const auto side : {king_side, queen_side}) {
                                if (castle_rights_.has(color, side)) {
                                    const auto rook_file
                                        = castle_rights_.get_rook_file(color, side);
                                    const auto filestr = static_cast<std::string>(rook_file);
                                    boardstr += (color == Color::WHITE) ? std::toupper(filestr[0])
                                                                        : filestr[0];
                                }
                            }
                        }
                    }
                }
            } else if (rank == Rank::R5) {
                boardstr += " Enpassant: ";
                boardstr
                    += ((enpassant_ == Square::NONE) ? "-" : static_cast<std::string>(enpassant_));
            } else if (rank == Rank::R4)
                boardstr += " Halfmoves: " + std::to_string(halfmoves());
            else if (rank == Rank::R3)
                boardstr += " Fullmoves: " + std::to_string(fullmoves());

            boardstr += '\n';
        }
        boardstr += "  a b c d e f g h\n";
        return boardstr;
    }

private:
    void reset() {
        pieces_.fill(0);
        occ_.fill(0);
        mailbox_.fill(Piece::NONE);

        threats_ = 0;

        castle_rights_.clear();
        castle_path_ = {};
        enpassant_ = Square::NONE;

        stm_ = Color::WHITE;
        halfmoves_ = 0;
        plies_ = 1;

        hash_ = 0;
        pawn_hash_ = 0;
        major_hash_ = 0;
        nonpawn_hash_[Color::WHITE] = 0;
        nonpawn_hash_[Color::BLACK] = 0;
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

        update_piece_hash(piece, sq);
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

        update_piece_hash(piece, sq);
    }


    void update_piece_hash(Piece piece, Square sq) {
        const auto key = Zobrist::piece(piece, sq);
        hash_ ^= key;
        if (piece.type() == PieceType::PAWN)
            pawn_hash_ ^= key;
        else
            nonpawn_hash_[piece.color()] ^= key;

        if (piece.type() == PieceType::ROOK || piece.type() == PieceType::QUEEN) major_hash_ ^= key;
    }

    void update_ep_hash(File file) {
        const auto key = Zobrist::enpassant(file);
        hash_ ^= key;
        pawn_hash_ ^= key;
    }

    void update_castling_hash(i32 castling) {
        const auto key = Zobrist::castling(castling);
        hash_ ^= key;
        major_hash_ ^= key;
    }

    void update_castling_change_hash(i32 index) {
        const auto key = Zobrist::castle_index(index);
        hash_ ^= key;
        major_hash_ ^= key;
    }

    void recompute_hash() {
        hash_ = 0;
        pawn_hash_ = 0;
        major_hash_ = 0;
        nonpawn_hash_[Color::WHITE] = 0;
        nonpawn_hash_[Color::BLACK] = 0;

        auto pieces = occ();
        while (pieces) {
            const auto sq = Square(pieces.poplsb());
            update_piece_hash(at(sq), sq);
        }

        if (enpassant_ != Square::NONE) update_ep_hash(enpassant_.file());

        if (stm_ == Color::WHITE) hash_ ^= Zobrist::stm();

        update_castling_hash(castle_rights_.hash_index());
    }


    [[nodiscard]] BitBoard compute_threats() const {
        BitBoard threats;

        const auto xrayocc = occ() ^ BitBoard::from_square(king_square(stm_));
        const auto queens = occ(PieceType::QUEEN, ~stm_);

        auto rooks = occ(PieceType::ROOK, ~stm_) | queens;
        while (rooks) {
            const auto sq = Square(rooks.poplsb());
            threats |= Attacks::rook(sq, xrayocc);
        }

        auto bishops = occ(PieceType::BISHOP, ~stm_) | queens;
        while (bishops) {
            const auto sq = Square(bishops.poplsb());
            threats |= Attacks::bishop(sq, xrayocc);
        }

        auto knights = occ(PieceType::KNIGHT, ~stm_);
        while (knights) {
            const auto sq = Square(knights.poplsb());
            threats |= Attacks::knight(sq);
        }

        const auto pawns = occ(PieceType::PAWN, ~stm_);
        if (~stm_ == Color::WHITE)
            threats |= Attacks::pawn_left<Color::WHITE>(pawns)
                       | Attacks::pawn_right<Color::WHITE>(pawns);
        else
            threats |= Attacks::pawn_left<Color::BLACK>(pawns)
                       | Attacks::pawn_right<Color::BLACK>(pawns);

        threats |= Attacks::king(king_square(~stm_));

        return threats;
    }

    [[nodiscard]] BitBoard compute_pinmask(Color color) const {
        const auto occ_us = occ(color);
        const auto occ_opp = occ(~color);
        const auto king_sq = king_square(color);

        const auto opp_queen = occ(PieceType::QUEEN, ~color);
        const auto opp_bishop = occ(PieceType::BISHOP, ~color);
        const auto opp_rook = occ(PieceType::ROOK, ~color);
        auto pt_attacks = (Attacks::bishop(king_sq, occ_opp) & (opp_bishop | opp_queen))
                          | (Attacks::rook(king_sq, occ_opp) & (opp_rook | opp_queen));

        BitBoard pin;
        while (pt_attacks) {
            const auto possible_pin = Attacks::between(king_sq, Square(pt_attacks.poplsb()));
            if ((possible_pin & occ_us).count() == 1) pin |= possible_pin;
        }
        return pin;
    }

    void update_checkzones() {
        checkzones_[PieceType::PAWN] = Attacks::pawn(king_square(~stm_), ~stm_);
        checkzones_[PieceType::KNIGHT] = Attacks::knight(king_square(~stm_));
        checkzones_[PieceType::BISHOP] = Attacks::bishop(king_square(~stm_), occ());
        checkzones_[PieceType::ROOK] = Attacks::rook(king_square(~stm_), occ());
    }

    void set_castling_rights(Color color, CastlingRights::Side side, File rook_file) {
        // check the rook and king actually exists where they're supposed to
        const auto king_sq = king_square(color);
        const auto rook_sq = Square(rook_file, king_sq.rank());
        const auto piece = at(rook_sq);
        if (piece != Piece(PieceType::ROOK, color)) return;

        castle_rights_.set(color, side, rook_file);
    }

    [[nodiscard]] File find_rook_on_side(Color color, CastlingRights::Side side) const {
        const auto king_side = CastlingRights::Side::KING_SIDE;
        const auto king_sq = king_square(color);
        const auto corner_sq = Square((side == king_side) ? File::H : File::A, king_sq.rank());

        if (side == king_side) {
            for (Square sq = king_sq + 1; sq <= corner_sq; ++sq)
                if (at(sq) == Piece(PieceType::ROOK, color)) return sq.file();
        } else {
            for (Square sq = king_sq - 1; sq >= corner_sq; --sq)
                if (at(sq) == Piece(PieceType::ROOK, color)) return sq.file();
        }

        return File::NONE;
    }
};

static_assert(sizeof(Board) == 264);
}  // namespace chess
