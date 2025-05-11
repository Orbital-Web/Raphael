#include <GameEngine/consts.h>
#include <Raphael/Raphaelv1.8.h>

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using std::cin, std::cout, std::flush;
using std::string, std::stoi, std::stoll;
using std::stringstream;
using std::thread, std::mutex, std::unique_lock, std::lock_guard, std::condition_variable;
using std::vector;

extern const bool UCI = true;



// engine
mutex engine_mutex;
Raphael::v1_8 engine("Raphael 1.8.1.0");  // TODO: make sure version is correct

// search globals
mutex search_mutex;
condition_variable search_cv;
struct SearchRequest {
    chess::Board board;
    Raphael::v1_8::SearchOptions options;
    int t_remain;
    int t_inc;
    bool go = false;
};
SearchRequest pending_request;

// other globals
bool halt = false;
bool quit = false;



/** Thread function for handling search */
void handle_search() {
    while (true) {
        // wait until a search request is made
        unique_lock<mutex> search_lock(search_mutex);
        search_cv.wait(search_lock, [] { return quit || pending_request.go; });
        if (quit) break;

        // get search arguments
        SearchRequest request = pending_request;  // creates a copy, safe to unlock
        pending_request.go = false;
        search_lock.unlock();

        // do search (ignore return value)
        {
            lock_guard<mutex> engine_lock(engine_mutex);
            sf::Event nullevent;
            halt = false;
            engine.set_searchoptions(request.options);
            engine.get_move(request.board, request.t_remain, request.t_inc, nullevent, halt);
        }
    }
}



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

    // set position/fen
    lock_guard<mutex> search_lock(search_mutex);
    int i = 2;
    if (tokens[1] == "startpos")
        pending_request.board.setFen(chess::STARTPOS);
    else if (tokens[1] == "fen") {
        string fen = tokens[2];
        i = 3;
        while (i < ntokens) {
            if (tokens[i] == "moves") break;
            fen += " " + tokens[i];
            i++;
        }
        pending_request.board.setFen(fen);
    }

    // play moves
    while (++i < ntokens)
        pending_request.board.makeMove(chess::uci::uciToMove(pending_request.board, tokens[i]));
}


/** Handles the go command
 * E.g., go wtime {wtime} btime {btime}
 *
 * \param tokens list of tokens for the command
 */
void search(const vector<string>& tokens) {
    // get arguments
    int ntokens = tokens.size();

    lock_guard<mutex> search_lock(search_mutex);
    bool is_white = pending_request.board.sideToMove() == chess::Color::WHITE;
    int i = 1;
    while (i < ntokens) {
        if (tokens[i] == "depth") {
            pending_request.options.maxdepth = stoi(tokens[i + 1]);
            break;
        } else if (tokens[i] == "nodes") {
            pending_request.options.maxnodes = stoll(tokens[i + 1]);
            break;
        } else if (tokens[i] == "movetime") {
            pending_request.options.movetime = stoi(tokens[i + 1]);
            break;
        } else if (tokens[i] == "infinite") {
            pending_request.options.infinite = true;
        } else if ((is_white && tokens[i] == "wtime") || (!is_white && tokens[i] == "btime"))
            pending_request.t_remain = stoi(tokens[i + 1]);
        else if ((is_white && tokens[i] == "winc") || (!is_white && tokens[i] == "binc"))
            pending_request.t_inc = stoi(tokens[i + 1]);
        else if (tokens[i] == "movestogo")
            pending_request.options.movestogo = stoi(tokens[i + 1]);
        i += 2;
    }
    pending_request.go = true;
    search_cv.notify_one();
}


int main() {
    std::ios::sync_with_stdio(false);
    string uci_command;

    // start search handler
    thread search_handler(handle_search);

    // listen for commands
    while (!quit) {
        getline(cin, uci_command);

        if (uci_command == "uci") {
            lock_guard<mutex> lock(cout_mutex);
            cout << "id name " << engine.name << "\n"
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
            search_cv.notify_one();

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

    search_handler.join();
    return 0;
}
