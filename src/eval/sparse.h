#pragma once
#ifdef EVAL_MULTILAYER
#include <eval/arch.h>
#include <eval/simd.h>



namespace raphael::nnue {
class SparseIterator {
#ifdef USE_SIMD
private:
    u16 indices_[L1_SIZE / 4] = {};
    i32 count_ = 0;

#ifdef USE_AVX512
    // clang-format off
    __m512i offset_ = _mm512_set_epi16(
        31, 30, 29, 28, 27, 26, 25, 24,
        23, 22, 21, 20, 19, 18, 17, 16,
        15, 14, 13, 12, 11, 10,  9,  8,
         7,  6,  5,  4,  3,  2,  1,  0
    );  // clang-format on
#else
    __m128i offset_ = _mm_setzero_si128();

    // precompute nonzero_idx[mask][nnz_idx] = position of nonzero block
    alignas(16) static constexpr MultiArray<u16, 256, 8> nonzero_idx = [] {
        MultiArray<u16, 256, 8> idx{};

        for (i32 i = 0; i < 256; i++) {
            i32 nnz = 0;

            for (u8 mask = i; mask != 0; mask &= mask - 1) idx[i][nnz++] = std::countr_zero(mask);
        }

        return idx;
    }();
#endif


public:
    /** Adds the nonzero indices to the sparse iterator
     *
     * \param l0_out0 first chunk of l0 outputs
     * \param l0_out1 second chunk of l0 outputs
     */
    void add_nonzeros(VecU8 l0_out0, VecU8 l0_out1);

    /** Returns the number of nonezero blocks
     *
     * \returns the nnz count
     */
    i32 count() const;

    /** Returns the tile id for a nonzero block
     *
     * \param nnz_id which nonzero chunk we want the index for
     * \returns the corresponding tile id
     */
    i32 index(i32 nnz_id) const;
#endif
};
}  // namespace raphael::nnue
#endif
