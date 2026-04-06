# Raphael

Raphael is a superhuman UCI Chess Engine built using C++. It also comes with a GUI built using [SFML](https://www.sfml-dev.org/).

You can [scroll to the bottom](#raphael-engine) to see a list of features currently implemented, and also download the prebuilt binaries to try Raphael out for yourself.

Raphael is largely inspired by [Sebastian Lague's Coding Adventure series on implementing a Chess Engine](https://youtu.be/U4ogK0MIzqk).

<p align="center">
  <img src="https://github.com/Orbital-Web/Raphael/blob/8667a6f6db60c5cacce297145246f89a22fa5333/Demo.png" alt="demo of Raphael" width=400/>
</p>

## Elo

The following are the historic elo for Raphael.

<table>
  <tr align="center">
    <th>Version</th>
    <th>Release Date</th>
    <th><a href="https://www.computerchess.org.uk/ccrl/404/cgi/compare_engines.cgi?family=Raphael">CCRL Blitz</a></th>
    <th><a href="https://www.computerchess.org.uk/ccrl/4040/cgi/compare_engines.cgi?family=Raphael">CCRL 40/15</a></th>
  </tr>
  <tr align="center"><td>3.3.0</td> <td>Apr 06, 2026</td> <td>3687*</td> <td>3554*</td></tr>
  <tr align="center"><td>3.2.0</td> <td>Mar 19, 2026</td> <td>3612*</td> <td>3483 </td></tr>
  <tr align="center"><td>3.1.0</td> <td>Mar 01, 2026</td> <td>3506 </td> <td>3416 </td></tr>
  <tr align="center"><td>3.0.0</td> <td>Feb 12, 2026</td> <td>3321*</td> <td>3206 </td></tr>
  <tr align="center"><td>2.3.0</td> <td>Jan 25, 2026</td> <td>3146*</td> <td>3061 </td></tr>
  <tr align="center"><td>2.2.0</td> <td>Jan 08, 2026</td> <td>3035*</td> <td>2953 </td></tr>
  <tr align="center"><td>2.1.0</td> <td>Dec 31, 2025</td> <td>2739*</td> <td>2689 </td></tr>
  <tr align="center"><td>2.0.0</td> <td>Dec 23, 2025</td> <td>2646*</td> <td></td></tr>
  <tr align="center"><td>1.8.0</td> <td>Dec 27, 2024</td> <td>2223*</td> <td></td></tr>
  <tr align="center"><td>1.7.6</td> <td>Dec 16, 2024</td> <td>1970 </td> <td></td></tr>
  <tr align="center"><td>1.7.0</td> <td>Aug 26, 2023</td> <td>1853 </td> <td></td></tr>
  <tr align="center"><td>1.6.0</td> <td>Aug 20, 2023</td> <td>1797*</td> <td></td></tr>
  <tr align="center"><td>1.5.0</td> <td>Aug 16, 2023</td> <td>1764*</td> <td></td></tr>
</table>
*estimated

## Getting Started

Prebuilt binaries of the UCI engine for Windows and Linux/WSL are available on the [Releases](https://github.com/Orbital-Web/Raphael/releases) page.
In general, you should use the `avx2-bmi2` build (on pre-Zen 3 AMD CPUs, the `avx2` build can be significantly faster than the `avx2-bmi2` build).

Please refer to the [following section](#compiling-from-source) to compile the GUI and/or the engine yourself on Windows and Linux.
The [features section](#features) outline the supported commands and features of the GUI and UCI engine.

### Compiling From Source

Follow these steps to build Raphael yourself. Note that it is highly recommended you build on [WSL](https://learn.microsoft.com/en-us/windows/wsl/install) if you are on Windows.

1. Clone the repository with

    ```shell
    git clone https://github.com/Orbital-Web/Raphael.git
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
    make -j uci       # build UCI engine
    make -j packages  # download SFML, required to build main
    make -j main      # build GUI
    ```

## Features

### Graphics User Interface (GUI)

The GUI is a quick and easy way to start engine battles or play against Raphael interactively.
To start a quick GUI match against yourself and Raphael as follows:

```shell
main.exe human "Human" Raphael "Raphael"
```

You can see other command-line arguments by running `main.exe -h`.
There are supports for board annotations (arrows and square highlights) using the right mouse button.

### Raphael (Engine)

Raphael is a UCI-compliant chess engine.
To use it in other UCI-compliant softwares, compile `uci.cpp` or download the prebuilt binaries using the [instructions above](#getting-started).
To see all supported commands, run `uci.exe help`

<details>

<summary>Click to show the list of implemented features:</summary>

- [x] Search                                (`v1.0+`)
  - [x] Iterative deepening                 (`v1.1+`)
  - [x] Aspiration window                   (`v1.3+`)
  - [x] Aspiration widening                 (`v3.0+`)
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
    - [ ] Quiet history pruning
    - [ ] Noisy history pruning
    - [x] Multi-cut                         (`v3.3+`)
    - [x] Improving heuristics              (`v3.2+`)
  - [x] Transposition table                 (`v1.1+`)
    - [x] Prefetching                       (`v2.2+`)
    - [x] Aging                             (`v3.0+`)
    - [x] Clusters                          (`v3.3+`)
    - [x] Storing static evaluations        (`v3.3+`)
  - [x] Principle variation search          (`v2.1+`)
  - [x] Extensions                          (`v1.4+`)
    - ~~Check extensions~~                  (`v1.4+`)
    - ~~Pawn push extensions~~              (`v1.4+`)
    - ~~One reply extensions~~              (`v1.7+`)
    - [x] Singular extensions               (`v3.0+`)
    - [x] Double extensions                 (`v3.0+`)
    - [ ] Triple extensions
    - [x] Negative extensions               (`v3.0+`)
    - [x] Cutnode negative extensions       (`v3.1+`)
    - [ ] Low depth singular extensions
  - [x] Reductions                          (`v1.5+`)
    - [x] Late move reductions              (`v1.5+`)
    - [x] Internal iterative reduction      (`v3.0+`)
  - [x] Move ordering                       (`v1.0+`)
    - [x] MVV-LVA                           (`v1.0+`)
    - [x] Promotions                        (`v1.0+`)
    - [x] Hash move                         (`v1.6+`)
    - [x] Killer heuristics                 (`v1.3+`)
    - [x] Butterfly history                 (`v1.5+`)
    - [x] Capture history                   (`v2.3+`)
    - [ ] Piece-to history
    - [x] Continuation history              (`v3.2+`)
    - [x] Threats in history                (`v3.2+`)
    - [x] SEE                               (`v1.7+`)
    - [x] Pawn correction history           (`v3.3+`)
    - [x] Major piece correction history    (`v3.3+`)
    - [x] Nonpawn correction history        (`v3.3+`)
    - [x] Continuation correction history   (`v3.3+`)
- [x] Quiescence search                     (`v1.0+`)
  - ~~Delta pruning~~                       (`v2.1+`)
  - [x] Futility pruning                    (`v2.3+`)
  - [x] SEE pruning                         (`v2.3+`)
  - [x] Late move pruning                   (`v3.2+`)
  - [x] Transposition table cutoff          (`v3.0+`)
  - [x] Storing into transposition table    (`v3.2+`)
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
    - [x] Lizard SCReLU                     (`v2.0+`)
    - [x] Lazy updates                      (`v2.1+`)
    - [x] Horizontal mirroring              (`v3.1+`)
    - [x] Output buckets                    (`v3.1+`)
    - [x] King buckets                      (`v3.2+`)
    - [x] Finny tables                      (`v3.2+`)
    - [x] Finetuning                        (`v3.3+`)
    - [ ] Relabeling/distillation
    - [ ] Pairwise multiplication
    - [ ] Dual activations
    - [ ] Multilayer network
    - [ ] Threat Inputs
- [x] Time management                       (`v1.0+`)
  - [x] Hard/soft limit                     (`v3.0+`)
  - [x] Node-based scaling                  (`v3.2+`)
  - [x] Bestmove stability                  (`v3.2+`)
  - [x] Score stability                     (`v3.2+`)
  - [ ] Complexity
- [x] Pondering                             (`v1.2+`)
- [x] Performance                           (`v1.8+`)
  - [x] Compiler optimizations              (`v1.8+`)
  - [x] Incremental selection sort          (`v2.2+`)
  - [x] Linux huge pages                    (`v2.2+`)
  - [x] Staged movegen                      (`v3.0+`)
- [ ] Lazy SMP
- [x] SPSA tuning                           (`v3.3+`)

</details>

For a more in-depth documentation on the NNUE and how it was trained, refer to the [NNUE History](https://github.com/Orbital-Web/Raphael/blob/main/src/NNUE/history.txt).
All iterations of Raphael's NNUE were trained on self-generated training data.
The net files can be found on the [Raphael-Net](https://github.com/Orbital-Web/Raphael-Net) repository.

## UCI Options

<table>
  <tr>
    <th>Name</th>
    <th>Type</th>
    <th>Default</th>
    <th>Range</th>
    <th>Description</th>
  </tr>
  <tr>
    <td>Hash</td> <td>spin</td> <td>64</td> <td>[1, 65536]</td>
    <td>Memory allocated for transposition table (in MiB)</td>
  </tr>
  <tr>
    <td>Threads</td> <td>spin</td> <td>1</td> <td>[1, 1]</td>
    <td>Number of search threads</td>
  </tr>
  <tr>
    <td>UCI_Chess960</td> <td>check</td> <td>false</td> <td>true/false</td>
    <td>Whether to play Chess960 (frc/dfrc) games</td>
  </tr>
  <tr>
    <td>MoveOverhead</td> <td>spin</td> <td>10</td> <td>[0, 5000]</td>
    <td>Amount of time assumed to be lost to overhead per move (in ms)</td>
  </tr>
  <tr>
    <td>Datagen</td> <td>check</td> <td>false</td> <td>true/false</td>
    <td>Whether to enable datagen mode (modifies search behavior)</td>
  </tr>
  <tr>
    <td>Softnodes</td> <td>check</td> <td>false</td> <td>true/false</td>
    <td>Whether to use a soft node limit when sent go nodes</td>
  </tr>
  <tr>
    <td>SoftNodeHardLimitMultiplier</td> <td>spin</td> <td>1678</td> <td>[1, 5000]</td>
    <td>Scale factor of hard node limit when using softnodes</td>
  </tr>
</table>

## Acknowledgements

Raphael uses or has used the following tools throughout its development:

- [fastchess](https://github.com/Disservin/fastchess) for running SPRTs locally
- [C++ chess library](https://github.com/Disservin/chess-library) for movegen up until v2.3, and a strong source of inspiration for the custom movegen logic from v3.0 onwards
- [GediminasMasaitis's Texel Tuner](https://github.com/GediminasMasaitis/texel-tuner) for tuning the HCE parameters for v1.8
- [OpenBench](https://github.com/AndyGrant/OpenBench) for data generations and distributed SPRTs from v3.1 onwards
- [bullet](https://github.com/jw1912/bullet) for NNUE training from v3.1 onwards
- [Pawnocchio](https://github.com/JonathanHallstrom/pawnocchio) for data processing and relabeling from v3.1 onwards
- [incbin](https://github.com/graphitemaster/incbin) for embedding network files from v3.1 onwards

## Special Thanks To

Furthermore, the following individuals have inspired me or have helped me tremendously throughout the development process of Raphael (in no particular order):

- [Sebastian Lague](https://www.youtube.com/c/SebastianLague) for inspiring me to start the development of Raphael through the Coding Adventures series
- [Jonathan Hallström](https://github.com/JonathanHallstrom), author of [Pawnocchio](https://github.com/JonathanHallstrom/pawnocchio) (and a contributor to Raphael!!)
- [Ciekce](https://github.com/Ciekce), author of [Stormphrax](https://github.com/Ciekce/Stormphrax)
- [Sp00ph](https://github.com/Sp00ph), author of [Icarus](https://github.com/Sp00ph/icarus)
- [Dan](https://github.com/kelseyde), author of [Hobbes](https://github.com/kelseyde/hobbes-chess-engine)
- [Tecci](https://github.com/Teccii), author of [Cherry](https://github.com/Teccii/cherry)
- [Zahrizhal Ali](https://github.com/ZahrizhalAli) for providing me with kaggle compute for running tests, datagen, etc.
- and many others on the Stockfish and AlphaBeta Discord servers
