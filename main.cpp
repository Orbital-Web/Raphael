#include "src/GameEngine/GameEngine.hpp"
#include "src/GameEngine/HumanPlayer.hpp"
#include <string>
#include <string.h>
#include <vector>
#include <array>



// struct for storing input arguments to GameEngine::run_match()
struct InputArgs {
    bool p1_is_white = true;
    std::string start_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    std::array<float, 2> t_remain = {600, 600};
};



// Prints usage
void print_help() {
    printf("Usage:\n"
    "main.exe -h:\n"
    "    Shows the help screen\n\n"

    "main.exe <playertype1> <name1> <playertype2> <name2>:\n"
    "main.exe [players]:\n"
    "    Starts a match (rapid) with the corresponding players\n\n"

    "main.exe [players] -c:\n"
    "main.exe [players] -compare:\n"
    "    Runs compare_players with the corresponding players\n\n"

    "main.exe [players] <n_matches>:\n"
    "    Runs n_matches matches (rapid) with the corresponding players,\n"
    "    starting with p1 as white and swapping every match\n\n"

    "main.exe [players] <n_matches> <timeW> <timeB>:\n"
    "    Runs n_matches matches with the corresponding players and time,\n"
    "    starting with p1 as white and swapping every match\n\n"

    "main.exe [players] <timeW> <timeB> <start_fen> <whiteplayer> ...\n"
    "    Runs a match with the corresponding players, time, and\n"
    "    starting position, with whiteplayer (1 or 2) playing\n"
    "    as white. Arguments can be repeated to play multiple\n"
    "    matches consecutively\n\n");
}


// Creates a player
cge::GamePlayer* player_factory(char* playertype, char* name) {
    if (!strcmp(playertype, "human"))
        return new cge::HumanPlayer(name);
    
    // invalid
    printf("Invalid playertype: %s\n", playertype);
    printf("Valid playertypes are:\n");
    printf("   human:\t cge::HumanPlayer\n");
    return nullptr;
}



int main(int argc, char** argv) {
    cge::GamePlayer* p1;
    cge::GamePlayer* p2;
    std::vector<InputArgs> matchargs;

    // invalid input
    if (argc<5) {
        print_help();
        return -1;
    }

    // create players
    p1 = player_factory(argv[1], argv[2]);
    p2 = player_factory(argv[3], argv[4]);
    if (!p1 || !p2)
        return -1;
    

    // main.exe [players]
    if (argc==5)
        matchargs.push_back({});

    // main.exe [players] -c  or  main.exe [players] <n_matches>
    else if (argc==6) {
        // main.exe [players] -c
        if (!strcmp(argv[5], "-c")) {
            printf("Not Implemented!\n");
            return 0;
        }
        // main.exe [players] <n_matches>
        for (int i=0; i<atoi(argv[5]); i++) {
            bool p1_is_white = (i+1)%2;
            matchargs.push_back({.p1_is_white=p1_is_white});
        }

    // main.exe [players] <n_matches> <timeW> <timeB>
    } else if(argc==8) {
        std::array<float, 2> t_remain = {(float)atof(argv[6]), (float)atof(argv[7])};
        for (int i=0; i<atoi(argv[5]); i++) {
            bool p1_is_white = (i+1)%2;
            matchargs.push_back({.p1_is_white=p1_is_white, .t_remain=t_remain});
        }
    
    // main.exe [players] <timeW> <timeB> <start_fen> <startplayer> ...
    } else if ((argc-5)%4 == 0){
        int n_matches = (argc-5) / 4;

        for (int i=0; i<n_matches; i++) {
            printf("A\n");
            std::array<float, 2> t_remain = {(float)atof(argv[4*i + 5]), (float)atof(argv[4*i + 6])};
            printf("B\n");
            std::string start_fen = argv[4*i + 7];
            bool p1_is_white = (strcmp(argv[4*i + 8], "1")==0);
            matchargs.push_back({p1_is_white, start_fen, t_remain});
        }
    } else {
        print_help();
        return -1;
    }


    // Initialize GameEngine
    std::array<cge::GamePlayer*, 2> players = {p1, p2};
    cge::GameEngine ge(players);

    // Play Matches
    int matchn = 1;
    int n_matches = matchargs.size();

    for (auto ma : matchargs) {
        printf("Starting match %i of %i\n", matchn, n_matches);
        ge.run_match(ma.p1_is_white, ma.start_fen, ma.t_remain, true);
        matchn++;
    }

    ge.print_report();
    delete p1;
    delete p2;
    return 0;
}