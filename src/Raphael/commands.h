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
 * \param dfrc whether to use DFRC positions
 */
void genfens(
    Raphael& engine, i32 count, u64 seed, const std::string& book, i32 randmoves, bool dfrc
);


/** Measures statistics of the static evals and computes the optimal output scale
 *
 * \param engine engine for evaluating positions
 * \param book book to use
 */
void evalstats(Raphael& engine, const std::string& book);
}  // namespace raphael::commands
