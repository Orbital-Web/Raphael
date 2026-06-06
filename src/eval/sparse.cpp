#ifdef EVAL_MULTILAYER
#include <eval/sparse.h>

using namespace raphael::nnue;
using std::popcount;



#ifdef USE_SIMD
void SparseIterator::add_nonzeros(VecU8 l0_out0, VecU8 l0_out1) {
    constexpr i32 regw32 = ALIGNMENT / sizeof(i32);
    static_assert(regw32 % 8 == 0);

#ifdef USE_AVX512
    static_assert(USE_SIMD == 512);
    const auto mask = _mm512_kunpackw(nonzero_mask(l0_out1), nonzero_mask(l0_out0));

    const auto idxs = _mm512_maskz_compress_epi16(mask, offset_);
    _mm512_storeu_si512(&indices_[count_], idxs);
    offset_ = add_i16(offset_, full_i16(32));
    count_ += popcount(mask);

#else
    u32 full_mask = (nonzero_mask(l0_out1) << regw32) | nonzero_mask(l0_out0);

    for (i32 i = 0; i < regw32 / 4; i++) {
        // get offset of up to 8 nonzeros at a time
        const u8 mask = full_mask & 0xFF;
        full_mask >>= 8;

        const auto idxs = _mm_add_epi16(
            offset_, _mm_load_si128(reinterpret_cast<const __m128i*>(&nonzero_idx[mask]))
        );
        _mm_storeu_si128(reinterpret_cast<__m128i*>(&indices_[count_]), idxs);
        offset_ = _mm_add_epi16(offset_, _mm_set1_epi16(8));
        count_ += popcount(mask);
    }
#endif

    assert(count_ <= L1_SIZE / 4);
}

i32 SparseIterator::count() const { return count_; }

i32 SparseIterator::index(i32 nnz_id) const {
    assert(nnz_id < count_);
    return indices_[nnz_id];
}
#endif
#endif
