# Compiler & flags
CC = g++
CCFLAGS = -Wall -O3 -march=native -DNDEBUG -fno-builtin -std=c++20 -I../../src -I../../chess-library/src
LDFLAGS = -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-system

# sources
GEN_SOURCES = ../GameEngine/consts.cpp ../GameEngine/GamePlayer.cpp ${wildcard ../Raphael/*.cpp} traingen.cpp
TEST_SOURCES = ../Raphael/consts.cpp ../Raphael/nnue.cpp ../Raphael/simd.cpp nnuetest.cpp



# obj files
GEN_OBJS = ${GEN_SOURCES:.cpp=.o}
TEST_OBJS = ${TEST_SOURCES:.cpp=.o}

# Default rule
all: packages traingen nnuetest

# Linking the executable
traingen: ${GEN_OBJS}
	$(CC) -o $@ $^ $(LDFLAGS)

nnuetest: ${TEST_OBJS}
	$(CC) -o $@ $^ $(LDFLAGS)

# Generic rules for compiling a source file to an object file
%.o: %.cpp
	$(CC) $(CCFLAGS) -c $< -o $@

# Rule to install SFML if not present
packages:
	@if ! dpkg -l | grep libsfml-dev -c >>/dev/null; then \
		echo "SFML not found. Downloading and installing..."; \
		sudo apt-get install libsfml-dev; \
	else \
		echo "SFML-2.6.0 already installed."; \
	fi

# Clean rule
clean:
	rm -f ${GEN_OBJS} ${TEST_OBJS}

clean_all:
	rm -f ${GEN_OBJS} ${TEST_OBJS} traingen nnuetest
