#include <Raphael/nnue.h>

#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace Raphael;
using std::copy;
using std::cout, std::endl;
using std::ifstream, std::ios;
using std::invalid_argument, std::runtime_error;
using std::max, std::min;
using std::string;
using std::vector;

extern const bool UCI;



Nnue::NnueWeights Nnue::params;
bool Nnue::loaded = false;

Nnue::Nnue() { load(default_nnue_path); }
Nnue::Nnue(const std::string& nnue_path) { load(nnue_path.c_str()); }

void Nnue::load(const char* nnue_path) {
    if (loaded) return;

    if (!UCI) {
#ifdef USE_SIMD
        cout << "Raphael: SIMD AVX-" << USE_SIMD << " available for NNUE" << endl;
#else
        cout << "Raphael: SIMD unavailable for NNUE" << endl;
#endif
        cout << "Raphael: Loading " << nnue_path << endl;
    }

    ifstream nnue_file(nnue_path, ios::binary);
    if (!nnue_file) throw runtime_error("could not open file");

    auto read_or_throw = [&](void* dest, std::size_t bytes) {
        if (!nnue_file.read(reinterpret_cast<char*>(dest), bytes))
            throw std::runtime_error("failed to read weights");
    };

    read_or_throw(params.W0, sizeof(params.W0));
    read_or_throw(params.b0, sizeof(params.b0));
    read_or_throw(params.W1, sizeof(params.W1));
    read_or_throw(params.b1, sizeof(params.b1));
    read_or_throw(params.W2, sizeof(params.W2));
    read_or_throw(params.b2, sizeof(params.b2));
    read_or_throw(params.W3, sizeof(params.W3));
    read_or_throw(&params.b3, sizeof(params.b3));

    if (!nnue_file.eof() && nnue_file.peek() != EOF)
        throw runtime_error("file size does not match nnue size");
    loaded = true;
}



/* ------------------------------- Evaluate ------------------------------- */

int32_t Nnue::evaluate(int ply, bool side) {
    // stack accumulators based on perspective
    alignas(ALIGNMENT) int16_t acc[2 * N_HIDDEN0];
    copy(accumulators[ply][side], accumulators[ply][side] + N_HIDDEN0, acc);
    copy(accumulators[ply][!side], accumulators[ply][!side] + N_HIDDEN0, acc + N_HIDDEN0);

    // make buffer for relu and linear output (reuse for all layers)
    alignas(ALIGNMENT) int8_t buf_relu[2 * N_HIDDEN0];
    alignas(ALIGNMENT) int32_t buf_linear[N_HIDDEN1];
    static_assert(2 * N_HIDDEN0 >= N_HIDDEN1);
    static_assert(N_HIDDEN1 >= N_HIDDEN2);

    // layer 0: accumulator
    crelu(acc, buf_relu, 2 * N_HIDDEN0);

    // layer 1: 2 * N_HIDDEN0 -> N_HIDDEN1
    linear(params.W1, params.b1, buf_relu, buf_linear, 2 * N_HIDDEN0, N_HIDDEN1, QLEVEL1);
    crelu(buf_linear, buf_relu, N_HIDDEN1);

    // layer 2: N_HIDDEN1 -> N_HIDDEN2
    linear(params.W2, params.b2, buf_relu, buf_linear, N_HIDDEN1, N_HIDDEN2, QLEVEL2);
    crelu(buf_linear, buf_relu, N_HIDDEN1);

    // layer 3: N_HIDDEN2 -> 1, can't use linear() as that requires output size be a multiple of 4
    int32_t eval = params.b3;
#ifdef USE_SIMD
    constexpr int regw = ALIGNMENT / sizeof(int8_t);
    static_assert(N_HIDDEN2 % regw == 0);
    const int n_chunks = N_HIDDEN2 / regw;
    simd256_t sum = ZERO256();
    for (int i = 0; i < n_chunks; i++)
        DOT256_32(sum, LOAD256(&buf_relu[i * regw]), LOAD256(&params.W3[i * regw]));
    eval += HADD256_32(sum);
#else
    for (int i = 0; i < N_HIDDEN2; i++) eval += params.W3[i] * buf_relu[i];
#endif
    return eval >> QLEVEL3;
}

int16_t* Nnue::NnueAccumulator::operator[](bool side) { return v[side]; }
const int16_t* Nnue::NnueAccumulator::operator[](bool side) const { return v[side]; }

void Nnue::refresh_accumulator(NnueAccumulator& new_acc, const vector<int>& features, bool side) {
#ifdef USE_SIMD
    constexpr int regw = ALIGNMENT / sizeof(int16_t);
    static_assert(N_HIDDEN0 % regw == 0);
    constexpr int n_chunks = N_HIDDEN0 / regw;
    simd256_t regs[n_chunks];

    // load bias into registers
    for (int i = 0; i < n_chunks; i++) regs[i] = LOAD256(&params.b0[i * regw]);

    // add active features
    for (int f : features)
        for (int i = 0; i < n_chunks; i++)
            regs[i] = ADDS256_16(regs[i], LOAD256(&params.W0[f * N_HIDDEN0 + i * regw]));

    // store result in accumulator
    for (int i = 0; i < n_chunks; i++) STORE256(&new_acc[side][i * regw], regs[i]);
#else
    // copy bias
    copy(params.b0, params.b0 + N_HIDDEN0, new_acc.v[side]);

    // accumulate columns of active features
    for (int f : features)
        for (int i = 0; i < N_HIDDEN0; i++) new_acc[side][i] += params.W0[f * N_HIDDEN0 + i];
#endif
}

void Nnue::update_accumulator(
    NnueAccumulator& new_acc,
    const NnueAccumulator& old_acc,
    const vector<int>& add_features,
    const vector<int>& rem_features,
    bool side
) {
#ifdef USE_SIMD
    constexpr int regw = ALIGNMENT / sizeof(int16_t);
    static_assert(N_HIDDEN0 % regw == 0);
    constexpr int n_chunks = N_HIDDEN0 / regw;
    simd256_t regs[n_chunks];

    // load previous accumulator values into registers
    for (int i = 0; i < n_chunks; i++) regs[i] = LOAD256(&old_acc[side][i * regw]);

    // subtract rem_features
    for (int f : rem_features)
        for (int i = 0; i < n_chunks; i++)
            regs[i] = SUBS256_16(regs[i], LOAD256(&params.W0[f * N_HIDDEN0 + i * regw]));

    // add add_features
    for (int f : add_features)
        for (int i = 0; i < n_chunks; i++)
            regs[i] = ADDS256_16(regs[i], LOAD256(&params.W0[f * N_HIDDEN0 + i * regw]));

    // store results in new accumulator
    for (int i = 0; i < n_chunks; i++) STORE256(&new_acc[side][i * regw], regs[i]);
#else
    // copy old_acc into new_acc if they aren't the same already
    if (&new_acc != &old_acc) copy(old_acc[side], old_acc[side] + N_HIDDEN0, new_acc[side]);

    // subtract rem_features
    for (int f : rem_features)
        for (int i = 0; i < N_HIDDEN0; i++) new_acc[side][i] -= params.W0[f * N_HIDDEN0 + i];

    // add add_features
    for (int f : add_features)
        for (int i = 0; i < N_HIDDEN0; i++) new_acc[side][i] += params.W0[f * N_HIDDEN0 + i];
#endif
}

void Nnue::linear(
    const int8_t* w,
    const int32_t* b,
    const int8_t* input,
    int32_t* output,
    int in_size,
    int out_size,
    int quantization_level
) {
#ifdef USE_SIMD
    constexpr int regw = ALIGNMENT / sizeof(int8_t);
    assert(in_size % regw == 0);
    assert(out_size % 4 == 0);  // unroll by 4
    const int n_inchunk = in_size / regw;
    const int n_outchunk = out_size / 4;

    // compute (W*input + b) / 2^quantization_level
    for (int i = 0; i < n_outchunk; i++) {
        // process 4 outputs at a time
        const int offset0 = (4 * i + 0) * in_size;
        const int offset1 = (4 * i + 1) * in_size;
        const int offset2 = (4 * i + 2) * in_size;
        const int offset3 = (4 * i + 3) * in_size;
        simd256_t sum0 = ZERO256();
        simd256_t sum1 = ZERO256();
        simd256_t sum2 = ZERO256();
        simd256_t sum3 = ZERO256();

        // compute input dot w[o_i] for each of the 4 outputs
        for (int j = 0; j < n_inchunk; j++) {
            const simd256_t inp = LOAD256(&input[j * regw]);
            DOT256_32(sum0, inp, LOAD256(&w[offset0 + j * regw]));
            DOT256_32(sum1, inp, LOAD256(&w[offset1 + j * regw]));
            DOT256_32(sum2, inp, LOAD256(&w[offset2 + j * regw]));
            DOT256_32(sum3, inp, LOAD256(&w[offset3 + j * regw]));
        }

        // sum up dot products and add bias
        const simd128_t bias = LOAD128(&b[4 * i]);
        simd128_t out = SUM128_32(sum0, sum1, sum2, sum3, bias);

        // divide by quantize scale factor and store
        out = RSHFT128_32(out, quantization_level);
        STORE128(&output[4 * i], out);
    }
#else
    // compute (W*input + b) / 2^quantization_level
    for (int m = 0; m < out_size; m++) {
        int32_t outm = b[m];
        for (int n = 0; n < in_size; n++) outm += input[n] * w[m * in_size + n];
        output[m] = outm >> quantization_level;
    }
#endif
}

void Nnue::crelu(const int32_t* input, int8_t* output, int size) {
#ifdef USE_SIMD
    constexpr int in_regw = ALIGNMENT / sizeof(int32_t);
    constexpr int out_regw = ALIGNMENT / sizeof(int8_t);
    assert(size % out_regw == 0);
    const int n_outchunk = size / out_regw;

    const simd256_t zero = ZERO256();
    const simd256_t control = _mm256_set_epi32(7, 3, 6, 2, 5, 1, 4, 0);

    for (int i = 0; i < n_outchunk; i++) {
        const simd256_t in0 = _mm256_packs_epi32(
            LOAD256(&input[(i * 4 + 0) * in_regw]), LOAD256(&input[(i * 4 + 1) * in_regw])
        );  // 2*8x int32_t [in0_0 in0_1] -> 16x int32_t (clamped 32767)
        const simd256_t in1 = _mm256_packs_epi32(
            LOAD256(&input[(i * 4 + 2) * in_regw]), LOAD256(&input[(i * 4 + 3) * in_regw])
        );  // 2*8x int32_t [in1_0 in1_1] -> 16x int32_t (clamped 32767)
        const simd256_t res = _mm256_permutevar8x32_epi32(
            _mm256_max_epi8(_mm256_packs_epi16(in0, in1), zero), control
        );  // 2*16x int16_t [in0, in1] -> 32x int8_t (clamped 127) -> clamp 0 -> fix ordering
        STORE256(&output[i * out_regw], res);
    }
#else
    for (int i = 0; i < size; i++) output[i] = (int8_t)min((int32_t)127, max((int32_t)0, input[i]));
#endif
}

void Nnue::crelu(const int16_t* input, int8_t* output, int size) {
#ifdef USE_SIMD
    constexpr int in_regw = ALIGNMENT / sizeof(int16_t);
    constexpr int out_regw = ALIGNMENT / sizeof(int8_t);
    assert(size % out_regw == 0);
    const int n_outchunk = size / out_regw;

    const simd256_t zero = ZERO256();
    const int control = 0b11011000;  // 3, 1, 2, 0

    for (int i = 0; i < n_outchunk; i++) {
        const simd256_t in0 = LOAD256(&input[(i * 2 + 0) * in_regw]);
        const simd256_t in1 = LOAD256(&input[(i * 2 + 1) * in_regw]);
        const simd256_t res = _mm256_permute4x64_epi64(
            _mm256_max_epi8(_mm256_packs_epi16(in0, in1), zero), control
        );  // 2*16x int16_t [in0, in1] -> 32x int8_t (clamped 127) -> clamp 0 -> fix ordering
        STORE256(&output[i * out_regw], res);
    }
#else
    for (int i = 0; i < size; i++) output[i] = (int8_t)min((int16_t)127, max((int16_t)0, input[i]));
#endif
}



/* ------------------------------- Update ------------------------------- */

void Nnue::set_board(const chess::Board& board) {
    int wkb = KING_BUCKETS[board.kingSq(chess::Color::WHITE)];
    int bkb = KING_BUCKETS[board.kingSq(chess::Color::BLACK) ^ 56];

    vector<int> w_features, b_features;
    w_features.reserve(64);
    b_features.reserve(64);

    auto pieces = board.occ();
    while (pieces) {
        auto sq = chess::builtin::poplsb(pieces);
        int sqi = (int)sq;

        int wpiece = (int)board.at(sq);  // 0...5, 6...11
        int bpiece = (wpiece + 6) % 12;  // 6...11, 0...5

        w_features.push_back(12 * 64 * wkb + 64 * wpiece + sqi);
        b_features.push_back(12 * 64 * bkb + 64 * bpiece + (sqi ^ 56));
    }
    refresh_accumulator(accumulators[0], w_features, true);
    refresh_accumulator(accumulators[0], b_features, false);
}

void Nnue::make_move(int ply, const chess::Move& move, const chess::Board& board) {
    assert((ply != 0));

    auto from_sq = move.from();
    auto to_sq = move.to();
    auto move_type = move.typeOf();

    // update black and white states
    for (bool side : {false, true}) {
        // get previous king bucket
        auto ksq = board.kingSq((side) ? chess::Color::WHITE : chess::Color::BLACK);
        auto ksqi = (side) ? (int)ksq : (int)ksq ^ 56;
        auto kb = KING_BUCKETS[ksqi];

        // check if our king moved
        if (ksq == from_sq) {
            // check if king bucket changed (new king square is to_sq, unless we're castling)
            int new_ksqi = (side) ? (int)to_sq : (int)to_sq ^ 56;
            if (move_type == move.CASTLING) new_ksqi = (new_ksqi == 0) ? 2 : 6;
            int new_kb = KING_BUCKETS[new_ksqi];

            if (new_kb != kb) {
                // king bucket changed, needs total refresh
                vector<int> features;
                features.reserve(64);

                // add new king feature
                features.push_back(12 * 64 * new_kb + 64 * 5 + new_ksqi);

                // add castled rook feature (if castling)
                if (move_type == move.CASTLING) {
                    int new_rsqi = (new_ksqi == 2) ? 3 : 5;
                    features.push_back(12 * 64 * new_kb + 64 * 3 + new_rsqi);
                }

                // add remaining features except old king, captured piece (if any/castled rook),
                auto pieces = board.occ();
                while (pieces) {
                    auto sq = chess::builtin::poplsb(pieces);
                    if (sq == from_sq || sq == to_sq) continue;
                    int sqi = (side) ? (int)sq : (int)sq ^ 56;
                    int piece = (side) ? (int)board.at(sq) : ((int)board.at(sq) + 6) % 12;
                    features.push_back(12 * 64 * new_kb + 64 * piece + sqi);
                }
                refresh_accumulator(accumulators[ply], features, side);
                continue;
            }
        }

        // do incremental update
        vector<int> add_features, rem_features;
        add_features.reserve(2);  // 2 if castling
        rem_features.reserve(2);  // 2 if castling/capturing/enpassant

        bool moving = (board.sideToMove() == chess::Color::WHITE) == side;
        int from_sqi = (side) ? (int)from_sq : (int)from_sq ^ 56;
        int to_sqi = (side) ? (int)to_sq : (int)to_sq ^ 56;
        auto from_piece = board.at(from_sq);
        auto to_piece = board.at(to_sq);
        int from_piecei = (side) ? (int)from_piece : ((int)from_piece + 6) % 12;
        int to_piecei = (side) ? (int)to_piece : ((int)to_piece + 6) % 12;

        // add moving piece to rem_features
        rem_features.push_back(12 * 64 * kb + 64 * from_piecei + from_sqi);

        // add moved piece to add_features (handle promotion and enemy castling)
        if (move_type == move.PROMOTION) {
            int promote_piecei = !moving * 6 + (int)move.promotionType();
            add_features.push_back(12 * 64 * kb + 64 * promote_piecei + to_sqi);
        } else if (move_type == move.CASTLING) {
            int new_ksqi = (to_sqi == 63) ? 62 : 58;
            int new_rsqi = (new_ksqi == 62) ? 61 : 59;
            add_features.push_back(12 * 64 * kb + 64 * from_piecei + new_ksqi);
            add_features.push_back(12 * 64 * kb + 64 * 9 + new_rsqi);
        } else
            add_features.push_back(12 * 64 * kb + 64 * from_piecei + to_sqi);

        // add captured piece (castling is treated as rook capture) to rem_features
        if (to_piece != chess::Piece::NONE)
            rem_features.push_back(12 * 64 * kb + 64 * to_piecei + to_sqi);
        else if (move_type == move.ENPASSANT)
            rem_features.push_back(12 * 64 * kb + 64 * 6 * moving + (to_sqi + 8 - moving * 16));
        update_accumulator(
            accumulators[ply], accumulators[ply - 1], add_features, rem_features, side
        );
    }
}
