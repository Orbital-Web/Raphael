#ifdef EVAL_NNUE
#include <eval/geometry.h>

using std::array;
using std::pair;



namespace raphael::nnue::geometry {
namespace internal {
struct Bits {
    static constexpr Bit WHITEPAWN = 1 << 0;
    static constexpr Bit BLACKPAWN = 1 << 1;
    static constexpr Bit KNIGHT = 1 << 2;
    static constexpr Bit BISHOP = 1 << 3;
    static constexpr Bit ROOK = 1 << 4;
    static constexpr Bit QUEEN = 1 << 5;
    static constexpr Bit KING = 1 << 6;
};

// generate lut of permutations to go from mailbox to a ray vector
static constexpr MultiArray<u8, 64, 64> PERMUTATIONS = [] {
    // https://87flowers.com/byteboard-attack-tables-1
    // each of these offsets shift a square some steps in some direction (first col = knight dir)
    constexpr array<u8, 64> OFFSETS = {
        0x1F, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,  // N
        0x21, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,  // NE
        0x12, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,  // E
        0xF2, 0xF1, 0xE2, 0xD3, 0xC4, 0xB5, 0xA6, 0x97,  // SE
        0xE1, 0xF0, 0xE0, 0xD0, 0xC0, 0xB0, 0xA0, 0x90,  // S
        0xDF, 0xEF, 0xDE, 0xCD, 0xBC, 0xAB, 0x9A, 0x89,  // SW
        0xEE, 0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9,  // W
        0x0E, 0x0F, 0x1E, 0x2D, 0x3C, 0x4B, 0x5A, 0x69,  // NW
    };  // N     1     2     3     4     5     6     7

    MultiArray<u8, 64, 64> perm{};

    for (u8 focus = 0; focus < 64; focus++) {
        for (u8 i = 0; i < 64; i++) {
            // convert from 00fffrrr to 0fff0rrr so we can detect overflow in 4th and 8th bit
            // then add offset and store in perm if we don't overflow (go out of bounds)
            // if we do overflow, we use 0x80 to encode that
            const u8 wide_focus = focus + (focus & 0x38);
            const u8 wide_result = wide_focus + OFFSETS[i];
            const u8 result = ((wide_result & 0x70) >> 1) | (wide_result & 0x07);
            const bool is_valid = (wide_result & 0x88) == 0;
            perm[focus][i] = (is_valid) ? result : 0x80;
        }
    }

    return perm;
}();

static constexpr array<Bit, 16> PIECE_TO_BIT = [] {
    array<Bit, 16> lut{};

    lut[chess::Piece::WHITEPAWN] = Bits::WHITEPAWN;
    lut[chess::Piece::BLACKPAWN] = Bits::BLACKPAWN;
    lut[chess::Piece::WHITEKNIGHT] = Bits::KNIGHT;
    lut[chess::Piece::BLACKKNIGHT] = Bits::KNIGHT;
    lut[chess::Piece::WHITEBISHOP] = Bits::BISHOP;
    lut[chess::Piece::BLACKBISHOP] = Bits::BISHOP;
    lut[chess::Piece::WHITEROOK] = Bits::ROOK;
    lut[chess::Piece::BLACKROOK] = Bits::ROOK;
    lut[chess::Piece::WHITEQUEEN] = Bits::QUEEN;
    lut[chess::Piece::BLACKQUEEN] = Bits::QUEEN;
    lut[chess::Piece::WHITEKING] = Bits::KING;
    lut[chess::Piece::BLACKKING] = Bits::KING;
    lut[chess::Piece::NONE] = 0;

    return lut;
}();

// generate lut of indices into PERMUTATIONS::OFFSETS for each piece's outgoing threats
static constexpr array<BitRays, 12> OUTGOING_THREATS = [] {
    // https://87flowers.com/byteboard-attack-tables-1
    // e.g., white pawns attack one step into NE and NW, thus we set the bits that correspond to
    // those directions in PERMUTATIONS::OFFSETS
    array<BitRays, 12> lut{};

    lut[chess::Piece::WHITEPAWN] = 0x02'00'00'00'00'00'02'00;
    lut[chess::Piece::BLACKPAWN] = 0x00'00'02'00'02'00'00'00;
    lut[chess::Piece::WHITEKNIGHT] = 0x01'01'01'01'01'01'01'01;
    lut[chess::Piece::BLACKKNIGHT] = 0x01'01'01'01'01'01'01'01;
    lut[chess::Piece::WHITEBISHOP] = 0xFE'00'FE'00'FE'00'FE'00;
    lut[chess::Piece::BLACKBISHOP] = 0xFE'00'FE'00'FE'00'FE'00;
    lut[chess::Piece::WHITEROOK] = 0x00'FE'00'FE'00'FE'00'FE;
    lut[chess::Piece::BLACKROOK] = 0x00'FE'00'FE'00'FE'00'FE;
    lut[chess::Piece::WHITEQUEEN] = 0xFE'FE'FE'FE'FE'FE'FE'FE;
    lut[chess::Piece::BLACKQUEEN] = 0xFE'FE'FE'FE'FE'FE'FE'FE;
    lut[chess::Piece::WHITEKING] = 0;
    lut[chess::Piece::BLACKKING] = 0;

    return lut;
}();

// generate map of which piece can attack from each direction
static constexpr array<Bit, 64> INCOMING_THREATS_MASK = [] {
    // no king threats but normally all the near bits would encode the king too
    constexpr Bit HORS = Bits::KNIGHT;
    constexpr Bit ORTH = Bits::ROOK | Bits::QUEEN;
    constexpr Bit DIAG = Bits::BISHOP | Bits::QUEEN;
    constexpr Bit ORNR = ORTH;
    constexpr Bit WPNR = Bits::WHITEPAWN | DIAG;
    constexpr Bit BPNR = Bits::BLACKPAWN | DIAG;

    return array<Bit, 64>{
        HORS, ORNR, ORTH, ORTH, ORTH, ORTH, ORTH, ORTH,  // N
        HORS, BPNR, DIAG, DIAG, DIAG, DIAG, DIAG, DIAG,  // NE
        HORS, ORNR, ORTH, ORTH, ORTH, ORTH, ORTH, ORTH,  // E
        HORS, WPNR, DIAG, DIAG, DIAG, DIAG, DIAG, DIAG,  // SE
        HORS, ORNR, ORTH, ORTH, ORTH, ORTH, ORTH, ORTH,  // S
        HORS, WPNR, DIAG, DIAG, DIAG, DIAG, DIAG, DIAG,  // SW
        HORS, ORNR, ORTH, ORTH, ORTH, ORTH, ORTH, ORTH,  // W
        HORS, BPNR, DIAG, DIAG, DIAG, DIAG, DIAG, DIAG,  // NW
    };
}();

// generate map of which slider can attack from each direction
static constexpr array<Bit, 64> INCOMING_SLIDER_MASK = [] {
    constexpr Bit ORTH = Bits::ROOK | Bits::QUEEN;
    constexpr Bit DIAG = Bits::BISHOP | Bits::QUEEN;
    constexpr Bit NONE = 0x80;

    return array<Bit, 64>{
        NONE, ORTH, ORTH, ORTH, ORTH, ORTH, ORTH, ORTH,  // N
        NONE, DIAG, DIAG, DIAG, DIAG, DIAG, DIAG, DIAG,  // NE
        NONE, ORTH, ORTH, ORTH, ORTH, ORTH, ORTH, ORTH,  // E
        NONE, DIAG, DIAG, DIAG, DIAG, DIAG, DIAG, DIAG,  // SE
        NONE, ORTH, ORTH, ORTH, ORTH, ORTH, ORTH, ORTH,  // S
        NONE, DIAG, DIAG, DIAG, DIAG, DIAG, DIAG, DIAG,  // SW
        NONE, ORTH, ORTH, ORTH, ORTH, ORTH, ORTH, ORTH,  // W
        NONE, DIAG, DIAG, DIAG, DIAG, DIAG, DIAG, DIAG,  // NW
    };
}();
}  // namespace internal



#ifdef USE_AVX512
Vector Vector::load(const void* src) { return {_mm512_loadu_si512(src)}; }

void Vector::store_into(void* dst) { _mm512_store_si512(dst, raw); }

Vector Vector::flip() const { return {_mm512_shuffle_i64x2(raw, raw, 0b01001110)}; }

#elif defined(USE_AVX2)
Vector Vector::load(const void* src) {
    return {
        _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src) + 0),
        _mm256_loadu_si256(reinterpret_cast<const __m256i*>(src) + 1),
    };
}

void Vector::store_into(void* dst) {
    _mm256_store_si256(reinterpret_cast<__m256i*>(dst) + 0, raw[0]);
    _mm256_store_si256(reinterpret_cast<__m256i*>(dst) + 1, raw[1]);
}

Vector Vector::flip() const { return {raw[1], raw[0]}; }

BitRays Vector::to_mask() const {
    return (static_cast<BitRays>(_mm256_movemask_epi8(raw[1])) << 32)
           | static_cast<u32>(_mm256_movemask_epi8(raw[0]));
}

#else
static_assert(false);  // FIXME:
#endif


BitRays ray_fill(BitRays br) {
    br = (br + 0x7E'7E'7E'7E'7E'7E'7E'7E) & 0x80'80'80'80'80'80'80'80;
    return br - (br >> 7);
}

BitRays outgoing_threats(chess::Piece piece, BitRays closest) {
    return internal::OUTGOING_THREATS[piece] & closest;
}


#ifdef USE_AVX512
BitRays incoming_attackers(Vector bits, BitRays closest) {
    const auto mask = Vector::load(internal::INCOMING_THREATS_MASK.data());
    return _mm512_test_epi8_mask(bits.raw, mask.raw) & closest;
}

BitRays incoming_sliders(Vector bits, BitRays closest) {
    const auto mask = Vector::load(internal::INCOMING_SLIDER_MASK.data());
    return _mm512_test_epi8_mask(bits.raw, mask.raw) & closest & 0xFE'FE'FE'FE'FE'FE'FE'FE;
}

BitRays closest_occupied(Vector bits) {
    const BitRays occupied = _mm512_test_epi8_mask(bits.raw, bits.raw);
    const BitRays o = occupied | 0x81'81'81'81'81'81'81'81;
    return (o ^ (o - 0x03'03'03'03'03'03'03'03)) & occupied;
}

Permutation permutation_for(chess::Square focus) {
    const auto indices = Vector::load(internal::PERMUTATIONS[focus].data());
    const auto valid = _mm512_testn_epi8_mask(indices.raw, _mm512_set1_epi8(0x80));
    return {indices, valid};
}

pair<Vector, Vector> permute_mailbox(const Permutation& perm, Vector masked_mailbox) {
    const auto lut = _mm512_broadcast_i32x4(
        _mm_loadu_si128(reinterpret_cast<const __m128i*>(internal::PIECE_TO_BIT.data()))
    );

    const Vector permuted{_mm512_permutexvar_epi8(perm.indices.raw, masked_mailbox.raw)};
    const Vector bits{_mm512_maskz_shuffle_epi8(perm.valid, lut, permuted.raw)};

    return {permuted, bits};
}

std::pair<Vector, Vector> permute_mailbox(
    const Permutation& perm, std::span<const chess::Piece, 64> mailbox
) {
    return permute_mailbox(perm, Vector::load(mailbox.data()));
}

std::pair<Vector, Vector> permute_mailbox(
    const Permutation& perm, std::span<const chess::Piece, 64> mailbox, chess::Square ignore
) {
    const Vector masked_mailbox{_mm512_mask_blend_epi8(
        static_cast<u64>(chess::BitBoard::from_square(ignore)),
        Vector::load(mailbox.data()).raw,
        _mm512_set1_epi8(chess::Piece::NONE)
    )};
    return permute_mailbox(perm, masked_mailbox);
}

#elif defined(USE_AVX2)
BitRays incoming_attackers(Vector bits, BitRays closest) {
    // AND every 8 bits with the threats mask and find the nonzeros
    const auto mask = Vector::load(internal::INCOMING_THREATS_MASK.data());
    const Vector v{
        {
         _mm256_cmpeq_epi8(_mm256_and_si256(bits.raw[0], mask.raw[0]), _mm256_setzero_si256()),
         _mm256_cmpeq_epi8(_mm256_and_si256(bits.raw[1], mask.raw[1]), _mm256_setzero_si256()),
         }
    };
    return ~v.to_mask() & closest;
}

BitRays incoming_sliders(Vector bits, BitRays closest) {
    // AND every 8 bits with the sliders mask and find the nonzeros
    const auto mask = Vector::load(internal::INCOMING_SLIDER_MASK.data());
    const Vector v{
        {
         _mm256_cmpeq_epi8(_mm256_and_si256(bits.raw[0], mask.raw[0]), _mm256_setzero_si256()),
         _mm256_cmpeq_epi8(_mm256_and_si256(bits.raw[1], mask.raw[1]), _mm256_setzero_si256()),
         }
    };
    return ~v.to_mask() & closest & 0xFE'FE'FE'FE'FE'FE'FE'FE;  // ignore knight rays
}

BitRays closest_occupied(Vector bits) {
    // https://87flowers.com/byteboard-attack-tables-1
    // e.g., if we are on square d4 and there is a piece north 2 steps (d6), the bit for that
    // north 2 step ray will be set
    // likewise, we do this for every direction as well as for the knights
    const Vector unoccupied{
        _mm256_cmpeq_epi8(bits.raw[0], _mm256_setzero_si256()),
        _mm256_cmpeq_epi8(bits.raw[1], _mm256_setzero_si256()),
    };
    const BitRays occupied = ~unoccupied.to_mask();
    const BitRays o = occupied | 0x81'81'81'81'81'81'81'81;
    return (o ^ (o - 0x03'03'03'03'03'03'03'03)) & occupied;
}

Permutation permutation_for(chess::Square focus) {
    const auto indices = Vector::load(internal::PERMUTATIONS[focus].data());
    const Vector invalid{
        {
         _mm256_cmpeq_epi8(indices.raw[0], _mm256_set1_epi8(0x80)),
         _mm256_cmpeq_epi8(indices.raw[1], _mm256_set1_epi8(0x80)),
         }
    };
    return {indices, invalid};
}

pair<Vector, Vector> permute_mailbox(const Permutation& perm, Vector masked_mailbox) {
    const auto lut = _mm256_broadcastsi128_si256(
        _mm_loadu_si128(reinterpret_cast<const __m128i*>(internal::PIECE_TO_BIT.data()))
    );

    const auto half_swizzler = [](__m256i bytes0, __m256i bytes1, __m256i idxs) {
        const auto mask0 = _mm256_slli_epi64(idxs, 2);
        const auto mask1 = _mm256_slli_epi64(idxs, 3);

        const auto lolo0
            = _mm256_shuffle_epi8(_mm256_permute2x128_si256(bytes0, bytes0, 0x00), idxs);
        const auto hihi0
            = _mm256_shuffle_epi8(_mm256_permute2x128_si256(bytes0, bytes0, 0x11), idxs);
        const auto x = _mm256_blendv_epi8(lolo0, hihi0, mask1);

        const auto lolo1
            = _mm256_shuffle_epi8(_mm256_permute2x128_si256(bytes1, bytes1, 0x00), idxs);
        const auto hihi1
            = _mm256_shuffle_epi8(_mm256_permute2x128_si256(bytes1, bytes1, 0x11), idxs);
        const auto y = _mm256_blendv_epi8(lolo1, hihi1, mask1);

        return _mm256_blendv_epi8(x, y, mask0);
    };

    const Vector permuted{
        {
         half_swizzler(masked_mailbox.raw[0], masked_mailbox.raw[1], perm.indices.raw[0]),
         half_swizzler(masked_mailbox.raw[0], masked_mailbox.raw[1], perm.indices.raw[1]),
         }
    };
    const Vector bits{
        {
         _mm256_andnot_si256(perm.invalid.raw[0], _mm256_shuffle_epi8(lut, permuted.raw[0])),
         _mm256_andnot_si256(perm.invalid.raw[1], _mm256_shuffle_epi8(lut, permuted.raw[1])),
         }
    };

    return {permuted, bits};
}

std::pair<Vector, Vector> permute_mailbox(
    const Permutation& perm, std::span<const chess::Piece, 64> mailbox
) {
    return permute_mailbox(perm, Vector::load(mailbox.data()));
}

std::pair<Vector, Vector> permute_mailbox(
    const Permutation& perm, std::span<const chess::Piece, 64> mailbox, chess::Square ignore
) {
    constexpr array<u8, 64> IOTA
        = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21,
           22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
           44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63};
    const auto iota = Vector::load(&IOTA);
    const auto ignore_vec = _mm256_set1_epi8(static_cast<i8>(ignore));
    const auto non_vec = _mm256_set1_epi8(static_cast<i8>(chess::Piece::NONE));
    const auto mb = Vector::load(mailbox.data());
    const Vector masked_mailbox{
        _mm256_blendv_epi8(mb.raw[0], non_vec, _mm256_cmpeq_epi8(iota.raw[0], ignore_vec)),
        _mm256_blendv_epi8(mb.raw[1], non_vec, _mm256_cmpeq_epi8(iota.raw[1], ignore_vec)),
    };
    return permute_mailbox(perm, masked_mailbox);
}

#else
static_assert(false);  // FIXME:
#endif
}  // namespace raphael::nnue::geometry
#endif
