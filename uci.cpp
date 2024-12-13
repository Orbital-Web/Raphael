#define UCI
#include <Raphael/Raphael_v2.0.hpp>
#include <Raphael/Transposition.hpp>
#include <SFML/Graphics.hpp>
#include <chess.hpp>
#include <string>
#include <thread>
#include <vector>

char VERSION[4] = "1.7";

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

    while (getline(ss, token, delim)) tokens.push_back(token);

    return tokens;
}

// Sets options such as tt size
// uci >> "setoption name Hash value [size(mb)]"
void setoption(const std::vector<std::string>& tokens) {
    if (tokens.size() != 5) return;

    if (tokens[2] == "Hash") {
        int tablesize_mb = std::stoi(tokens[4]);
        uint32_t tablesize
            = (tablesize_mb * 1024U * 1024) / sizeof(Raphael::TranspositionTable::Entry);
        engine.set_options({.tablesize = tablesize});
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
        board.setFen(uci.substr(13));  // get text after "fen"

    int i = 1;
    while (++i < ntokens)
        if (tokens[i] == "moves") break;

    // play moves
    while (++i < ntokens) board.makeMove(chess::uci::uciToMove(board, tokens[i]));
}

// Handles the go command
// uci >> "go wtime {wtime} btime {btime}"
// uci << "uci << "bestmove {move}"
void search(const std::vector<std::string>& tokens) {
    // get arguments
    int ntokens = tokens.size();
    int i = 1;
    int t_remain = 0, t_inc = 0;
    Raphael::v2_0::SearchOptions searchopt;

    while (i < ntokens) {
        if (tokens[i] == "depth") {
            searchopt.maxdepth = std::stoi(tokens[i + 1]);
            break;
        } else if (tokens[i] == "nodes") {
            searchopt.maxnodes = std::stoi(tokens[i + 1]);
            break;
        } else if (tokens[i] == "movetime") {
            searchopt.movetime = std::stoi(tokens[i + 1]);
            break;
        } else if (tokens[i] == "infinite") {
            searchopt.infinite = true;
            break;
        } else if ((whiteturn && tokens[i] == "wtime") || (!whiteturn && tokens[i] == "btime"))
            t_remain = std::stoi(tokens[i + 1]);
        else if ((whiteturn && tokens[i] == "winc") || (!whiteturn && tokens[i] == "binc"))
            t_inc = std::stoi(tokens[i + 1]);
        i += 2;
    }

    // search best move in separate thread
    halt = false;
    engine.set_searchoptions(searchopt);
    sf::Event nullevent;
    std::thread movereceiver(
        &Raphael::v2_0::get_move, engine, board, t_remain, t_inc, std::ref(nullevent),
        std::ref(halt)
    );
    movereceiver.detach();
}

int main() {
    std::string uci;
    Raphael::v2_0::EngineOptions engine_opt;

    while (!quit) {
        // get input
        getline(std::cin, uci);
        auto tokens = splitstr(uci, ' ');
        auto keyword = tokens[0];

        // parse input
        if (keyword == "uci") {
            printf("id name Raphael %s\n", VERSION);
            printf("id author Rei Meguro\n");
            printf("option name Hash type spin default 192 min 1 max 2560\n");
            printf("uciok\n");
        }

        else if (keyword == "setoption")
            setoption(tokens);

        else if (keyword == "isready")
            printf("readyok\n");

        else if (keyword == "ucinewgame\n")
            engine.reset();

        else if (keyword == "position")
            setposition(uci, tokens);

        else if (keyword == "go") {
            halt = false;
            search(tokens);
        }

        else if (keyword == "stop")
            halt = true;

        else if (keyword == "quit") {
            halt = true;
            quit = true;
        }
    }

    return 0;
}
