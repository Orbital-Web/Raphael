# Raphael
A GUI-based Chess Player as well as a Chess Engine (Raphael), coded in C++, [SFML](https://www.sfml-dev.org/), and with [this Chess Library](https://github.com/Disservin/chess-library).

Raphael is still a work in progress and will be updated as time goes by (though it is already quite competent). Its main features are negamax with alpha-beta pruning, a transposition table, and iterative deepening. [Scroll to the bottom](https://github.com/Orbital-Web/Raphael#raphael-1) to see a full list of features currently implemented. 

Raphael is largely inspired by [Sebastian Lague's Coding Adventure video on creating a chess engine](https://youtu.be/U4ogK0MIzqk), as well as ideas from my own chess engine I made a while back in Python (switching to C++ was already an improvement on its own...). 

<p align="center">
    <img src="https://github.com/Orbital-Web/Raphael/blob/c0396fcec6b3221369353dcabe812fb068a03534/Demo.png" alt="demo of Raphael" width=400/>
</p>



## Getting started (Windows)
1. Clone the repository
2. Install g++
3. Download SFML-2.6.0 (GCC MinGW) from https://www.sfml-dev.org/download/sfml/2.6.0/ and add it to the root directory
4. Copy `openal32.dll` from SFML-2.6.0/bin/ and add it to the root directory
5. Compile and run `main.exe` with the following commands (optionally, compile with the `-DNDEBUG` flag)
    ```
    g++ -c main.cpp -ISFML-2.6.0/include -DSFML_STATIC
    g++ -o main main.o -LSFML-2.6.0/lib -lsfml-graphics-s -lsfml-window-s -lsfml-audio-s -lsfml-system-s -lopengl32 -lfreetype -lwinmm -lgdi32 -lopenal32 -lflac -lvorbisenc -lvorbisfile -lvorbis -logg
    del main.o
    main.exe human "Adam" Raphaelv1.0.0 "Bob"
    ```



## Features
### Game Engine
The Game Engine is used to run the game. It can be initialized with an array of two players extending `cge::GamePlayer`. 

Calling `run_match()` will start a chess match between these two players.
- If `p1_is_white` is set to true, the first player will play as white, and vice versa. 
- A starting position, formatted as a [FEN string](https://www.chess.com/terms/fen-chess) must be passed into `start_fen`. These can be easily generated from the [Lichess Board Editor](https://lichess.org/editor).
- `t_remain_in` is an array with 2 floats, indicating the time remaining for each player (white and black), in seconds.
- If `is_interactive` is true, it will play sounds and keep the window open after the game finishes.

Calling `print_report()` will print out the number of wins for each players, as well as the number of draws. If `run_match()` was called multiple times, it will print out the cumulative result. 


### Game Player
The Game Player is a pure virtual function whose primary purpose is to return a move for the given game state. This class can be extended to implement custom move selection behaviours/strategies, and their performances can be compared using the `cge::GameEngine`. 


### HumanPlayer
The Human Player is an extension of `cge::GamePlayer` which will return a move based on UI interactions. Selecting a piece and clicking its destination tile will push that move forward. Note that castling may only be played by selecting the King and clicking on the castling destination tile of the King. Clicking on the King and then the Rook will not castle the King and will instead select the Rook. 


### Raphael
Raphael is an extension of `cge::GamePlayer` which at its core uses a negamax search tree to return the best move it can find. 

#### General Optimizations
- [x] Alpha-beta pruning        (`v1.0+`)
- [x] Move ordering             (`v1.0+`)
- [x] Transposition table (fix) (`v1.1+`)
- [x] Iterative deepening (fix) (`v1.1+`)
- [ ] Opening book
- [x] Quiescence with captures  (`v1.0+`)
- [ ] Quiescence with checks
- [x] Time management           (`v1.0+`)
- [x] Pondering                 (`v1.2+`)

#### Evaluation
- [x] Material cost             (`v1.0+`)
- [x] Piece-square tables       (`v1.0+`)
- [ ] Midgame King safety
- [x] Endgame King proximity    (`v1.0+`)
- [ ] Pawn structure

#### Move Ordering
- [x] Captures                  (`v1.0+`)
- [x] Promotions                (`v1.0+`)
- [ ] Checks
- [ ] Moving into attacks

## Comparisons

Different versions of the engine were put against each other in 400 matches (20 seconds each), starting from a different  position (within a ±300 centipawn stockfish evaluation) and alternating between playing as white and black. 
- `v1.0` 🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜🟥🟥🟥🟥🟥🟥🟥🟥🟥 `v1.0` [177 / 34 / 189]
- `v1.1` 🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜🟥🟥🟥🟥🟥🟥 `v1.0` [245 / 39 / 116]
- `v1.2` 🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜🟥🟥🟥🟥🟥🟥 `v1.0` [253 / 34 / 113]

The estimated [ELO](https://www.chessprogramming.org/Match_Statistics#Elo-Rating_.26_Win-Probability) of the different versions in relation to one another is as follows (with `v1.0` set to an ELO of 1000). Note that these ELO do not reflect each version's strength against human players. Rather, they are just used as a way to compare each version's performance. 
<table>
    <tr align="center">
        <th>Version</th>
        <th>ELO</th>
    </tr>
    <!--Results-->
    <tr align="center">
        <td>v1.0</td>
        <td>1000</td>
    <tr>
    <tr align="center">
        <td>v1.1</td>
        <td>1116</td>
    <tr>
    <tr align="center">
        <td>v1.2</td>
        <td>1127</td>
    <tr>
</table>

**Detailed Results**
<table>
    <tr align="center">
        <th rowspan="2">Player</th>
        <th colspan="3"">Wins</th>
        <th rowspan="2">Draws</th>
        <th colspan="3"">Losses</th>
        <th rowspan="2">Opponent</th>
    </tr>
    <tr align="center">
        <th>White</th>
        <th>Black</th>
        <th>Timeout</th>
        <th>White</th>
        <th>Black</th>
        <th>Timeout</th>
    </tr>
    <!--Results-->
    <tr align="center">
        <td>v1.0</td>
        <td>99</td>
        <td>62</td>
        <td>16</td>
        <td>34</td>
        <td>106</td>
        <td>56</td>
        <td>27</td>
        <td>v1.0</td>
    </tr>
    <tr align="center">
        <td>v1.1</td>
        <td>117</td>
        <td>81</td>
        <td>47</td>
        <td>39</td>
        <td>53</td>
        <td>63</td>
        <td>0</td>
        <td>v1.0</td>
    </tr>
    <tr align="center">
        <td>v1.2</td>
        <td>135</td>
        <td>84</td>
        <td>34</td>
        <td>34</td>
        <td>62</td>
        <td>51</td>
        <td>0</td>
        <td>v1.0</td>
    </tr>
    <tr align="center">
        <td>v1.2</td>
        <td>115</td>
        <td>93</td>
        <td>6</td>
        <td>30</td>
        <td>90</td>
        <td>57</td>
        <td>9</td>
        <td>v1.1</td>
    </tr>
</table>