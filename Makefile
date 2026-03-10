# Use ?= so Portage can provide its own values
CXX      ?= g++
# Use += so we add your requirements to the user's preferred CXXFLAGS
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

# This is the vital part for GURU/Portage!
install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 0755 $(BIN) $(DESTDIR)$(PREFIX)/bin/

clean:
	rm -rf $(BUILD_DIR) $(BIN)
