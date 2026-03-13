CXX      ?= g++
CXXFLAGS += -std=c++17 -Wall -Wextra -I./Utilities
LDFLAGS  += -ltag -lfftw3 -lebur128 -lsndfile -lavformat -lavcodec -lswresample -lavutil

BUILD_DIR := build
# DESTDIR is the 'fake' root used by Portage
DESTDIR   ?= 
PREFIX    ?= /usr/local

SRC := main.cpp helperfunctions.cpp filechecks.cpp globals.cpp nonusrfunctions.cpp coreoperations.cpp
OBJ := $(addprefix $(BUILD_DIR)/,$(SRC:.cpp=.o))
BIN := bitf

.PHONY: all clean format check-format install

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $@ $(LDFLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

PREFIX ?= /usr/local
BINDIR = $(DESTDIR)$(PREFIX)/bin

install: bitf
	mkdir -p $(BINDIR)
	cp bitf $(BINDIR)/bitf
	chmod 755 $(BINDIR)/bitf

uninstall:
	rm -f $(BINDIR)/bitf 
