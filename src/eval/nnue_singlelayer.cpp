#ifdef EVAL_SINGLELAYER
#include <eval/nnue_singlelayer.h>

#define INCBIN_PREFIX g_
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#include <thirdparty/incbin.h>

#include <fstream>
#include <stdexcept>

using namespace raphael::nnue;
using std::max;
using std::min;
using std::ofstream;
using std::popcount;
using std::runtime_error;

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

INCBIN(unsigned char, netfile, TOSTRING(NETWORK_FILE));



Nnue::Nnue(): params(load_network()), state_(params->W0_psq, params->W0_ti, params->b0) {}

const Nnue::NnueParams* Nnue::load_network() {
    constexpr usize padded_size = 64 * ((sizeof(NnueParams) + 63) / 64);
    if (g_netfile_size != padded_size)
        throw runtime_error("network file and architecture doesn't match");

    return reinterpret_cast<const NnueParams*>(g_netfile_data);
}



i32 Nnue::evaluate(const chess::Board& board) {
    // get address to accumulators
    const auto& acc = state_.get_top_accumulator(board);
    const auto stm_acc_psq = acc.psq_vals[board.stm()];
    const auto ntm_acc_psq = acc.psq_vals[~board.stm()];
    const auto stm_acc_ti = acc.ti_vals[board.stm()];
    const auto ntm_acc_ti = acc.ti_vals[~board.stm()];

    constexpr i32 bucket_div = (32 + N_OUTBUCKETS - 1) / N_OUTBUCKETS;
    const i32 bucket_idx = (board.occ().count() - 2) / bucket_div;

    constexpr i32 n_pairs = L1_SIZE / 2;

#ifdef USE_SIMD
    constexpr i32 regw16 = ALIGNMENT / sizeof(i16);
    constexpr i32 n_chunks = L1_SIZE / (2 * regw16);
    static_assert(L1_SIZE % (2 * regw16) == 0);

    const VecI16 zs = zero_i16();
    const VecI16 qa = full_i16(QA);

    VecI32 sum = zero_i16();
    for (i32 i = 0; i < n_chunks; i++) {
        const VecI16 stm_psq0 = load_i16(&stm_acc_psq[i * regw16]);
        const VecI16 stm_psq1 = load_i16(&stm_acc_psq[i * regw16 + n_pairs]);
        const VecI16 stm_ti0 = load_i16(&stm_acc_ti[i * regw16]);
        const VecI16 stm_ti1 = load_i16(&stm_acc_ti[i * regw16 + n_pairs]);
        const VecI16 ntm_psq0 = load_i16(&ntm_acc_psq[i * regw16]);
        const VecI16 ntm_psq1 = load_i16(&ntm_acc_psq[i * regw16 + n_pairs]);
        const VecI16 ntm_ti0 = load_i16(&ntm_acc_ti[i * regw16]);
        const VecI16 ntm_ti1 = load_i16(&ntm_acc_ti[i * regw16 + n_pairs]);

        const VecI16 stm_v0 = clamp_i16(add_i16(stm_psq0, stm_ti0), zs, qa);
        const VecI16 stm_v1 = clamp_i16(add_i16(stm_psq1, stm_ti1), zs, qa);
        const VecI16 ntm_v0 = clamp_i16(add_i16(ntm_psq0, ntm_ti0), zs, qa);
        const VecI16 ntm_v1 = clamp_i16(add_i16(ntm_psq1, ntm_ti1), zs, qa);

        const VecI16 stm_w = load_i16(&params->W1[bucket_idx][i * regw16]);
        const VecI16 ntm_w = load_i16(&params->W1[bucket_idx][i * regw16 + n_pairs]);

        const VecI16 stm_pw = madd_i16(mullo_i16(stm_w, stm_v0), stm_v1);
        const VecI16 ntm_pw = madd_i16(mullo_i16(ntm_w, ntm_v0), ntm_v1);

        sum = add_i32(sum, add_i32(stm_pw, ntm_pw));
    }

    i64 eval = QA * params->b1[bucket_idx] + hadd_i32(sum);
#else
    i64 eval = QA * params->b1[bucket_idx];

    // compute W1 dot pw(acc)
    for (i32 i = 0; i < n_pairs; i++) {
        const i32 stm_v0 = min(max(static_cast<i32>(stm_acc_psq[i] + stm_acc_ti[i]), 0), QA);
        const i32 stm_v1
            = min(max(static_cast<i32>(stm_acc_psq[i + n_pairs] + stm_acc_ti[i + n_pairs]), 0), QA);
        const i32 ntm_v0 = min(max(static_cast<i32>(ntm_acc_psq[i] + ntm_acc_ti[i]), 0), QA);
        const i32 ntm_v1
            = min(max(static_cast<i32>(ntm_acc_psq[i + n_pairs] + ntm_acc_ti[i + n_pairs]), 0), QA);

        eval += params->W1[bucket_idx][i] * stm_v0 * stm_v1;
        eval += params->W1[bucket_idx][i + n_pairs] * ntm_v0 * ntm_v1;
    }
#endif
    eval *= OUTPUT_SCALE;
    eval /= (QA * QA * QB);
    return static_cast<i32>(eval);
}

NnueState& Nnue::observer() { return state_; }
#endif
