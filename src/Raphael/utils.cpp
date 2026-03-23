#include <Raphael/consts.h>
#include <Raphael/utils.h>

#include <algorithm>
#include <cctype>
#include <cmath>

using std::string_view;
using std::tolower;
using std::ranges::equal;


namespace raphael::utils {
bool is_win(i32 score) { return score >= MATE_SCORE - MAX_DEPTH; }

bool is_loss(i32 score) { return score <= -MATE_SCORE + MAX_DEPTH; }

bool is_mate(i32 score) { return is_win(score) || is_loss(score); }

i32 mate_distance(i32 score) { return ((score >= 0) ? 1 : -1) * (MATE_SCORE - abs(score) + 1) / 2; }


bool is_case_insensitive_equals(string_view str1, string_view str2) {
    return equal(str1, str2, [](char c1, char c2) {
        return tolower(static_cast<unsigned char>(c1)) == tolower(static_cast<unsigned char>(c2));
    });
}
}  // namespace raphael::utils
