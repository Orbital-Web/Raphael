#pragma once
#include <chess/types.h>

#include <cassert>



namespace chess {
class BitBoard {
private:
    u64 bits_;

public:
    constexpr BitBoard(): bits_(0) {}

    constexpr BitBoard(u64 bits): bits_(bits) {}
    explicit constexpr BitBoard(Square sq): bits_(u64(1) << sq) {}
    explicit constexpr BitBoard(File file): bits_(0x0101010101010101ULL << file) {}
    explicit constexpr BitBoard(Rank rank): bits_(0xFFULL << (8 * rank)) {}

    [[nodiscard]] constexpr operator u64() const { return bits_; }

    [[nodiscard]] constexpr BitBoard operator&(BitBoard rhs) const { return bits_ & rhs; }
    [[nodiscard]] constexpr BitBoard operator|(BitBoard rhs) const { return bits_ | rhs; }
    [[nodiscard]] constexpr BitBoard operator^(BitBoard rhs) const { return bits_ ^ rhs; }
    [[nodiscard]] constexpr BitBoard operator~() const { return ~bits_; }

    constexpr BitBoard& operator&=(const BitBoard& rhs) {
        bits_ &= rhs;
        return *this;
    }
    constexpr BitBoard& operator|=(const BitBoard& rhs) {
        bits_ |= rhs;
        return *this;
    }
    constexpr BitBoard& operator^=(const BitBoard& rhs) {
        bits_ ^= rhs;
        return *this;
    }

    constexpr BitBoard& set(i32 index) {
        assert(index >= 0 && index < 64);
        bits_ |= (u64(1) << index);
        return *this;
    }
    constexpr BitBoard& unset(i32 index) {
        assert(index >= 0 && index < 64);
        bits_ &= ~(u64(1) << index);
        return *this;
    }

    constexpr BitBoard& clear() {
        bits_ = 0;
        return *this;
    }

    [[nodiscard]] constexpr bool is_set(i32 index) const {
        assert(index >= 0 && index < 64);
        return bits_ & (u64(1) << index);
    }
    [[nodiscard]] constexpr bool is_empty() const { return bits_ == 0; }

    [[nodiscard]] i32 count() const { return __builtin_popcountll(bits_); }

    [[nodiscard]] i32 lsb() const {
        assert(bits_ != 0);
        return __builtin_ctzll(bits_);
    }
    [[nodiscard]] i32 poplsb() {
        i32 index = lsb();
        bits_ &= bits_ - 1;
        return index;
    }
};
}  // namespace chess
