#include <Raphael/Raphael.h>

#include <atomic>
#include <mutex>



namespace raphael::datagen {
namespace internal {
inline std::mutex gen_mutex;
inline i32 num_batch_remaining = 0;
inline std::atomic<i32> num_games_generated{0};
inline std::chrono::_V2::system_clock::time_point start_time;


enum class Outcome : u8 { BLACK_WIN = 0, DRAW, WHITE_WIN, INVALID };

struct PackedBoard {
    u64 occ;
    u8 pieces[32 / 2] = {0};
    u8 stm_enpassant;
    u8 halfmoves;
    u16 fullmoves;
    i16 eval;
    Outcome outcome;
    u8 pad = 0;

    /** Packs a board into a 32 byte entry
     *
     * \param board board to pack
     * \param score score to record
     * \returns the packed board
     */
    static PackedBoard pack(const chess::Board& board, i16 score);
};
static_assert(sizeof(PackedBoard) == 32);

struct ViriMove {
    u16 move = 0;
    u16 score = 0;

    /** Packs a move and a score into a 4 byte entry
     *
     * \param move move to record
     * \param score score to record
     * \returns the packed viriformat move
     */
    static ViriMove from_move(chess::Move move, i32 score);
};
static_assert(sizeof(ViriMove) == 4);


/** Thread function to generate games until num_batch_remaining is 0
 *
 * \param engine pointer to engine or nullptr to internally create one
 * \param softnodes number of softnodes to use
 * \param seed_fens positions from the opening book
 * \param filename output filename to write to
 * \param seed rng seed to use
 * \param randmoves number of random moves to play on top of the book
 * \param dfrc whether to use DFRC positions
 */
void generation_thread(
    Raphael* engine,
    i32 softnodes,
    std::vector<std::string> seed_fens,
    std::string filename,
    u64 seed,
    i32 randmoves,
    bool dfrc
);


/** Gracefully terminates the datagen */
void handle_interrupt(i32);
}  // namespace internal



/** Generates games for training data
 *
 * \param main_engine engine used by the first thread
 * \param softnodes number of softnodes to use
 * \param games minimum number of games to generate
 * \param book book to use
 * \param randmoves number of random moves to play on top of the book
 * \param dfrc whether to use DFRC positions
 * \param concurrency number of threads to use
 */
void generate_games(
    Raphael& main_engine,
    i32 softnodes,
    i32 games,
    const std::string& book,
    i32 randmoves,
    bool dfrc,
    i32 concurrency
);
}  // namespace raphael::datagen
