#include <GameEngine/GameEngine.hpp>
#include <GameEngine/HumanPlayer.hpp>
#include <Raphael/Raphael_v1.0.hpp>
#include <Raphael/Raphael_v1.4.hpp>
#include <Raphael/Raphael_v1.5.hpp>
#include <string.h>
#include <fstream>



// Creates the specified player
cge::GamePlayer* player_factory(char* playertype, char* name) {
    if (!strcmp(playertype, "human"))
        return new cge::HumanPlayer(name);
    else if (!strcmp(playertype, "Raphaelv1.0"))
        return new Raphael::v1_0(name);
    else if (!strcmp(playertype, "Raphaelv1.4"))
        return new Raphael::v1_4(name);
    else if (!strcmp(playertype, "Raphaelv1.5"))
        return new Raphael::v1_5(name);
    else if (!strcmp(playertype, "Raphael"))
        return new Raphael::v1_5(name);
    
    // invalid
    printf("Invalid player type: %s\n", playertype);
    printf("Valid player types are:\n");
    printf("   human:\t cge::HumanPlayer\n");
    printf("   Raphael:\t Raphael::v1_5\n");
    printf("   Raphaelv1.0:\t Raphael::v1_0\n");
    printf("   Raphaelv1.4:\t Raphael::v1_4\n");
    printf("   Raphaelv1.5:\t Raphael::v1_5\n");
    return nullptr;
}


// Prints usage comment
void print_usage() {
    std::cout << "Usage: main.exe <p1type> <p1name> <p2type> <p2name> [mode] [options]\n\n"
              << "Modes:\n"
              << "  [int]\tNumber of matches (defaults to 1 if not specified)\n"
              << "  -c\tComparison mode (options will be ignored)\n\n"
              << "Options:\n"
              << "  -t <int> <int>\tTime (sec) for white and black (defaults to 10min each)\n"
              << "  -f <FENstring>\tStarting position FEN (defaults to standard chess board)\n"
              << "  -s <filename> \tFilename for saving as pgn (saves inside /logs folder)\n\n";
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

main.exe Raphael "Raph1" Raphael "Raph2" 3 -f "8/8/2q5/2k5/8/5K2/8/8 w - - 0 1" -t 300 300
    3 blitz matches with set starting position, both Raphael
*/
int main(int argc, char** argv) {
    cge::GamePlayer* p1;
    cge::GamePlayer* p2;
    std::vector<cge::GameEngine::GameOptions> gameoptions;

    // invalid
    if (argc<5) {
        print_usage();
        return -1;
    }

    // create players
    p1 = player_factory(argv[1], argv[2]);
    p2 = player_factory(argv[3], argv[4]);
    if (!p1 || !p2)
        return -1;

    // parse mode
    int i = 5;
    if (argc >= 6) {
        // comparison mode
        if (!strcmp(argv[i], "-c")) {
            std::ifstream pgns("src/Games/randomGames.txt");
            std::string pgn;
            bool p1_is_white = true;
            // create 400 matches with alterating color
            while (std::getline(pgns, pgn)) {
                gameoptions.push_back({p1_is_white, pgn, {20, 20}, false, "./logs/compare.pgn"});
                p1_is_white = !p1_is_white;
            }
            pgns.close();
            i = INT_MAX;    // ignore all options
        }

        // number of games
        else {
            // create n matches with alternating color
            int n_matches = atoi(argv[i]);
            if (n_matches) {
                bool p1_is_white = true;
                for (int n=0; n<n_matches; n++) {
                    gameoptions.push_back({.p1_is_white=p1_is_white});
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
        if(!strcmp(argv[i], "-t")) {
            // set the time for every match
            float t_white = (float)atof(argv[++i]);
            float t_black = (float)atof(argv[++i]);
            for (auto& gopt : gameoptions) {
                gopt.t_remain[0] = t_white;
                gopt.t_remain[1] = t_black;
            }
        }

        // fen
        else if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "-fen")) {
            i++;
            for (auto& gopt : gameoptions)
                gopt.start_fen = argv[i];
        }

        // pgn save
        else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "-save")) {
            std::string pgn_file = "./logs/" + (std::string)argv[++i];
            for (auto& gopt : gameoptions)
                gopt.pgn_file = pgn_file;
        }

        else {
            print_usage();
            return -1;
        }

        i++;
    }


    // Initialize GameEngine
    std::vector<cge::GamePlayer*> players = {p1, p2};
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