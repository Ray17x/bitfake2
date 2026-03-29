CXX= clang++

OS!= uname

.if $(OS) == "NetBSD"
LOCALBASE?= /usr/pkg
CXX= g++
.  if exists($(LOCALBASE)/include/ffmpeg8)
CXXFLAGS += -I$(LOCALBASE)/include/ffmpeg8
LDFLAGS  += -L$(LOCALBASE)/lib/ffmpeg8 -Wl,-rpath,$(LOCALBASE)/lib/ffmpeg8
.  elif exists($(LOCALBASE)/include/ffmpeg7)
CXXFLAGS += -I$(LOCALBASE)/include/ffmpeg7
LDFLAGS  += -L$(LOCALBASE)/lib/ffmpeg7 -Wl,-rpath,$(LOCALBASE)/lib/ffmpeg7
.  elif exists($(LOCALBASE)/include/ffmpeg6)
CXXFLAGS += -I$(LOCALBASE)/include/ffmpeg6
LDFLAGS  += -L$(LOCALBASE)/lib/ffmpeg6 -Wl,-rpath,$(LOCALBASE)/lib/ffmpeg6
.  elif exists($(LOCALBASE)/include/ffmpeg5)
CXXFLAGS += -I$(LOCALBASE)/include/ffmpeg5
LDFLAGS  += -L$(LOCALBASE)/lib/ffmpeg5 -Wl,-rpath,$(LOCALBASE)/lib/ffmpeg5
.  endif
LDFLAGS  += -Wl,-rpath,$(LOCALBASE)/lib
.elif $(OS) == "OpenBSD"
LOCALBASE?= /usr/local
CXX= clang++
.elif $(OS) == "FreeBSD" || $(OS) == "DragonFly"
LOCALBASE?= /usr/local
.else
LOCALBASE?= /usr/local
.endif

CXXFLAGS += -std=c++17 -Wall -Wextra -I./Utilities -I$(LOCALBASE)/include -Wno-deprecated-declarations
LDFLAGS  += -L$(LOCALBASE)/lib -ltag -lfftw3 -lebur128 -lsndfile -lavformat -lavcodec -lswresample -lavutil -pthread

BUILD_DIR= build
DESTDIR  ?=
PREFIX   ?= $(LOCALBASE)
BINDIR   = $(DESTDIR)$(PREFIX)/bin

SRC= main.cpp helperfunctions.cpp filechecks.cpp globals.cpp nonusrfunctions.cpp coreoperations.cpp

.for _src in $(SRC)
OBJ+= $(BUILD_DIR)/$(_src:S/.cpp/.o/)
.endfor

BIN= bitf

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(.TARGET) $(LDFLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

.for _src in $(SRC)
$(BUILD_DIR)/$(_src:S/.cpp/.o/): $(_src) $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $(_src) -o $(.TARGET)
.endfor

install: $(BIN)
	mkdir -p $(BINDIR)
	install -m 755 $(BIN) $(BINDIR)/

uninstall:
	rm -f $(BINDIR)/$(BIN)

clean:
	rm -rf $(BUILD_DIR) $(BIN)

format:
	find . -name "*.cpp" -o -name "*.h" | xargs clang-format -i

check-format:
	find . -name "*.cpp" -o -name "*.h" | xargs clang-format --dry-run --Werror

.PHONY: all clean format check-format install uninstall
