#include <GameEngine/consts.h>
#include <Raphael/Raphael.h>
#include <Raphael/commands.h>
#include <Raphael/tunable.h>

#include <condition_variable>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using std::atomic;
using std::cin;
using std::condition_variable;
using std::cout;
using std::exception;
using std::flush;
using std::lock_guard;
using std::memory_order_relaxed;
using std::mutex;
using std::stoi;
using std::stoll;
using std::stoull;
using std::string;
using std::stringstream;
using std::thread;
using std::unique_lock;
using std::vector;



// engine
mutex engine_mutex;
raphael::Raphael engine("Raphael");

// search globals
mutex search_mutex;
condition_variable search_cv;
struct SearchRequest {
    raphael::Position<false> position;
    raphael::TimeManager::SearchOptions options;
    i32 t_remain;
    i32 t_inc;
    bool go = false;
};
SearchRequest pending_request;

// other globals
atomic<bool> halt{false};
atomic<bool> quit{false};



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
        lock_guard<mutex> engine_lock(engine_mutex);
        halt.store(false, memory_order_relaxed);
        engine.set_searchoptions(request.options);
        engine.get_move(request.t_remain, request.t_inc, halt);
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
    i32 value;
    try {
        value = stoi(tokens[4]);
    } catch (const exception& e) {
        return;
    }

    lock_guard<mutex> engine_lock(engine_mutex);
#ifdef TUNE
    if (raphael::set_tunable(tokens[2], value)) {
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
    i32 ntokens = tokens.size();
    if (ntokens < 2) return;

    // set initial board
    chess::Board board;
    i32 i = 2;
    if (tokens[1] == "startpos")
        board.set_fen(chess::Board::STARTPOS);
    else if (tokens[1] == "fen") {
        string fen = tokens[2];
        i = 3;
        while (i < ntokens) {
            if (tokens[i] == "moves") break;
            fen += " " + tokens[i];
            i++;
        }
        board.set_fen(fen);
    }
    lock_guard<mutex> search_lock(search_mutex);
    pending_request.position.set_board(board);

    // play moves
    while (++i < ntokens)
        pending_request.position.make_move(
            chess::uci::to_move(pending_request.position.board(), tokens[i])
        );

    // set engine position
    lock_guard<mutex> engine_lock(engine_mutex);
    engine.set_position(pending_request.position);
}

/** Handles the go command
 * E.g., go wtime {wtime} btime {btime}
 *
 * \param tokens list of tokens for the command
 */
void search(const vector<string>& tokens) {
    // get arguments
    i32 ntokens = tokens.size();

    lock_guard<mutex> search_lock(search_mutex);
    pending_request.options = {};
    pending_request.t_remain = 0;
    pending_request.t_inc = 0;

    bool is_white = pending_request.position.board().stm() == chess::Color::WHITE;
    i32 i = 1;
    while (i < ntokens) {
        if (tokens[i] == "depth")
            pending_request.options.maxdepth = stoi(tokens[i + 1]);
        else if (tokens[i] == "nodes")
            pending_request.options.maxnodes = stoll(tokens[i + 1]);
        else if (tokens[i] == "movetime")
            pending_request.options.movetime = stoi(tokens[i + 1]);
        else if (tokens[i] == "infinite") {
            pending_request.options.infinite = true;
            i -= 1;
        } else if ((is_white && tokens[i] == "wtime") || (!is_white && tokens[i] == "btime"))
            pending_request.t_remain = stoi(tokens[i + 1]);
        else if ((is_white && tokens[i] == "winc") || (!is_white && tokens[i] == "binc"))
            pending_request.t_inc = stoi(tokens[i + 1]);
        else if (tokens[i] == "movestogo")
            pending_request.options.movestogo = stoi(tokens[i + 1]);
        i += 2;
    }
    if (pending_request.t_remain < 0) pending_request.t_remain = 1;
    if (ntokens == 1) pending_request.options.infinite = true;
    pending_request.go = true;
    search_cv.notify_one();
}

/** Handles the genfens command
 *
 * \param tokens list of tokens for the command
 */
void genfens(const vector<string>& tokens) {
    if (tokens.size() < 2) {
        cout << "info string missing required positional parameter 'count'\n" << flush;
        return;
    }

    i32 count = stoi(tokens[1]);
    u64 seed = 0;
    std::string book = "None";
    i32 randmoves = 0;

    usize i = 2;
    while (i < tokens.size()) {
        if (tokens[i] == "seed")
            seed = stoull(tokens[i + 1]);
        else if (tokens[i] == "book")
            book = tokens[i + 1];
        else if (tokens[i] == "randmoves")
            randmoves = stoi(tokens[i + 1]);
        i += 2;
    }

    if (count <= 0) {
        lock_guard<mutex> lock(cout_mutex);
        cout << "info string count must be positive\n" << flush;
        return;
    }

    if (randmoves < 0) {
        lock_guard<mutex> lock(cout_mutex);
        cout << "info string randmoves must be non-negative\n" << flush;
        return;
    }

    lock_guard<mutex> engine_lock(engine_mutex);
    raphael::commands::genfens(engine, count, seed, book, randmoves);
}

/** Handles the evalstats command
 *
 * \param tokens list of tokens for the command
 */
void evalstats(const vector<string>& tokens) {
    if (tokens.size() < 2) {
        cout << "info string missing required positional parameter 'book'\n" << flush;
        return;
    }

    lock_guard<mutex> engine_lock(engine_mutex);
    raphael::commands::evalstats(engine, tokens[1]);
}

/** Shows the help message */
void show_help() {
    lock_guard<mutex> lock(cout_mutex);
    cout << engine.name << " " << engine.version << "\n\n"
         << "TOOLS:\n"
         << "  bench\n"
         << "      run benchmark\n\n"
         << "  genfens <COUNT> [seed SEED] [book BOOK] [randmoves RANDMOVES]\n"
         << "      generate FENs\n"
         << "      COUNT: number of FENs to generate\n"
         << "      SEED: random seed, u64. default 0\n"
         << "      BOOK: book to start with. default is None, AKA startpos\n"
         << "      RANDMOVES: number of random moves to play from book position. default 0\n\n"
         << "  evalstats <BOOK>\n"
         << "      print statistics of NNUE evaluation\n"
         << "      BOOK: book to benchmark with\n\n"
         << "  obspsa\n"
         << "      print the OpenBench SPSA configs\n\n"
         << "  help\n"
         << "      show this help message and exit\n\n"
         << "UCI COMMANDS:\n"
         << "  uci                      - handshake\n"
         << "  isready                  - synchronization\n"
         << "  setoption                - set Hash, Threads, etc.\n"
         << "  ucinewgame               - clear Hash and reset\n"
         << "  position                 - set board (fen <FEN> | startpos) [moves ...]\n"
         << "  go                       - start search. params: depth, nodes, movetime, movestogo\n"
         << "                             wtime, btime, winc, binc, infinite\n"
         << "  stop                     - stop current search\n"
         << "  eval                     - show static eval\n"
         << "  quit                     - exit\n";
}


/** Handles a single uci command
 *
 * \param uci_command the command string
 */
void handle_command(const string& uci_command) {
    if (uci_command == "uci") {
        const auto params = engine.default_params();
        lock_guard<mutex> lock(cout_mutex);
        cout << "id name " << engine.name << " " << engine.version << "\n"
             << "id author Rei Meguro\n"
             << params.hash.uci() << params.threads.uci() << params.moveoverhead.uci()
             << params.datagen.uci() << params.softnodes.uci() << params.softhardmult.uci();
#ifdef TUNE
        for (const auto tunable : raphael::tunables) cout << tunable->uci();
#endif
        cout << "uciok\n" << flush;

    } else if (uci_command == "isready") {
        lock_guard<mutex> lock(cout_mutex);
        lock_guard<mutex> engine_lock(engine_mutex);
        cout << "readyok\n" << flush;

    } else if (uci_command == "stop")
        halt.store(true, memory_order_relaxed);

    else if (uci_command == "quit") {
        halt.store(true, memory_order_relaxed);
        quit.store(true, memory_order_relaxed);
        search_cv.notify_one();

    } else if (uci_command == "ucinewgame") {
        halt.store(true, memory_order_relaxed);
        lock_guard<mutex> engine_lock(engine_mutex);
        engine.reset();

    } else if (uci_command == "help") {
        halt.store(true, memory_order_relaxed);
        show_help();

        quit.store(true, memory_order_relaxed);
        search_cv.notify_one();

    } else if (uci_command == "obspsa") {
#ifdef TUNE
        lock_guard<mutex> lock(cout_mutex);
        for (const auto tunable : raphael::tunables) cout << tunable->ob();
        cout << flush;
#else
        cout << "info string this is not a tunable build\n" << flush;
#endif

    } else if (uci_command == "bench") {
        halt.store(true, memory_order_relaxed);
        lock_guard<mutex> engine_lock(engine_mutex);
        engine.set_uciinfolevel(raphael::Raphael::UciInfoLevel::MINIMAL);
        engine.reset();
        raphael::commands::bench(engine);

        quit.store(true, memory_order_relaxed);
        search_cv.notify_one();

    } else if (uci_command == "eval") {
        halt.store(true, memory_order_relaxed);
        lock_guard<mutex> engine_lock(engine_mutex);
        lock_guard<mutex> lock(cout_mutex);
        cout << "info string eval: " << engine.static_eval() << "\n" << flush;

    } else {
        // tokenize command
        vector<string> tokens;
        stringstream ss(uci_command);
        string token;
        while (getline(ss, token, ' ')) tokens.push_back(token);
        if (tokens.empty()) return;

        string& keyword = tokens[0];
        if (keyword == "setoption") {
            halt.store(true, memory_order_relaxed);
            setoption(tokens);

        } else if (keyword == "position") {
            halt.store(true, memory_order_relaxed);
            setposition(tokens);

        } else if (keyword == "go") {
            halt.store(true, memory_order_relaxed);
            search(tokens);

        } else if (keyword == "genfens") {
            halt.store(true, memory_order_relaxed);
            genfens(tokens);

            quit.store(true, memory_order_relaxed);
            search_cv.notify_one();

        } else if (keyword == "evalstats") {
            halt.store(true, memory_order_relaxed);
            evalstats(tokens);

            quit.store(true, memory_order_relaxed);
            search_cv.notify_one();

        } else {
            lock_guard<mutex> lock(cout_mutex);
            cout << "info string unknown command: '" << keyword << "'\n" << flush;
        }
    }
}


/** Splits command line arguments into a list of commands, separated by ""
 *
 * \param argc number of command line arguments
 * \param argv contents of the command line arguments
 * \returns the list of split arguments
 */
vector<string> split_args(i32 argc, char** argv) {
    assert(argc > 1);

    vector<string> args;
    string arg = "";
    bool hold = false;
    bool has_quotes = false;

    for (i32 i = 1; i < argc; i++) {
        arg += argv[i];

        // handle commands in quotations
        if (arg.front() == '"') {
            hold = true;
            has_quotes = true;
        }
        if (arg.back() == '"') hold = false;

        if (!hold) {
            if (has_quotes)
                args.push_back(arg.substr(1, arg.length() - 2));
            else
                args.push_back(arg);

            arg = "";
            has_quotes = false;
        }
    }

    return args;
}


int main(int argc, char** argv) {
    // set to startpos
    engine.set_uciinfolevel(raphael::Raphael::UciInfoLevel::ALL);

    // start search handler
    thread search_handler(handle_search);

    // handle command line arguments
    if (argc > 1) {
        vector<string> args = split_args(argc, argv);
        for (const auto& arg : args)
            if (!quit) handle_command(arg);
    }

    // listen for commands from cin
    string uci_command;
    while (!quit) {
        getline(cin, uci_command);
        handle_command(uci_command);
    }

    search_handler.join();
    return 0;
}
