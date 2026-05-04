#include <Raphael/nnue.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

using namespace raphael;
using std::cout;
using std::flush;
using std::ifstream;
using std::make_unique;
using std::ofstream;
using std::runtime_error;
using std::string;
using std::unique_ptr;



class NnuePreprocessor {
public:
    NnuePreprocessor(const string& filename)
        : filename(filename),
          original_params(make_unique<Nnue::NnueParams>()),
          processed_params(make_unique<Nnue::NnueParams>()) {
        load_network();
    };

    void permute_network() {
        const auto current_perm = original_params->permutation;
        const auto current_idx = static_cast<u8>(current_perm);
        if (current_idx > 2) throw runtime_error("invalid network permutation");
        if (current_idx == target_idx) return;

        // create inverse perm
        u8 inv_perms[8];
        for (usize i = 0; i < 8; i++) inv_perms[PERMS[current_idx][i]] = i;

        // create composed perms
        u8 perms[8];
        for (usize i = 0; i < 8; i++) perms[i] = PERMS[target_idx][inv_perms[i]];

        apply_perm(perms);
        processed_params->permutation = static_cast<Nnue::NnuePerm>(target_idx);

        write_network();
    }

private:
    string filename;
    unique_ptr<Nnue::NnueParams> original_params;
    unique_ptr<Nnue::NnueParams> processed_params;

    static constexpr u8 PERMS[3][8] = {
        {0, 1, 2, 3, 4, 5, 6, 7}, // generic
        {0, 2, 1, 3, 4, 6, 5, 7}, // avx2
        {0, 2, 4, 6, 1, 3, 5, 7}, // avx512
    };

#ifdef USE_AVX512
    static constexpr u8 target_idx = 2;
#elif defined(USE_AVX2)
    static constexpr u8 target_idx = 1;
#else
    static constexpr u8 target_idx = 0;
#endif


    void load_network() {
        ifstream file(filename, std::ios::binary);
        if (!file.is_open()) throw runtime_error("could not open " + filename);

        file.seekg(0, std::ios::end);
        const auto netfile_size = file.tellg();
        constexpr usize padded_size = 64 * ((sizeof(Nnue::NnueParams) + 63) / 64);
        if (netfile_size != padded_size)
            throw runtime_error("network file and architecture doesn't match");
        file.seekg(0, std::ios::beg);

        file.read(reinterpret_cast<char*>(original_params.get()), sizeof(Nnue::NnueParams));
        file.close();

        *processed_params = *original_params;
    }

    void write_network() {
        ofstream file(filename, std::ios::binary);
        if (!file.is_open()) throw runtime_error("could not open " + filename);

        file.write(reinterpret_cast<char*>(processed_params.get()), sizeof(Nnue::NnueParams));
        file.close();
    }


    void apply_perm(const u8 perm[8]) {
        // permute W0 to cancel out packus
        for (usize b = 0; b < Nnue::N_INBUCKETS; b++) {
            for (usize i = 0; i < Nnue::N_INPUTS; i++) {
                for (usize j = 0; j < Nnue::L1_SIZE; j += 64) {
                    const auto src = &original_params->W0[b][i][j];
                    auto dst = &processed_params->W0[b][i][j];

                    for (usize jj = 0; jj < 64; jj++) {
                        const auto src_chunk = jj / 8;
                        const auto dst_chunk = perm[src_chunk];
                        const auto offset = jj % 8;
                        dst[8 * dst_chunk + offset] = src[jj];
                    }
                }
            }
        }

        // permute b0 to cancel out packus
        for (usize j = 0; j < Nnue::L1_SIZE; j += 64) {
            const auto src = &original_params->b0[j];
            auto dst = &processed_params->b0[j];

            for (usize jj = 0; jj < 64; jj++) {
                const auto src_chunk = jj / 8;
                const auto dst_chunk = perm[src_chunk];
                const auto offset = jj % 8;
                dst[8 * dst_chunk + offset] = src[jj];
            }
        }
    }
};


int main(int argc, char** argv) {
    if (argc < 2) {
        cout << "usage: " << argv[0] << " <network_file>\n" << flush;
        return 1;
    }

    NnuePreprocessor pre(argv[1]);
    pre.permute_network();

    return 0;
}
