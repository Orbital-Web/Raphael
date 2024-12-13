# Compiler & flags
CC = g++
CCFLAGS = -Wall -fno-builtin -std=c++20 -Isrc -Ichess-library/src
LDFLAGS = -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-system



# Default rule
all: packages main

# Linking the main executable
main: main.o
	$(CC) -o $@ $^ $(LDFLAGS)
	rm -f main.o

# Linking the UCI executable
uci: uci.o
	$(CC) -o $@ $^ -lsfml-graphics
	rm -f uci.o

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
	rm -f main.o uci.o main uci