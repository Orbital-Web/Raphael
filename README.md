# Raphael

**Raphael** is a UCI Chess Engine built using C++ and [Disservin's Chess Library](https://github.com/Disservin/chess-library). It also comes with a GUI built using [SFML](https://www.sfml-dev.org/).

**Raphael** is a hobby project that is still a work in progress, but it will be updated as time goes by. It has comparable strengths to a human candidate, FIDE, or international master, and performs decently against other [CCRL-ranked](https://www.computerchess.org.uk/ccrl/404/) chess engines. You can [scroll to the bottom](https://github.com/Orbital-Web/Raphael#raphael-engine) to see a list of features currently implemented.

**Raphael** is largely inspired by [Sebastian Lague's Coding Adventure series on implementing a Chess Engine](https://youtu.be/U4ogK0MIzqk), and is a revisit/successor to a previous engine I coded in Python.

*Note: v1.8 will be the last of the minor releases to **Raphael**. The next major release will be v2.0 using a custom NNUE evaluator currently in the works of being trained on evaluations from my own engine.*

<p align="center">
    <img src="https://github.com/Orbital-Web/Raphael/blob/8667a6f6db60c5cacce297145246f89a22fa5333/Demo.png" alt="demo of Raphael" width=400/>
</p>

## ELO

**[Estimated CCRL 40/2 ELO](https://www.computerchess.org.uk/ccrl/404/): 2084**

To estimate **Raphael's** [ELO](https://www.computerchess.org.uk/ccrl/404/), I paired it up against several other engines in a 10 rounds 40/2 gauntlet tournament inside of [Arena](http://www.playwitharena.de), incrementally updating **Raphael's** ELO using the [this calculator](https://www.omnicalculator.com/sports/elo#a-detailed-analysis-the-meaning-of-elo-coefficients) based on the statistical model between [win probability and ELO](https://www.chessprogramming.org/Match_Statistics#Elo-Rating_.26_Win-Probability).

**Raphaelv1.8** was matched against [**Claudia**](https://github.com/antoniogarro/Claudia) (1846), [**Monarch 1.7**](http://www.monarchchess.com/index.html) (2008), and [**BBChess 1.1**](https://github.com/maksimKorzh/bbc) (2024), and the results were a WDL of 7-3-0 (+36.8), 6-0-4 (+21.5), and 7-2-1 (+59.9), respectively. Previously, the ELO of **Raphaelv1.7.6** was estimated to be around 1966, thus the estimated ELO of **Raphaelv1.8** is around 2084.

Note that this method of ELO estimation is very crude, as it only only compares against a few other engines with only 10 rounds. In the future, I will conduct a more thorough comparison.

Past ELOs
<table>
    <tr align="center">
        <th>Version</th>
        <th>CCRL 40/2</th>
    </tr>
    <tr align="center"><td>1.8.0</td><td>2084</td></tr>
    <tr align="center"><td>1.7.6</td><td>1966</td></tr>
    <tr align="center"><td>1.7.0</td><td>1865</td></tr>
    <tr align="center"><td>1.6.0</td><td>1797</td></tr>
    <tr align="center"><td>1.5.0</td><td>1764</td></tr>
</table>
<br/><br/>

## Getting Started

Builds for Windows and Ubuntu/WSL are available on the [Releases](https://github.com/Orbital-Web/Raphael/releases) page.

The UCI engine is a standalone executable. The GUI is in `main.zip` and should be extracted and kept in the `main` folder to ensure the executable can correctly find its dependencies (you may rename the folder). If you are on Ubuntu, please run `sudo apt install libsfml-dev` as the build is dynamically linked.

Please see the sections below on how to compile the code yourself if the executables do not work for you.

You can start a quick GUI match against yourself and **Raphael** as follows:

```shell
main.exe human "Human" Raphael "Raphael" -s "game.pgn"  # Windows
./main human "Human" Raphael "Raphael" -s "game.pgn"    # Ubuntu/WSL
```

You can see other command-line arguments by running `main.exe -h`. The UCI engine has no command-line arguments.

### Compiling on Ubuntu/WSL (Recommended)

This is the recommended way of compiling **Raphael**. If you are on Windows, you can install [WSL](https://learn.microsoft.com/en-us/windows/wsl/install) to follow these steps (it may be easier than the compilation steps described in the Windows portion).

1. Clone the repository with

    ```shell
    git clone https://github.com/Orbital-Web/Raphael.git --recurse-submodules
    ```

2. Ensure you have Make and g++ installed. You can do so by running

   ```shell
   sudo apt-get install build-essential
   sudo apt-get install g++
   ```

3. Compile as follows:

    ```shell
    make packages  # install dependencies (SFML)
    make main      # build GUI
    make uci       # build UCI engine
    ```

### Compiling on Windows

If Ubuntu/WSL does not work for you, or you would like to compile the code statically, you can follow these steps:

1. Clone the repository with

    ```shell
    git clone https://github.com/Orbital-Web/Raphael.git --recurse-submodules
    ```

2. Download [SFML-2.6.0 GCC 13.1.0 MinGW 64-bit](https://www.sfml-dev.org/download/sfml/2.6.0/) and add it to the root directory
3. Copy `openal32.dll` from `SFML-2.6.0/bin/` and add it to the root directory
4. Compile dependencies with the following commands (in the root directory)

    ```shell
    cd src/GameEngine
    g++ -c -O3 -march=native -DNDEBUG consts.cpp GameEngine.cpp GamePlayer.cpp HumanPlayer.cpp utils.cpp -I"../../src" -I"../../chess-library/src" -I"../../SFML-2.6.0/include" -DSFML_STATIC
    cd ../../src/Raphael
    g++ -c -O3 -march=native -DNDEBUG consts.cpp History.cpp Killers.cpp SEE.cpp Transposition.cpp nnue.cpp simd.cpp Raphaelv1.0.cpp Raphaelv1.8.cpp Raphaelv2.0.cpp -Isrc -Ichess-library/src -I"../../src" -I"../../chess-library/src" -I"../../SFML-2.6.0/include" -DSFML_STATIC
    cd ../../
    ```

5. Compile `main.exe` with the following commands (optionally, compile with the `-DMUTEEVAL` flag to mute evaluations)

    ```shell
    g++ -c -O3 -march=native -DNDEBUG main.cpp -Isrc -Ichess-library/src -I"SFML-2.6.0/include" -DSFML_STATIC
    g++ -o main main.o src/GameEngine/consts.o src/GameEngine/GameEngine.o src/GameEngine/GamePlayer.o src/GameEngine/HumanPlayer.o src/GameEngine/utils.o src/Raphael/consts.o src/Raphael/History.o src/Raphael/Killers.o src/Raphael/See.o src/Raphael/Transposition.o src/Raphael/nnue.o src/Raphael/simd.o -L"SFML-2.6.0/lib" -lsfml-graphics-s -lsfml-window-s -lsfml-audio-s -lsfml-system-s -lopengl32 -lfreetype -lwinmm -lgdi32 -lopenal32 -lflac -lvorbisenc -lvorbisfile -lvorbis -logg -static
    ```

6. Compile the UCI engine with the following commands

    ```shell
    g++ -c -O3 -march=native -DNDEBUG uci.cpp -Isrc -Ichess-library/src -I"SFML-2.6.0/include" -DSFML_STATIC
    g++ -o uci uci.o src/GameEngine/consts.o src/GameEngine/GamePlayer.o src/Raphael/consts.o src/Raphael/History.o src/Raphael/Killers.o src/Raphael/See.o src/Raphael/Transposition.o src/Raphael/nnue.o src/Raphael/simd.o -L"SFML-2.6.0/lib" -lsfml-graphics-s -static
    ```

<br/><br/>

## Features

### Graphics User Interface (GUI)

The GUI is a quick and easy way to start engine battles or play against Raphael interactively. You can play against Raphael by starting a match against a human player and Raphael engine in the command line (see `main.exe -h`).

The human player can move a piece by either dragging and dropping a piece to the destination square, or by clicking a piece and clicking the destination square. Castling can be done by clicking the destination square of the king after castling. Only promotion by queening is currently supported. You can also annotate the board with arrows by holding and dragging the right mouse button.

You can also play with different time controls, increments, and player combinations. Again, please refer to `main.exe -h` and the [setup instructions above](https://github.com/Orbital-Web/Raphael#getting-started) for a more in-depth guide.

### Raphael (Engine)

**Raphael** is a UCI-compliant chess engine. To use it in other UCI-compliant softwares, compile `uci.cpp` using the [instructions above](https://github.com/Orbital-Web/Raphael#getting-started). The UCI engine currently supports the following commands: `uci`, `isready`, `ucinewgame`, `stop`, `quit`, `position`, and `go [wtime|btime|winc|binc|depth|nodes|movestogo|movetime|infinite]`. Pondering is not implemented yet in the UCI engine, though it does come in the GUI version. The engine contains the following features:

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
- [x] Pondering with pv         (`v1.6+`)
- [x] Check extensions          (`v1.4+`)
- [x] Passed pawn extensions    (`v1.4+`)
- [x] One reply extensions      (`v1.7+`)
- [x] Late move reductions      (`v1.5+`)
- [x] Mate distance pruning     (`v1.6+`)
- [x] SEE pruning               (`v1.7+`)
- [ ] Lazy SMP

#### Evaluation

- [x] Materials                 (`v1.0+`)
- [x] Piece-square tables       (`v1.0+`)
- [ ] Midgame King safety
- [ ] Endgame King opposition
- [x] Endgame King proximity    (`v1.0+`)
- [x] Evaluation tapering       (`v1.0+`)
- [x] Passed Pawn               (`v1.3+`)
- [x] Isolated Pawn             (`v1.3+`)
- [x] Mobility                  (`v1.5+`)
- [x] Bishop pair               (`v1.8+`)
- [x] Bishop-colored corner     (`v1.8+`)
- [x] Draw evaluation           (`v1.8+`)
- [x] Evaluation tuning         (`v1.8+`)
- [ ] NNUE
- [ ] NNUE feature factorization
- [ ] NNUE PSQT factorization

#### Move Ordering

- [x] MVV-LVA                   (`v1.0+`)
- [x] Promotions                (`v1.0+`)
- [x] Hash move                 (`v1.6+`)
- [x] Killer heuristics         (`v1.3+`)
- [x] History heuristics        (`v1.5+`)
- [x] SEE                       (`v1.7+`)
<br/><br/>

## Comparisons

Below is the result of each new version against `v1.0` out of 400 matches (20 seconds each), starting from a different  position (within a ±300 centipawn stockfish evaluation) and alternating between playing as white and black.

<div style="display: flex; gap: 10px;">
    <p>v1.0</p>
    <svg width="400" height="30">
        <rect y="5" x="0"   width="177" height="20" style="fill:#4CAF50" />
        <rect y="5" x="177" width="34"  height="20" style="fill:#FFC107" />
        <rect y="5" x="211" width="189" height="20" style="fill:#F44336" />
    </svg>
    <p>v1.0 [177 / 34 / 189]</p>
</div>
<div style="display: flex; gap: 10px;">
    <p>v1.1</p>
    <svg width="400" height="30">
        <rect y="5" x="0"   width="245" height="20" style="fill:#4CAF50" />
        <rect y="5" x="245" width="39"  height="20" style="fill:#FFC107" />
        <rect y="5" x="284" width="116" height="20" style="fill:#F44336" />
    </svg>
    <p>v1.0 [245 / 39 / 116]</p>
</div>
<div style="display: flex; gap: 10px;">
    <p>v1.2</p>
    <svg width="400" height="30">
        <rect y="5" x="0"   width="253" height="20" style="fill:#4CAF50" />
        <rect y="5" x="253" width="34"  height="20" style="fill:#FFC107" />
        <rect y="5" x="287" width="113" height="20" style="fill:#F44336" />
    </svg>
    <p>v1.0 [253 / 34 / 113]</p>
</div>
<div style="display: flex; gap: 10px;">
    <p>v1.3</p>
    <svg width="400" height="30">
        <rect y="5" x="0"   width="301" height="20" style="fill:#4CAF50" />
        <rect y="5" x="301" width="23"  height="20" style="fill:#FFC107" />
        <rect y="5" x="324" width="76"  height="20" style="fill:#F44336" />
    </svg>
    <p>v1.0 [301 / 23 / 76]</p>
</div>
<div style="display: flex; gap: 10px;">
    <p>v1.4</p>
    <svg width="400" height="30">
        <rect y="5" x="0"   width="333" height="20" style="fill:#4CAF50" />
        <rect y="5" x="333" width="25"  height="20" style="fill:#FFC107" />
        <rect y="5" x="358" width="42"  height="20" style="fill:#F44336" />
    </svg>
    <p>v1.0 [333 / 25 / 42]</p>
</div>
<div style="display: flex; gap: 10px;">
    <p>v1.5</p>
    <svg width="400" height="30">
        <rect y="5" x="0"   width="344" height="20" style="fill:#4CAF50" />
        <rect y="5" x="344" width="23"  height="20" style="fill:#FFC107" />
        <rect y="5" x="367" width="33"  height="20" style="fill:#F44336" />
    </svg>
    <p>v1.0 [344 / 23 / 33]</p>
</div>
<div style="display: flex; gap: 10px;">
    <p>v1.6</p>
    <svg width="400" height="30">
        <rect y="5" x="0"   width="355" height="20" style="fill:#4CAF50" />
        <rect y="5" x="355" width="27"  height="20" style="fill:#FFC107" />
        <rect y="5" x="382" width="18"  height="20" style="fill:#F44336" />
    </svg>
    <p>v1.0 [355 / 27 / 18]</p>
</div>
<div style="display: flex; gap: 10px;">
    <p>v1.7</p>
    <svg width="400" height="30">
        <rect y="5" x="0"   width="374" height="20" style="fill:#4CAF50" />
        <rect y="5" x="374" width="20"  height="20" style="fill:#FFC107" />
        <rect y="5" x="394" width="6"   height="20" style="fill:#F44336" />
    </svg>
    <p>v1.0 [374 / 20 / 6]</p>
</div>
<div style="display: flex; gap: 10px;">
    <p>v1.8</p>
    <svg width="400" height="30">
        <rect y="5" x="0"   width="380" height="20" style="fill:#4CAF50" />
        <rect y="5" x="380" width="16"  height="20" style="fill:#FFC107" />
        <rect y="5" x="396" width="4"   height="20" style="fill:#F44336" />
    </svg>
    <p>v1.0 [380 / 16 / 4]</p>
</div>

And below are the more detailed comparisons.
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
    <tr align="center">
        <td>v1.6</td>
        <td>169</td>
        <td>178</td>
        <td>8</td>
        <td>27</td>
        <td>5</td>
        <td>13</td>
        <td>0</td>
        <td>v1.0</td>
    </tr>
    <tr align="center">
        <td>v1.6</td>
        <td>122</td>
        <td>127</td>
        <td>2</td>
        <td>77</td>
        <td>33</td>
        <td>39</td>
        <td>0</td>
        <td>v1.5</td>
    </tr>
    <tr align="center">
        <td>v1.7</td>
        <td>182</td>
        <td>187</td>
        <td>5</td>
        <td>20</td>
        <td>2</td>
        <td>4</td>
        <td>0</td>
        <td>v1.0</td>
    </tr>
    <tr align="center">
        <td>v1.7</td>
        <td>92</td>
        <td>98</td>
        <td>0</td>
        <td>108</td>
        <td>53</td>
        <td>49</td>
        <td>0</td>
        <td>v1.6</td>
    </tr>
    <tr align="center">
        <td>v1.8</td>
        <td>187</td>
        <td>191</td>
        <td>2</td>
        <td>16</td>
        <td>2</td>
        <td>2</td>
        <td>0</td>
        <td>v1.0</td>
    </tr>
    <tr align="center">
        <td>v1.8</td>
        <td>99</td>
        <td>98</td>
        <td>0</td>
        <td>100</td>
        <td>45</td>
        <td>53</td>
        <td>5</td>
        <td>v1.7</td>
    </tr>
</table>

*Note: a timeout usually means that the game was relatively equal*
