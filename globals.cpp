#include "Utilites/globals.hpp"

namespace globals
{
    fs::path inputFile;
    fs::path outputFile;
    op::AudioFormat outputFormat = op::AudioFormat::MP3;
    bool outputToTerminal = true;
    std::string version = "0.0.1-alpha"; // Pls dont change me :D
}
