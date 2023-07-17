#pragma once
#include "GamePlayer.hpp"
#include "chess.hpp"
#include <SFML/Graphics.hpp>
#include <string>



namespace cge { // chess game engine
class HumanPlayer: public cge::GamePlayer {
public:
    // Initializes a HumanPlayer with a name
    HumanPlayer(std::string name_in): GamePlayer(name_in) {}


    // Asks the human to return a move using the UI. Should return immediately if halt becomes true
    chess::Move get_move(chess::Board board, int t_remain, sf::Event& event, bool& halt) {
        chess::Square sq_from = chess::Square::NO_SQ;
        chess::Square sq_to = chess::Square::NO_SQ;

        // generate movelist
        chess::Movelist movelist;
        chess::movegen::legalmoves(movelist, board);

        // ui controls for move selection
        while (!halt && (sq_to==chess::Square::NO_SQ || sq_from==chess::Square::NO_SQ)) {
            // onclick
            if (event.type==sf::Event::MouseButtonPressed && event.mouseButton.button==sf::Mouse::Left) {
                int x = event.mouseButton.x;
                int y = event.mouseButton.y;

                // board clicked
                if (x>50 && x<850 && y>70 && y<870) {
                    chess::Square sq = get_square(x, y);
                    int piece = (int)board.at(sq);
                    
                    // own pieces clicked
                    if ((iswhite && piece>=0 && piece<6) || (!iswhite && piece>=6 && piece<12)) {
                        sq_from = sq;
                        sq_to = chess::Square::NO_SQ;
                    }

                    // destination clicked
                    if (sq_from!=chess::Square::NO_SQ && sq_from!=sq) {
                        chess::Move move = move_if_valid(sq_from, sq, movelist, board);
                        if (move!=chess::Move::NO_MOVE)
                            return move;
                        else
                            sq_from = chess::Square::NO_SQ;
                    }
                }
            }
        }
        return chess::Move::NO_MOVE;
    }

private:
    // Converts x and y coordinates into a Square
    static chess::Square get_square(int x, int y) {
        int rank = (870 - y) / 100;
        int file = (x - 50) / 100;
        return chess::utils::fileRankSquare(chess::File(file), chess::Rank(rank));
    }


    // Returns a move if the move from sq_from to sq_to is valid
    static chess::Move move_if_valid(chess::Square sq_from, chess::Square sq_to, const chess::Movelist& movelist, const chess::Board& board) {
        chess::Piece piece = board.at(sq_from);

        // castling
        if (piece==chess::Piece::WHITEKING || piece==chess::Piece::BLACKKING) {
            chess::Color col = board.color(piece);

            // white
            if (col==chess::Color::WHITE) {
                if (sq_to==chess::Square::SQ_G1 &&      // white king-side
                    board.castlingRights().hasCastlingRight(col, chess::CastleSide::KING_SIDE))
                    sq_to = chess::Square::SQ_H1;
                else if (sq_to==chess::Square::SQ_C1 && // white queen-side
                    board.castlingRights().hasCastlingRight(col, chess::CastleSide::QUEEN_SIDE))
                    sq_to = chess::Square::SQ_A1;
            
            // black
            } else {
                if (sq_to==chess::Square::SQ_G8 &&      // black king-side
                    board.castlingRights().hasCastlingRight(col, chess::CastleSide::KING_SIDE))
                    sq_to = chess::Square::SQ_H8;
                else if (sq_to==chess::Square::SQ_C8 && // black queen-side
                    board.castlingRights().hasCastlingRight(col, chess::CastleSide::QUEEN_SIDE))
                    sq_to = chess::Square::SQ_A8;
            }
        }
        
        for (auto move : movelist)
            if (move.from()==sq_from && move.to()==sq_to)
                return move;
        return chess::Move::NO_MOVE;
    }
};  // HumanPlayer
}   // namespace cge