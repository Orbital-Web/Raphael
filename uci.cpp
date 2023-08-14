#include "Raphael/Raphael_v1.5.hpp"
#include <SFML/Graphics.hpp>
#include <future>
#include <string>
#include <vector>

#ifdef WIN32
#include <io.h>
#include "windows.h"
#else
#include "sys/select.h"
#endif


// global vars
chess::Board board;
Raphael::v1_5 engine("Raphael");
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



// Checks if an input has been entered
int InputWaiting() {
#ifndef WIN32
    fd_set readfds;
    struct timeval tv;
    FD_ZERO (&readfds);
    FD_SET (fileno(stdin), &readfds);
    tv.tv_sec=0; tv.tv_usec=0;
    select(16, &readfds, 0, 0, &tv);

    return (FD_ISSET(fileno(stdin), &readfds));
#else
    static int init = 0, pipe;
    static HANDLE inh;
    DWORD dw;

    if (!init) {
        init = 1;
        inh = GetStdHandle(STD_INPUT_HANDLE);
        pipe = !GetConsoleMode(inh, &dw);
        if (!pipe) {
            SetConsoleMode(inh, dw & ~(ENABLE_MOUSE_INPUT|ENABLE_WINDOW_INPUT));
            FlushConsoleInputBuffer(inh);
        }
    }
    if (pipe) {
        if (!PeekNamedPipe(inh, NULL, 0, NULL, &dw, NULL)) return 1;
        return dw;
    } else {
        GetNumberOfConsoleInputEvents(inh, &dw);
        return dw <= 1 ? 0 : dw;
    }
#endif
}



// Checks if quit was called and modifies halt
void handle_quit(bool& halt) {
    int bytes;
    char input[256] = "";
    char* endc;
    fd_set readset;

    while (!halt) {
        if (InputWaiting()) {
            do { bytes = read(STDIN_FILENO, input, 256);
            } while (bytes < 0);

            endc = strchr(input,'\n');
            if (endc) *endc=0;
            if (strlen(input) > 0)
                if (!strncmp(input, "quit", 4)) {
                    halt = true;
                    quit = true;
                } else if (!strncmp(input, "stop", 4))
                    halt = true;
        }
    }
}



// Handles the go command
// uci >> "go wtime {wtime} btime {btime}"
// uci << "uci << "bestmove {move}"
void search(const std::vector<std::string>& tokens) {
    // look for quit/stop in separate thread
    auto _ = std::async(&handle_quit, std::ref(halt));

    // find best move
    int ntokens = tokens.size();
    int i=1;
    int64_t t_remain;
    while (i<ntokens) {
        if ((whiteturn && tokens[i]=="wtime") || (!whiteturn && tokens[i]=="btime"))
            t_remain = std::stof(tokens[i+1]);
        i += 2;
    }
    // search best move
    sf::Event nullevent;
    auto toPlay = engine.get_move(board, t_remain, nullevent, halt);
    std::cout << "bestmove " << chess::uci::moveToUci(toPlay) << "\n";
}



int main() {
    std::string uci;

    // handle uci commands
    while (!quit) {
        halt = false;
        getline(std::cin, uci);
        auto tokens = splitstr(uci, ' ');
        auto keyword = tokens[0];

        if (keyword == "uci") {
            printf("id name Raphael\n");
            printf("id author Rei Meguro\n");
            printf("uciok\n");
        }

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