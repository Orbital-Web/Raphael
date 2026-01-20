#pragma once
#include <chess/types.h>

#include <charconv>
#include <optional>
#include <string>



namespace chess::utils {

/** Splits a string view into N parts, separated by delimiter
 *
 * \tparam N number of splits
 * \param str string to split
 * \param delimiter character to split by
 * \returns an array of size N
 */
template <int N>
inline std::array<std::optional<std::string_view>, N> split_string_view(
    std::string_view str, char delimiter = ' '
) {
    std::array<std::optional<std::string_view>, N> arr = {};

    usize start = 0;
    usize end = 0;

    for (usize i = 0; i < N; i++) {
        end = str.find(delimiter, start);
        if (end == std::string::npos) {
            arr[i] = str.substr(start);
            break;
        }
        arr[i] = str.substr(start, end - start);
        start = end + 1;
    }

    return arr;
}

/** Converts a string view into an int
 *
 * \param str string to convert
 * \returns the converted string (if valid)
 */
inline std::optional<i32> stringview_to_int(std::string_view str) {
    if (str.empty()) return std::nullopt;

    if (str.back() == ';') str.remove_suffix(1);  // remove semicolon for epd parsing
    if (str.empty()) return std::nullopt;

    i32 result;
    const char* begin = str.data();
    const char* end = begin + str.size();
    const auto [ptr, ec] = std::from_chars(begin, end, result);

    if (ec == std::errc() && ptr == end) return result;
    return std::nullopt;
}
}  // namespace chess::utils
