# Raphael
Both a UCI Chess Engine (**Raphael**) and a Chess GUI (to play against **Raphael** or to make **Raphael** play itself), coded in C++, using [SFML](https://www.sfml-dev.org/) and [Disservin's Chess Library](https://github.com/Disservin/chess-library).

**Raphael** is still a work in progress and will be updated as time goes by (though it's already quite competent). [Scroll to the bottom](https://github.com/Orbital-Web/Raphael#raphael-engine) to see a list of features currently implemented.

**Raphael** is largely inspired by [Sebastian Lague's Coding Adventure series on implementing a Chess Engine](https://youtu.be/U4ogK0MIzqk), and is a revisit/successor to a previous engine I coded in Python. 

*Note: v1.6 will be the last of the minor releases to **Raphael**. The next major release will be v2.0 using a custom NNUE evaluation function.*

<p align="center">
    <img src="https://github.com/Orbital-Web/Raphael/blob/8667a6f6db60c5cacce297145246f89a22fa5333/Demo.png" alt="demo of Raphael" width=400/>
</p>



## Getting started (Windows)
1. Clone the repository (*make sure to use `--recurse-submodules` when pulling too*)
    ```
    git clone https://github.com/Orbital-Web/Raphael.git --recurse-submodules
    ```
2. Download [SFML-2.6.0 (GCC MinGW Version)](https://www.sfml-dev.org/download/sfml/2.6.0/) and add it to the root directory
3. Copy `openal32.dll` from `SFML-2.6.0/bin/` and add it to the root directory
4. Compile and run `main.exe` with the following commands (optionally, compile with the `-DMUTEEVAL` flag to mute evaluations)
    ```
    g++ -c main.cpp -Isrc -Ichess-library/src -ISFML-2.6.0/include -DSFML_STATIC
    g++ -o main main.o -LSFML-2.6.0/lib -lsfml-graphics-s -lsfml-window-s -lsfml-audio-s -lsfml-system-s -lopengl32 -lfreetype -lwinmm -lgdi32 -lopenal32 -lflac -lvorbisenc -lvorbisfile -lvorbis -logg
    del main.o
    main.exe human "Human" Raphael "Raphael" -s "game.pgn"
    ```
5. Try out other features (check out `main.exe -help`)

The compilation process should be similar for Linux and macOS, though the process of setting up SFML may be slightly different. Please refer to the [official SFML documentation](https://www.sfml-dev.org/tutorials/2.6/).

To compile and run the uci engine, follow steps 1~2, and use the following commands (consider also increasing the `TABLE_SIZE` to `2^24`)
```
g++ -c uci.cpp -Isrc -Ichess-library/src -ISFML-2.6.0/include -DSFML_STATIC
g++ -o uci uci.o -LSFML-2.6.0/lib -lsfml-graphics-s
del uci.o
uci.exe
```



## Features
### Game Engine (GUI)
The game engine is a combination of `GameEngine.hpp`, `GamePlayer.hpp`, and `HumanPlayer.hpp`. It is a GUI-based chess game engine (not to be confused with a chess engine) which lets the user interactively play chess. 

If using a human player, the user may annotate the board with arrows and select/play moves similarly to other chess GUIs. The user may also specify the number of rounds, different time controls, starting positions (with fens), and even swap out the players (e.g., with different versions of Raphael). 

To use it, refer to the [setup instructions above](https://github.com/Orbital-Web/Raphael#getting-started-windows), and run the command `main.exe -help` in the command line. 


### Raphael (Engine)
**Raphael** is a UCI-compliant chess engine that comes with this project. To use it in other UCI-compliant softwares, compile `uci.cpp` using the [instructions above](https://github.com/Orbital-Web/Raphael#getting-started-windows). The UCI engine currently supports the following commands: `uci`, `ready`, `ucinewgame`, `position`, and `go`. Pondering is not implemented yet. The engine contains the following features:

#### General
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
- [x] Skip search on stable pv  (`v1.6+`)
- [x] Pondering                 (`v1.2+`)
- [x] Check extensions          (`v1.4+`)
- [x] Promotion extensions      (`v1.4+`)
- [ ] Recapture extensions
- [x] Late move reductions      (`v1.5+`)

#### Evaluation
- [x] Materials                 (`v1.0+`)
- [x] Piece-square tables       (`v1.0+`)
- [ ] Midgame King safety
- [x] Endgame King proximity    (`v1.0+`)
- [x] Evaluation tapering       (`v1.0+`)
- [x] Passed Pawn               (`v1.3+`)
- [x] Isolated Pawn             (`v1.3+`)
- [x] Mobility                  (`v1.5+`)
- [ ] NNUE

#### Move Ordering
- [x] MVV-LVA                   (`v1.0+`)
- [x] Promotions                (`v1.0+`)
- [x] Previous iteration        (`v1.0+`)
- [x] Killer heuristics         (`v1.3+`)
- [x] History heuristics        (`v1.5+`)
- [ ] SEE



## ELO
**[Estimated CCRL 40/2 ELO](http://ccrl.chessdom.com/ccrl/402.archive/): 1764**

To estimate **Raphael's** [ELO](http://ccrl.chessdom.com/ccrl/402.archive/), I paired **Raphaelv1.6** (the newest version at the time of testing) against [**Shallow Blue**](https://github.com/GunshipPenguin/shallow-blue) (1734 ELO) and [**Sayuri**](https://github.com/MetalPhaeton/sayuri) (1838 ELO) inside of [Arena](http://www.playwitharena.de) in a 10 rounds 40/2 gauntlet tournament with random starting positions. 

It scored a WDL of 6-2-2 against **Shallow Blue** and 2-1-7 against **Sayuri**. I then used the approximate relation between [win probability and ELO difference](https://www.chessprogramming.org/Match_Statistics#Elo-Rating_.26_Win-Probability) to calculate the ELO difference against the 2 engines, and averaged the resulting ELO to get the final estimated ELO of 1764.    

Note that this method of ELO estimation is very crude, as it only only compares against two other engines in just 10 rounds each. In the future, I will conduct a more thorough comparison (maybe once v2.0 is out).



## Comparisons
Below is the result of each new version against `v1.0` out of 400 matches (20 seconds each), starting from a different  position (within a ±300 centipawn stockfish evaluation) and alternating between playing as white and black. 
- `v1.0` 🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜🟥🟥🟥🟥🟥🟥🟥🟥🟥 `v1.0` [177 / 34 / 189]
- `v1.1` 🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜🟥🟥🟥🟥🟥🟥 `v1.0` [245 / 39 / 116]
- `v1.2` 🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜⬜🟥🟥🟥🟥🟥🟥 `v1.0` [253 / 34 / 113]
- `v1.3` 🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜🟥🟥🟥🟥 `v1.0` [301 / 23 / 76]
- `v1.4` 🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜🟥🟥 `v1.0` [333 / 25 / 42]
- `v1.5` 🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜🟥🟥 `v1.0` [344 / 23 / 43]
- `v1.6` 🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩🟩⬜🟥 `v1.0` [360 / 20 / 20]

And below is the more detailed comparison.
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
    <tr align="center">
        <td>v1.4</td>
        <td>144</td>
        <td>136</td>
        <td>53</td>
        <td>25</td>
        <td>14</td>
        <td>27</td>
        <td>1</td>
        <td>v1.0</td>
    </tr>
    <tr align="center">
        <td>v1.4</td>
        <td>117</td>
        <td>114</td>
        <td>13</td>
        <td>40</td>
        <td>57</td>
        <td>49</td>
        <td>10</td>
        <td>v1.3</td>
    </tr>
    <tr align="center">
        <td>v1.5</td>
        <td>156</td>
        <td>154</td>
        <td>34</td>
        <td>23</td>
        <td>15</td>
        <td>18</td>
        <td>0</td>
        <td>v1.0</td>
    </tr>
    <tr align="center">
        <td>v1.5</td>
        <td>99</td>
        <td>99</td>
        <td>13</td>
        <td>62</td>
        <td>61</td>
        <td>60</td>
        <td>6</td>
        <td>v1.4</td>
    </tr>
</table>

*Note: a timeout usually means that the game was relatively equal*