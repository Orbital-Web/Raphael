#include <Raphael/Raphael.h>
#include <Raphael/commands.h>
#include <Raphael/datagen.h>
#include <Raphael/tunable.h>
#include <Raphael/wdl.h>

#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using std::cin;
using std::cout;
using std::exception;
using std::flush;
using std::stoi;
using std::stoll;
using std::stoull;
using std::string;
using std::stringstream;
using std::vector;



// search globals
raphael::Position<false> position;
bool chess960 = false;
bool position_ready = false;

bool quit = false;

raphael::Raphael engine;



/** Sets options such as tt size
 * E.g., setoption name Hash value [size(MB)]
 *
 * \param tokens list of tokens for the command
 */
inline void handle_setoption(const vector<string>& tokens) {
    if (!engine.is_search_complete()) {
        cout << "info string still searching\n" << flush;
        return;
    }

    if (tokens.size() != 5 || tokens[1] != "name" || tokens[3] != "value") {
        cout << "info string usage: setoption name <NAME> value <VALUE>\n" << flush;
        return;
    }

    // check option
    if (tokens[4] == "true" || tokens[4] == "false") {
        const bool value = (tokens[4][0] == 't');

        // UCI_Chess960
        if (raphael::utils::is_case_insensitive_equals(tokens[2], "UCI_Chess960")) chess960 = value;

        engine.set_option(tokens[2], value);
        return;
    }

    // spin option
    i32 value;
    try {
        value = stoi(tokens[4]);
    } catch (const exception& e) {
        cout << "info string value must either be a bool or an int\n" << flush;
        return;
    }

#ifdef TUNE
    if (raphael::set_tunable(tokens[2], value)) {
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
inline void handle_position(const vector<string>& tokens) {
    if (!engine.is_search_complete()) {
        cout << "info string still searching\n" << flush;
        return;
    }

    i32 ntokens = tokens.size();
    if (ntokens < 2) return;

    // set initial board
    chess::Board board;
    board.set960(chess960);

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
    position.set_board(board);

    // apply moves
    while (++i < ntokens) position.make_move(chess::uci::to_move(position.board(), tokens[i]));

    // we modified the position, engine must call set_position
    position_ready = false;
}

/** Handles the go command
 * E.g., go wtime {wtime} btime {btime}
 *
 * \param tokens list of tokens for the command
 */
inline void handle_go(const vector<string>& tokens) {
    if (!engine.is_search_complete()) {
        cout << "info string already searching\n" << flush;
        return;
    }

    // get arguments
    raphael::TimeManager::SearchOptions options = {};

    bool is_white = position.board().stm() == chess::Color::WHITE;
    i32 ntokens = tokens.size();
    i32 i = 1;
    while (i < ntokens) {
        if (tokens[i] == "depth")
            options.maxdepth = stoi(tokens[i + 1]);
        else if (tokens[i] == "nodes")
            options.maxnodes = stoll(tokens[i + 1]);
        else if (tokens[i] == "movetime")
            options.movetime = stoi(tokens[i + 1]);
        else if (tokens[i] == "infinite") {
            options.infinite = true;
            i -= 1;
        } else if ((is_white && tokens[i] == "wtime") || (!is_white && tokens[i] == "btime"))
            options.t_remain = stoi(tokens[i + 1]);
        else if ((is_white && tokens[i] == "winc") || (!is_white && tokens[i] == "binc"))
            options.t_inc = stoi(tokens[i + 1]);
        else if (tokens[i] == "movestogo")
            options.movestogo = stoi(tokens[i + 1]);
        i += 2;
    }
    if (options.t_remain < 0) options.t_remain = 1;
    if (ntokens == 1) options.infinite = true;

    if (!position_ready) {
        engine.set_position(position);
        position_ready = true;

        cout << "info string warning: to avoid overhead, call isready or ucinewgame after "
                "setting position\n"
             << flush;
    }

    engine.start_search(options);
}

/** Handles the eval command
 *
 * \param corrected whether to show the corrected or raw static eval
 */
inline void handle_eval(bool corrected) {
    if (!engine.is_search_complete()) {
        cout << "info string still searching\n" << flush;
        return;
    }

    if (!position_ready) {
        engine.set_position(position);
        position_ready = true;
    }

    const auto raw_eval = engine.static_eval(corrected);
    const auto norm_eval = raphael::wdl::normalize_score(raw_eval, position.board());

    cout << "info string eval: " << raw_eval << "\n"
         << "info string normalized eval: " << norm_eval << "\n"
         << flush;
}

/** Handles the isready command */
inline void handle_isready() {
    if (!engine.is_search_complete()) {
        // if we are still searching, simply return readyok to indicate we are alive
        cout << "readyok\n" << flush;
        return;
    }

    // otherwise, set up internal states
    if (!position_ready) {
        engine.set_position(position);
        position_ready = true;
    }

    cout << "readyok\n" << flush;
}

/** Handles the ucinewgame command */
inline void handle_ucinewgame() {
    if (!engine.is_search_complete()) {
        cout << "info string still searching\n" << flush;
        return;
    }

    engine.reset();

    if (!position_ready) {
        engine.set_position(position);
        position_ready = true;
    }
}

/** Handles the wait command */
inline void handle_wait() {
    engine.wait_search();

    cout << "info string search finished\n" << flush;
}

/** Handles the bench command */
inline void handle_bench() {
    if (!engine.is_search_complete()) {
        cout << "info string still searching\n" << flush;
        return;
    }

    raphael::commands::bench(engine);

    quit = true;
}

/** Handles the genfens command
 *
 * \param tokens list of tokens for the command
 */
inline void handle_genfens(const vector<string>& tokens) {
    if (!engine.is_search_complete()) {
        cout << "info string still searching\n" << flush;
        return;
    }

    if (tokens.size() < 2) {
        cout << "info string missing required positional parameter 'count'\n" << flush;
        return;
    }

    i32 count = stoi(tokens[1]);
    u64 seed = 0;
    std::string book = "None";
    i32 randmoves = 0;
    bool dfrc = false;

    usize i = 2;
    while (i < tokens.size()) {
        if (tokens[i] == "seed")
            seed = stoull(tokens[i + 1]);
        else if (tokens[i] == "book")
            book = tokens[i + 1];
        else if (tokens[i] == "randmoves")
            randmoves = stoi(tokens[i + 1]);
        else if (tokens[i] == "dfrc")
            dfrc = (tokens[i + 1] == "true");
        i += 2;
    }

    if (count <= 0) {
        cout << "info string count must be positive\n" << flush;
        return;
    }

    if (randmoves < 0) {
        cout << "info string randmoves must be non-negative\n" << flush;
        return;
    }

    raphael::commands::genfens(engine, count, seed, book, randmoves, dfrc);

    quit = true;
}

/** Handles the datagen command
 *
 * \param tokens list of tokens for the command
 */
inline void handle_datagen(const vector<string>& tokens) {
    if (!engine.is_search_complete()) {
        cout << "info string still searching\n" << flush;
        return;
    }

    if (tokens.size() < 3) {
        cout << "info string missing required positional parameters 'softnodes' and 'games'\n"
             << flush;
        return;
    }

    i32 softnodes = stoi(tokens[1]);
    i32 games = stoi(tokens[2]);
    std::string book = "None";
    i32 randmoves = 0;
    bool dfrc = false;
    i32 concurrency = 1;

    usize i = 3;
    while (i < tokens.size()) {
        if (tokens[i] == "book")
            book = tokens[i + 1];
        else if (tokens[i] == "randmoves")
            randmoves = stoi(tokens[i + 1]);
        else if (tokens[i] == "dfrc")
            dfrc = (tokens[i + 1] == "true");
        else if (tokens[i] == "threads")
            concurrency = stoi(tokens[i + 1]);
        i += 2;
    }

    if (softnodes <= 0) {
        cout << "info string softnodes must be positive\n" << flush;
        return;
    }

    if (games <= 0) {
        cout << "info string count must be positive\n" << flush;
        return;
    }

    if (randmoves < 0) {
        cout << "info string randmoves must be non-negative\n" << flush;
        return;
    }

    if (concurrency <= 0) {
        cout << "info string threads must be positive\n" << flush;
        return;
    }

    raphael::datagen::generate_games(engine, softnodes, games, book, randmoves, dfrc, concurrency);

    quit = true;
}

/** Handles the evalstats command
 *
 * \param tokens list of tokens for the command
 */
inline void handle_evalstats(const vector<string>& tokens) {
    if (!engine.is_search_complete()) {
        cout << "info string still searching\n" << flush;
        return;
    }

    if (tokens.size() < 2) {
        cout << "info string missing required positional parameter 'book'\n" << flush;
        return;
    }

    raphael::commands::evalstats(engine, tokens[1]);

    quit = true;
}

/** Shows the help message */
inline void show_help() {
    // help message style from pawnocchio
    cout << "Raphael " << engine.version << "\n\n"
         << "TOOLS:\n"
         << "  bench\n"
         << "      run benchmark\n\n"
         << "  genfens <COUNT> [seed SEED] [book BOOK] [randmoves RANDMOVES] [dfrc DFRC]\n"
         << "      generate FENs\n"
         << "      COUNT: number of FENs to generate\n"
         << "      SEED: random seed, u64. default 0\n"
         << "      BOOK: book to start with. default is None, AKA startpos\n"
         << "      RANDMOVES: number of random moves to play from book position. default 0\n"
         << "      DFRC: whether to generate DFRC positions, true/false. default false\n\n"
         << " datagen <SOFTNODES> <GAMES> [book BOOK] [randmoves RANDMOVES] [dfrc DFRC] [threads "
            "THREADS]\n"
         << "      generate training data\n"
         << "      SOFTNODES: number of softnodes to generate with\n"
         << "      GAMES: number of games to generate\n"
         << "      BOOK: book to start with. defualt is None, AKA startpos\n"
         << "      RANDMOVES: number of random moves to play from book position. default 0\n"
         << "      DFRC: whether to generate DFRC positions, true/false. default false\n"
         << "      THREADS: number of threads to generate with\n\n"
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
         << "  eval                     - show raw static eval\n"
         << "  ceval                    - show corrected static eval\n"
         << "  fen                      - show current position's fen\n"
         << "  board                    - show current position's board\n"
         << "  stop                     - stop current search\n"
         << "  wait                     - wait until the current search finishes\n"
         << "  quit                     - exit\n";

    quit = true;
}


/** Handles a single uci command
 *
 * \param uci_command the command string
 */
inline void handle_command(const string& uci_command) {
    if (uci_command == "uci") {
        const auto params = engine.default_params();
        cout << "id name Raphael " << engine.version << "\n"
             << "id author Rei Meguro\n"
             << params.hash.uci() << params.threads.uci()
             << "option name UCI_Chess960 type check default false\n"
             << params.moveoverhead.uci() << params.datagen.uci() << params.softnodes.uci()
             << params.softhardmult.uci();
#ifdef TUNE
        for (const auto tunable : raphael::tunables) cout << tunable->uci();
#endif
        cout << "uciok\n" << flush;

    } else if (uci_command == "isready")
        handle_isready();

    else if (uci_command == "stop")
        engine.stop_search();

    else if (uci_command == "quit")
        quit = true;

    else if (uci_command == "wait")
        handle_wait();

    else if (uci_command == "ucinewgame")
        handle_ucinewgame();

    else if (uci_command == "help")
        show_help();

    else if (uci_command == "obspsa") {
#ifdef TUNE
        for (const auto tunable : raphael::tunables) cout << tunable->ob();
        cout << flush;
#else
        cout << "info string this is not a tunable build\n" << flush;
#endif

    } else if (uci_command == "bench")
        handle_bench();

    else if (uci_command == "eval")
        handle_eval(false);

    else if (uci_command == "ceval")
        handle_eval(true);

    else if (uci_command == "fen") {
        cout << position.board().get_fen() << "\n" << flush;

    } else if (uci_command == "board") {
        cout << position.board().pretty_print() << flush;

    } else {
        // tokenize command
        vector<string> tokens;
        stringstream ss(uci_command);
        string token;
        while (getline(ss, token, ' ')) tokens.push_back(token);
        if (tokens.empty()) return;

        string& keyword = tokens[0];
        if (keyword == "setoption")
            handle_setoption(tokens);

        else if (keyword == "position")
            handle_position(tokens);

        else if (keyword == "go")
            handle_go(tokens);

        else if (keyword == "genfens")
            handle_genfens(tokens);

        else if (keyword == "datagen")
            handle_datagen(tokens);

        else if (keyword == "evalstats")
            handle_evalstats(tokens);

        else
            cout << "info string unknown command: '" << keyword << "'\n" << flush;
    }
}


/** Splits command line arguments into a list of commands, separated by ""
 *
 * \param argc number of command line arguments
 * \param argv contents of the command line arguments
 * \returns the list of split arguments
 */
inline vector<string> split_args(i32 argc, char** argv) {
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
    engine.set_uciinfolevel(raphael::Raphael::UciInfoLevel::ALL);

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

    return 0;
}
