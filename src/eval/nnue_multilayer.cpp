#ifdef EVAL_MULTILAYER
#include <eval/nnue_multilayer.h>

#define INCBIN_PREFIX g_
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#include <thirdparty/incbin.h>

#include <fstream>
#include <stdexcept>

using namespace raphael::nnue;
using std::max;
using std::min;
using std::ofstream;
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

    alignas(ALIGNMENT) u8 l0_out[L1_SIZE];
    SparseIterator sp;
    activate_l0(stm_acc_psq, stm_acc_ti, l0_out, sp);
    activate_l0(ntm_acc_psq, ntm_acc_ti, l0_out + L1_SIZE / 2, sp);

    constexpr i32 bucket_div = (32 + N_OUTBUCKETS - 1) / N_OUTBUCKETS;
    const i32 bucket_idx = (board.occ().count() - 2) / bucket_div;

    alignas(ALIGNMENT) i32 l1_out[L2_SIZE];
    i64 eval;
    forward_l1(l0_out, l1_out, sp, bucket_idx);
    forward_l2l3(l1_out, eval, bucket_idx);

    eval *= OUTPUT_SCALE;
    eval /= (QC * QC * QC * QC);
    return static_cast<i32>(eval);
}

NnueState& Nnue::observer() { return state_; }



void Nnue::activate_l0(
    const i16 acc_psq[L1_SIZE],
    const i16 acc_ti[L1_SIZE],
    u8 l0_out[L1_SIZE / 2],
    [[maybe_unused]] SparseIterator& sp
) const {
    constexpr i32 n_pairs = L1_SIZE / 2;

#ifdef USE_SIMD
    constexpr i32 regw16 = ALIGNMENT / sizeof(i16);
    static_assert(L1_SIZE % (8 * regw16) == 0);

    constexpr i32 n_chunks = n_pairs / regw16;
    const VecI16 zs = zero_i16();
    const VecI16 qa = full_i16(QA);

    for (i32 i = 0; i < n_chunks; i += 4) {
        // compute 4 * regw16 values of the pairwise mul at once, input in [0, QA]
        VecI16 acc[4][2];
        #pragma GCC unroll 4
        for (i32 r = 0; r < 4; r++) {
            const VecI16 acc_psq0 = load_i16(&acc_psq[(i + r) * regw16]);
            const VecI16 acc_ti0 = load_i16(&acc_ti[(i + r) * regw16]);
            const VecI16 acc_psq1 = load_i16(&acc_psq[(i + r) * regw16 + n_pairs]);
            const VecI16 acc_ti1 = load_i16(&acc_ti[(i + r) * regw16 + n_pairs]);
            acc[r][0] = clamp_i16(add_i16(acc_psq0, acc_ti0), zs, qa);
            acc[r][1] = clamp_i16(add_i16(acc_psq1, acc_ti1), zs, qa);
        }

        const VecI16 pw0 = mulhi_i16(lshift_i16(acc[0][0], 7), acc[0][1]);
        const VecI16 pw1 = mulhi_i16(lshift_i16(acc[1][0], 7), acc[1][1]);
        const VecI16 pw2 = mulhi_i16(lshift_i16(acc[2][0], 7), acc[2][1]);
        const VecI16 pw3 = mulhi_i16(lshift_i16(acc[3][0], 7), acc[3][1]);

        // packus will interleave every 8 values thus l1w must be permuted, output in [0, 127]
        const VecU8 out0 = pack_u8_i16(pw0, pw1);
        const VecU8 out1 = pack_u8_i16(pw2, pw3);
        store_u8(&l0_out[(i + 0) * regw16], out0);
        store_u8(&l0_out[(i + 2) * regw16], out1);

        // track nonzero blocks
        sp.add_nonzeros(out0, out1);
    }
#else
    for (i32 i = 0; i < n_pairs; i++) {
        const i32 acc_v0 = min(max(acc_psq[i] + acc_ti[i], 0), QA);
        const i32 acc_v1 = min(max(acc_psq[i + n_pairs] + acc_ti[i + n_pairs], 0), QA);

        // simulate mulhi, assuming non-permuted weights
        l0_out[i] = ((acc_v0 << 7) * acc_v1) >> 16;
    }
#endif

#ifdef MEASURE_SPARSITY
    record_ft_activations(l0_out);
#endif
}

void Nnue::forward_l1(
    const u8 l0_out[L1_SIZE],
    i32 l1_out[L2_SIZE],
    [[maybe_unused]] const SparseIterator& sp,
    i32 bucket_idx
) const {
#ifdef USE_SIMD
    // get nnz
    constexpr i32 regw8 = ALIGNMENT / sizeof(i8);
    constexpr i32 regw32 = ALIGNMENT / sizeof(i32);
    static_assert(L2_SIZE % regw32 == 0);
    static_assert(L1_SIZE % 16 == 0);

    const i32 nnz = sp.count();
    const i32 nnz4 = (nnz / 4) * 4;

    // compute l1 matmul
    constexpr i32 n_chunks = L2_SIZE / regw32;
    VecI32 l1_pre[n_chunks][4];

    #pragma GCC unroll 32
    for (i32 r = 0; r < n_chunks; r++) {
        l1_pre[r][0] = zero_i32();
        l1_pre[r][1] = zero_i32();
        l1_pre[r][2] = zero_i32();
        l1_pre[r][3] = zero_i32();
    }

    for (i32 nnz_id = 0; nnz_id < nnz4; nnz_id += 4) {
        const i32 tile_id0 = sp.index(nnz_id + 0);
        const i32 tile_id1 = sp.index(nnz_id + 1);
        const i32 tile_id2 = sp.index(nnz_id + 2);
        const i32 tile_id3 = sp.index(nnz_id + 3);

        const VecU8 inputs0 = tile_u8(&l0_out[4 * tile_id0]);
        const VecU8 inputs1 = tile_u8(&l0_out[4 * tile_id1]);
        const VecU8 inputs2 = tile_u8(&l0_out[4 * tile_id2]);
        const VecU8 inputs3 = tile_u8(&l0_out[4 * tile_id3]);

        for (i32 r = 0; r < n_chunks; r++) {
            const VecI8 weights0 = load_i8(&params->W1[bucket_idx][tile_id0][r * regw8]);
            const VecI8 weights1 = load_i8(&params->W1[bucket_idx][tile_id1][r * regw8]);
            const VecI8 weights2 = load_i8(&params->W1[bucket_idx][tile_id2][r * regw8]);
            const VecI8 weights3 = load_i8(&params->W1[bucket_idx][tile_id3][r * regw8]);

            l1_pre[r][0] = dpbusd_i32(l1_pre[r][0], inputs0, weights0);
            l1_pre[r][1] = dpbusd_i32(l1_pre[r][1], inputs1, weights1);
            l1_pre[r][2] = dpbusd_i32(l1_pre[r][2], inputs2, weights2);
            l1_pre[r][3] = dpbusd_i32(l1_pre[r][3], inputs3, weights3);
        }
    }

    for (i32 nnz_id = nnz4; nnz_id < nnz; nnz_id++) {
        const i32 tile_id = sp.index(nnz_id);
        const VecU8 inputs = tile_u8(&l0_out[4 * tile_id]);

        for (i32 r = 0; r < n_chunks; r++) {
            const VecI8 weights = load_i8(&params->W1[bucket_idx][tile_id][r * regw8]);
            l1_pre[r][0] = dpbusd_i32(l1_pre[r][0], inputs, weights);
        }
    }

    // activate l1
    const VecI32 zs = zero_i32();
    const VecI32 qs = full_i32(QC << L1_SHIFT);

    for (i32 r = 0; r < n_chunks; r++) {
        const VecI32 pre0 = add_i32(l1_pre[r][0], l1_pre[r][1]);
        const VecI32 pre1 = add_i32(l1_pre[r][2], l1_pre[r][3]);

        // apply screlu and downshift into QC^2 space
        const VecI32 bias = load_i32(&params->b1[bucket_idx][r * regw32]);
        const VecI32 pre = add_i32(add_i32(pre0, pre1), bias);
        const VecI32 crelu = clamp_i32(pre, zs, qs);
        const VecI32 screlu = rshift_i32(mullo_i32(crelu, crelu), 2 * L1_SHIFT);
        store_i32(&l1_out[r * regw32], screlu);
    }
#else
    constexpr i32 n_tiles = L1_SIZE / 4;
    i32 l1_pre[L2_SIZE] = {0};

    // compute l1 matmul
    for (i32 i = 0; i < n_tiles; i++)
        for (i32 j = 0; j < L2_SIZE; j++)
            for (i32 k = 0; k < 4; k++)
                l1_pre[j] += l0_out[4 * i + k] * params->W1[bucket_idx][i][4 * j + k];

    // activate l1
    for (i32 i = 0; i < L2_SIZE; i++) {
        const i32 bias = params->b1[bucket_idx][i];
        const i32 pre = l1_pre[i] + bias;
        const i32 crelu = min(max(pre, 0), QC << L1_SHIFT);
        const i32 screlu = (crelu * crelu) >> (2 * L1_SHIFT);
        l1_out[i] = screlu;
    }
#endif
}

void Nnue::forward_l2l3(const i32 l1_out[L2_SIZE], i64& l3_out, i32 bucket_idx) const {
#ifdef USE_SIMD
    // compute l2 matmul
    constexpr i32 regw32 = ALIGNMENT / sizeof(i32);
    static_assert(L3_SIZE % regw32 == 0);

    constexpr i32 n_chunks = L3_SIZE / regw32;
    VecI32 l2_pre[n_chunks];

    #pragma GCC unroll 32
    for (i32 r = 0; r < n_chunks; r++) l2_pre[r] = load_i32(&params->b2[bucket_idx][r * regw32]);

    for (i32 i = 0; i < L2_SIZE; i++) {
        // l2_pre += W2[:, i] * l1_out[i], inputs in QC^2 space, weights in QC space
        VecI32 input = full_i32(l1_out[i]);
        VecI32 weights[n_chunks];

        #pragma GCC unroll 32
        for (i32 r = 0; r < n_chunks; r++)
            weights[r] = load_i32(&params->W2[bucket_idx][i][r * regw32]);

        #pragma GCC unroll 32
        for (i32 r = 0; r < n_chunks; r++) l2_pre[r] = fmadd_i32(input, weights[r], l2_pre[r]);
    }

    // activate l2 and compute l3 matmul
    const VecI32 zs = zero_i32();
    const VecI32 qs = full_i32(QC * QC * QC);

    l3_out = params->b3[bucket_idx];
    VecI32 sums = zero_i32();

    for (i32 r = 0; r < n_chunks; r++) {
        const VecI32 crelu = clamp_i32(l2_pre[r], zs, qs);
        const VecI32 weights = load_i32(&params->W3[bucket_idx][r * regw32]);
        sums = fmadd_i32(crelu, weights, sums);
    }

    l3_out += hadd_i32(sums);
#else
    i32 l2_out[L3_SIZE];
    for (i32 i = 0; i < L3_SIZE; i++) l2_out[i] = params->b2[bucket_idx][i];

    // compute l2 matmul
    for (i32 i = 0; i < L2_SIZE; i++)
        for (i32 j = 0; j < L3_SIZE; j++) l2_out[j] += l1_out[i] * params->W2[bucket_idx][i][j];

    // activate l2 and compute l3 dotprod
    l3_out = params->b3[bucket_idx];
    for (i32 i = 0; i < L3_SIZE; i++) {
        const i32 crelu = min(max(l2_out[i], 0), QC * QC * QC);
        l3_out += crelu * params->W3[bucket_idx][i];
    }
#endif
}


#ifdef MEASURE_SPARSITY
u64 Nnue::save_ft_activations() {
    ofstream outfile("ft_activations.json");
    if (!outfile.is_open()) throw runtime_error("could not save ft activations");

    outfile << "[ ";
    for (i32 i = 0; i < L1_SIZE / 2; i++) {
        if (i != 0) outfile << ", ";
        outfile << ft_activations[i];
    }
    outfile << " ]\n";
    outfile.close();

    // called once for each perspective
    assert(total_calls % 2 == 0);
    return total_nnz / (total_calls / 2);
}

void Nnue::record_ft_activations(const u8 l0_out[L1_SIZE / 2]) {
    // track activations
    for (i32 i = 0; i < L1_SIZE / 2; i++)
        if (l0_out[i] != 0) ft_activations[i]++;

    // track nonzero blocks
    for (i32 i = 0; i < L1_SIZE / 2; i += 4) {
        bool nonzero = false;
        for (i32 j = 0; j < 4; j++) {
            if (l0_out[i + j] != 0) {
                nonzero = true;
                break;
            }
        }
        if (nonzero) total_nnz++;
    }
    total_calls++;
}
#endif
#endif
