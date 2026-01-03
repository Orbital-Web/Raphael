#include <GameEngine/consts.h>
#include <Raphael/Raphael.h>
#include <Raphael/options.h>

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using std::cin, std::cout, std::flush;
using std::exception;
using std::string, std::stoi, std::stoll;
using std::stringstream;
using std::thread, std::mutex, std::unique_lock, std::lock_guard, std::condition_variable;
using std::vector;

extern const bool UCI = true;



// engine
mutex engine_mutex;
Raphael::RaphaelNNUE engine("Raphael");

// search globals
mutex search_mutex;
condition_variable search_cv;
struct SearchRequest {
    chess::Board board;
    Raphael::RaphaelNNUE::SearchOptions options;
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
            cge::MouseInfo mouse = {.x = 0, .y = 0, .event = cge::MouseEvent::NONE};
            halt = false;
            engine.set_searchoptions(request.options);
            engine.get_move(request.board, request.t_remain, request.t_inc, mouse, halt);
        }
    }
}



/** Sets options such as tt size
 * E.g., setoption name Hash value [size(MB)]
 *
 * \param tokens list of tokens for the command
 */
void setoption(const vector<string>& tokens) {
    if (tokens.size() != 5) return;
    if (tokens[1] != "name" || tokens[3] != "value") return;

    // check option
    if (tokens[4] == "true" || tokens[4] == "false") {
        const bool value = (tokens[4][0] == 't');
        lock_guard<mutex> engine_lock(engine_mutex);
        engine.set_option(tokens[2], value);
        return;
    }

    // spin option
    int value;
    try {
        value = stoi(tokens[4]);
    } catch (const exception& e) {
        return;
    }

    lock_guard<mutex> engine_lock(engine_mutex);
#ifdef TUNE
    if (Raphael::set_tunable(tokens[2], value)) {
        lock_guard<mutex> lock(cout_mutex);
        cout << "info string set " << tokens[2] << " to " << value << "\n" << flush;
        return;
    }
#endif
    engine.set_option(tokens[2], value);
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
        pending_request.board.setFen(chess::constants::STARTPOS);
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
    pending_request.options = {};
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
    if (pending_request.t_remain < 0) pending_request.t_remain = 1;
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
            cout << "id name " << engine.name << " " << engine.version << "\n"
                 << "id author Rei Meguro\n"
                 << engine.default_params.hash.uci() << engine.default_params.softnodes.uci()
                 << engine.default_params.softhardmult.uci();
#ifdef TUNE
            for (const auto tunable : Raphael::tunables) cout << tunable->uci();
#endif
            cout << "uciok\n" << flush;

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
