# Compiler & flags
CC = g++
LD = ld
CCFLAGS = -Wall -O3 -DNDEBUG -fno-builtin -std=c++20 -Isrc -Ichess-library/src -ISFML-3.0.2/include
LDFLAGS = -LSFML-3.0.2/lib -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-system

ifneq ($(OS),Windows_NT)
    LDFLAGS += -Wl,-rpath,'$$ORIGIN/SFML-3.0.2/lib',-z,noexecstack
endif

# Architecture
ARCH ?=
CCFLAGS_NATIVE = -march=native
CCFLAGS_AVX2_BMI2 = -march=haswell
CCFLAGS_AVX2 = -march=haswell -mno-bmi2
CCFLAGS_GENERIC = -march=x86-64
CCFLAGS_TUNABLE = -march=native -DTUNE

ifeq ($(ARCH),)
    $(warning ARCH not set, building for native)
    EXTRA_CCFLAGS = $(CCFLAGS_NATIVE)
else ifeq ($(ARCH),native)
    $(info Building for ARCH=native)
    EXTRA_CCFLAGS = $(CCFLAGS_NATIVE)
else ifeq ($(ARCH),avx2_bmi2)
    $(info Building for ARCH=avx2_bmi2)
    EXTRA_CCFLAGS = $(CCFLAGS_AVX2_BMI2)
else ifeq ($(ARCH),avx2)
    $(info Building for ARCH=avx2)
    EXTRA_CCFLAGS = $(CCFLAGS_AVX2)
else ifeq ($(ARCH),generic)
    $(info Building for ARCH=generic)
    EXTRA_CCFLAGS = $(CCFLAGS_GENERIC)
else ifeq ($(ARCH),tunable)
    $(info Building for ARCH=tunable)
    EXTRA_CCFLAGS = $(CCFLAGS_TUNABLE)
else
    $(error Unknown architecture '$(ARCH)')
endif

override CCFLAGS += $(EXTRA_CCFLAGS)

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
ifeq ($(OS),Windows_NT)
	@if not exist SFML-3.0.2 ( \
		echo SFML-3.0.2 not found. Downloading... && \
		powershell -Command "Invoke-WebRequest https://www.sfml-dev.org/files/SFML-3.0.2-windows-gcc-14.2.0-mingw-64-bit.zip -OutFile sfml.zip" && \
		tar -xf sfml.zip && \
		del sfml.zip && \
		echo Copying SFML DLLs next to executables... && \
		copy SFML-3.0.2\bin\*.dll . >nul 2>&1 || true \
	) else ( \
		echo SFML-3.0.2 already installed. \
	)
else
	@if [ ! -d "SFML-3.0.2" ]; then \
		echo "SFML-3.0.2 not found. Downloading..."; \
		wget https://www.sfml-dev.org/files/SFML-3.0.2-linux-gcc-64-bit.tar.gz; \
		tar -xzf SFML-3.0.2-linux-gcc-64-bit.tar.gz; \
		rm SFML-3.0.2-linux-gcc-64-bit.tar.gz; \
	else \
		echo "SFML-3.0.2 already installed."; \
	fi
endif

# Clean rule
clean:
ifeq ($(OS),Windows_NT)
	del /Q $(subst /,\,$(MAIN_OBJS) $(UCI_OBJS)) 2>nul
else
	rm -f $(MAIN_OBJS) $(UCI_OBJS)
endif

clean_all: clean
ifeq ($(OS),Windows_NT)
	del /Q main.exe uci.exe 2>nul
else
	rm -f main uci
endif
