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

    // We can now begin implementing these fuckass features:

    void ConvertToFileType(const fs::path& inputPath, const fs::path& outputPath, AudioFormat format, VBRQualities quality)
    {
        if (!fc::IsSpecficAudioFormat(inputPath, op::AudioFormat::FLAC) || !fc::IsSpecficAudioFormat(inputPath, op::AudioFormat::WAV))
        {
            warn("Input file is not a lossless file. Conversion may result in quality loss. :(");
        }


        /* Redundant check here, alr implemnted after strcmp passes for a -cvrt / --convert flag */

        // // Output path here will be the global var -po if specified.
        // if (!fs::exists(outputPath) || !fs::is_directory(outputPath))
        // {
        //     err("Output path does not exist or is not a regular file!");
        //     return; // exit function early due to invalid output path
        // }


        // Get qual val from enum class and that plugs into ffmpegCmd string
        int qualval = static_cast<int>(quality);

        std::string_view lib = op::ConversionLibMap.at(format); // declare lib to hold name of lib when plugging into cmd
        std::string ffmpegCmd = "ffmpeg -i \"" + inputPath.string() + "\" -c:a " + op::ConversionLibMap.at(op::AudioFormat::GENERAL) + "-q:a " + std::to_string(qualval) + " \"" + outputPath.string() + "\"";

        // Build ffmpeg command
        plog("Built ffmpeg command: ");
        yay(ffmpegCmd.c_str());
    }
}