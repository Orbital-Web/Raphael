#pragma once
#include <array>
#include <cstdint>

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using usize = std::size_t;

using f32 = float;
using f64 = double;



namespace internal {
template <typename T, usize kN, usize... kNs>
struct MultiArrayImpl {
    using Type = std::array<typename MultiArrayImpl<T, kNs...>::Type, kN>;
};

template <typename T, usize kN>
struct MultiArrayImpl<T, kN> {
    using Type = std::array<T, kN>;
};
}  // namespace internal

template <typename T, usize... kNs>
using MultiArray = typename internal::MultiArrayImpl<T, kNs...>::Type;
