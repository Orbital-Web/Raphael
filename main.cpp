#include <GameEngine/GameEngine.h>

#include <cstring>
#include <iostream>

using std::cout;
using std::flush;
using std::string;


/* Prints help/usage text */
void print_usage() {
    cout << "Usage: main.exe [MODE] [GAMES] [OPTIONS]\n\n"
         << "  Starts a match against Raphael or between two Raphael instances\n\n"
         << "  MODE: w or b to play against Raphael as white/black, e for engine vs engine match. "
         << "default w\n"
         << "  GAMES: number of matches. default 1\n"
         << "  OPTIONS:\n"
         << "    -t <float> <float>  Time (sec) for white and black. defaults to 10min each\n"
         << "    -i <float>          Time increment (sec). defaults to 1s\n"
         << "    -f <FEN>            Starting position FEN. defaults to startpos\n"
         << "    -s <filename>       Filename for saving as pgn (saves inside /logs)\n"
         << "    -h                  Show this message and exit\n"
         << flush;
}


int main(int argc, char** argv) {
    // parse positional args
    cge::GameEngine::GameOptions gameoption{};
    i32 num_games = 1;
    bool engine_v_engine = false;

    i32 i = 1;
    if (argc > 1 && argv[1][0] != '-') {
        if (!strcmp(argv[1], "b")) {
            gameoption.w_is_engine = true;
            gameoption.b_is_engine = false;
        } else if (!strcmp(argv[1], "e")) {
            gameoption.w_is_engine = true;
            gameoption.b_is_engine = true;
            engine_v_engine = true;
        } else if (strcmp(argv[1], "w")) {
            cout << "error: MODE must be one of: 'w' 'b' 'e'\n" << flush;
            return 1;
        }
        i++;

        if (argc > 2 && argv[2][0] != '-') {
            num_games = atoi(argv[2]);
            i++;
        }
    }

    // parse remaining arguments
    while (i < argc) {
        // time
        if (!strcmp(argv[i], "-t")) {
            gameoption.w_time = static_cast<i32>(1000.0 * atof(argv[++i]));
            gameoption.b_time = static_cast<i32>(1000.0 * atof(argv[++i]));
        }

        // time increment
        else if (!strcmp(argv[i], "-i"))
            gameoption.inc_time = static_cast<i32>(1000.0 * atof(argv[++i]));

        // fen
        else if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "-fen"))
            gameoption.start_fen = argv[++i];

        // pgn save
        else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "-save"))
            gameoption.pgn_file = "./logs/" + string(argv[++i]);

        else {
            print_usage();
            return 1;
        }
        i++;
    }


    // Initialize GameEngine
    cge::GameEngine gameengine;

    for (i32 m = 0; m < num_games; m++) {
        cout << "Starting match " << m + 1 << " of " << num_games << "\n" << flush;
        gameengine.run_match(gameoption);

        // swap colors and time
        if (!engine_v_engine) {
            const auto temp = gameoption.w_is_engine;
            gameoption.w_is_engine = gameoption.b_is_engine;
            gameoption.b_is_engine = temp;
        }
        const auto temp = gameoption.w_time;
        gameoption.w_time = gameoption.b_time;
        gameoption.b_time = temp;
    }

    return 0;
}
