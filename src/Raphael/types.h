#pragma once
#include <array>
#include <cstdint>

using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;



namespace raphael {

namespace internal {
template <typename T, std::size_t kN, std::size_t... kNs>
struct MultiArrayImpl {
    using Type = std::array<typename MultiArrayImpl<T, kNs...>::Type, kN>;
};

template <typename T, std::size_t kN>
struct MultiArrayImpl<T, kN> {
    using Type = std::array<T, kN>;
};
}  // namespace internal

template <typename T, std::size_t... kNs>
using MultiArray = typename internal::MultiArrayImpl<T, kNs...>::Type;

}  // namespace raphael
