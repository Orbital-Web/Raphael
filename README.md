# Raphael
A (soon to be) custom chess engine, coded in C++, made using the Negamax algorithm, a variant of the Minimax algorithm.
Will probably start work on it around September 2023. The engine will be based on a previous engine I built in Python, with a few optimization and improvement ideas in mind.


## Getting started (Windows)
1. Install g++
2. Download SFML from https://www.sfml-dev.org/download.php and add it to the root directory
3. Compile `main.cpp` with the following commands
    ```
    g++ -c main.cpp src/GameEngine/thc.cpp -ISFML-2.6.0/include -DSFML_STATIC
    g++ -o main main.o thc.o -LSFML-2.6.0/lib -lsfml-graphics-s -lsfml-window-s -lsfml-system-s -lopengl32 -lwinmm -lgdi32
    ```