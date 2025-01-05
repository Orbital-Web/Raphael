#pragma once
#include <immintrin.h>
#include <stdint.h>

#if defined(__AVX__) || defined(__AVX2__)
    #define USE_SIMD 256
    #define ALIGNMENT 32
    // 256 common operations
    #define simd256_t __m256i
    #define LOAD256(src) _mm256_load_si256(reinterpret_cast<const simd256_t*>(src))
    #define STORE256(dst, src) _mm256_store_si256(reinterpret_cast<simd256_t*>(dst), src)
    #define ZERO256 _mm256_setzero_si256  // sets everything to 0
    #define ADDS256_16 _mm256_adds_epi16  // adds 16x int16_t pairs with clamping
    #define SUBS256_16 _mm256_subs_epi16  // subtracts 16x int16_t pairs with clamping
    // 128 common operations
    #define simd128_t __m128i
    #define LOAD128(src) _mm_load_si128(reinterpret_cast<const simd128_t*>(src))
    #define STORE128(dst, src) _mm_store_si128(reinterpret_cast<simd128_t*>(dst), src)
    #define RSHFT128_32 _mm_srai_epi32  // right shifts 4x int32_t


/** Computes partial dot product of uint8_t a[32] and int8_t b[32] and adds into int32_t c[8]
 *
 * E.g., if a = [0, 0, 1, 1, ...] b = [12, -3, 6, -2, ...]
 * Out = [0(12) + 0(-3) + 1(6) + 1(-2), ...]
 *
 * \param c int32_t[8] to add into
 * \param a uint8_t[32] to dot with b
 * \param b int8_t[32] to dot with a
 */
void DOT256_32(simd256_t& c, simd256_t a, simd256_t b);

/** Computes bias + [sum s0, sum s1, sum s2, sum s3]
 *
 * \param s0 int32_t[8] list of partial dot products 0
 * \param s1 int32_t[8] list of partial dot products 1
 * \param s2 int32_t[8] list of partial dot products 2
 * \param s3 int32_t[8] list of partial dot products 3
 * \param bias int32_t[4] bias for each dot products
 * \returns bias + the accumulated sums of s0, s1, s2, and s3
 */
simd128_t SUM128_32(simd256_t s0, simd256_t s1, simd256_t s2, simd256_t s3, simd128_t bias);

/** Computes the horizontal sum of an 8x int32_t register
 *
 * \param reg int32_t[8] list of values to add
 * \returns int32_t the sum of the 8 values
 */
int32_t HADD256_32(simd256_t reg);
#else
    #define ALIGNMENT 0
#endif
