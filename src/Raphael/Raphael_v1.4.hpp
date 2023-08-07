#pragma once
#include <chess.hpp>
#include <GameEngine/utils.hpp>
#include <Raphael/consts.hpp>
#include <Raphael/Transposition.hpp>
#include <Raphael/Killers.hpp>
#include <GameEngine/GamePlayer.hpp>
#include <future>



namespace Raphael {
class v1_4: public cge::GamePlayer {
// Raphael vars
private:
    TranspositionTable tt;
    chess::Move itermove;       // best move from previous iteration
    uint64_t ponderkey = 0;     // hashed board after ponder move
    int pondereval = 0;         // eval we got during ponder
    int ponderdepth = 1;        // depth we searched to during ponder
    Killers killers;            // killer moves at each ply



// Raphael methods
public:
    // Initializes Raphael with a name
    v1_4(std::string name_in): GamePlayer(name_in), tt(TABLE_SIZE) {
        PST::init_pst();
        PMASK::init_pawnmask();
    }


    // Uses iterative deepening on Negamax to find best move
    // Should return immediately if halt becomes true
    chess::Move get_move(chess::Board board, float t_remain, sf::Event& event, bool& halt) {
        int depth = 1;
        int eval = 0;
        int alpha = -INT_MAX;
        int beta = INT_MAX;
        chess::Move toPlay = chess::Move::NO_MOVE;    // overall best move

        // if ponderhit, start with ponder result and depth
        if (board.hash() != ponderkey)
            itermove = chess::Move::NO_MOVE;
        else {
            depth = ponderdepth;
            eval = pondereval;
            alpha = eval - ASPIRATION_WINDOW;
            beta = eval + ASPIRATION_WINDOW;
        }

        // stop search after an appropriate duration
        int duration = search_time(board, t_remain);
        auto _ = std::async(manage_time, std::ref(halt), duration);

        // begin iterative deepening
        while (!halt && depth<=MAX_DEPTH) {
            killers.clear();
            int itereval = negamax(board, depth, 0, alpha, beta, halt);

            // not timeout
            if (!halt) {
                eval = itereval;
            
                // re-search required
                if ((eval<=alpha) || (eval>=beta)) {
                    alpha = -INT_MAX;
                    beta = INT_MAX;
                    continue;
                }

                // narrow window
                alpha = eval - ASPIRATION_WINDOW;
                beta = eval + ASPIRATION_WINDOW;
                depth++;
            }

            if (itermove != chess::Move::NO_MOVE)
                toPlay = itermove;
            
            // checkmate, no need to continue
            if (tt.isMate(eval)) {
                #ifndef MUTEEVAL
                // get absolute evaluation (i.e, set to white's perspective)
                if (whiteturn == (eval > 0))
                    printf("Eval: +#\n");
                else
                    printf("Eval: -#\n");
                #endif
                halt = true;
                return toPlay;
            }
        }
        #ifndef MUTEEVAL
        // get absolute evaluation (i.e, set to white's perspective)
        if (!whiteturn)
            eval *= -1;
        printf("Eval: %.2f\tDepth: %d\n", eval/100.0f, depth-1);
        #endif
        return toPlay;
    }


    // Think during opponent's turn. Should return immediately if halt becomes true
    void ponder(chess::Board board, bool& halt) {
        pondereval = 0;
        ponderdepth = 1;
        int depth = 1;
        itermove = chess::Move::NO_MOVE;    // opponent's best move

        // begin iterative deepening up to depth 4 for opponent's best move
        while (!halt && depth<=4) {
            killers.clear();
            int eval = negamax(board, depth, 0, -INT_MAX, INT_MAX, halt);
            
            // checkmate, no need to continue
            if (tt.isMate(eval))
                break;
            depth++;
        }

        // not enough time to continue
        if (halt || itermove==chess::Move::NO_MOVE) return;

        // store move to check for ponderhit on our turn
        board.makeMove(itermove);
        ponderkey = board.hash();
        chess::Move toPlay = chess::Move::NO_MOVE;    // our best response
        itermove = chess::Move::NO_MOVE;

        int alpha = -INT_MAX;
        int beta = INT_MAX;

        // begin iterative deepening for our best response
        while (!halt && ponderdepth<=MAX_DEPTH) {
            killers.clear();
            int itereval = negamax(board, ponderdepth, 0, alpha, beta, halt);

            if (!halt) {
                pondereval = itereval;

                // re-search required
                if ((pondereval<=alpha) || (pondereval>=beta)) {
                    alpha = -INT_MAX;
                    beta = INT_MAX;
                    continue;
                }

                // narrow window
                alpha = pondereval - ASPIRATION_WINDOW;
                beta = pondereval + ASPIRATION_WINDOW;
                ponderdepth++;
            }

            // store into toPlay to prevent NO_MOVE
            if (itermove != chess::Move::NO_MOVE)
                toPlay = itermove;
            
            // checkmate, no need to continue (but don't edit halt)
            if (tt.isMate(pondereval))
                break;
        }

        // override in case of NO_MOVE
        itermove = toPlay;
    }


    // Resets the player
    void reset() {
        tt.clear();
        itermove = chess::Move::NO_MOVE;
    }

private:
    // Estimates the time (ms) it should spend on searching a move
    int search_time(const chess::Board& board, const float t_remain) {
        // ratio: a function within [0, 1]
        // in this case it's a function at 0 when n_pieces = 0 or 32
        // and 1 when n_pieces is 11
        float n = chess::builtin::popcount(board.occ());
        float ratio = 0.0138*(32-n)*(n/32)*pow(2.5-n/32, 3);
        // use 0.5~4% of the remaining time based on the ratio
        float duration = t_remain * (0.005 + 0.035*ratio);
        return duration*1000;
    }


    // Sets halt to true if duration (ms) passes
    // Must be called asynchronously
    static void manage_time(bool& halt, const int duration) {
        auto start = std::chrono::high_resolution_clock::now();
        while (!halt) {
            auto now = std::chrono::high_resolution_clock::now();
            auto dtime = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            if (dtime >= duration)
                halt = true;
        }
    }


    // The Negamax search algorithm to search for the best move
    int negamax(chess::Board& board, unsigned int depth, int ply, int alpha, int beta, bool& halt) {
        // timeout
        if (halt)
            return 0;
        
        // transposition lookup
        int alphaorig = alpha;
        auto ttkey = board.hash();
        auto entry = tt.get(ttkey, ply);
        if (tt.valid(entry, ttkey, depth)) {
            if (entry.flag == tt.EXACT) {
                if (!ply) itermove = entry.move;
                return entry.eval;
            }
            else if (entry.flag == tt.LOWER)
                alpha = std::max(alpha, entry.eval);
            else
                beta = std::min(beta, entry.eval);
            
            // prune
            if (alpha >= beta) {
                if (!ply) itermove = entry.move;
                return entry.eval;
            }
        }

        // checkmate/draw
        auto result = board.isGameOver().second;
        if (result == chess::GameResult::DRAW)
            return 0;
        else if (result == chess::GameResult::LOSE)
            return -MATE_EVAL + ply;  // reward faster checkmate
        
        // terminal depth
        if (depth == 0)
            return quiescence(board, alpha, beta, halt);
        
        // search
        chess::Movelist movelist;
        order_moves(movelist, board, ply);
        chess::Move bestmove = chess::Move::NO_MOVE;  // best move in this position

        for (const auto& move : movelist) {
            board.makeMove(move);
            int eval = -negamax(board, depth-1, ply+1, -beta, -alpha, halt);
            board.unmakeMove(move);

            // timeout
            if (halt)
                return 0;

            // update eval
            if (eval > alpha) {
                alpha = eval;
                bestmove = move;
                if (!ply) itermove = move;
            }

            // prune
            if (alpha >= beta) {
                // store killer move (ignore captures/promotions)
                int to = (int)board.at(move.to());
                if (board.isCapture(move) || move.typeOf()==chess::Move::PROMOTION)
                    killers.put(move, ply);
                // update transposition
                tt.set({ttkey, depth, tt.LOWER, alpha, move}, ply);
                return beta;
            }
        }

        // update transposition
        TranspositionTable::Flag flag  = (alpha <= alphaorig) ? tt.UPPER : tt.EXACT;
        tt.set({ttkey, depth, flag, alpha, bestmove}, ply);
        return alpha;
    }


    // Quiescence search for all captures
    int quiescence(chess::Board& board, int alpha, int beta, bool& halt) const {
        int eval = evaluate(board);

        // timeout
        if (halt)
            return eval;

        // prune
        alpha = std::max(alpha, eval);
        if (alpha >= beta)
            return alpha;
        
        // search
        chess::Movelist movelist;
        order_moves(movelist, board, -1);
        
        for (const auto& move : movelist) {
            board.makeMove(move);
            eval = -quiescence(board, -beta, -alpha, halt);
            board.unmakeMove(move);

            // prune
            alpha = std::max(alpha, eval);
            if (alpha >= beta)
                break;
        }
        
        return alpha;
    }


    // Modifies movelist to contain a list of moves, ordered from best to worst
    // Generates capture moves only if ply = -1 for quiescence search
    void order_moves(chess::Movelist& movelist, const chess::Board& board, const int ply) const {
        if (ply>=0)
            chess::movegen::legalmoves<chess::MoveGenType::ALL>(movelist, board);
        else
            chess::movegen::legalmoves<chess::MoveGenType::CAPTURE>(movelist, board);
        for (auto& move : movelist)
            score_move(move, board, ply);
        movelist.sort();
    }


    // Assigns a score to the given move
    void score_move(chess::Move& move, const chess::Board& board, const int ply) const {
        // prioritize best move from previous iteraton
        if (move == itermove) {
            move.setScore(INT16_MAX);
            return;
        }

        // killer move
        int16_t score = 0;
        if (ply>0 && killers.isKiller(move, ply))
            score += KILLER_WEIGHT;

        // calculate other scores
        int from = (int)board.at(move.from());
        int to = (int)board.at(move.to());

        // enemy piece captured
        if (board.isCapture(move))
            score += abs(PVAL::VALS[to]) - abs(PVAL::VALS[from]) + 13;  // small bias to encourage trades
        
        // promotion
        if (move.typeOf()==chess::Move::PROMOTION)
            score += abs(PVAL::VALS[(int)move.promotionType()]);

        move.setScore(score);
    }


    // Evaluates the current position (from the current player's perspective)
    int evaluate(const chess::Board& board) const {
        int eval = 0;
        auto pieces = board.occ();
        int n_pieces_left = chess::builtin::popcount(pieces);
        float eg_weight = std::min(1.0f, float(32-n_pieces_left)/(32-N_PIECES_END));    // 0~1 as pieces left decreases
        int krd = 0, kfd = 0;   // king rank and file distance

        // loop through all pieces
        while (pieces) {
            auto sq = chess::builtin::poplsb(pieces);
            int sqi = (int)sq;
            int piece = (int)board.at(sq);
            
            // add material value
            eval += PVAL::VALS[piece];
            // add positional value
            eval += PST::MID[piece][sqi] + eg_weight*(PST::END[piece][sqi] - PST::MID[piece][sqi]);

            // pawn structure
            if (piece==0) {
                // passed (+ for white) (more important in endgame)
                if ((PMASK::WPASSED[sqi] & board.pieces(chess::PieceType::PAWN, chess::Color::BLACK)) == 0) 
                    eval += PMASK::PASSEDBONUS[7 - (sqi/8)] * eg_weight;
                // isolated (- for white)
                if ((PMASK::ISOLATED[sqi] & board.pieces(chess::PieceType::PAWN, chess::Color::WHITE)) == 0)
                    eval -= PMASK::ISOLATION_WEIGHT;

            } else if (piece==6) {
                // passed (- for white) (more important in endgame)
                if ((PMASK::BPASSED[sqi] & board.pieces(chess::PieceType::PAWN, chess::Color::WHITE)) == 0)
                    eval -= PMASK::PASSEDBONUS[(sqi/8)] * eg_weight;
                // isolated (+ for white)
                if ((PMASK::ISOLATED[sqi] & board.pieces(chess::PieceType::PAWN, chess::Color::BLACK)) == 0)
                    eval += PMASK::ISOLATION_WEIGHT;
            }

            // rook on open files
            else if (piece==3 || piece==9) {
                if (chess::builtin::popcount((0x0101010101010101ULL<<(sqi%8)) & board.occ()) == 1)
                    eval += (piece==3) ? OPEN_ROOK_WEIGHT : -OPEN_ROOK_WEIGHT;
            }

            // King proximity
            else if (piece==5) {
                krd += (int)chess::utils::squareRank(sq);
                kfd += (int)chess::utils::squareFile(sq);
            } else if (piece == 11) {
                krd -= (int)chess::utils::squareRank(sq);
                kfd -= (int)chess::utils::squareFile(sq);
            }
        }
        
        // convert perspective
        if (!whiteturn)
            eval *= -1;

        // King proximity bonus (if winning)
        if (eval>=0)
            eval += (14 - abs(krd) - abs(kfd)) * KING_DIST_WEIGHT * eg_weight;
        
        return eval;
    }
};  // Raphael
}   // namespace Raphael