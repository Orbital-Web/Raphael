#pragma once
#include <chess/types.h>

#if defined(__AVX512F__)
    #include <immintrin.h>
    #define USE_SIMD 512
    #define ALIGNMENT 64
    #define SIMD_UNROLL 32
using VecI16 = __m512i;  // a list of 32x i16
using VecI32 = __m512i;  // a list of 16x i32



/** Loads an i16[32] array into a VecI16 register
 *
 * \param src an array of 32x i16 elements
 * \returns the loaded register
 */
inline VecI16 load_i16(const i16* src) {
    return _mm512_load_si512(reinterpret_cast<const VecI16*>(src));
}

/** Stores a VecI16 register into an i16[32] array
 *
 * \param dst the array of 32x i16 elements to store into
 * \param src the register to store
 */
inline void store_i16(i16* dst, VecI16& src) {
    _mm512_store_si512(reinterpret_cast<VecI16*>(dst), src);
}


/** Returns a VecI16 register with all zeros
 *
 * \returns an all zero register
 */
inline VecI16 zero_i16() { return _mm512_setzero_si512(); }

/** Returns a VecI16 register with all values set to val
 *
 * \param val the value to set to
 * \returns an all val register
 */
inline VecI16 full_i16(i16 val) { return _mm512_set1_epi16(val); }


/** Does an element-wise saturated addition of two VecI16 registers
 *
 * \param a register 1
 * \param b register 2
 * \returns the result of the addition
 */
inline VecI16 adds_i16(VecI16 a, VecI16 b) { return _mm512_adds_epi16(a, b); }

/** Does an element-wise saturated subtraction of two VecI16 registers
 *
 * \param a register 1
 * \param b register 2
 * \returns the result of the subtraction
 */
inline VecI16 subs_i16(VecI16 a, VecI16 b) { return _mm512_subs_epi16(a, b); }

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

/** Does an element-wise multiplication of two VecI16 registers.
 * The bottom 16 bits are taken for each multiplication.
 *
 * \param a register 1
 * \param b register 2
 * \returns the result of the multiplication
 */
inline VecI16 mul_i16(VecI16 a, VecI16 b) { return _mm512_mullo_epi16(a, b); }

/** Does an element-wise multiplication of two VecI16 registers and horizontally adds the results,
 * creating a VecI32 result.
 *
 * \param a register 1
 * \param b register 2
 * \returns the result of the multiplication-addition
 */
inline VecI32 madd_i16(VecI16 a, VecI16 b) { return _mm512_madd_epi16(a, b); }

/** Does an element-wise addition of two VecI32 registers
 *
 * \param a register 1
 * \param b register 2
 * \returns the result of the addition
 */
inline VecI32 add_i32(VecI32 a, VecI32 b) { return _mm512_add_epi32(a, b); }


/** Computes the horizontal sum of a VecI32 register
 *
 * \param reg register to sum up
 * \returns the sum of the 16 i32 in the register
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
    #define USE_SIMD 256
    #define ALIGNMENT 32
    #define SIMD_UNROLL 16
using VecI16 = __m256i;  // a list of 16x i16
using VecI32 = __m256i;  // a list of 8x i32



/** Loads an i16[16] array into a VecI16 register
 *
 * \param src an array of 16x i16 elements
 * \returns the loaded register
 */
inline VecI16 load_i16(const i16* src) {
    return _mm256_load_si256(reinterpret_cast<const VecI16*>(src));
}

/** Stores a VecI16 register into an i16[16] array
 *
 * \param dst the array of 16x i16 elements to store into
 * \param src the register to store
 */
inline void store_i16(i16* dst, VecI16& src) {
    _mm256_store_si256(reinterpret_cast<VecI16*>(dst), src);
}


/** Returns a VecI16 register with all zeros
 *
 * \returns an all zero register
 */
inline VecI16 zero_i16() { return _mm256_setzero_si256(); }

/** Returns a VecI16 register with all values set to val
 *
 * \param val the value to set to
 * \returns an all val register
 */
inline VecI16 full_i16(i16 val) { return _mm256_set1_epi16(val); }


/** Does an element-wise saturated addition of two VecI16 registers
 *
 * \param a register 1
 * \param b register 2
 * \returns the result of the addition
 */
inline VecI16 adds_i16(VecI16 a, VecI16 b) { return _mm256_adds_epi16(a, b); }

/** Does an element-wise saturated subtraction of two VecI16 registers
 *
 * \param a register 1
 * \param b register 2
 * \returns the result of the subtraction
 */
inline VecI16 subs_i16(VecI16 a, VecI16 b) { return _mm256_subs_epi16(a, b); }

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

/** Does an element-wise multiplication of two VecI16 registers.
 * The bottom 16 bits are taken for each multiplication.
 *
 * \param a register 1
 * \param b register 2
 * \returns the result of the multiplication
 */
inline VecI16 mul_i16(VecI16 a, VecI16 b) { return _mm256_mullo_epi16(a, b); }

/** Does an element-wise multiplication of two VecI16 registers and horizontally adds the results,
 * creating a VecI32 result.
 *
 * \param a register 1
 * \param b register 2
 * \returns the result of the multiplication-addition
 */
inline VecI32 madd_i16(VecI16 a, VecI16 b) { return _mm256_madd_epi16(a, b); }

/** Does an element-wise addition of two VecI32 registers
 *
 * \param a register 1
 * \param b register 2
 * \returns the result of the addition
 */
inline VecI32 add_i32(VecI32 a, VecI32 b) { return _mm256_add_epi32(a, b); }


/** Computes the horizontal sum of a VecI32 register
 *
 * \param reg register to sum up
 * \returns the sum of the 8 i32 in the register
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
