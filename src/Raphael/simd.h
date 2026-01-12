#pragma once
#include <Raphael/types.h>
#include <immintrin.h>

#if defined(__AVX__) || defined(__AVX2__)
    #define USE_SIMD 256
    #define ALIGNMENT 32
typedef __m256i VecI16;  // a list of 16x i16
typedef __m256i VecI32;  // a list of 8x i32



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
    __m128i lo = _mm256_castsi256_si128(reg);       // get bottom 4x i32 from reg
    __m128i hi = _mm256_extracti128_si256(reg, 1);  // get top 4x i32 from reg

    __m128i sum = _mm_add_epi32(lo, hi);  // add pairs [s0, s1, s2, s3]
    sum = _mm_hadd_epi32(sum, sum);       // add adjacent pairs [s0 + s1, s2 + s3, ...]
    sum = _mm_hadd_epi32(sum, sum);       // add remaining pair [s0 + s1 + s2 + s3, ...]
    return _mm_extract_epi32(sum, 0);     // extract first value with sum
}
#else
    #define ALIGNMENT 0
#endif
