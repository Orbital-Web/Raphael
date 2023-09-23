#define UCI
#include <chess.hpp>
#include <Raphael/Raphael_v2.0.hpp>
#include <Raphael/Transposition.hpp>
#include <SFML/Graphics.hpp>
#include <future>
#include <string>
#include <vector>


// global vars
chess::Board board;
Raphael::v2_0 engine("Raphael");
bool halt = false;
bool quit = false;



// Splits strings into words seperated by delimiter
std::vector<std::string> splitstr(const std::string& str, const char delim) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;

    while (getline(ss, token, delim)) 
        tokens.push_back(token);

    return tokens;
}


// Sets options such as tt size
void setoption(const std::vector<std::string>& tokens) {
    if (tokens.size() != 5)
        return;

    if (tokens[2] == "Hash") {
        int tablesize_mb = std::stoi(tokens[4]);
        uint32_t tablesize = (tablesize_mb*1024U*1024) / sizeof(Raphael::TranspositionTable::Entry);
        engine.set_options({.tablesize=tablesize});
        printf("Set table size to %jumb (%ju entries)\n", tablesize_mb, tablesize);
    }
}



// Sets up internal board using fen string and list of ucimoves
// uci >> "position [startpos|fen {fen}] (moves {move1} {move2} ...)"
void setposition(const std::string& uci, const std::vector<std::string>& tokens) {
    int ntokens = tokens.size();

    // starting position
    if (tokens[1] == "startpos")
        board.setFen(chess::STARTPOS);
    else if (tokens[1] == "fen")
        board.setFen(uci.substr(13));   // get text after "fen"

    int i = 1;
    while (++i<ntokens)
        if (tokens[i] == "moves")
            break;
    
    // play moves
    while (++i<ntokens)
        board.makeMove(chess::uci::uciToMove(board, tokens[i]));
}



// Checks if quit was called and modifies halt
void handle_quit() {
    // thanks https://github.com/antoniogarro/Claudia/blob/master/main.c
    const int BUFFER = 2048;
    setvbuf(stdin, 0, _IONBF, 0);
    setvbuf(stdout, 0, _IONBF, 0);
    fflush(NULL);
    char input[BUFFER];

    while (!halt && fgets(input, BUFFER, stdin)) {
        if (!strncmp(input, "quit", 4)) {
            halt = true;
            quit = true;
        } else if (!strncmp(input, "stop", 4))
            halt = true;
        
        memset(input, 0, BUFFER);
    }
}



// Handles the go command
// uci >> "go wtime {wtime} btime {btime}"
// uci << "uci << "bestmove {move}"
void search(const std::vector<std::string>& tokens) {
    // get arguments
    int ntokens = tokens.size();
    int i=1;
    int t_remain=0, t_inc=0;
    while (i<ntokens) {
        if ((whiteturn && tokens[i]=="wtime") || (!whiteturn && tokens[i]=="btime"))
            t_remain = std::stoi(tokens[i+1]);
        else if ((whiteturn && tokens[i]=="winc") || (!whiteturn && tokens[i]=="binc"))
            t_inc = std::stoi(tokens[i+1]);
        i += 2;
    }

    // search best move in separate thread
    sf::Event nullevent;
    auto movereceiver = std::async(&Raphael::v2_0::get_move, engine, board, t_remain, t_inc, std::ref(nullevent), std::ref(halt));

    // check for "stop" or "quit"
    handle_quit();
    std::cout << "bestmove " << chess::uci::moveToUci(movereceiver.get()) << "\n";
}



int main() {
    std::string uci;
    Raphael::v2_0::EngineOptions engine_opt;

    // handle uci commands
    while (!quit) {
        halt = false;
        getline(std::cin, uci);
        auto tokens = splitstr(uci, ' ');
        auto keyword = tokens[0];

        if (keyword == "uci") {
            printf("id name Raphael 1.7\n");
            printf("id author Rei Meguro\n");
            printf("option name Hash type spin default 192 min 1 max 2560\n");
            printf("uciok\n");
        }

        else if (keyword == "setoption")
            setoption(tokens);

        else if (keyword == "ready")
            printf("readyok\n");

        else if (keyword == "ucinewgame\n")
            engine.reset();

        else if (keyword == "position")
            setposition(uci, tokens);
        
        else if (keyword == "go")
            search(tokens);
        
        else if (keyword == "quit")
            quit = true;
    }

    return 0;
}