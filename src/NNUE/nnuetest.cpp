#include <Raphael/nnue.h>

#include <chess.hpp>
#include <filesystem>
#include <iostream>
#include <string>

using std::cin, std::cout, std::endl, std::flush;
using std::getline;
using std::string;
using std::filesystem::directory_iterator, std::filesystem::exists;


/** Prints help message. */
void print_help() {
    cout << "Usage: nnuetest [OPTIONS]\n\n"
         << "  Outputs nnue evaluation for each input fen\n\n"
         << "Options:\n"
         << "  PATH  NNUE file. Defaults to best.nnue from last train output\n"
         << "  -h    Show this message and exit" << endl;
}

/** Parse command line arguments and returns the NNUE file.
 *
 * \param argc number of command line arguments
 * \param argv vector of cstrings with the argument text
 * \returns the NNUE file path
 */
string parse_args(int argc, char* argv[]) {
    string nnue_file = "";

    if (argc > 2) {
        // should either be 1 or 2 arguments
        print_help();
        exit(1);

    } else if (argc == 2) {
        // check if argument is -h command or nnue file
        string s(argv[1]);
        if (s == "-h") {
            print_help();
            exit(0);
        } else
            nnue_file = s;

    } else {
        // find default nnue file
        for (const auto& entry : directory_iterator(".")) {
            if (entry.is_directory()) {
                auto file = entry.path() / "best.nnue";
                if (!exists(file)) continue;
                if (file.string() > nnue_file) nnue_file = file.string();
            }
        }

        if (nnue_file.empty()) {
            cout << "error: could not find nnue file in any train folder" << endl;
            exit(1);
        }
    }

    return nnue_file;
}



int main(int argc, char* argv[]) {
    string nnue_file = parse_args(argc, argv);

    // load nnue
    cout << "Loading nnue file " << nnue_file << endl;
    Raphael::Nnue net(nnue_file);

    // continuously listen for input
    string command;
    chess::Board board;

    cout << "Enter a FEN to evaluate or 'q' to quit:" << endl;
    while (getline(cin, command)) {
        if (command == "q") break;
        board.setFen(command);
        net.set_board(board);
        cout << "eval: " << flush;
        int eval = net.evaluate(0, board.sideToMove() == chess::Color::WHITE);
        cout << eval << endl;
    }
}
