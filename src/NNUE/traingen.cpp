#define MUTEEVAL

#include <Raphael/Raphael_v1.8.hpp>
#include <chess.hpp>
#include <cstdint>
#include <iostream>
#include <string>

using std::cout, std::endl;
using std::ostream;
using std::string, std::stoi, std::stoll, std::to_string;



struct GenArgs {
    // files
    string input_file;
    string output_file = "traindata.csv";
    int start_line = 0;

    // eval
    int depth;
    int64_t maxnodes = -1;

    // data options
    int threshold_eval = 300;        // threshold of abs(eval - static_eval) to skip data
    int threshold_quiescence = 100;  // threshold of abs(quiescence_eval - static_eval) to skip data
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
       << "    tquiescence: " << ga.threshold_quiescence << "\n"
       << "    checks:      " << ((ga.include_checks) ? "true" : "false") << endl;
    return os;
}

/** Prints help message. */
void print_help() {
    cout
        << "Usage: traingen [OPTIONS] INPUT_FILE DEPTH\n\n"
        << "  Takes in a INPUT_FILE with rows \"fen [wdl]\" and generates the NNUE traindata of "
           "the using the evals at the specified DEPTH\n\n"
        << "Options:\n"
        << "  -o PATH  Output file. Defaults to traindata.csv\n"
        << "  -s LINE  Line of INPUT_FILE to read from. Defaults to 0\n"
        << "  -n N     Max number of nodes to search before moving on. Defaults to -1 (infinite)\n"
        << "  -e TE    Will drop data if abs(eval - static_eval) > TE. Defaults to 300\n"
        << "  -q TQ    Will drop data if abs(quiescence_eval - static_eval) > TQ. Defaults to 100\n"
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
        else if (s == "-q")
            args.threshold_quiescence = stoi(argv[++i]);
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
        cout << "error: depth and maxnodes should be positive (unless it is -1)" << endl;
        exit(1);
    }
    if (args.start_line < 0) {
        cout << "error: starting line number cannot be negative" << endl;
        exit(1);
    }
    if (args.threshold_eval < 0 || args.threshold_quiescence < 0) {
        cout << "error: threshold cannot be negative" << endl;
        exit(1);
    }

    return args;
}



int main(int argc, char* argv[]) {
    GenArgs args = parse_args(argc, argv);
    cout << "Generating traindata with " << args;
}
