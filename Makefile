CXX := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -I./Utilites
LDFLAGS := -ltag -lfftw3

SRC := main.cpp helperfunctions.cpp filechecks.cpp globals.cpp nonusrfunctions.cpp coreoperations.cpp
OBJ := $(SRC:.cpp=.o)
BIN := bitf

.PHONY: all clean

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(OBJ) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)
