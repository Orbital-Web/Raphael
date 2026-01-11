#include <GameEngine/GameEngine.h>
#include <GameEngine/HumanPlayer.h>
#include <Raphael/Raphael.h>

#include <climits>
#include <cstring>
#include <fstream>

using std::cout;
using std::ifstream;
using std::string;
using std::vector;

extern const bool UCI = false;



/** Creates a specified GamePlayer
 *
 * \param playertype player type as a null-terminated cstring
 * \param name name of player as a null-terminated cstring
 * \returns a dynamically allocated player
 */
cge::GamePlayer* player_factory(char* playertype, char* name) {
    if (!strcmp(playertype, "human") || !strcmp(playertype, "Human"))
        return new cge::HumanPlayer(name);
    else if (!strcmp(playertype, "Raphael"))
        return new raphael::Raphael(name);

    // invalid
    printf("Invalid player type: %s\n", playertype);
    printf("Valid player types are:\n");
    printf("   human:         Human-controlled player\n");
    printf("   Raphael:       Raphael %s\n", raphael::Raphael::version.c_str());
    return nullptr;
}


/* Prints help/usage text */
void print_usage() {
    cout << "Usage: main.exe <p1type> <p1name> <p2type> <p2name> [mode] [options]\n\n"
         << "Modes:\n"
         << "  [int]  Number of matches (defaults to 1 if not specified)\n"
         << "  -c     Comparison mode (options will be ignored)\n"  // comparing 2 engines
         << "  -r     Results mode (options will be ignored)\n\n"   // records fen + wdl
         //  << "  -e     Evaluation mode (options will be ignored)\n\n"  // records fen + eval
         << "Options:\n"
         << "  -t <int> <int>  Time (sec) for white and black (defaults to 10min each)\n"
         << "  -i <int>        Time increment (sec) (defaults to 0sec)\n"
         << "  -f <FENstring>  Starting position FEN (defaults to standard chess board)\n"
         << "  -s <filename>   Filename for saving as pgn (saves inside /logs folder)\n\n";
}


/*
Example
main.exe human "Adam" Raphael "Raphael"
    1 rapid match, human (white) vs Raphael (black)

main.exe human "Adam" Raphael "Raphael" 5
    5 rapid matches, human vs Raphael (human plays white 3 times)

main.exe human "Adam" human "Bob" -t 60 60
    1 bullet match, both human players

main.exe Raphael "Raphael" Human "Bob" 3 -f "8/8/2q5/2k5/8/5K2/8/8 w - - 0 1"
    3 rapid matches with set starting position, Raphael vs human (human plays white 1 time)

main.exe Raphael "Raph1" Raphael "Raph2" 3 -f "8/8/2q5/2k5/8/5K2/8/8 w - - 0 1" -t 120 120 -i 1
    3 2|1 blitz matches with set starting position, both Raphael\

main.exe Raphael "new" Raphaelv1.0 "old" -c
    400 comparison matches between the newest version of Raphael and Raphaelv1.0
*/
int main(int argc, char** argv) {
    cge::GamePlayer* p1;
    cge::GamePlayer* p2;
    vector<cge::GameEngine::GameOptions> gameoptions;

    // invalid
    if (argc < 5) {
        print_usage();
        return -1;
    }

    // create players
    p1 = player_factory(argv[1], argv[2]);
    p2 = player_factory(argv[3], argv[4]);
    if (!p1 || !p2) return -1;

    // parse mode
    int i = 5;
    if (argc >= 6) {
        // comparison mode
        if (!strcmp(argv[i], "-c")) {
            ifstream pgns("src/Games/randomGames.txt");
            string pgn;
            bool p1_is_white = true;
            // create 400 matches with alterating color
            while (getline(pgns, pgn)) {
                gameoptions.push_back({
                    .p1_is_white = p1_is_white,
                    .interactive = false,
                    .t_inc = 0,
                    .t_remain = {20000, 20000},
                    .start_fen = pgn,
                    .pgn_file = "./logs/compare.pgn"
                });
                p1_is_white = !p1_is_white;
            }
            pgns.close();
            i = INT_MAX;  // ignore all options
        }

        else if (!strcmp(argv[i], "-r")) {
            ifstream pgns("src/Games/randomGamesBig.txt");
            string pgn;
            bool p1_is_white = true;
            // create 400 matches with alterating color
            while (getline(pgns, pgn)) {
                gameoptions.push_back({
                    .p1_is_white = p1_is_white,
                    .interactive = false,
                    .t_inc = 20,
                    .t_remain = {1000, 1000},
                    .start_fen = pgn,
                    .pgn_file = "./logs/results.pgn"
                });
                p1_is_white = !p1_is_white;
            }
            pgns.close();
            i = INT_MAX;  // ignore all options
        }

        // number of games
        else {
            // create n matches with alternating color
            int n_matches = atoi(argv[i]);
            if (n_matches) {
                bool p1_is_white = true;
                for (int n = 0; n < n_matches; n++) {
                    gameoptions.push_back({.p1_is_white = p1_is_white});
                    p1_is_white = !p1_is_white;
                }
                i++;
            } else
                gameoptions.push_back({});  // defaults to 1 match
        }
    } else
        gameoptions.push_back({});  // defaults to 1 match


    // parse arguments
    while (i < argc) {
        // time
        if (!strcmp(argv[i], "-t")) {
            // set the time for every match
            float t_white = (float)atof(argv[++i]);
            float t_black = (float)atof(argv[++i]);
            for (auto& gopt : gameoptions) {
                gopt.t_remain[0] = t_white * 1000;
                gopt.t_remain[1] = t_black * 1000;
            }
        }

        // time increment
        else if (!strcmp(argv[i], "-i")) {
            float t_inc = (float)atof(argv[++i]);
            for (auto& gopt : gameoptions) gopt.t_inc = t_inc * 1000;
        }

        // fen
        else if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "-fen")) {
            i++;
            for (auto& gopt : gameoptions) gopt.start_fen = argv[i];
        }

        // pgn save
        else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "-save")) {
            string pgn_file = "./logs/" + (string)argv[++i];
            for (auto& gopt : gameoptions) gopt.pgn_file = pgn_file;
        }

        else {
            print_usage();
            return -1;
        }

        i++;
    }


    // Initialize GameEngine
    vector<cge::GamePlayer*> players = {p1, p2};
    cge::GameEngine ge(players);

    // Play Matches
    int matchn = 1;
    int n_matches = gameoptions.size();

    for (auto gopt : gameoptions) {
        printf("Starting match %i of %i\n", matchn, n_matches);
        ge.run_match(gopt);
        matchn++;
    }

    ge.print_report();
    delete p1;
    delete p2;
    return 0;
}
