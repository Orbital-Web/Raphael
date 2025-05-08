#include <Raphael/nnue.h>

#include <chess.hpp>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using std::cin, std::cout, std::endl, std::flush;
using std::getline;
using std::string;
using std::vector;
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



/** Tests Nnue::make_move works correctly and is consistent with Nnue::set_board (assuming the
 * latter is correct, which can be verified with the python nnuetest script)
 *
 * \param net the pointer to a NNUE model to test with
 */
void test_make_move(Raphael::Nnue& net, chess::Board& board) {
    cout << "Testing make_move..." << endl;

    // test suite
    vector<vector<string>> test_suite = {
        // non-capture, non-king moves
        {chess::STARTPOS, "e2e4", "d7d5", "g1f3", "c8g4", "h1g1", "d8d6"},
        // TODO:
        // non-refreshing king moves
        // refreshing king moves
        // king-side castling
        // queen-side castling
        // enpassant
        // promotion
    };

    for (vector<string>& test_moves : test_suite) {
        for (int ply = 0; ply < (int)test_moves.size(); ply++) {
            string& movestr = test_moves[ply];

            // first movestr is the starting position
            if (ply == 0) {
                board.setFen(movestr);
                continue;
            }

            // check consistency with net updated with make_move and set_board
            chess::Move move = chess::uci::uciToMove(board, movestr);
            net.make_move(ply, move, board);
            auto evalw = net.evaluate(ply, true);
            auto evalb = net.evaluate(ply, false);

            board.makeMove(move);
            net.set_board(board);
            auto true_evalw = net.evaluate(0, true);
            auto true_evalb = net.evaluate(0, false);

            if (evalw != true_evalw || evalb != true_evalb) {
                cout << "  Fail: eval after make_move not consistent with eval after set_board "
                     << "([" << evalw << ", " << evalb << "] != [" << true_evalw << ", "
                     << true_evalb << "]) after moves:\n ";
                for (int p = 0; p <= ply; p++) cout << " " << test_moves[p];
                cout << endl;
                exit(1);
            }
        }
    }
    cout << "  Passed\n" << endl;
}



int main(int argc, char* argv[]) {
    string nnue_file = parse_args(argc, argv);

    // load nnue
    cout << "Loading nnue file " << nnue_file << endl;
    Raphael::Nnue net(nnue_file);

    // continuously listen for input
    string command;
    chess::Board board;

    cout << "Commands:\n"
         << "  FEN: get eval\n"
         << "  tm: test make_move\n"
         << "  q: quit\n\n"
         << "Start typing commands:" << endl;

    while (getline(cin, command)) {
        if (command == "q")
            break;
        else if (command == "tm")
            test_make_move(net, board);
        else {
            board.setFen(command);
            net.set_board(board);
            cout << "eval: " << flush;
            int eval = net.evaluate(0, board.sideToMove() == chess::Color::WHITE);
            cout << eval << endl;
        }
    }
}
