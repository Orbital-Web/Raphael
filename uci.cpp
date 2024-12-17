#define UCI
#include <GameEngine/consts.h>

#include <Raphael/Raphael_v2.0.hpp>
#include <atomic>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using std::cin;
using std::cout;
using std::flush;
using std::ref;
using std::stoi;
using std::stoll;
using std::string;
using std::stringstream;
using std::thread;
using std::vector;



// global vars
const char version[8] = "1.7.6.3";
chess::Board board;
Raphael::v2_0 engine("Raphael");
bool halt = false;
bool quit = false;
mutex engine_mutex;



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

        uint32_t tablesize = (tablesize_mb * 1024U * 1024) / Raphael::TranspositionTable::entrysize;
        {
            lock_guard<mutex> engine_lock(engine_mutex);
            engine.set_options({.tablesize = tablesize});
        }

        lock_guard<mutex> lock(cout_mutex);
        cout << "Set table size to " << tablesize_mb << "mb (" << tablesize << " entries)\n"
             << flush;
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
    lock_guard<mutex> engine_lock(engine_mutex);
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
            searchopt.maxnodes = stoll(tokens[i + 1]);
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
    lock_guard<mutex> engine_lock(engine_mutex);
    engine.set_searchoptions(searchopt);
    halt = false;
    sf::Event nullevent;
    thread(&Raphael::v2_0::get_move, engine, board, t_remain, t_inc, ref(nullevent), ref(halt))
        .detach();
}


int main() {
    std::ios::sync_with_stdio(false);
    string uci_command;
    Raphael::v2_0::EngineOptions engine_opt;

    while (!quit) {
        getline(cin, uci_command);

        if (uci_command == "uci") {
            lock_guard<mutex> lock(cout_mutex);
            cout << "id name Raphael " << version << "\n"
                 << "id author Rei Meguro\n"
                 << "option name Hash type spin default 192 min 1 max 2560\n"
                 << "uciok\n"
                 << flush;

        } else if (uci_command == "isready") {
            lock_guard<mutex> lock(cout_mutex);
            cout << "readyok\n" << flush;

        } else if (uci_command == "stop")
            halt = true;

        else if (uci_command == "quit") {
            halt = true;
            quit = true;

        } else if (uci_command == "ucinewgame") {
            halt = true;
            lock_guard<mutex> engine_lock(engine_mutex);
            engine.reset();

        } else {
            // tokenize command
            vector<string> tokens;
            stringstream ss(uci_command);
            string token;
            while (getline(ss, token, ' ')) tokens.push_back(token);
            if (tokens.empty()) continue;

            string& keyword = tokens[0];

            if (keyword == "setoption") {
                halt = true;
                setoption(tokens);

            } else if (keyword == "position") {
                halt = true;
                setposition(tokens);

            } else if (keyword == "go") {
                halt = true;
                search(tokens);
            }
        }
    }

    return 0;
}
