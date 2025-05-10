#include <Raphael/consts.h>

#include <Raphael/Raphael_v1.8.hpp>
#include <chess.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

using std::cout, std::endl, std::flush, std::fixed, std::setprecision;
using std::numeric_limits;
using std::ostream, std::ifstream, std::ofstream, std::ios, std::streamsize, std::getline;
using std::string, std::stoi, std::stoll, std::to_string;

extern const bool UCI = false;



namespace Raphael {
class v1_8_traingen: public v1_8 {
public:
    v1_8_traingen(string name_in): v1_8(name_in) {}

    // Returns the relative eval of this position using the get_move logic of the engine
    int get_eval(chess::Board board) {
        bool halt = false;
        int depth = 1;
        int eval = 0;
        int alpha = -INT_MAX;
        int beta = INT_MAX;
        history.clear();
        itermove = chess::Move::NO_MOVE;
        nodes = 0;
        startSearchTimer(board, 0, 0);

        // begin iterative deepening
        while (!halt && depth <= MAX_DEPTH) {
            // max depth override
            if (searchopt.maxdepth != -1 && depth > searchopt.maxdepth) break;

            int itereval = negamax(board, depth, 0, params.MAX_EXTENSIONS, alpha, beta, halt);

            // not timeout
            if (!halt) {
                eval = itereval;

                // re-search required
                if ((eval <= alpha) || (eval >= beta)) {
                    alpha = -INT_MAX;
                    beta = INT_MAX;
                    continue;
                }

                // narrow window
                alpha = eval - params.ASPIRATION_WINDOW;
                beta = eval + params.ASPIRATION_WINDOW;
                depth++;
            }

            // checkmate, no need to continue
            if (tt.isMate(eval)) return MATE_EVAL;
        }
        return eval;
    }

    // Returns the relative static eval of the board
    int static_eval(const chess::Board& board) { return evaluate(board); }

    // Returns the relative eval of the board after quiescence
    int quiescence_eval(chess::Board board) {
        bool halt = false;
        history.clear();
        nodes = 0;
        startSearchTimer(board, 0, 0);

        return quiescence(board, -INT_MAX, INT_MAX, halt);
    }
};  // Raphael
}  // namespace Raphael



struct GenArgs {
    // files
    string input_file;
    string output_file = "traindata.csv";
    int start_line = 1;

    // eval
    int depth;
    int64_t maxnodes = -1;

    // data options
    int threshold_eval = 300;  // threshold of abs(eval - static_eval) to skip data
    bool include_checks = false;
};
ostream& operator<<(ostream& os, const GenArgs& ga) {
    string maxnodes = (ga.maxnodes == -1) ? "infinite" : to_string(ga.maxnodes);
    os << "GenArgs:\n"
       << "  input:  " << ga.input_file << " (from line " << ga.start_line << ")\n"
       << "  output: " << ga.output_file << "\n"
       << "  options:\n"
       << "    depth:       " << ga.depth << "\n"
       << "    max nodes:   " << maxnodes << "\n"
       << "    teval:       " << ga.threshold_eval << "\n"
       << "    checks:      " << ((ga.include_checks) ? "true" : "false") << endl;
    return os;
}

/** Prints help message. */
void print_help() {
    cout << "Usage: traingen [OPTIONS] INPUT_FILE DEPTH\n\n"
         << "  Takes in a INPUT_FILE with rows \"fen [wdl]\" and generates the NNUE traindata of "
            "the using the evals at the specified DEPTH\n\n"
         << "Options:\n"
         << "  -o PATH  Output file. Defaults to traindata.csv\n"
         << "  -s LINE  Line of INPUT_FILE to read from. Defaults to 1\n"
         << "  -n N     Max number of nodes to search before moving on. Defaults to -1 (infinite)\n"
         << "  -e TE    Will drop data if abs(eval - static_eval) > TE. Defaults to 300\n"
         << "  -c       Include checks in data\n"
         << "  -h       Show this message and exit" << endl;
}

/** Parse command line arguments. Does not error check.
 *
 * \param argc number of command line arguments
 * \param argv vector of cstrings with the argument text
 * \returns the corresponding GenArgs struct
 */
GenArgs parse_args(int argc, char* argv[]) {
    GenArgs args;

    int i = 0;
    int p = 0;
    while (++i < argc) {
        string s(argv[i]);
        if (s == "-o")
            args.output_file = string(argv[++i]);
        else if (s == "-s")
            args.start_line = stoi(argv[++i]);
        else if (s == "-d")
            args.depth = stoi(argv[++i]);
        else if (s == "-n")
            args.maxnodes = stoll(argv[++i]);
        else if (s == "-e")
            args.threshold_eval = stoi(argv[++i]);
        else if (s == "-c")
            args.include_checks = true;
        else if (s == "-h") {
            print_help();
            exit(0);
        } else {  // positional arguments
            if (p == 0)
                args.input_file = string(argv[i]);
            else if (p == 1)
                args.depth = stoi(argv[i]);
            else {
                print_help();
                exit(1);
            }
            p++;
        }
    }

    // check input (no. positional args, value is positive, etc.)
    if (p < 2) {
        print_help();
        exit(1);
    }
    if (args.depth <= 0 || (args.maxnodes != -1 && args.maxnodes <= 0)) {
        cout << "error: depth and maxnodes must be positive (unless it is -1)" << endl;
        exit(1);
    }
    if (args.start_line < 1) {
        cout << "error: starting line number must be positive" << endl;
        exit(1);
    }
    if (args.threshold_eval < 0) {
        cout << "error: threshold cannot be negative" << endl;
        exit(1);
    }

    return args;
}



/** Generate a single row of traindata and appends to outfile
 *
 * \param outfile file to write row to
 * \param line input line in the format "fen [wdl]"
 * \param engine engine to compute eval with
 * \param args arguments for the traindata generation
 * \returns whether the data was added to outfile
 */
bool generate_one(
    ofstream& outfile, string line, Raphael::v1_8_traingen& engine, const GenArgs& args
) {
    // read line
    size_t split = line.find("[");
    string fen = line.substr(0, split - 1);
    string wdl = line.substr(split + 1, line.find("]") - split - 1);

    // TODO: use the quiescene position instead as that is what will actually get used in the engine
    // make sure to add a -q flag and add documentation to nnue.md

    // exclude checks if requested and get eval
    chess::Board board(fen);
    if (!args.include_checks && board.inCheck()) return false;
    int eval = engine.get_eval(board);

    // exclude mate and those outside the threshold
    if (eval == Raphael::MATE_EVAL || abs(eval - engine.static_eval(board)) > args.threshold_eval)
        return false;

    outfile << fen << "," << wdl << "," << eval << "\n";
    return true;
}



int main(int argc, char* argv[]) {
    GenArgs args = parse_args(argc, argv);
    cout << "Generating traindata with " << args;

    // open input file and skip to args.start_line
    ifstream infile(args.input_file);
    if (!infile.is_open()) {
        cout << "error: failed to open " << args.input_file << endl;
        exit(1);
    }
    for (int i = 0; i < args.start_line - 1; i++) {
        if (!infile.ignore(numeric_limits<streamsize>::max(), '\n')) {
            cout << "error: cannot skip line " << i + 1 << ". End of file" << endl;
            exit(1);
        }
    }

    // open output file in append mode and write header if the file is new
    bool write_header = !std::filesystem::exists(args.output_file);
    ofstream outfile(args.output_file, ios::app);
    if (write_header)
        outfile << "fen,wdl,eval\n";
    else
        cout << "info: " << args.output_file << " already exists. Appending to it" << endl;

    // load engine
    cout << "info: loading engine" << endl;
    Raphael::v1_8_traingen engine("Raphael");
    Raphael::v1_8::SearchOptions searchopt;
    searchopt.maxdepth = args.depth;
    searchopt.maxnodes = args.maxnodes;
    engine.set_searchoptions(searchopt);

    // read input file line by line, get traindata, and add to the output file
    cout << "info: beginning training data generation" << endl;
    int n_added = 0;
    int n_total = 0;
    int iline = args.start_line;
    string line;
    while (getline(infile, line)) {
        n_added += (int)generate_one(outfile, line, engine, args);
        n_total++;
        float percent = n_added * 100 / n_total;

        // print progress bar
        cout << "\rline " << iline << "    added: " << n_added << "/" << n_total << " (" << fixed
             << setprecision(2) << percent << "%)" << flush;
        iline++;

        // flush the output buffer ocassionally so progress isn't lost on program interrupt
        if (iline % 64 == 0) outfile.flush();
    }
    cout << endl;
}
