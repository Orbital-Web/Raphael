# Compiler & flags
CC = g++
LD = ld
CCFLAGS = -Wall -O3 -march=native -DNDEBUG -fno-builtin -std=c++20 -Isrc -Ichess-library/src
LDFLAGS = -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-system -z noexecstack

# NNUE binary
NNUE_FILE = net.nnue
NNUE_OBJ = net.o

# sources
MAIN_SOURCES = $(wildcard src/GameEngine/*.cpp) $(wildcard src/Raphael/*.cpp) main.cpp
UCI_SOURCES = src/GameEngine/consts.cpp src/GameEngine/GamePlayer.cpp $(wildcard src/Raphael/*.cpp) uci.cpp



# obj files
MAIN_OBJS = $(MAIN_SOURCES:.cpp=.o) $(NNUE_OBJ)
UCI_OBJS = $(UCI_SOURCES:.cpp=.o) $(NNUE_OBJ)

# Default rule
all: main uci

# Building NNUE object
$(NNUE_OBJ): $(NNUE_FILE)
	$(LD) -r -b binary $< -o $@

# Linking the main executable
main: packages $(MAIN_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# Linking the UCI executable
uci: $(UCI_OBJS)
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
	rm -f $(MAIN_OBJS) $(UCI_OBJS)

clean_all:
	rm -f $(MAIN_OBJS) $(UCI_OBJS) main uci
