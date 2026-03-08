#ifndef GLOBALS_HPP
#define GLOBALS_HPP

#include <filesystem>
#include <string>
#include "operations.hpp"

namespace fs = std::filesystem;
namespace op = Operations;

namespace globals {
extern fs::path inputFile;
extern fs::path outputFile;
extern op::AudioFormat outputFormat;
extern bool outputToTerminal;
extern op::VBRQualities VBRQuality;
extern int opusBitrateKbps;
extern std::string version;
extern fs::path conversionOutputDirectory; // For storing converted files if -po/--pathout is specified
extern std::string tag, val;
} // namespace globals
#endif