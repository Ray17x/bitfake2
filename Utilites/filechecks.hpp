#ifndef FILECHECKS_HPP
#define FILECHECKS_HPP

#include <cstdio>
#include <filesystem>
#include "consoleout.hpp"
namespace fs = std::filesystem;
using namespace ConsoleOut;

namespace FileChecks
{
    // Checks if the parent directory of the given path exists. If the file itself exists,
    // it returns true without checking the parent directory.
    inline bool ParentExists(const fs::path& path)
    {
        if (fs::exists(path) && fs::is_directory(path))
        {
            plog("File exists, no need to check parent dir.");           
        }
        else
        {
            // Since the file doesn't exist, check if the parent dir exists
            fs::path parent = path.parent_path();
            if (fs::exists(parent) && fs::is_directory(parent))
            {
                plog("Parent directory exists.");
                return true;
            }
            else
            {
                warn("Parent directory does not exist.");
                return false;
            }
        }
        // fall back to false if the conditon gets passed this if function
        return false;
    }

    bool IsValidAudioFile(const fs::path& path); // Checks via extensions
    bool IsTrueAudio(const fs::path& path); // Checks via file signatures for senstive operations (As described in filechecks.cpp)
    // More later

}

#endif // FILECHECKS_HPP