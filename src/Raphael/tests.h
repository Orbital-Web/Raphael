#include <Raphael/Raphael.h>



namespace raphael {
namespace test {
/** Tests the see implementation
 *
 * \returns number of failed tests
 */
i32 test_see();

/** Runs all tests
 *
 * \param raise whether to raise error on fail
 * \returns number of failed tests
 */
i32 run_all(bool raise);
}  // namespace test



namespace bench {
/** Runs the benchmark
 *
 * \param engine engine to benchmark
 */
void run(Raphael& engine);
}  // namespace bench
}  // namespace raphael
