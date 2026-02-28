#include <cstdio>
#include "Utilites/consoleout.hpp"
using namespace ConsoleOut;
#include <filesystem>
namespace fs = std::filesystem;
#include "Utilites/filechecks.hpp"
namespace fc = FileChecks;
#include "Utilites/operations.hpp"
namespace op = Operations;
#include "Utilites/globals.hpp"
namespace gb = globals;


namespace Operations
{
    // Core operations

    /*
        This file will define functions for core operations of the program, such as conversion, replaygain calculation,
        mass tagging for a directory, and other nitty gritty functions that will be a hassle to program. Will try to keep things
        as clean and organized as possible, but no promises >:D
    */

    // void ConvertToFileType(const fs::path& inputPath, const fs::path& outputPath, AudioFormat format);
    // void MassTagDirectory(const fs::path& dirPath, const std::string& tag, const std::string& value);
    // void ApplyReplayGain(const fs::path& path, float trackGain, float albumGain);
    // void CalculateReplayGain(const fs::path& path); 

    // TODO FIRST:
    // define void ApplyMetaDataTag(const fs::path& path, const std::string& tag, const std::string& value);
    // Needed for some functions. (found in nonusrfunctions.cpp) 
}