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
    const vector<vector<string>> test_suite = {
        // non-king moves
        {chess::STARTPOS, "e2e4", "d7d5", "g1f3", "c8g4", "h1g1", "d8d6"},
        // non-king captures
        {"4kr2/8/8/6nq/4B3/5R2/4P3/Q3K3 b - - 0 1", "f8f3", "e4f3", "g5f3", "e2f3", "h5f3"},
        {"4k2q/3rp1n1/5b2/8/4N3/4P1R1/3Q4/4K3 w - - 0 1", "g3g7", "d7d2", "e4f6", "e7f6"},
        // non-refreshing king moves
        {"Q3K3/8/2N5/8/8/2q3n1/8/4k3 b - - 0 1", "e1d1", "e8f7", "d1c2", "f7f8"},
        // non-refreshing king captures
        {"3bK3/3r4/6p1/8/8/1P6/4BN2/4k3 w - - 0 1", "e8d7", "e1e2", "d7d8", "e2f2"},
        // refreshing king moves
        {"8/k7/3p4/p2P1p2/P2P1P2/8/8/K7 w - - 0 1", "a1b1", "a7b7", "b1c1", "b7c7"},
        {"8/3k2r1/8/8/8/2K2P2/5B2/8 b - - 0 1", "d7e7", "c3c4", "e7e6", "c4d4"},
        // refreshing king captures
        {"3k4/3B2r1/8/8/8/3n1P2/3K1B2/8 w - - 0 1", "d2d3", "d8d7"},
        // castling
        {"r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", "e1g1", "e8c8"},
        {"r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1", "e8g8", "e1c1"},
        // enpassant
        {"4kr2/8/8/4pP2/2p5/8/3P4/1R1K4 w - e6 0 1", "f5e6", "f8h8", "d2d4", "c4d3"},
        // promotion
        {"8/2k1PPP1/8/8/8/8/3K1pp1/8 w - - 0 1", "g7g8b", "g2g1n", "f7f8n", "f2f1q", "e7e8r"},
        // promotion capture
        {"4bnb1/2k1PPP1/8/8/8/8/3K1pp1/5BN1 b - - 0 1", "f2g1n", "f7g8q", "g2f1r", "g7f8b"},
    };
    const vector<string> test_pos = {
        "2r1n3/1N1NpPk1/4p1P1/p1pP1p1P/1rQ3p1/1b6/2B5/R3K2R w KQ c6 0 1",
    };

    // test lines in test suite
    for (const vector<string>& test_moves : test_suite) {
        for (int ply = 0; ply < (int)test_moves.size(); ply++) {
            const string& movestr = test_moves[ply];

            // first movestr is the starting position
            if (ply == 0) {
                board.setFen(movestr);
                net.set_board(board);
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

    // test all possible moves from test pos
    for (const string& pos : test_pos) {
        board.setFen(pos);
        net.set_board(board);
        chess::Movelist movelist;
        chess::movegen::legalmoves<chess::MoveGenType::ALL>(movelist, board);

        for (auto& move : movelist) {
            net.make_move(1, move, board);
            auto evalw = net.evaluate(1, true);
            auto evalb = net.evaluate(1, false);

            board.makeMove(move);
            net.set_board(board);
            auto true_evalw = net.evaluate(0, true);
            auto true_evalb = net.evaluate(0, false);

            if (evalw != true_evalw || evalb != true_evalb) {
                cout << "  Fail: eval after make_move not consistent with eval after set_board "
                     << "([" << evalw << ", " << evalb << "] != [" << true_evalw << ", "
                     << true_evalb << "]) after moves:\n  " << pos << " "
                     << chess::uci::moveToUci(move) << endl;
                exit(1);
            }

            board.setFen(pos);
            net.set_board(board);
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
