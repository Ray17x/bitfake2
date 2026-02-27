#ifndef GLOBALS_HPP
#define GLOBALS_HPP

#include <filesystem>
#include "operations.hpp"

namespace fs = std::filesystem;
namespace op = Operations;

namespace globals
{
	extern fs::path inputFile;
	extern fs::path outputFile;
	extern op::AudioFormat outputFormat;
	extern bool outputToTerminal;
	extern op::AudioFormat desiredFormat;
	extern std::string version;
}
#endif