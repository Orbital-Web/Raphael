#pragma once
#include <array>



namespace Raphael {

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

};  // namespace Raphael
