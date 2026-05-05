#pragma once
#include <chess/types.h>

#if defined(__AVX512F__)
    #include <immintrin.h>
    #define USE_AVX512
    #define USE_SIMD 512
    #define ALIGNMENT 64
    #define SIMD_REGS 32
using VecU8 = __m512i;   // a list of 64x u8
using VecI8 = __m512i;   // a list of 64x i8
using VecI16 = __m512i;  // a list of 32x i16
using VecI32 = __m512i;  // a list of 16x i32


/** Loads an i8[64] array into a VecI8 register
 *
 * \param src an array of 64x i8 elements
 * \returns the loaded register
 */
inline VecI8 load_i8(const i8* src) {
    return _mm512_load_si512(reinterpret_cast<const VecI8*>(src));
}

/** Loads an i16[32] array into a VecI16 register
 *
 * \param src an array of 32x i16 elements
 * \returns the loaded register
 */
inline VecI16 load_i16(const i16* src) {
    return _mm512_load_si512(reinterpret_cast<const VecI16*>(src));
}

/** Loads an i32[16] array into a VecI32 register
 *
 * \param src an array of 16x i32 elements
 * \returns the loaded register
 */
inline VecI32 load_i32(const i32* src) {
    return _mm512_load_si512(reinterpret_cast<const VecI32*>(src));
}

/** Stores a VecU8 register into a u8[64] array
 *
 * \param dst the array of 64x u8 elements to store into
 * \param src the register to store
 */
inline void store_u8(u8* dst, VecU8 src) { _mm512_store_si512(reinterpret_cast<VecU8*>(dst), src); }

/** Stores a VecI16 register into an i16[32] array
 *
 * \param dst the array of 32x i16 elements to store into
 * \param src the register to store
 */
inline void store_i16(i16* dst, VecI16 src) {
    _mm512_store_si512(reinterpret_cast<VecI16*>(dst), src);
}

/** Stores a VecI32 register into an i32[16] array
 *
 * \param dst the array of 16x i32 elements to store into
 * \param src the register to store
 */
inline void store_i32(i32* dst, VecI32 src) {
    _mm512_store_si512(reinterpret_cast<VecI32*>(dst), src);
}

/** Returns a VecI16 register with all zeros
 *
 * \returns an all zero register
 */
inline VecI16 zero_i16() { return _mm512_setzero_si512(); }

/** Returns a VecI32 register with all zeros
 *
 * \returns an all zero register
 */
inline VecI32 zero_i32() { return _mm512_setzero_si512(); }

/** Returns a VecI16 register with all values set to val
 *
 * \param val the value to set to
 * \returns an all val register
 */
inline VecI16 full_i16(i16 val) { return _mm512_set1_epi16(val); }

/** Returns a VecI32 register with all values set to val
 *
 * \param val the value to set to
 * \returns an all val register
 */
inline VecI32 full_i32(i32 val) { return _mm512_set1_epi32(val); }

/** Does an element-wise addition of two VecI16 registers
 *
 * \param a register 1
 * \param b register 2
 * \returns the result of the addition
 */
inline VecI16 add_i16(VecI16 a, VecI16 b) { return _mm512_add_epi16(a, b); }

/** Does an element-wise addition of two VecI32 registers
 *
 * \param a register 1
 * \param b register 2
 * \returns the result of the addition
 */
inline VecI32 add_i32(VecI32 a, VecI32 b) { return _mm512_add_epi32(a, b); }

/** Does an element-wise subtraction of two VecI16 registers
 *
 * \param a register 1
 * \param b register 2
 * \returns the result of the subtraction
 */
inline VecI16 sub_i16(VecI16 a, VecI16 b) { return _mm512_sub_epi16(a, b); }

/** Does an element-wise product of two VecI32 registers and keeps the low 16 bits
 *
 * \param a register 1
 * \param b register 2
 * \returns the result of the multiplication
 */
inline VecI32 mullo_i32(VecI32 a, VecI32 b) { return _mm512_mullo_epi32(a, b); }

/** Does an element-wise product of two VecI16 registers and keeps the high 16 bits
 *
 * \param a register 1
 * \param b register 2
 * \returns the result of the multiplication
 */
inline VecI16 mulhi_i16(VecI16 a, VecI16 b) { return _mm512_mulhi_epi16(a, b); }


/** Does an element-wise clamping of a VecI16 register
 *
 * \param reg register to clamp
 * \param mins register containing the min values
 * \param maxs register containing the max values
 * \returns the result of the clamp
 */
inline VecI16 clamp_i16(VecI16 reg, VecI16 mins, VecI16 maxs) {
    return _mm512_min_epi16(_mm512_max_epi16(reg, mins), maxs);
}

/** Does an element-wise clamping of a VecI32 register
 *
 * \param reg register to clamp
 * \param mins register containing the min values
 * \param maxs register containing the max values
 * \returns the result of the clamp
 */
inline VecI32 clamp_i32(VecI32 reg, VecI32 mins, VecI32 maxs) {
    return _mm512_min_epi32(_mm512_max_epi32(reg, mins), maxs);
}

/** Does an element-wise logical left shift of a VecI16 register by a constexpr shift amount
 *
 * \param reg register to shift
 * \param shift the amount to shift by, known at compile time
 * \returns the shifted register
 */
inline VecI16 lshift_i16(VecI16 reg, i32 shift) { return _mm512_slli_epi16(reg, shift); }

/** Does an element-wise arithmetic right shift of a VecI32 register by a constexpr shift amount
 *
 * \param reg register to shift
 * \param shift the amount to shift by, known at compile time
 * \returns the shifted register
 */
inline VecI32 rshift_i32(VecI32 reg, i32 shift) { return _mm512_srai_epi32(reg, shift); }

/** Packs two VecI16 registers into a VecU8 register
 * Result is [a[0:8] b[0:8] a[8:16] b[8:16] a[16:24] b[16:24] a[24:32] b[24:32]]
 *
 * \param a register 1
 * \param b register 2
 * \returns the packed register
 */
inline VecU8 pack_u8_i16(VecI16 a, VecI16 b) { return _mm512_packus_epi16(a, b); }

/** Tiles a u8[4] array 16 times into a VecU8 register
 *
 * \param vals the value to tile
 * \returns the tiled register
 */
inline VecU8 tile_u8(const u8* vals) {
    return _mm512_set1_epi32(*reinterpret_cast<const u32*>(vals));  // technically UB
}

/** Computes out[i] = a[i] + dot(b[4*i : 4*(i+1)], c[4*i : 4*(i+1)])
 *
 * \param a register 1
 * \param b register 2
 * \param c register 3
 * \returns the result of the accumulated dot product
 */
inline VecI32 dpbusd_i32(VecI32 a, VecU8 b, VecI8 c) {
    // for some reason, dpbusd is a lot slower...
    // #ifdef __AVX512VNNI__
    // return _mm512_dpbusd_epi32(a, b, c);
    // #else
    return add_i32(a, _mm512_madd_epi16(_mm512_maddubs_epi16(b, c), full_i16(1)));
    // #endif
}

/** Computes out[i] = a[i] + dot(b[4*i : 4*(i+1)], c[4*i : 4*(i+1)]) +
 * dot(d[4*i : 4*(i+1)], e[4*i : 4*(i+1)])
 *
 * \param a register 1
 * \param b register 2
 * \param c register 3
 * \param d register 4
 * \param e register 5
 * \returns the result of the accumulated dot product
 */
inline VecI32 dpbusd2_i32(VecI32 a, VecU8 b, VecI8 c, VecU8 d, VecI8 e) {
    // for some reason, dpbusd is a lot slower...
    // #ifdef __AVX512VNNI__
    // return _mm512_dpbusd_epi32(_mm512_dpbusd_epi32(a, b, c), d, e);
    // #else
    return add_i32(
        a,
        _mm512_madd_epi16(
            add_i16(_mm512_maddubs_epi16(b, c), _mm512_maddubs_epi16(d, e)), full_i16(1)
        )
    );
    // #endif
}

/** Does an element-wise fused multiply add a[i] * b[i] + c[i]
 *
 * \param a register 1
 * \param b register 2
 * \param c register 3
 * \returns the result of the fused multiply add
 */
inline VecI32 fmadd_i32(VecI32 a, VecI32 b, VecI32 c) { return add_i32(mullo_i32(a, b), c); }

/** Does a horizontal sum of a VecI32 register
 *
 * \param reg register to horizontally sum
 * \returns the horizontally summed result
 */
inline i32 hadd_i32(VecI32 reg) {
    // https://stackoverflow.com/a/60109639/9984384
    // return _mm512_reduce_add_epi32(reg);  suboptimal in GCC

    __m256i lo256 = _mm512_castsi512_si256(reg);
    __m256i hi256 = _mm512_extracti64x4_epi64(reg, 1);
    __m256i sum256 = _mm256_add_epi32(lo256, hi256);

    __m128i lo128 = _mm256_castsi256_si128(sum256);
    __m128i hi128 = _mm256_extracti128_si256(sum256, 1);
    __m128i sum128 = _mm_add_epi32(lo128, hi128);

    __m128i hi64 = _mm_unpackhi_epi64(sum128, sum128);
    __m128i sum64 = _mm_add_epi32(hi64, sum128);
    __m128i hi32 = _mm_shuffle_epi32(sum64, _MM_SHUFFLE(2, 3, 0, 1));
    __m128i sum32 = _mm_add_epi32(sum64, hi32);
    return _mm_cvtsi128_si32(sum32);
}

#elif defined(__AVX__) || defined(__AVX2__)
    #include <immintrin.h>
    #define USE_AVX2
    #define USE_SIMD 256
    #define ALIGNMENT 32
    #define SIMD_REGS 16
using VecU8 = __m256i;   // a list of 32x u8
using VecI8 = __m256i;   // a list of 32x i8
using VecI16 = __m256i;  // a list of 16x i16
using VecI32 = __m256i;  // a list of 8x i32



/** Loads an i8[32] array into a VecI8 register
 *
 * \param src an array of 32x i8 elements
 * \returns the loaded register
 */
inline VecI8 load_i8(const i8* src) {
    return _mm256_load_si256(reinterpret_cast<const VecI8*>(src));
}

/** Loads an i16[16] array into a VecI16 register
 *
 * \param src an array of 16x i16 elements
 * \returns the loaded register
 */
inline VecI16 load_i16(const i16* src) {
    return _mm256_load_si256(reinterpret_cast<const VecI16*>(src));
}

/** Loads an i32[8] array into a VecI32 register
 *
 * \param src an array of 8x i32 elements
 * \returns the loaded register
 */
inline VecI32 load_i32(const i32* src) {
    return _mm256_load_si256(reinterpret_cast<const VecI32*>(src));
}

/** Stores a VecU8 register into a u8[32] array
 *
 * \param dst the array of 32x u8 elements to store into
 * \param src the register to store
 */
inline void store_u8(u8* dst, VecU8 src) { _mm256_store_si256(reinterpret_cast<VecU8*>(dst), src); }

/** Stores a VecI16 register into an i16[16] array
 *
 * \param dst the array of 16x i16 elements to store into
 * \param src the register to store
 */
inline void store_i16(i16* dst, VecI16 src) {
    _mm256_store_si256(reinterpret_cast<VecI16*>(dst), src);
}

/** Stores a VecI32 register into an i32[8] array
 *
 * \param dst the array of 8x i32 elements to store into
 * \param src the register to store
 */
inline void store_i32(i32* dst, VecI32 src) {
    _mm256_store_si256(reinterpret_cast<VecI32*>(dst), src);
}

/** Returns a VecI16 register with all zeros
 *
 * \returns an all zero register
 */
inline VecI16 zero_i16() { return _mm256_setzero_si256(); }

/** Returns a VecI32 register with all zeros
 *
 * \returns an all zero register
 */
inline VecI32 zero_i32() { return _mm256_setzero_si256(); }

/** Returns a VecI16 register with all values set to val
 *
 * \param val the value to set to
 * \returns an all val register
 */
inline VecI16 full_i16(i16 val) { return _mm256_set1_epi16(val); }

/** Returns a VecI32 register with all values set to val
 *
 * \param val the value to set to
 * \returns an all val register
 */
inline VecI32 full_i32(i32 val) { return _mm256_set1_epi32(val); }

/** Does an element-wise addition of two VecI16 registers
 *
 * \param a register 1
 * \param b register 2
 * \returns the result of the addition
 */
inline VecI16 add_i16(VecI16 a, VecI16 b) { return _mm256_add_epi16(a, b); }

/** Does an element-wise addition of two VecI32 registers
 *
 * \param a register 1
 * \param b register 2
 * \returns the result of the addition
 */
inline VecI32 add_i32(VecI32 a, VecI32 b) { return _mm256_add_epi32(a, b); }

/** Does an element-wise subtraction of two VecI16 registers
 *
 * \param a register 1
 * \param b register 2
 * \returns the result of the subtraction
 */
inline VecI16 sub_i16(VecI16 a, VecI16 b) { return _mm256_sub_epi16(a, b); }

/** Does an element-wise product of two VecI32 registers and keeps the low 16 bits
 *
 * \param a register 1
 * \param b register 2
 * \returns the result of the multiplication
 */
inline VecI32 mullo_i32(VecI32 a, VecI32 b) { return _mm256_mullo_epi32(a, b); }

/** Does an element-wise product of two VecI16 registers and keeps the high 16 bits
 *
 * \param a register 1
 * \param b register 2
 * \returns the result of the multiplication
 */
inline VecI16 mulhi_i16(VecI16 a, VecI16 b) { return _mm256_mulhi_epi16(a, b); }

/** Does an element-wise clamping of a VecI16 register
 *
 * \param reg register to clamp
 * \param mins register containing the min values
 * \param maxs register containing the max values
 * \returns the result of the clamp
 */
inline VecI16 clamp_i16(VecI16 reg, VecI16 mins, VecI16 maxs) {
    return _mm256_min_epi16(_mm256_max_epi16(reg, mins), maxs);
}

/** Does an element-wise clamping of a VecI32 register
 *
 * \param reg register to clamp
 * \param mins register containing the min values
 * \param maxs register containing the max values
 * \returns the result of the clamp
 */
inline VecI32 clamp_i32(VecI32 reg, VecI32 mins, VecI32 maxs) {
    return _mm256_min_epi32(_mm256_max_epi32(reg, mins), maxs);
}

/** Does an element-wise logical left shift of a VecI16 register by a constexpr shift amount
 *
 * \param reg register to shift
 * \param shift the amount to shift by, known at compile time
 * \returns the shifted register
 */
inline VecI16 lshift_i16(VecI16 reg, i32 shift) { return _mm256_slli_epi16(reg, shift); }

/** Does an element-wise arithmetic right shift of a VecI32 register by a constexpr shift amount
 *
 * \param reg register to shift
 * \param shift the amount to shift by, known at compile time
 * \returns the shifted register
 */
inline VecI32 rshift_i32(VecI32 reg, i32 shift) { return _mm256_srai_epi32(reg, shift); }

/** Packs two VecI16 registers into a VecU8 register
 * Result is [a[:8] b[:8] a[8:] b[8:]]
 *
 * \param a register 1
 * \param b register 2
 * \returns the packed register
 */
inline VecU8 pack_u8_i16(VecI16 a, VecI16 b) { return _mm256_packus_epi16(a, b); }

/** Tiles a u8[4] array 8 times into a VecU8 register
 *
 * \param vals the value to tile
 * \returns the tiled register
 */
inline VecU8 tile_u8(const u8* vals) {
    return _mm256_set1_epi32(*reinterpret_cast<const u32*>(vals));  // technically UB
}

/** Computes out[i] = a[i] + dot(b[4*i : 4*(i+1)], c[4*i : 4*(i+1)])
 *
 * \param a register 1
 * \param b register 2
 * \param c register 3
 * \returns the result of the accumulated dot product
 */
inline VecI32 dpbusd_i32(VecI32 a, VecU8 b, VecI8 c) {
    return add_i32(a, _mm256_madd_epi16(_mm256_maddubs_epi16(b, c), full_i16(1)));
}

/** Computes out[i] = a[i] + dot(b[4*i : 4*(i+1)], c[4*i : 4*(i+1)]) +
 * dot(d[4*i : 4*(i+1)], e[4*i : 4*(i+1)])
 *
 * \param a register 1
 * \param b register 2
 * \param c register 3
 * \param d register 4
 * \param e register 5
 * \returns the result of the accumulated dot product
 */
inline VecI32 dpbusd2_i32(VecI32 a, VecU8 b, VecI8 c, VecU8 d, VecI8 e) {
    return add_i32(
        a,
        _mm256_madd_epi16(
            add_i16(_mm256_maddubs_epi16(b, c), _mm256_maddubs_epi16(d, e)), full_i16(1)
        )
    );
}

/** Does an element-wise fused multiply add a[i] * b[i] + c[i]
 *
 * \param a register 1
 * \param b register 2
 * \param c register 3
 * \returns the result of the fused multiply add
 */
inline VecI32 fmadd_i32(VecI32 a, VecI32 b, VecI32 c) { return add_i32(mullo_i32(a, b), c); }

/** Does a horizontal sum of a VecI32 register
 *
 * \param reg register to horizontally sum
 * \returns the horizontally summed result
 */
inline i32 hadd_i32(VecI32 reg) {
    // https://stackoverflow.com/a/60109639/9984384
    __m128i lo128 = _mm256_castsi256_si128(reg);
    __m128i hi128 = _mm256_extracti128_si256(reg, 1);
    __m128i sum128 = _mm_add_epi32(lo128, hi128);

    __m128i hi64 = _mm_unpackhi_epi64(sum128, sum128);
    __m128i sum64 = _mm_add_epi32(hi64, sum128);
    __m128i hi32 = _mm_shuffle_epi32(sum64, _MM_SHUFFLE(2, 3, 0, 1));
    __m128i sum32 = _mm_add_epi32(sum64, hi32);
    return _mm_cvtsi128_si32(sum32);
}

#else
    #define ALIGNMENT 32
#endif
