#---------------------------------------------------------------------------------------------------
# Project Configuration (Makefile inspired by https://github.com/KierenP/Halogen)
#---------------------------------------------------------------------------------------------------

.DEFAULT_GOAL := uci

# Executables
MAIN_EXE := main
EXE  := uci
TEST_EXE := test

# NNUE file
EVALFILE := default

# Architecture configuration
ARCH ?= native

# Debug option
DEBUG ?= off

#---------------------------------------------------------------------------------------------------
# Source Files
#---------------------------------------------------------------------------------------------------

MAIN_SOURCES := \
    $(wildcard src/GameEngine/*.cpp) \
    $(wildcard src/Raphael/*.cpp) \
    main.cpp

UCI_SOURCES := \
    src/GameEngine/consts.cpp \
    $(wildcard src/Raphael/*.cpp) \
    uci.cpp

TEST_SOURCES := \
	src/GameEngine/consts.cpp \
    $(wildcard src/Raphael/*.cpp) \
	$(wildcard src/tests/*.cpp)

MAIN_OBJS := $(MAIN_SOURCES:.cpp=.o)
UCI_OBJS  := $(UCI_SOURCES:.cpp=.o)
TEST_OBJS := $(TEST_SOURCES:.cpp=.o)

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
	override MAIN_EXE := $(MAIN_EXE).exe
	override EXE := $(EXE).exe
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

CXXFLAGS := -std=c++20 -O3 -flto=auto $(WARN_FLAGS) \
    -Isrc -ISFML-3.0.2/include

LDFLAGS     :=
LDFLAGS_UCI :=

# SFML dynamic libs
SFML_LIBS := -LSFML-3.0.2/lib \
    -lsfml-graphics -lsfml-window -lsfml-audio -lsfml-system

# Non-Windows: fixes SFML libs not found issue
ifneq ($(DETECTED_OS),Windows)
    LDFLAGS += -Wl,-rpath,'$$ORIGIN/SFML-3.0.2/lib',-z,noexecstack
endif

#---------------------------------------------------------------------------------------------------
# Architecture Flags
#---------------------------------------------------------------------------------------------------

CCFLAGS_NATIVE    := -march=native
CCFLAGS_AVX2_BMI2 := -march=haswell -DCHESS_USE_PEXT
CCFLAGS_AVX2      := -march=haswell -mno-bmi2
CCFLAGS_GENERIC   := -march=x86-64
CCFLAGS_TUNABLE   := -march=native -DTUNE

ifeq ($(ARCH),native)
    ARCH_FLAGS := $(CCFLAGS_NATIVE)
else ifeq ($(ARCH),avx2_bmi2)
    ARCH_FLAGS := $(CCFLAGS_AVX2_BMI2)
else ifeq ($(ARCH),avx2)
    ARCH_FLAGS := $(CCFLAGS_AVX2)
else ifeq ($(ARCH),generic)
    ARCH_FLAGS := $(CCFLAGS_GENERIC)
else ifeq ($(ARCH),tunable)
    ARCH_FLAGS := $(CCFLAGS_TUNABLE)
else
    $(error Unknown architecture '$(ARCH)')
endif

override CXXFLAGS += $(ARCH_FLAGS)

$(info Building for ARCH=$(ARCH))

#---------------------------------------------------------------------------------------------------
# Debug Flags
#---------------------------------------------------------------------------------------------------

CCFLAGS_RELEASE  := -DNDEBUG
CCFLAGS_DEBUG    := -g
CCFLAGS_SANITIZE := -g -fsanitize=address,undefined

ifeq ($(DEBUG),on)
    $(info Debug enabled)
    DEBUG_FLAGS := $(CCFLAGS_DEBUG)
else ifeq ($(DEBUG),off)
    $(info Building for release)
    DEBUG_FLAGS := $(CCFLAGS_RELEASE)
	override LDFLAGS_UCI += -static
else ifeq ($(DEBUG),san)
    $(info Debug and address, ub sanitization enabled)
    DEBUG_FLAGS := $(CCFLAGS_SANITIZE)
	override LDFLAGS += -fsanitize=address,undefined
else
    $(error Unknown debug flag '$(DEBUG)')
endif

override CXXFLAGS += $(DEBUG_FLAGS)

#---------------------------------------------------------------------------------------------------
# Networks
#---------------------------------------------------------------------------------------------------

ifeq ($(EVALFILE),default)
    ifeq ($(DETECTED_OS),Windows)
        DEFAULT_NET := $(shell type network.txt)
    else
        DEFAULT_NET := $(shell cat network.txt)
    endif
    EVALFILE = $(DEFAULT_NET).nnue

$(EVALFILE):
	curl -sL https://github.com/Orbital-Web/Raphael-Net/releases/download/$(DEFAULT_NET)/$(DEFAULT_NET).nnue -o $(EVALFILE)

endif

override CXXFLAGS += -DNETWORK_FILE=$(EVALFILE)

$(info Using network: $(EVALFILE))
$(info )

#---------------------------------------------------------------------------------------------------
# Main Build Targets
#---------------------------------------------------------------------------------------------------

all: uci packages main test

# main executable
.PHONY: main
main: $(MAIN_OBJS) $(EVALFILE)
	$(CXX) -o $(MAIN_EXE) $(MAIN_OBJS) $(LDFLAGS) $(SFML_LIBS)

# uci executable
.PHONY: uci
uci: $(UCI_OBJS) $(EVALFILE)
	$(CXX) -o $(EXE) $(UCI_OBJS) $(LDFLAGS) $(LDFLAGS_UCI)

.PHONY: test
test: $(TEST_OBJS) $(EVALFILE)
	$(CXX) -o $(TEST_EXE) $(TEST_OBJS) $(LDFLAGS)

# compile .cpp -> .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

#---------------------------------------------------------------------------------------------------
# Packages
#---------------------------------------------------------------------------------------------------

.PHONY: packages
packages:
ifeq ($(DETECTED_OS),Windows)
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

.PHONY: clean clean_all
clean:
ifeq ($(DETECTED_OS),Windows)
	del /Q $(subst /,\,$(MAIN_OBJS) $(UCI_OBJS) $(TEST_OBJS)) 2>nul
else
	rm -f $(MAIN_OBJS) $(UCI_OBJS) $(TEST_OBJS)
endif

clean_all: clean
ifeq ($(DETECTED_OS),Windows)
	del /Q $(MAIN_EXE) $(EXE) $(TEST_EXE) 2>nul
else
	rm -f $(MAIN_EXE) $(EXE) $(TEST_EXE)
endif
