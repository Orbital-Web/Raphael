# Raphael
A (soon to be) custom chess engine, coded in C++, made using the Negamax algorithm, a variant of the Minimax algorithm.
Will probably start work on it around September 2023. The engine will be based on a previous engine I built in Python, with a few optimization and improvement ideas in mind.


## Getting started (Windows)
1. Clone the repository
2. Install g++
3. Download SFML-2.6.0 (GCC MinGW) from https://www.sfml-dev.org/download/sfml/2.6.0/ and add it to the root directory
4. Copy `openal32.dll` from SFML-2.6.0/bin/ and add it to the root directory
5. Compile and run `main.exe` with the following commands
    ```
    g++ -c main.cpp -ISFML-2.6.0/include -DSFML_STATIC
    g++ -o main main.o -LSFML-2.6.0/lib -lsfml-graphics-s -lsfml-window-s -lsfml-audio-s -lsfml-system-s -lopengl32 -lfreetype -lwinmm -lgdi32 -lopenal32 -lflac -lvorbisenc -lvorbisfile -lvorbis -logg
    del main.o
    main.exe human "Adam" human "Bob"
    ```