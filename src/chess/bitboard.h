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
    explicit constexpr BitBoard(File file): bits_(u64(0x101010101010101) << file) {}
    explicit constexpr BitBoard(Rank rank): bits_(u64(0xFF) << (8 * rank)) {}

    [[nodiscard]] explicit constexpr operator u64() const { return bits_; }
    [[nodiscard]] explicit operator bool() const { return bits_ != 0; }

    [[nodiscard]] constexpr BitBoard operator&(BitBoard rhs) const { return bits_ & rhs.bits_; }
    [[nodiscard]] constexpr BitBoard operator|(BitBoard rhs) const { return bits_ | rhs.bits_; }
    [[nodiscard]] constexpr BitBoard operator^(BitBoard rhs) const { return bits_ ^ rhs.bits_; }
    [[nodiscard]] constexpr BitBoard operator<<(BitBoard rhs) const { return bits_ << rhs.bits_; }
    [[nodiscard]] constexpr BitBoard operator>>(BitBoard rhs) const { return bits_ >> rhs.bits_; }
    [[nodiscard]] constexpr bool operator==(BitBoard rhs) const { return bits_ == rhs.bits_; }
    [[nodiscard]] constexpr bool operator!=(BitBoard rhs) const { return bits_ != rhs.bits_; }
    [[nodiscard]] constexpr bool operator||(BitBoard rhs) const { return bits_ || rhs.bits_; }
    [[nodiscard]] constexpr bool operator&&(BitBoard rhs) const { return bits_ && rhs.bits_; }
    [[nodiscard]] constexpr BitBoard operator~() const { return ~bits_; }

    [[nodiscard]] constexpr BitBoard operator&(u64 rhs) const { return bits_ & rhs; }
    [[nodiscard]] constexpr BitBoard operator|(u64 rhs) const { return bits_ | rhs; }
    [[nodiscard]] constexpr BitBoard operator^(u64 rhs) const { return bits_ ^ rhs; }
    [[nodiscard]] constexpr BitBoard operator<<(u64 rhs) const { return bits_ << rhs; }
    [[nodiscard]] constexpr BitBoard operator>>(u64 rhs) const { return bits_ >> rhs; }
    [[nodiscard]] constexpr bool operator==(u64 rhs) const { return bits_ == rhs; }
    [[nodiscard]] constexpr bool operator!=(u64 rhs) const { return bits_ != rhs; }
    [[nodiscard]] constexpr bool operator||(u64 rhs) const { return bits_ || rhs; }
    [[nodiscard]] constexpr bool operator&&(u64 rhs) const { return bits_ && rhs; }

    constexpr BitBoard& operator&=(BitBoard rhs) {
        bits_ &= rhs.bits_;
        return *this;
    }
    constexpr BitBoard& operator|=(BitBoard rhs) {
        bits_ |= rhs.bits_;
        return *this;
    }
    constexpr BitBoard& operator^=(BitBoard rhs) {
        bits_ ^= rhs.bits_;
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

    [[nodiscard]] i32 msb() const {
        assert(bits_ != 0);
        return 63 ^ __builtin_clzll(bits_);
    }

    [[nodiscard]] i32 lsb() const {
        assert(bits_ != 0);
        return __builtin_ctzll(bits_);
    }

    [[nodiscard]] i32 poplsb() {
        i32 index = lsb();
        bits_ &= bits_ - 1;
        return index;
    }

    [[nodiscard]] static constexpr BitBoard from_square(Square sq) {
        return BitBoard(u64(1) << sq);
    }
};
}  // namespace chess
