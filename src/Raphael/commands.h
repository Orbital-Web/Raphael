#include <Raphael/Raphael.h>



namespace raphael::commands {
/** Runs the benchmark
 *
 * \param engine engine to benchmark
 */
void bench(Raphael& engine);


/** Generates randomized fens
 *
 * \param engine engine for evaluating generated fens
 * \param count number of fens to generate
 * \param seed random seed
 * \param book book to use
 * \param randmoves number of random moves to play
 */
void genfens(Raphael& engine, i32 count, u64 seed, std::string book, i32 randmoves);
}  // namespace raphael::commands
