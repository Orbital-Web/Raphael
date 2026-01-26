# Raphael

**Raphael** is a UCI Chess Engine built using C++ and [Disservin's Chess Library](https://github.com/Disservin/chess-library). It also comes with a GUI built using [SFML](https://www.sfml-dev.org/).

**Raphael** is a hobby project that is still a work in progress, but it plays decent chess. You can [scroll to the bottom](#raphael-engine) to see a list of features currently implemented, and also download the prebuilt binaries to try Raphael out for yourself.

**Raphael** is largely inspired by [Sebastian Lague's Coding Adventure series on implementing a Chess Engine](https://youtu.be/U4ogK0MIzqk), and is a revisit/successor to a previous engine I coded in Python.

<p align="center">
    <img src="https://github.com/Orbital-Web/Raphael/blob/8667a6f6db60c5cacce297145246f89a22fa5333/Demo.png" alt="demo of Raphael" width=400/>
</p>

## Elo

The following are the historic Elo for Raphael.

Note that Elos with an asterics next to them are estimates and not official, and were estimated by running tournaments against [Stash](https://gitlab.com/mhouppin/stash-bot/-/releases) (and sometimes a few other engines) using [fastchess](https://github.com/Disservin/fastchess).

<table>
    <tr align="center">
        <th>Version</th>
        <th><a href="https://www.computerchess.org.uk/ccrl/404/">CCRL Blitz</a></th>
        <th><a href="https://www.computerchess.org.uk/ccrl/4040/">CCRL 40/15</a></th>
    </tr>
    <tr align="center"><td>2.3.0</td> <td>3146*</td> <td>3100*</td></tr>
    <tr align="center"><td>2.2.0</td> <td>3035*</td> <td>2954</td></tr>
    <tr align="center"><td>2.1.0</td> <td>2739*</td> <td>2692</td></tr>
    <tr align="center"><td>2.0.0</td> <td>2646*</td> <td></td></tr>
    <tr align="center"><td>1.8.0</td> <td>2223*</td> <td></td></tr>
    <tr align="center"><td>1.7.6</td> <td>1967</td>  <td></td></tr>
    <tr align="center"><td>1.7.0</td> <td>1851</td>  <td></td></tr>
    <tr align="center"><td>1.6.0</td> <td>1797*</td> <td></td></tr>
    <tr align="center"><td>1.5.0</td> <td>1764*</td> <td></td></tr>
</table>

## Getting Started

Builds of the UCI engine for Windows and Linux/WSL are available on the [Releases](https://github.com/Orbital-Web/Raphael/releases) page. In general, you should use the `avx2-bmi2` build (on pre-Zen 3 AMD CPUs, the `avx2` build can be significantly faster than `avx2-bmi2` build).

Please refer to the [following section](#compiling-from-source) to compile the GUI and/or the engine yourself on Windows and Linux.

With the GUI compiled, you can start a quick GUI match against yourself and **Raphael** as follows:

```shell
main.exe human "Human" Raphael "Raphael" -s "game.pgn"  # Windows
./main human "Human" Raphael "Raphael" -s "game.pgn"    # Linux/WSL
```

You can see other command-line arguments by running `main.exe -h`. The UCI engine has no command-line arguments.

### Compiling From Source

Follow these steps to build Raphael yourself. Note that it is **highly recommended** you build on [WSL](https://learn.microsoft.com/en-us/windows/wsl/install) if you are on Windows.

1. Clone the repository with

    ```shell
    git clone https://github.com/Orbital-Web/Raphael.git --recurse-submodules
    ```

2. Ensure you have Make and g++ installed. If you are on Linux/WSL, you can do so by running:

   ```shell
   sudo apt-get install build-essential g++  # Linux/WSL
   ```

   Otherwise,  if you are on Windows, follow [this guide](https://code.visualstudio.com/docs/cpp/config-mingw) to install MSYS2 and run the following command inside the MSYS2 UCRT64 terminal:

   ```shell
   pacman -S --needed mingw-w64-ucrt-x86_64-toolchain make  # Windows
   ```

3. Compile as follows:

    ```shell
    make uci       # build UCI engine
    make packages  # download SFML, required to build main
    make main      # build GUI
    ```

## Features

### Graphics User Interface (GUI)

The GUI is a quick and easy way to start engine battles or play against Raphael interactively. You can play against Raphael by starting a match against a human player and Raphael engine in the command line (see `main.exe -h`).

The human player can move a piece by either dragging and dropping a piece to the destination square, or by clicking a piece and clicking the destination square. Castling can be done by clicking the destination square of the king after castling. Only promotion by queening is currently supported. You can also annotate the board with arrows by holding and dragging the right mouse button.

You can also play with different time controls, increments, and player combinations. Again, please refer to `main.exe -h` and the [setup instructions above](#getting-started) for a more in-depth guide.

### Raphael (Engine)

**Raphael** is a UCI-compliant chess engine. To use it in other UCI-compliant softwares, compile `uci.cpp` using the [instructions above](#getting-started). The UCI engine currently supports the following commands: `uci`, `isready`, `ucinewgame`, `stop`, `quit`, `position`, and `go [wtime|btime|winc|binc|depth|nodes|movestogo|movetime|infinite]`. Pondering is not implemented yet in the UCI engine, though it does come in the GUI version. The engine contains the following features:

- [x] Search                                (`v1.0+`)
  - [x] Iterative deepening                 (`v1.1+`)
  - [x] Aspiration window                   (`v1.3+`)
  - [ ] Endgame table base
- [x] Alpha-beta search                     (`v1.0+`)
  - [x] Pruning                             (`v1.0+`)
    - [x] Alpha-beta pruning                (`v1.0+`)
    - [x] Transposition table cutoff        (`v1.1+`)
    - [x] Mate distance pruning             (`v1.6+`)
    - [x] Reverse futility pruning          (`v2.2+`)
    - [x] Null move pruning                 (`v2.2+`)
    - [x] Razoring                          (`v2.3+`)
    - [ ] Probcut
    - [x] Late move pruning                 (`v2.3+`)
    - [x] Futility pruning                  (`v2.3+`)
    - [x] SEE pruning                       (`v2.3+`)
    - [ ] History pruning
    - [ ] Multi-cut
    - [ ] Improving heuristics
  - [x] Transposition table                 (`v1.1+`)
    - [x] Prefetching                       (`v2.2+`)
  - [x] Principle variation search          (`v2.1+`)
  - [x] Extensions                          (`v1.4+`)
    - [x] Check extensions                  (`v1.4+`)
    - ~~Pawn push extensions~~              (`v1.4+`)
    - [x] One reply extensions              (`v1.7+`)
    - [ ] Singular extensions
  - [x] Reductions                          (`v1.5+`)
    - [x] Late move reductions              (`v1.5+`)
    - [ ] Internal iterative reduction
  - [x] Move ordering                       (`v1.0+`)
    - [x] MVV-LVA                           (`v1.0+`)
    - [x] Promotions                        (`v1.0+`)
    - [x] Hash move                         (`v1.6+`)
    - [x] Killer heuristics                 (`v1.3+`)
    - [x] Butterfly history                 (`v1.5+`)
    - [x] Capture history                   (`v2.3+`)
    - [ ] Continuation history
    - [ ] Correction history
    - [ ] Threats in history
    - [x] SEE                               (`v1.7+`)
- [x] Quiescence search                     (`v1.0+`)
  - ~~Delta pruning~~                       (`v2.1+`)
  - [x] Futility pruning                    (`v2.3+`)
  - [x] SEE pruning                         (`v2.3+`)
  - [ ] Transposition table probing
- [x] Evaluation                            (`v1.0+`)
  - [x] Hand-crafted evaluation             (`v1.0+`)
    - [x] Materials                         (`v1.0+`)
    - [x] Piece-square tables               (`v1.0+`)
    - [ ] Midgame King safety
    - [ ] Endgame King opposition
    - [x] Endgame King proximity            (`v1.0+`)
    - [x] Evaluation tapering               (`v1.0+`)
    - [x] Passed Pawn                       (`v1.3+`)
    - [x] Isolated Pawn                     (`v1.3+`)
    - [x] Mobility                          (`v1.5+`)
    - [x] Bishop pair                       (`v1.8+`)
    - [x] Bishop-colored corner             (`v1.8+`)
    - [x] Draw evaluation                   (`v1.8+`)
    - [x] Evaluation texel tuning           (`v1.8+`)
  - [x] NNUE                                (`v2.0+`)
    - [x] NNUE lazy updates                 (`v2.1+`)
    - [ ] NNUE output buckets
- [x] Time management                       (`v1.0+`)
- [x] Pondering                             (`v1.2+`)
- [x] Performance                           (`v1.8+`)
  - [x] Compiler optimizations              (`v1.8+`)
  - [x] Incremental selection sort          (`v2.2+`)
  - [x] Linux huge pages                    (`v2.2+`)
- [ ] Lazy SMP
- [ ] SPSA tuning

For a more in-depth documentation on the NNUE and how it was trained, refer to the [NNUE README](https://github.com/Orbital-Web/Raphael/tree/main/src/NNUE). No external engines were used to generate Raphael NNUE's training data.

## Special Thanks to

- [Sebastian Lague](https://www.youtube.com/c/SebastianLague) for inspiring me to start the development of Raphael through the Coding Adventures series
- [Disservin](https://github.com/Disservin) for creating the [C++ chess library](https://github.com/Disservin/chess-library), [fastchess](https://github.com/Disservin/fastchess), and [Python Chess Engine](https://github.com/Disservin/python-chess-engine), all of which I've used extensively while developing my engine
- Those on the Stockfish Discord for teaching me and helping me out with the NNUE dataset collection, training, and evaluation, as well as engine development in general. Without their help, my engine wouldn't be as good as it is now
- [GediminasMasaitis's Texel Tuner](https://github.com/GediminasMasaitis/texel-tuner) which I've used to tune my parameters back when I was using a HCE
