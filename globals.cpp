#include "Utilities/globals.hpp"

namespace globals {
fs::path inputFile;
fs::path outputFile;
op::AudioFormat outputFormat = op::AudioFormat::MP3;
op::VBRQualities VBRQuality;
int opusBitrateKbps = 192;
bool outputToTerminal = true;
std::string version = "0.1.0-alpha"; // Pls dont change me :D
fs::path conversionOutputDirectory;
std::string tag, val;
} // namespace globals
