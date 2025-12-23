# Raphael

**Raphael** is a UCI Chess Engine built using C++ and [Disservin's Chess Library](https://github.com/Disservin/chess-library). It also comes with a GUI built using [SFML](https://www.sfml-dev.org/).

**Raphael** is a hobby project that is still a work in progress, but it will be updated as time goes by. It has comparable strengths to a human candidate, FIDE, or international master, and performs decently against other [CCRL-ranked](https://www.computerchess.org.uk/ccrl/404/) chess engines. You can [scroll to the bottom](https://github.com/Orbital-Web/Raphael#raphael-engine) to see a list of features currently implemented.

**Raphael** is largely inspired by [Sebastian Lague's Coding Adventure series on implementing a Chess Engine](https://youtu.be/U4ogK0MIzqk), and is a revisit/successor to a previous engine I coded in Python.

<p align="center">
    <img src="https://github.com/Orbital-Web/Raphael/blob/8667a6f6db60c5cacce297145246f89a22fa5333/Demo.png" alt="demo of Raphael" width=400/>
</p>

## ELO

The following are the historic **[CCRL 40/2 ELO](https://www.computerchess.org.uk/ccrl/404/)** for Raphael. Note that italicized ELOs are estimates and not official. These ELOs were estimated using [fastchess](https://github.com/Disservin/fastchess).

<table>
    <tr align="center">
        <th>Version</th>
        <th>CCRL 40/2</th>
    </tr>
    <tr align="center"><td>2.0.0</td><td><i>2273</i></td></tr>
    <tr align="center"><td>1.8.0</td><td><i>2084</i></td></tr>
    <tr align="center"><td>1.7.6</td><td>1967</td></tr>
    <tr align="center"><td>1.7.0</td><td><i>1865</i></td></tr>
    <tr align="center"><td>1.6.0</td><td><i>1797</i></td></tr>
    <tr align="center"><td>1.5.0</td><td><i>1764</i></td></tr>
</table>

## Getting Started

Builds of the UCI engine for Windows and Ubuntu/WSL are available on the [Releases](https://github.com/Orbital-Web/Raphael/releases) page.

Please refer to the [following section](#compiling-on-ubuntuwsl-recommended) to compile the GUI and/or the engine yourself on Windows or Linux.

With the GUI compiled, you can start a quick GUI match against yourself and **Raphael** as follows:

```shell
main.exe human "Human" Raphael "Raphael" -s "game.pgn"  # Windows
./main human "Human" Raphael "Raphael" -s "game.pgn"    # Ubuntu/WSL
```

You can see other command-line arguments by running `main.exe -h`. The UCI engine has no command-line arguments.

### Compiling on Ubuntu/WSL (Recommended)

This is the recommended way of compiling **Raphael**. If you are on Windows, you can install [WSL](https://learn.microsoft.com/en-us/windows/wsl/install) to follow these steps:

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
    make main      # build GUI (note this will also install SFML)
    make uci       # build UCI engine
    ```

<!-- ### Compiling on Windows
FIXME: update SFML to 3.0 so we can use makefile and easily compile on Windows too

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
    ``` -->

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
- [x] NNUE                      (`v2.0+`)
- [ ] NNUE lazy updates
- [ ] NNUE output bucket

#### Move Ordering

- [x] MVV-LVA                   (`v1.0+`)
- [x] Promotions                (`v1.0+`)
- [x] Hash move                 (`v1.6+`)
- [x] Killer heuristics         (`v1.3+`)
- [x] History heuristics        (`v1.5+`)
- [x] SEE                       (`v1.7+`)

## Special Thanks to

Those on the Stockfish Discord for helping me out with various aspects of the NNUE dataset collection and training. Without their help, my NNUE would have been very very crappy.
