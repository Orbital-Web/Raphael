#---------------------------------------------------------------------------------------------------
# Project Configuration (Makefile inspired by https://github.com/KierenP/Halogen)
#---------------------------------------------------------------------------------------------------

.DEFAULT_GOAL := all

# Executables
MAIN_EXE := main
UCI_EXE  := uci

# NNUE file
NNUE_FILE := net.nnue

# Architecture configuration
ARCH ?= native

#---------------------------------------------------------------------------------------------------
# Source Files
#---------------------------------------------------------------------------------------------------

MAIN_SOURCES := \
    $(wildcard src/GameEngine/*.cpp) \
    $(wildcard src/Raphael/*.cpp) \
    main.cpp

UCI_SOURCES := \
    src/GameEngine/consts.cpp \
    src/GameEngine/GamePlayer.cpp \
    $(wildcard src/Raphael/*.cpp) \
    uci.cpp

NNUE_OBJ  := net.o
MAIN_OBJS := $(MAIN_SOURCES:.cpp=.o) $(NNUE_OBJ)
UCI_OBJS  := $(UCI_SOURCES:.cpp=.o)  $(NNUE_OBJ)

#---------------------------------------------------------------------------------------------------
# Platform and Compiler Detection
#---------------------------------------------------------------------------------------------------

ifeq ($(OS),Windows_NT)
    DETECTED_OS := Windows
else
    DETECTED_OS := $(shell uname)
endif
$(info Detected OS: $(DETECTED_OS))

ifeq ($(DETECTED_OS),Windows)
	CXX_VERSION := $(shell $(CXX) --version 2>nul)
else
	CXX_VERSION := $(shell $(CXX) --version 2>/dev/null)
endif
ifneq ($(findstring clang,$(CXX_VERSION)),)
    COMPILER := clang++
else
    COMPILER := g++
endif
$(info Detected compiler: $(COMPILER))

CXX := $(COMPILER)

#---------------------------------------------------------------------------------------------------
# Compiler and Linker Flags
#---------------------------------------------------------------------------------------------------

WARN_FLAGS := -Wall -Wextra

CXXFLAGS := -std=c++20 -O3 $(WARN_FLAGS) \
    -Isrc -Ichess-library/include -ISFML-3.0.2/include

LDFLAGS  :=

# SFML dynamic libs
SFML_LIBS := -LSFML-3.0.2/lib \
    -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-system

# Non-Windows: fixes sfml libs not found issue
ifneq ($(DETECTED_OS),Windows)
    LDFLAGS += -Wl,-rpath,'$$ORIGIN/SFML-3.0.2/lib',-z,noexecstack
endif

#---------------------------------------------------------------------------------------------------
# Architecture Flags
#---------------------------------------------------------------------------------------------------

CCFLAGS_RELEASE   := -DNDEBUG
CCFLAGS_NATIVE    := $(CCFLAGS_RELEASE) -march=native -DCHESS_USE_PEXT
CCFLAGS_AVX2_BMI2 := $(CCFLAGS_RELEASE) -march=haswell -DCHESS_USE_PEXT
CCFLAGS_AVX2      := $(CCFLAGS_RELEASE) -march=haswell -mno-bmi2
CCFLAGS_GENERIC   := $(CCFLAGS_RELEASE) -march=x86-64
CCFLAGS_TUNABLE   := $(CCFLAGS_RELEASE) -march=native -DTUNE
CCFLAGS_DEBUG     := -march=native -g

ifeq ($(ARCH),native)
    EXTRA_FLAGS := $(CCFLAGS_NATIVE)
else ifeq ($(ARCH),avx2_bmi2)
    EXTRA_FLAGS := $(CCFLAGS_AVX2_BMI2)
else ifeq ($(ARCH),avx2)
    EXTRA_FLAGS := $(CCFLAGS_AVX2)
else ifeq ($(ARCH),generic)
    EXTRA_FLAGS := $(CCFLAGS_GENERIC)
else ifeq ($(ARCH),tunable)
    EXTRA_FLAGS := $(CCFLAGS_TUNABLE)
else ifeq ($(ARCH),debug)
    EXTRA_FLAGS := $(CCFLAGS_DEBUG)
else
    $(error Unknown architecture '$(ARCH)')
endif

override CXXFLAGS += $(EXTRA_FLAGS)

$(info Building for ARCH=$(ARCH))
$(info )

#---------------------------------------------------------------------------------------------------
# Main Build Targets
#---------------------------------------------------------------------------------------------------

all: uci main

# NNUE embeddings
$(NNUE_OBJ): $(NNUE_FILE)
	ld -r -b binary $< -o $@

# main executable
main: $(MAIN_OBJS)
	$(CXX) -o $(MAIN_EXE) $^ $(LDFLAGS) $(SFML_LIBS)

# uci executable
uci: $(UCI_OBJS)
	$(CXX) -o $(UCI_EXE) $^ $(LDFLAGS) -static

# compile .cpp -> .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

#---------------------------------------------------------------------------------------------------
# Packages
#---------------------------------------------------------------------------------------------------

packages:
ifeq ($(OS),Windows_NT)
	@if not exist SFML-3.0.2 ( \
		echo SFML-3.0.2 not found. Downloading... && \
		powershell -Command "Invoke-WebRequest https://www.sfml-dev.org/files/SFML-3.0.2-windows-gcc-14.2.0-mingw-64-bit.zip -OutFile sfml.zip" && \
		tar -xf sfml.zip && \
		del sfml.zip && \
		echo Copying SFML DLLs... && \
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


#---------------------------------------------------------------------------------------------------
# Cleaning
#---------------------------------------------------------------------------------------------------

clean:
ifeq ($(DETECTED_OS),Windows)
	del /Q $(subst /,\,$(MAIN_OBJS) $(UCI_OBJS)) 2>nul
else
	rm -f $(MAIN_OBJS) $(UCI_OBJS)
endif

clean_all: clean
ifeq ($(DETECTED_OS),Windows)
	del /Q main.exe uci.exe 2>nul
else
	rm -f main uci
endif
