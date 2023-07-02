# Raphael
A (soon to be) custom chess engine, coded in C++, made using the Negamax algorithm, a variant of the Minimax algorithm.
Will probably start work on it around September 2023. The engine will be based on a previous engine I built in Python, with a few optimization and improvement ideas in mind.


## Getting started (Windows)
1. Clone the repository
2. Install g++
3. Download SFML-2.6.0 from https://www.sfml-dev.org/download/sfml/2.6.0/ and add it to the root directory
4. Compile `main.cpp` with the following commands
    ```
    g++ -c main.cpp src/GameEngine/GameEngine.cpp  src/GameEngine/HumanPlayer.cpp src/GameEngine/thc.cpp -ISFML-2.6.0/include -DSFML_STATIC 
    g++ -o main main.o GameEngine.o HumanPlayer.o thc.o -LSFML-2.6.0/lib -lsfml-graphics-s -lsfml-window-s -lsfml-system-s -lopengl32 -lwinmm -lgdi32
    del main.o thc.o
    ```