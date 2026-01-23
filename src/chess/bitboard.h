#pragma once
#include <chess/piece.h>



namespace chess {
class BitBoard {
private:
    u64 bits_;

public:
    constexpr BitBoard(): bits_(0) {}

    constexpr BitBoard(u64 bits): bits_(bits) {}

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
    [[nodiscard]] constexpr BitBoard operator-(BitBoard rhs) const { return bits_ - rhs.bits_; }
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

    template <Direction dir>
    [[nodiscard]] constexpr BitBoard shifted() const {
        switch (dir) {
            case Direction::UP: return bits_ << 8;
            case Direction::DOWN: return bits_ >> 8;
            case Direction::UP_LEFT: return (bits_ & ~FILEA) << 7;
            case Direction::LEFT: return (bits_ & ~FILEA) >> 1;
            case Direction::DOWN_LEFT: return (bits_ & ~FILEA) >> 9;
            case Direction::UP_RIGHT: return (bits_ & ~FILEH) << 9;
            case Direction::RIGHT: return (bits_ & ~FILEH) << 1;
            case Direction::DOWN_RIGHT: return (bits_ & ~FILEH) >> 7;
        }
    }

    [[nodiscard]] static constexpr BitBoard from_square(Square sq) {
        assert(sq.is_valid());
        return BitBoard(u64(1) << sq);
    }

    [[nodiscard]] static constexpr BitBoard from_file(File file) {
        assert(file != File::NONE);
        return BitBoard(u64(0x0101010101010101) << file);
    }

    [[nodiscard]] static constexpr BitBoard from_rank(Rank rank) {
        assert(rank != Rank::NONE);
        return BitBoard(u64(0xFF) << (8 * rank));
    }

    static constexpr auto FILEA = u64(0x0101010101010101);
    static constexpr auto FILEB = u64(0x0202020202020202);
    static constexpr auto FILEC = u64(0x0404040404040404);
    static constexpr auto FILED = u64(0x0808080808080808);
    static constexpr auto FILEE = u64(0x1010101010101010);
    static constexpr auto FILEF = u64(0x2020202020202020);
    static constexpr auto FILEG = u64(0x4040404040404040);
    static constexpr auto FILEH = u64(0x8080808080808080);

    static constexpr auto RANK1 = u64(0x00000000000000FF);
    static constexpr auto RANK2 = u64(0x000000000000FF00);
    static constexpr auto RANK3 = u64(0x0000000000FF0000);
    static constexpr auto RANK4 = u64(0x00000000FF000000);
    static constexpr auto RANK5 = u64(0x000000FF00000000);
    static constexpr auto RANK6 = u64(0x0000FF0000000000);
    static constexpr auto RANK7 = u64(0x00FF000000000000);
    static constexpr auto RANK8 = u64(0xFF00000000000000);
};
}  // namespace chess
