# Raphael
A GUI-based Chess Player and Chess Engine (Raphael), coded in C++, with [SFML](https://www.sfml-dev.org/), and [Disservin's Chess Library](https://github.com/Disservin/chess-library).

Raphael is still a work in progress and will be updated as time goes by (though it's already quite competent). Its main features are negamax with alpha-beta pruning, a transposition table, and iterative deepening. [Scroll to the bottom](https://github.com/Orbital-Web/Raphael#raphael-1) to see a full list of features currently implemented. There are currently no UCI support (comming soon). 

Raphael is largely inspired by [Sebastian Lague's Coding Adventure series on implementing a Chess Engine](https://youtu.be/U4ogK0MIzqk), as well as ideas from my own chess engine I made a while back in Python.

My goal is to eventually implement NNUE-based evaluations and compare its ELO with other engines. 

<p align="center">
    <img src="https://github.com/Orbital-Web/Raphael/blob/8667a6f6db60c5cacce297145246f89a22fa5333/Demo.png" alt="demo of Raphael" width=400/>
</p>



## Getting started (Windows)
1. Clone the repository (*make sure to use `--recurse-submodules` when pulling too*)
    ```
    git clone https://github.com/Orbital-Web/Raphael.git --recurse-submodules
    ```
2. Download [SFML-2.6.0 (GCC MinGW)](https://www.sfml-dev.org/download/sfml/2.6.0/) and add it to the root directory
3. Copy `openal32.dll` from `SFML-2.6.0/bin/` and add it to the root directory
4. Compile and run `main.exe` with the following commands (optionally, compile with the `-DNDEBUG` flag to mute evaluations)
    ```
    g++ -c main.cpp -Isrc -Ichess-library/src -ISFML-2.6.0/include -DSFML_STATIC
    g++ -o main main.o -LSFML-2.6.0/lib -lsfml-graphics-s -lsfml-window-s -lsfml-audio-s -lsfml-system-s -lopengl32 -lfreetype -lwinmm -lgdi32 -lopenal32 -lflac -lvorbisenc -lvorbisfile -lvorbis -logg
    del main.o
    main.exe human "Human" Raphael "Raphael"
    ```
5. Try out other features (`main.exe -help`)

The compilation process should be similar for Linux and macOS, though setting up SFML may be slightly different. Please refer to the [official SFML documentation](https://www.sfml-dev.org/tutorials/2.6/).



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
The Human Player is an extension of `cge::GamePlayer` which will return a move based on UI interactions. Selecting a piece and clicking (or dragging to) its destination tile will push that move forward. Note that castling may only be played by selecting the King and clicking on the castling destination tile of the King. Clicking on the King and then the Rook will not castle the King and will instead select the Rook. 


### Raphael
Raphael is an extension of `cge::GamePlayer` which at its core uses a negamax search tree to return the best move it can find. 

#### General Optimizations
- [x] Alpha-beta pruning        (`v1.0+`)
- [x] Move ordering             (`v1.0+`)
- [x] Transposition table (fix) (`v1.1+`)
- [x] Iterative deepening (fix) (`v1.1+`)
- [x] Aspiration window         (`v1.3+`)
- [ ] Opening book
- [ ] Endgame table
- [x] Quiescence with captures  (`v1.0+`)
- [ ] Quiescence with queening
- [x] Time management           (`v1.0+`)
- [x] Pondering                 (`v1.2+`)
- [ ] Check extensions
- [ ] Promotion extensions

#### Evaluation
- [x] Materials                 (`v1.0+`)
- [x] Piece-square tables       (`v1.0+`)
- [ ] Midgame King safety
- [x] Endgame King proximity    (`v1.0+`)
- [ ] Evaluation tapering       (`v1.0+`)
- [x] Passed Pawn               (`v1.3+`)
- [x] Isolated Pawn             (`v1.3+`)
- [ ] Mobility
- [x] Rook on open file         (`v1.4+`)
- [ ] NNUE

#### Move Ordering
- [x] MVV-LVA                   (`v1.0+`)
- [x] Promotions                (`v1.0+`)
- [x] Previous iteration        (`v1.0+`) 
- [x] Killer heuristics         (`v1.3+`)
- [ ] Checks
- [ ] Moving into attacks

## Comparisons

Different versions of the engine were put against each other in 400 matches (20 seconds each), starting from a different  position (within a ±300 centipawn stockfish evaluation) and alternating between playing as white and black. 
- `v1.0` 🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜🟥🟥🟥🟥🟥🟥🟥🟥🟥 `v1.0` [177 / 34 / 189]
- `v1.1` 🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜🟥🟥🟥🟥🟥🟥 `v1.0` [245 / 39 / 116]
- `v1.2` 🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜🟥🟥🟥🟥🟥🟥 `v1.0` [253 / 34 / 113]
- `v1.3` 🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜🟥🟥🟥🟥 `v1.0` [301 / 23 / 76]

The estimated [ELO](https://www.chessprogramming.org/Match_Statistics#Elo-Rating_.26_Win-Probability) of the different versions in relation to one another is as follows (with `v1.0` set to an ELO of 1000). Note that these ELO do not reflect each version's strength against humans and other engines. Rather, they are just used as a way to compare each version's performance.
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
    <tr align="center">
        <td>v1.3</td>
        <td>1221</td>
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
        <td>91</td>
        <td>70</td>
        <td>16</td>
        <td>34</td>
        <td>92</td>
        <td>70</td>
        <td>27</td>
        <td>v1.0</td>
    </tr>
    <tr align="center">
        <td>v1.1</td>
        <td>94</td>
        <td>104</td>
        <td>47</td>
        <td>39</td>
        <td>53</td>
        <td>63</td>
        <td>0</td>
        <td>v1.0</td>
    </tr>
    <tr align="center">
        <td>v1.2</td>
        <td>115</td>
        <td>104</td>
        <td>34</td>
        <td>34</td>
        <td>62</td>
        <td>51</td>
        <td>0</td>
        <td>v1.0</td>
    </tr>
    <tr align="center">
        <td>v1.2</td>
        <td>112</td>
        <td>96</td>
        <td>6</td>
        <td>30</td>
        <td>86</td>
        <td>61</td>
        <td>9</td>
        <td>v1.1</td>
    </tr>
    <tr align="center">
        <td>v1.3</td>
        <td>144</td>
        <td>120</td>
        <td>37</td>
        <td>23</td>
        <td>43</td>
        <td>32</td>
        <td>1</td>
        <td>v1.0</td>
    </tr>
    <tr align="center">
        <td>v1.3</td>
        <td>107</td>
        <td>107</td>
        <td>10</td>
        <td>38</td>
        <td>61</td>
        <td>61</td>
        <td>16</td>
        <td>v1.2</td>
    </tr>
</table>

*Note: a timeout usually means that the game was relatively equal*