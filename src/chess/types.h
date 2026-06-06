#pragma once
#include <array>
#include <cassert>
#include <cstdint>
#include <utility>

using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using i128 = __int128_t;

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using u128 = __uint128_t;

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



template <typename T, usize N>
class StaticVector {
private:
    std::array<T, N> list_ = {};
    usize size_ = 0;


public:
    void push(const T& item) {
        assert(size_ < N);
        list_[size_++] = item;
    }
    void push(T&& item) {
        assert(size_ < N);
        list_[size_++] = std::move(item);
    }

    T pop() {
        assert(size_ > 0);
        return std::move(list_[--size_]);
    }

    void clear() { size_ = 0; }

    [[nodiscard]] usize size() const { return size_; }

    [[nodiscard]] usize empty() const { return size_ == 0; }

    [[nodiscard]] T& operator[](usize i) {
        assert(i < size_);
        return list_[i];
    }
    [[nodiscard]] const T& operator[](usize i) const {
        assert(i < size_);
        return list_[i];
    }

    [[nodiscard]] auto begin() { return list_.begin(); }
    [[nodiscard]] auto end() { return list_.begin() + size_; }

    [[nodiscard]] auto begin() const { return list_.begin(); }
    [[nodiscard]] auto end() const { return list_.begin() + size_; }

    template <typename F>
    void unsafe_write(F f) {
        size_ += f(&list_[size_]);
    }
};
