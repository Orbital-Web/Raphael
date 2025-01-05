#include <Raphael/simd.h>

#ifdef USE_SIMD
void DOT256_32(simd256_t& c, simd256_t a, simd256_t b) {
    simd256_t prod = _mm256_maddubs_epi16(a, b);           // add 2 (uint8_t a_i * int8_t b_i) pairs
    prod = _mm256_madd_epi16(prod, _mm256_set1_epi16(1));  // add adjacent int16_t pairs
    c = _mm256_add_epi32(c, prod);                         // add to accumulator
}

simd128_t SUM128_32(simd256_t s0, simd256_t s1, simd256_t s2, simd256_t s3, simd128_t bias) {
    s0 = _mm256_hadd_epi32(s0, s1);  // compute [2x sum0, 2x sum1, 2x sum0, 2x sum1]
    s2 = _mm256_hadd_epi32(s2, s3);  // compute [2x sum2, 2x sum3, 2x sum2, 2x sum3]
    s0 = _mm256_hadd_epi32(s0, s2);  // compute [sum0, sum1, sum2, sum3, sum0, sum1, sum2, sum3]

    simd128_t lo = _mm256_castsi256_si128(s0);          // get bottom 4x int32_t from s0
    simd128_t hi = _mm256_extracti128_si256(s0, 1);     // get top 4x int32_t from s0
    return _mm_add_epi32(_mm_add_epi32(lo, hi), bias);  // compute bias + [sum0, sum1, sum2, sum3]
}

int32_t HADD256_32(simd256_t reg) {
    simd128_t lo = _mm256_castsi256_si128(reg);       // get bottom 4x int32_t from reg
    simd128_t hi = _mm256_extracti128_si256(reg, 1);  // get top 4x int32_t from reg

    simd128_t sum = _mm_add_epi32(lo, hi);  // add pairs [s0, s1, s2, s3]
    sum = _mm_hadd_epi32(sum, sum);         // add adjacent pairs [s0 + s1, s2 + s3, ...]
    sum = _mm_hadd_epi32(sum, sum);         // add remaining pair [s0 + s1 + s2 + s3, ...]
    return _mm_extract_epi32(sum, 0);       // extract first value with sum
}
#endif
