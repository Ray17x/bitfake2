#ifndef CONSOLEOUT_HPP
#define CONSOLEOUT_HPP

#include <cstdlib>
#include <stdio.h>

namespace ConsoleOut
{
    static constexpr char* RED = "\033[31m";
    static constexpr char* GREEN = "\033[32m";
    static constexpr char* YELLOW = "\033[33m";
    static constexpr char* BLUE = "\033[34m";
    static constexpr char* MAGENTA = "\033[35m";
    static constexpr char* CYAN = "\033[36m";
    static constexpr char* RESET = "\033[0m";

    inline void err(const char* msg = "")
    {
        if (msg[0] != '\0') {
            printf("%s[ERR]\t%s%s\n", RED, RESET, msg);
        } else {
            printf("%s[ERR]%sAn unknown error has occurred\n", RED, RESET);
        }
    }
    inline void warn(const char* msg)
    {
        if (msg[0] != '\0') {
            printf("%s[WARN]\t%s%s\n", YELLOW, RESET, msg);
        }
    }
    inline void plog(const char* msg) // Pretty log :D
    {
        if (msg[0] != '\0') {
            printf("%s[LOG]\t%s%s\n", BLUE, RESET, msg);
        }
    }
    inline void yay(const char* msg)
    {
        if (msg[0] != '\0') {
            printf("%s[YAY]\t%s%s\n", GREEN, RESET, msg);
        }
    }
}

#endif // CONSOLEOUT_HPP