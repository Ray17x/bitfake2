CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -I./Utilities
LDFLAGS := -ltag -lfftw3 -lebur128 -lsndfile -lavformat -lavcodec -lswresample -lavutil
BUILD_DIR := build

SRC := main.cpp helperfunctions.cpp filechecks.cpp globals.cpp nonusrfunctions.cpp coreoperations.cpp
FORMAT_FILES := $(SRC) Utilities/*.hpp
OBJ := $(addprefix $(BUILD_DIR)/,$(SRC:.cpp=.o))
BIN := bitf

.PHONY: all clean format check-format

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)


$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN) $(BUILD_DIR)/bitf

format:
	clang-format -i $(FORMAT_FILES)

check-format:
	clang-format --dry-run -Werror $(FORMAT_FILES)
