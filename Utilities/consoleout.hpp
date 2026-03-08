#ifndef CONSOLEOUT_HPP
#define CONSOLEOUT_HPP

#include <cstdlib>
#include <stdio.h>

namespace ConsoleOut {
static constexpr const char *RED = "\033[31m";
static constexpr const char *GREEN = "\033[32m";
static constexpr const char *YELLOW = "\033[33m";
static constexpr const char *BLUE = "\033[34m";
static constexpr const char *MAGENTA = "\033[35m";
static constexpr const char *CYAN = "\033[36m";
static constexpr const char *RESET = "\033[0m";

inline void err(const char *msg = "") {
    if (msg[0] != '\0') {
        printf("%s[ERR]\t%s%s\n", RED, RESET, msg);
    } else {
        printf("%s[ERR]%sAn unknown error has occurred\n", RED, RESET);
    }
}
inline void warn(const char *msg) {
    if (msg[0] != '\0') {
        printf("%s[WRN]\t%s%s\n", YELLOW, RESET, msg);
    }
}
inline void plog(const char *msg) // Pretty log :D
{
    if (msg[0] != '\0') {
        printf("%s[LOG]\t%s%s\n", BLUE, RESET, msg);
    }
}
inline void yay(const char *msg) {
    if (msg[0] != '\0') {
        printf("%s[YAY]\t%s%s\n", GREEN, RESET, msg);
    }
}
} // namespace ConsoleOut

#endif // CONSOLEOUT_HPP