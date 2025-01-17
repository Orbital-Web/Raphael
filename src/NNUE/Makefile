# Compiler & flags
CC = g++
CCFLAGS = -Wall -O3 -march=native -DNDEBUG -fno-builtin -std=c++20 -I../../src -I../../chess-library/src
LDFLAGS = -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-system

# sources
SOURCES = ../GameEngine/consts.cpp ../GameEngine/GamePlayer.cpp ${wildcard ../Raphael/*.cpp} traingen.cpp



# obj files
OBJS = ${SOURCES:.cpp=.o}

# Default rule
all: packages traingen

# Linking the executable
traingen: ${OBJS}
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
	rm -f ${OBJS}

clean_all:
	rm -f ${OBJS} traingen
