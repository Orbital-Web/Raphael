#include <Raphael/utils.h>
#include <Raphael/wdl.h>

#include <algorithm>
#include <cmath>

using std::clamp;
using std::exp;
using std::round;



namespace raphael::wdl {
namespace internal {
f64 wdl_a(i32 material) {
    static constexpr f64 as[] = {-56.68614451, 147.07269963, -183.22284031, 240.87662059};

    const auto m = static_cast<f64>(clamp(material, 17, 78)) / 58.0;
    return ((as[0] * m + as[1]) * m + as[2]) * m + as[3];
}

f64 wdl_b(i32 material) {
    static constexpr f64 bs[] = {11.58191170, -9.61969055, 5.92615161, 28.93615867};

    const auto m = static_cast<f64>(clamp(material, 17, 78)) / 58.0;
    return ((bs[0] * m + bs[1]) * m + bs[2]) * m + bs[3];
}
}  // namespace internal



WDL get_wdl(i32 score, const chess::Board& board) {
    if (utils::is_win(score)) return {1000, 0, 0};
    if (utils::is_loss(score)) return {0, 0, 1000};

    const i32 material = board.material();
    const auto p_a = internal::wdl_a(material);
    const auto p_b = internal::wdl_b(material);
    const f64 x = static_cast<f64>(score);

    const i32 w = static_cast<i32>(round(1000.0 / (1 + exp((p_a - x) / p_b))));
    const i32 l = static_cast<i32>(round(1000.0 / (1 + exp((p_a + x) / p_b))));
    const i32 d = clamp(1000 - w - l, 0, 1000);
    return {w, d, l};
}

i32 normalize_score(i32 score, const chess::Board& board) {
    if (score == 0 || utils::is_mate(score)) return score;

    const i32 material = board.material();
    const auto p_a = internal::wdl_a(material);
    const f64 x = static_cast<f64>(score);

    return static_cast<i32>(round(100.0 * x / p_a));
}
}  // namespace raphael::wdl
