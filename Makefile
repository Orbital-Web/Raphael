# Compiler & flags
CC = g++
LD = ld
CCFLAGS = -Wall -O3 -march=native -DNDEBUG -fno-builtin -std=c++20 -Isrc -Ichess-library/src -ISFML-3.0.2/include
LDFLAGS = -LSFML-3.0.2/lib -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-system \
		  -Wl,-rpath,'$$ORIGIN/SFML-3.0.2/lib',-z,noexecstack

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
main: $(MAIN_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# Linking the UCI executable
uci: $(UCI_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# Generic rules for compiling a source file to an object file
%.o: %.cpp
	$(CC) $(CCFLAGS) -c $< -o $@

# Rule to download SFML if not present
packages:
	@if [ ! -d "SFML-3.0.2" ]; then \
		echo "SFML-3.0.2 not found. Downloading..."; \
		wget https://www.sfml-dev.org/files/SFML-3.0.2-linux-gcc-64-bit.tar.gz; \
		tar -xzf SFML-3.0.2-linux-gcc-64-bit.tar.gz; \
		rm SFML-3.0.2-linux-gcc-64-bit.tar.gz; \
	else \
		echo "SFML-3.0.2 already installed."; \
	fi

# Clean rule
clean:
	rm -f $(MAIN_OBJS) $(UCI_OBJS)

clean_all:
	rm -f $(MAIN_OBJS) $(UCI_OBJS) main uci
