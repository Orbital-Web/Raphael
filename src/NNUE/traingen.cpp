#include <Raphael/Raphaelv1.8.h>
#include <Raphael/consts.h>

#include <chess.hpp>
#include <climits>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

using std::cout, std::endl, std::flush;
using std::fixed, std::setprecision;
using std::numeric_limits;
using std::ostream, std::ifstream, std::ofstream, std::ios, std::streamsize, std::getline;
using std::pair;
using std::string, std::stoi, std::stoll, std::to_string;
using std::filesystem::exists, std::filesystem::is_directory, std::filesystem::create_directories;

extern const bool UCI = false;



namespace Raphael {
class v1_8_traingen: public v1_8 {
public:
    v1_8_traingen(string name_in): v1_8(name_in) {}

    // Returns the relative eval and bestmove of this position using the get_move logic of the
    // engine
    pair<int, chess::Move> get_eval(chess::Board board) {
        bool halt = false;
        int depth = 1;
        int eval = 0;
        int alpha = -INT_MAX;
        int beta = INT_MAX;
        history.clear();
        itermove = chess::Move::NO_MOVE;
        nodes = 0;
        start_search_timer(board, 0, 0);

        // begin iterative deepening
        while (!halt && depth <= MAX_DEPTH) {
            // max depth override
            if ((searchopt.maxdepth != -1 && depth > searchopt.maxdepth)
                || (searchopt.maxnodes_soft != -1 && nodes >= searchopt.maxnodes_soft))
                break;

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
            if (tt.isMate(eval)) return {MATE_EVAL, itermove};
        }
        return {eval, itermove};
    }

    // Returns the relative static eval of the board
    int static_eval(const chess::Board& board) { return evaluate(board); }
};  // Raphael
}  // namespace Raphael



struct GenArgs {
    // files
    string input_dir;
    string output_dir = "dataset/eval/train";
    int b_start = 1;
    int b_end = -1;

    // eval
    int depth = Raphael::MAX_DEPTH;
    int64_t maxnodes;
    int64_t maxnodes_soft = -1;

    // data options
    bool include_checks = false;
};
ostream& operator<<(ostream& os, const GenArgs& ga) {
    string maxnodes_soft = (ga.maxnodes_soft == -1) ? "infinite" : to_string(ga.maxnodes_soft);
    string b_end = (ga.b_end == -1) ? "" : " to " + to_string(ga.b_end) + ".epd";
    os << "GenArgs:\n"
       << "  input_dir:  " << ga.input_dir << " (from " << ga.b_start << ".epd" << b_end << ")\n"
       << "  output_dir: " << ga.output_dir << "\n"
       << "  max nodes:  " << ga.maxnodes << "\n"
       << "  options:\n"
       << "    max depth:      " << ga.depth << "\n"
       << "    max soft nodes: " << maxnodes_soft << "\n"
       << "    checks:         " << ((ga.include_checks) ? "true" : "false") << "\n"
       << endl;
    return os;
}

/** Prints help message. */
void print_help() {
    cout << "Usage: traingen [OPTIONS] INPUT_DIR OUTPUT_DIR MAX_NODES\n\n"
         << "  Takes in an INPUT_DIR containing files 1.epd, 2.epd, ... with contents of the form\n"
            "  \"fen [wdl]\" in each row to generate the NNUE training data in OUTPUT_DIR, using\n"
            "  the evals at the specified MAX_NODES\n\n"
         << "Options:\n"
         << "  -d DEPTH Max depths to search. Defaults to " << Raphael::MAX_DEPTH << "\n"
         << "  -n N     Max number of soft nodes (depth at N nodes) to search. Defaults to -1 "
         << "(infinite)\n"
         << "  -b BS BE Start and end (inclusive) epd index to generate for. Defaults to 1, -1\n"
         << "           I.e., go through the entire INPUT_DIR\n"
         << "  -c       Include checks in data. Defaults to false\n"
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
        if (s == "-d")
            args.depth = stoi(argv[++i]);
        else if (s == "-n")
            args.maxnodes_soft = stoll(argv[++i]);
        else if (s == "-b") {
            args.b_start = stoi(argv[++i]);
            args.b_end = stoi(argv[++i]);
        } else if (s == "-c")
            args.include_checks = true;
        else if (s == "-h") {
            print_help();
            exit(0);
        } else {  // positional arguments
            if (p == 0)
                args.input_dir = string(argv[i]);
            else if (p == 1)
                args.output_dir = string(argv[i]);
            else if (p == 2)
                args.maxnodes = stoll(argv[i]);
            else {
                print_help();
                exit(1);
            }
            p++;
        }
    }

    // check input (no. positional args, value is positive, etc.)
    if (p < 3) {
        print_help();
        exit(1);
    }
    if (args.maxnodes <= 0) {
        cout << "error: maxnodes must be positive" << endl;
        exit(1);
    }
    if (args.maxnodes_soft != -1 and args.maxnodes_soft <= 0) {
        cout << "error: maxnodes_soft must be positive (unless it is -1)" << endl;
        exit(1);
    }
    if (args.depth <= 0 || args.depth > Raphael::MAX_DEPTH) {
        cout << "error: depth must be positive and under " << Raphael::MAX_DEPTH << endl;
        exit(1);
    }
    if (args.b_start <= 0 || (args.b_end != -1 && args.b_end <= 0)) {
        cout << "error: BS and BE must be positive (unless BE is -1)" << endl;
        exit(1);
    }

    return args;
}



enum Flags {
    NONE = 0,
    CHECK = 1,
    CAPTURE = 2,
    PROMOTION = 4,
};

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
    int flag = Flags::NONE;

    // exclude checks if requested
    chess::Board board(fen);
    if (board.inCheck()) {
        flag |= Flags::CHECK;
        if (!args.include_checks) return false;
    }

    // eval and skip mate
    auto [eval, bestmove] = engine.get_eval(board);
    if (eval == Raphael::MATE_EVAL) return false;

    // record
    assert(bestmove != chess::Move::NO_MOVE);
    auto bestmv = chess::uci::moveToUci(bestmove);
    if (board.isCapture(bestmove)) flag |= Flags::CAPTURE;
    if (bestmove.typeOf() == chess::Move::PROMOTION) flag |= Flags::PROMOTION;

    outfile << fen << "," << wdl << "," << eval << "," << flag << "," << bestmv << "\n";
    return true;
}



int main(int argc, char* argv[]) {
    GenArgs args = parse_args(argc, argv);
    cout << "Generating traindata with " << args;

    // check input directory
    if (!is_directory(args.input_dir)) {
        cout << "error: " << args.input_dir << " is not a directory or does not exist" << endl;
        exit(1);
    }

    // check and create output directory
    if (is_directory(args.output_dir))
        cout << "warning: " << args.output_dir << " already exists" << endl;
    else {
        cout << "Creating output directory " << args.output_dir << endl;
        create_directories(args.output_dir);
    }

    // load engine
    cout << "Loading engine" << endl;
    Raphael::v1_8_traingen engine("Raphael");
    Raphael::v1_8::SearchOptions searchopt;
    searchopt.maxdepth = args.depth;
    searchopt.maxnodes = args.maxnodes;
    searchopt.maxnodes_soft = args.maxnodes_soft;
    engine.set_searchoptions(searchopt);

    // traverse input dir
    int i = args.b_start;
    while (args.b_end == -1 || i <= args.b_end) {
        // read input file
        string in_path = args.input_dir + "/" + to_string(i) + ".epd";
        ifstream infile(in_path);
        if (!infile.is_open()) break;
        cout << "Processing " << in_path << endl;

        // create output file
        string out_path = args.output_dir + "/" + to_string(i) + ".csv";
        bool write_header = !exists(out_path);
        ofstream outfile(out_path, ios::app);
        if (write_header) outfile << "fen,wdl,eval,flag,bm\n";

        // generate
        int n_added = 0;
        int n_total = 0;
        int iline = args.b_start;
        string line;
        while (getline(infile, line)) {
            n_added += (int)generate_one(outfile, line, engine, args);
            n_total++;
            float percent = n_added * 100 / n_total;

            // print progress bar
            cout << "\rline " << iline << "    added: " << n_added << "/" << n_total << " ("
                 << fixed << setprecision(2) << percent << "%)" << flush;
            iline++;

            // flush the output buffer ocassionally so progress isn't lost on program interrupt
            if (iline % 64 == 0) outfile.flush();
        }
        cout << endl;
        i += 1;
    }
    cout << "All done" << endl;
}
