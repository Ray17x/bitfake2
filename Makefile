CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -I./Utilites
LDFLAGS := -ltag -lfftw3 -lebur128 -lsndfile
BUILD_DIR := build

SRC := main.cpp helperfunctions.cpp filechecks.cpp globals.cpp nonusrfunctions.cpp coreoperations.cpp
OBJ := $(addprefix $(BUILD_DIR)/,$(SRC:.cpp=.o))
BIN := bitf

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)


$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN) $(BUILD_DIR)/bitf
