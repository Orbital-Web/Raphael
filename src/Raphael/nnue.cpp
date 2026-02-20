#include <Raphael/nnue.h>

#include <cstring>
#include <fstream>
#include <iostream>

using namespace raphael;
using std::copy;
using std::cout;
using std::endl;
using std::ifstream;
using std::invalid_argument;
using std::ios;
using std::max;
using std::memcpy;
using std::min;
using std::runtime_error;
using std::string;
using std::vector;

extern const unsigned char _binary_net_nnue_start[];
extern const unsigned char _binary_net_nnue_end[];



Nnue::NnueWeights Nnue::params;
bool Nnue::loaded = false;

Nnue::Nnue() { load(); }
Nnue::Nnue(const string& nnue_path) { load(nnue_path.c_str()); }

void Nnue::load() {
    if (loaded) return;

    const unsigned char* nnue_data = _binary_net_nnue_start;

    auto read_or_throw = [&](void* dest, usize bytes) {
        if (nnue_data + bytes > _binary_net_nnue_end) throw runtime_error("failed to read weights");

        memcpy(dest, nnue_data, bytes);
        nnue_data += bytes;
    };

    read_or_throw(params.W0, sizeof(params.W0));
    read_or_throw(params.b0, sizeof(params.b0));
    read_or_throw(params.W1, sizeof(params.W1));
    read_or_throw(&params.b1, sizeof(params.b1));
    loaded = true;
}
void Nnue::load(const char* nnue_path) {
    if (loaded) return;

    ifstream nnue_file(nnue_path, ios::binary);
    if (!nnue_file) throw runtime_error("could not open file");

    auto read_or_throw = [&](void* dest, usize bytes) {
        if (!nnue_file.read(reinterpret_cast<char*>(dest), bytes))
            throw runtime_error("failed to read weights");
    };

    read_or_throw(params.W0, sizeof(params.W0));
    read_or_throw(params.b0, sizeof(params.b0));
    read_or_throw(params.W1, sizeof(params.W1));
    read_or_throw(&params.b1, sizeof(params.b1));
    loaded = true;
}



/* ------------------------------- Evaluate ------------------------------- */

i32 Nnue::evaluate(i32 ply, chess::Color color) {
    // lazy update accumulators
    i32 p = ply;
    while (accumulator_states[p].dirty) p--;
    while (p++ < ply) {
        auto& state = accumulator_states[p];
        update_accumulator(
            accumulators[p],
            accumulators[p - 1],
            state.add1[chess::Color::WHITE],
            state.add2[chess::Color::WHITE],
            state.rem1[chess::Color::WHITE],
            state.rem2[chess::Color::WHITE],
            chess::Color::WHITE
        );
        update_accumulator(
            accumulators[p],
            accumulators[p - 1],
            state.add1[chess::Color::BLACK],
            state.add2[chess::Color::BLACK],
            state.rem1[chess::Color::BLACK],
            state.rem2[chess::Color::BLACK],
            chess::Color::BLACK
        );
        state.dirty = false;
    }

    // get address to accumulators and weights
    const auto us_base = accumulators[ply][color];
    const auto them_base = accumulators[ply][~color];
    const auto us_w_base = params.W1;
    const auto them_w_base = params.W1 + N_HIDDEN;

#ifdef USE_SIMD
    constexpr i32 regw = ALIGNMENT / sizeof(i16);
    static_assert(N_HIDDEN % regw == 0);
    constexpr i32 n_chunks = N_HIDDEN / regw;

    VecI32 sum = zeros;
    for (i32 i = 0; i < n_chunks; i++) {
        const VecI16 us = load_i16(&us_base[i * regw]);
        const VecI16 them = load_i16(&them_base[i * regw]);
        const VecI16 us_w = load_i16(&us_w_base[i * regw]);
        const VecI16 them_w = load_i16(&them_w_base[i * regw]);

        const VecI16 us_clamped = clamp_i16(us, zeros, qas);
        const VecI16 them_clamped = clamp_i16(them, zeros, qas);

        const VecI32 us_screlu = madd_i16(mul_i16(us_w, us_clamped), us_clamped);
        const VecI32 them_screlu = madd_i16(mul_i16(them_w, them_clamped), them_clamped);

        sum = add_i32(sum, add_i32(us_screlu, them_screlu));
    }

    const i32 eval = QA * params.b1 + hadd_i32(sum);
    return (eval / QA) * OUTPUT_SCALE / (QA * QB);
#else
    i32 eval = QA * params.b1;

    // compute W1 dot SCReLU(acc)
    for (i32 i = 0; i < N_HIDDEN; i++) {
        const i32 us_clamped = min(max((i32)us_base[i], 0), QA);
        const i32 them_clamped = min(max((i32)them_base[i], 0), QA);

        eval += us_w_base[i] * us_clamped * us_clamped;
        eval += them_w_base[i] * them_clamped * them_clamped;
    }

    return (eval / QA) * OUTPUT_SCALE / (QA * QB);
#endif
}

i16* Nnue::NnueAccumulator::operator[](chess::Color color) { return v[color]; }
const i16* Nnue::NnueAccumulator::operator[](chess::Color color) const { return v[color]; }

void Nnue::refresh_accumulator(
    NnueAccumulator& new_acc, const vector<i32>& features, chess::Color color
) {
#ifdef USE_SIMD
    constexpr i32 regw = ALIGNMENT / sizeof(i16);
    static_assert(N_HIDDEN % regw == 0);
    constexpr i32 n_chunks = N_HIDDEN / regw;
    VecI16 regs[n_chunks];

    // load bias into registers
    for (i32 i = 0; i < n_chunks; i++) regs[i] = load_i16(&params.b0[i * regw]);

    // add active features
    for (i32 f : features)
        for (i32 i = 0; i < n_chunks; i++)
            regs[i] = adds_i16(regs[i], load_i16(&params.W0[f * N_HIDDEN + i * regw]));

    // store result in accumulator
    for (i32 i = 0; i < n_chunks; i++) store_i16(&new_acc[color][i * regw], regs[i]);
#else
    // copy bias
    copy(params.b0, params.b0 + N_HIDDEN, new_acc.v[color]);

    // accumulate columns of active features
    for (i32 f : features)
        for (i32 i = 0; i < N_HIDDEN; i++) new_acc[color][i] += params.W0[f * N_HIDDEN + i];
#endif
}

void Nnue::update_accumulator(
    NnueAccumulator& new_acc,
    const NnueAccumulator& old_acc,
    i32 add1,
    i32 add2,
    i32 rem1,
    i32 rem2,
    chess::Color color
) {
#ifdef USE_SIMD
    constexpr i32 regw = ALIGNMENT / sizeof(i16);
    static_assert(N_HIDDEN % regw == 0);
    constexpr i32 n_chunks = N_HIDDEN / regw;
    VecI16 regs[n_chunks];

    // load previous accumulator values into registers
    for (i32 i = 0; i < n_chunks; i++) regs[i] = load_i16(&old_acc[color][i * regw]);

    // subtract rem_features
    if (rem1 >= 0)
        for (i32 i = 0; i < n_chunks; i++)
            regs[i] = subs_i16(regs[i], load_i16(&params.W0[rem1 * N_HIDDEN + i * regw]));
    if (rem2 >= 0)
        for (i32 i = 0; i < n_chunks; i++)
            regs[i] = subs_i16(regs[i], load_i16(&params.W0[rem2 * N_HIDDEN + i * regw]));

    // add add_features
    if (add1 >= 0)
        for (i32 i = 0; i < n_chunks; i++)
            regs[i] = adds_i16(regs[i], load_i16(&params.W0[add1 * N_HIDDEN + i * regw]));
    if (add2 >= 0)
        for (i32 i = 0; i < n_chunks; i++)
            regs[i] = adds_i16(regs[i], load_i16(&params.W0[add2 * N_HIDDEN + i * regw]));

    // store results in new accumulator
    for (i32 i = 0; i < n_chunks; i++) store_i16(&new_acc[color][i * regw], regs[i]);
#else
    // copy old_acc into new_acc
    copy(old_acc[color], old_acc[color] + N_HIDDEN, new_acc[color]);

    // subtract rem_features
    if (rem1 >= 0)
        for (i32 i = 0; i < N_HIDDEN; i++) new_acc[color][i] -= params.W0[rem1 * N_HIDDEN + i];
    if (rem2 >= 0)
        for (i32 i = 0; i < N_HIDDEN; i++) new_acc[color][i] -= params.W0[rem2 * N_HIDDEN + i];

    // add add_features
    if (add1 >= 0)
        for (i32 i = 0; i < N_HIDDEN; i++) new_acc[color][i] += params.W0[add1 * N_HIDDEN + i];
    if (add2 >= 0)
        for (i32 i = 0; i < N_HIDDEN; i++) new_acc[color][i] += params.W0[add2 * N_HIDDEN + i];
#endif
}



/* ------------------------------- Update ------------------------------- */

void Nnue::set_board(const chess::Board& board) {
    vector<i32> w_features, b_features;
    w_features.reserve(32);
    b_features.reserve(32);

    auto pieces = board.occ();
    while (pieces) {
        const auto sq = static_cast<chess::Square>(pieces.poplsb());

        const i32 wpiece = board.at(sq);       // 0...5, 6...11
        const i32 bpiece = (wpiece + 6) % 12;  // 6...11, 0...5

        w_features.push_back(64 * wpiece + sq);
        b_features.push_back(64 * bpiece + (sq ^ 56));
    }
    refresh_accumulator(accumulators[0], w_features, chess::Color::WHITE);
    refresh_accumulator(accumulators[0], b_features, chess::Color::BLACK);
}

void Nnue::make_move(i32 ply, chess::Move move, const chess::Board& board) {
    assert((ply != 0));

    const auto from_sq = move.from();
    const auto to_sq = move.to();
    const auto move_type = move.type();

    auto& state = accumulator_states[ply];
    state.dirty = true;

    if (move == move.NULL_MOVE) {
        state.add1[false] = -1;
        state.add2[false] = -1;
        state.rem1[false] = -1;
        state.rem2[false] = -1;
        state.add1[true] = -1;
        state.add2[true] = -1;
        state.rem1[true] = -1;
        state.rem2[true] = -1;
        return;
    }

    // update white and black states incrementally
    for (const auto color : {chess::Color::WHITE, chess::Color::BLACK}) {
        // do incremental update
        const bool moving = (board.stm() == color);
        const i32 from_sqi = (color == chess::Color::WHITE) ? from_sq : (from_sq ^ 56);
        const i32 to_sqi = (color == chess::Color::WHITE) ? to_sq : (to_sq ^ 56);
        const auto from_piece = board.at(from_sq);
        const auto to_piece = board.at(to_sq);
        const i32 from_piecei = (color == chess::Color::WHITE) ? from_piece : (from_piece + 6) % 12;
        const i32 to_piecei = (color == chess::Color::WHITE) ? to_piece : (to_piece + 6) % 12;
        assert(from_piece != chess::Piece::NONE);

        // remove moving piece
        state.add1[color] = -1;
        state.add2[color] = -1;
        state.rem1[color] = 64 * from_piecei + from_sqi;
        state.rem2[color] = -1;

        // add moved piece to add_features (handle promotion and enemy castling)
        if (move_type == move.PROMOTION) {
            const i32 promote_piecei = !moving * 6 + move.promotion_type();
            state.add1[color] = 64 * promote_piecei + to_sqi;
        } else if (move_type == move.CASTLING) {
            const i32 new_ksqi = (to_sqi % 8 == 7) ? to_sqi - 1 : to_sqi + 2;
            const i32 new_rsqi = (to_sqi % 8 == 7) ? to_sqi - 2 : to_sqi + 3;
            state.add1[color] = 64 * from_piecei + new_ksqi;
            state.add2[color] = 64 * to_piecei + new_rsqi;
        } else
            state.add1[color] = 64 * from_piecei + to_sqi;

        // add captured piece (castling is treated as rook capture) to rem_features
        if (to_piece != chess::Piece::NONE)
            state.rem2[color] = 64 * to_piecei + to_sqi;
        else if (move_type == move.ENPASSANT)
            state.rem2[color] = 64 * 6 * moving + (to_sqi + 8 - moving * 16);
    }
}
