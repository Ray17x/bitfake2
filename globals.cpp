#include "Utilites/globals.hpp"

namespace globals
{
    fs::path inputFile;
    fs::path outputFile;
    op::AudioFormat outputFormat = op::AudioFormat::MP3;
    op::VBRQualities VBRQuality;
    bool outputToTerminal = true;
    std::string version = "0.0.3-alpha"; // Pls dont change me :D
}
