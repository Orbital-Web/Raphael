#define UCI
#include <Raphael/Raphael_v2.0.hpp>
#include <atomic>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using std::cin;
using std::cout;
using std::lock_guard;
using std::mutex;
using std::stoi;
using std::string;
using std::stringstream;
using std::thread;
using std::vector;



// global vars
const char version[4] = "1.7";
chess::Board board;
Raphael::v2_0 engine("Raphael");
bool halt = false;
bool quit = false;

mutex cout_mutex;



/** Sets options such as tt size
 * E.g., setoption name Hash value [size(mb)]
 *
 * \param tokens list of tokens for the command
 */
void setoption(const vector<string>& tokens) {
    if (tokens.size() != 5) return;

    if (tokens[2] == "Hash") {
        int tablesize_mb = stoi(tokens[4]);
        if (tablesize_mb < 1 || tablesize_mb > 2560) return;

        uint32_t tablesize
            = (tablesize_mb * 1024U * 1024) / sizeof(Raphael::TranspositionTable::Entry);
        engine.set_options({.tablesize = tablesize});

        lock_guard<mutex> lock(cout_mutex);
        cout << "Set table size to " << tablesize_mb << "mb (" << tablesize << " entries)\n";
    }
}


/** Sets up internal board using fen string and list of ucimoves
 * E.g., position [startpos|fen {fen}] (moves {move1} {move2} ...)
 *
 * \param tokens list of tokens for the command
 */
void setposition(const vector<string>& tokens) {
    int ntokens = tokens.size();
    if (ntokens < 2) return;

    // starting position
    int i = 2;
    if (tokens[1] == "startpos")
        board.setFen(chess::STARTPOS);

    else if (tokens[1] == "fen") {
        string fen = tokens[2];
        i = 3;
        while (i < ntokens) {
            if (tokens[i] == "moves") break;
            fen += " " + tokens[i];
            i++;
        }
        board.setFen(fen);
    }

    // play moves
    while (++i < ntokens) board.makeMove(chess::uci::uciToMove(board, tokens[i]));
}


/** Handles the go command
 * E.g., go wtime {wtime} btime {btime}
 *
 * \param tokens list of tokens for the command
 */
void search(const vector<string>& tokens) {
    // get arguments
    int ntokens = tokens.size();
    int i = 1;
    int t_remain = 0, t_inc = 0;
    Raphael::v2_0::SearchOptions searchopt;

    while (i < ntokens) {
        if (tokens[i] == "depth") {
            searchopt.maxdepth = stoi(tokens[i + 1]);
            break;
        } else if (tokens[i] == "nodes") {
            searchopt.maxnodes = stoi(tokens[i + 1]);
            break;
        } else if (tokens[i] == "movetime") {
            searchopt.movetime = stoi(tokens[i + 1]);
            break;
        } else if (tokens[i] == "infinite") {
            searchopt.infinite = true;
            break;
        } else if ((whiteturn && tokens[i] == "wtime") || (!whiteturn && tokens[i] == "btime"))
            t_remain = stoi(tokens[i + 1]);
        else if ((whiteturn && tokens[i] == "winc") || (!whiteturn && tokens[i] == "binc"))
            t_inc = stoi(tokens[i + 1]);
        i += 2;
    }

    // search best move
    halt = false;
    engine.set_searchoptions(searchopt);
    sf::Event nullevent;
    engine.get_move(board, t_remain, t_inc, std::ref(nullevent), std::ref(halt));
}


void process_command(const string& uci_command) {
    // tokenize command
    vector<std::string> tokens;
    stringstream ss(uci_command);
    string token;
    while (getline(ss, token, ' ')) tokens.push_back(token);
    if (tokens.empty()) return;

    string& keyword = tokens[0];

    if (keyword == "setoption")
        return setoption(tokens);
    else if (keyword == "position")
        return setposition(tokens);
    else if (keyword == "go")
        return search(tokens);
}



int main() {
    string uci_command;
    Raphael::v2_0::EngineOptions engine_opt;

    while (!quit) {
        getline(std::cin, uci_command);

        if (uci_command == "uci") {
            lock_guard<mutex> lock(cout_mutex);
            cout << "id name Raphael " << version << "\n"
                 << "id author Rei Meguro\n"
                 << "option name Hash type spin default 192 min 1 max 2560\n"
                 << "uciok\n";

        } else if (uci_command == "isready") {
            lock_guard<mutex> lock(cout_mutex);
            cout << "readyok\n";

        } else if (uci_command == "ucinewgame")
            engine.reset();

        else if (uci_command == "stop")
            halt = true;

        else if (uci_command == "quit") {
            halt = true;
            quit = true;

        } else
            thread(process_command, uci_command).detach();
    }

    return 0;
}
